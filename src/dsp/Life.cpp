#include "Life.h"

namespace gx
{
namespace
{
inline float ss01 (float a, float b, float x) { const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a)); return t * t * (3.0f - 2.0f * t); }
inline uint32_t mix32 (uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
}

float Life::hashSlot (uint32_t salt) const { return (float) (mix32 (seed + salt * 7919u, (uint32_t) tickCount) >> 8) * (1.0f / 16777216.0f); }

void Life::prepare (double s)
{
    sr = s;
    dl[0].init ((int) (sr * 0.05)); dl[1].init ((int) (sr * 0.05));
    smear.init ((int) (sr * 0.12));
    for (auto* x : { &tapeS, &vinylS, &worldS }) x->setTime (0.08f, sr);
    for (auto& b : crackHp) b.highpass (sr, 1500.0);
    reset (seed);
}

void Life::reset (uint32_t s)
{
    seed = s; rng = Rng (mix32 (s, 77)); tickCount = 0;
    for (auto& e : ev) e = Ev();
    for (auto& b : birds) b = Bird();
    dl[0].clear(); dl[1].clear();
    for (auto& d : patter) d = Patter();
    for (int c = 0; c < 2; ++c) { rbp[c] = rrum[c] = rrum2[c] = 0; for (auto& x : rk[c]) x = 0; }
    gust = gustTarget = 1; gustCount = 0;
    for (auto& k : talk) k = Talker();
    smear.clear();
    wg1 = wg1t = 0.4f; wg2 = wg2t = wg3 = 0.5f; wc1 = wc2 = wCoef = 0; wPhase = 0;
    for (int c = 0; c < 2; ++c) { wRum[c] = wRum2[c] = 0; for (auto& x : wk[c]) x = 0; wBody[c].reset(); wAir[c].reset(); for (auto& f : wWhist[c]) f.reset(); }
    babLp[0] = babLp[1] = 0;
    wowPh = 0; flPh = 0.3f; drift = driftTarget = 0; dropG = dropTarget = 1; dropLeft = 0;
    for (auto& b : tapeLp) b.reset();
    for (auto& b : tapeHp) b.reset();
    for (auto& b : crackHp) b.reset();
    for (int c = 0; c < 2; ++c) { hissLp[c] = rumbleLp[c] = rumbleLp2[c] = bed[c] = bed2[c] = murEnv[c] = fireLp[c] = fireLp2[c] = rainBed[c] = 0; for (auto& m : mur) m[c] = 0; for (auto& f : fm) f[c].reset(); }
    popDone = false; popLeft = 0; birdQueue = 0; birdGap = 0; cacheTape = -1; murTarget = 0;
    tapeS.v = vinylS.v = worldS.v = 0;
}

void Life::setLevels (const LifeLevels& l) { lv = l; }

void Life::spawn (float f0, float f1, float q, float seconds, float amp, float pan, float freqTo)
{
    Ev* slot = nullptr;
    for (auto& e : ev) if (! e.on) { slot = &e; break; }
    if (slot == nullptr) slot = &ev[(size_t) (rng.next() % ev.size())];
    slot->on = true; slot->env = 1; slot->amp = amp; slot->pan = pan; slot->f0 = f0; slot->f1 = freqTo < 0 ? f0 : freqTo; slot->t = 0;
    slot->coef = std::exp (-6.9f / (seconds * (float) sr));
    slot->f.reset();
    slot->f.set (std::tan (kPi * juce::jmin (f0, (float) sr * 0.45f) / (float) sr), 1.0f / q);
    (void) f1;
}

void Life::noteOn (int note, float vel)
{
    const float w = lv.wood;
    if (w <= 0.001f) return;
    const float pan = juce::jlimit (0.0f, 1.0f, (float) (note - 21) / 87.0f);
    const float near = 0.4f + 1.2f * lv.intimacy;
    spawn (110.0f + (float) (108 - note) * 1.2f, 0, 1.2f, 0.035f, 0.05f * w * vel * near, 0.5f);                 // key hitting the key bed
    spawn (1700.0f + 25.0f * (float) (note - 60), 0, 2.5f, 0.007f, 0.035f * w * vel * near, pan);              // action click
}

void Life::noteOff (int note, float vel)
{
    const float w = lv.wood;
    if (w <= 0.001f) return;
    const float pan = juce::jlimit (0.0f, 1.0f, (float) (note - 21) / 87.0f);
    spawn (900.0f, 0, 1.6f, 0.012f, 0.02f * w * (0.4f + vel) * (0.4f + lv.intimacy), pan);                   // felt damper
}

void Life::pedal (bool down)
{
    const float w = lv.wood;
    if (w <= 0.001f) return;
    spawn (85.0f, 0, 0.9f, down ? 0.09f : 0.06f, (down ? 0.09f : 0.06f) * w * (0.5f + lv.intimacy), 0.5f);
    spawn (420.0f, 0, 1.5f, 0.02f, 0.015f * w, 0.5f);
}

void Life::tick()
{
    ++tickCount;
    // the bench creaks now and then, a tape dropout dips the level for a moment
    if (lv.wood > 0.05f && hashSlot (1) < 2.8e-5f * lv.wood)
        spawn (260.0f, 0, 6.0f, 0.22f, 0.05f * lv.wood * (0.4f + lv.intimacy), 0.4f + 0.2f * hashSlot (2), 520.0f);
    if (lv.tape > 0.2f && dropLeft <= 0 && hashSlot (3) < 1.2e-5f * lv.tape * lv.tape)
    { dropLeft = (int) ((0.03f + 0.2f * hashSlot (4)) * (float) sr); dropTarget = 0.35f + 0.4f * hashSlot (5); }
    if (dropLeft > 0) { dropLeft -= kControlBlock * 2; if (dropLeft <= 0) dropTarget = 1.0f; }
    if ((tickCount & 63) == 0) driftTarget = hashSlot (6) * 2.0f - 1.0f;
    // birds: a short song of 2-5 chirps
    if (lv.worldType == 4 && birdQueue == 0 && hashSlot (7) < 6.0e-4f)
    { birdQueue = 2 + (int) (hashSlot (8) * 4.0f); birdGap = 0; }
    if (birdQueue > 0 && --birdGap <= 0)
    {
        for (auto& b : birds)
            if (! b.on)
            {
                const float base = 2600.0f + 2400.0f * hashSlot (9 + birdQueue);
                b.on = true; b.ph = 0; b.f = base; b.df = (hashSlot (20 + birdQueue) - 0.4f) * base * 0.9f; b.t = 0; b.env = 0;
                b.len = (0.05f + 0.07f * hashSlot (30 + birdQueue)) * (float) sr; b.pan = 0.2f + 0.6f * hashSlot (40); break;
            }
        --birdQueue; birdGap = 8 + hashSlot (50) * 18;
    }
}

void Life::wood (float& l, float& r)
{
    for (auto& e : ev)
    {
        if (! e.on) continue;
        float bp, hp;
        e.t += 1.0f;
        const float s = e.f.tick (rng.bi() * e.env, bp, hp) * e.amp * 6.0f;
        e.env *= e.coef;
        if (e.f1 != e.f0 && ((int) e.t & 15) == 0)
        {
            const float k = juce::jlimit (0.0f, 1.0f, e.t / (0.22f * (float) sr));
            e.f.set (std::tan (kPi * (e.f0 + (e.f1 - e.f0) * k) / (float) sr), 1.0f / 6.0f);
        }
        if (e.env < 1.0e-4f) e.on = false;
        const float gl = std::cos (e.pan * 1.5708f), gr = std::sin (e.pan * 1.5708f);
        l += s * gl; r += s * gr;
    }
}

void Life::tape (float& l, float& r)
{
    const float a = tapeS.step (lv.tape);
    if (a <= 0.0005f && lv.tape <= 0.0005f) return;
    if (std::abs (a - cacheTape) > 0.01f)
    {
        cacheTape = a;
        for (auto& b : tapeLp) b.lowpass (sr, 16000.0 * std::pow (0.3, (double) a * 1.2), 0.6);
        for (auto& b : tapeHp) b.highpass (sr, 25.0 + 40.0 * a, 0.7);
        tapeSat = 1.0f + 2.5f * a;
    }
    wowPh += 0.55f / (float) sr; if (wowPh > 1) wowPh -= 1;
    flPh += 6.8f / (float) sr; if (flPh > 1) flPh -= 1;
    drift += (driftTarget - drift) * 2.0e-5f;
    dropG += (dropTarget - dropG) * 0.0015f;
    const float base = 0.004f * (float) sr;
    const float d = base + a * ((0.0011f * fastSin (wowPh) + 0.0013f * drift) * (float) sr + 0.00012f * fastSin (flPh) * (float) sr);
    dl[0].push (l); dl[1].push (r);
    l = dl[0].read (d); r = dl[1].read (d + 1.5f);
    l = fastTanh (l * tapeSat) / tapeSat;
    r = fastTanh (r * tapeSat) / tapeSat;
    l = tapeHp[0].tick (tapeLp[0].tick (l)) * dropG;
    r = tapeHp[1].tick (tapeLp[1].tick (r)) * dropG;
    // hiss
    const float h = 0.0022f * a * a;
    hissLp[0] += 0.3f * (rng.bi() - hissLp[0]); hissLp[1] += 0.3f * (rng.bi() - hissLp[1]);
    l += (rng.bi() - hissLp[0] * 0.6f) * h; r += (rng.bi() - hissLp[1] * 0.6f) * h;
}

void Life::murmur (float& l, float& r, float level, float dark, int talkers)
{
    // distant conversation: breathy and voiced vowels gliding through two formants, in phrases of 3-9 syllables with pauses between them
    const int var = juce::jlimit (0, 2, crowdVariant());
    const float voicedMix = var == 0 ? 0.0f : 0.55f;
    float sl = 0, sr_ = 0;
    for (int t = 0; t < talkers; ++t)
    {
        auto& k = talk[(size_t) t];
        if (--k.left <= 0)
        {
            if (k.phrase <= 0) { k.phrase = 3 + (int) (rng.next() % 7); k.left = (int) ((0.4f + 2.2f * rng.uni()) * (float) sr); k.envT = 0.0f;
                                 k.pitchT = 95.0f + 150.0f * rng.uni(); const float u = rng.uni(); k.pl = std::cos (u * 1.5708f); k.pr = std::sin (u * 1.5708f); }
            else
            {
                --k.phrase;
                k.left = (int) ((0.09f + 0.14f * rng.uni()) * (float) sr);
                k.envT = rng.uni() < 0.12f ? 0.0f : 0.35f + 0.65f * rng.uni();
                k.f1t = 300.0f + 520.0f * rng.uni(); k.f2t = 850.0f + 1500.0f * rng.uni();
                k.pitchT *= 1.0f + (rng.uni() - 0.5f) * 0.16f;                           // intonation
            }
        }
        k.env += (k.envT - k.env) * 0.0011f;
        k.f0 += (k.pitchT - k.f0) * 0.0008f;
        k.f1 += (k.f1t - k.f1) * 0.0007f; k.f2 += (k.f2t - k.f2) * 0.0007f;
        if ((++k.coef & 31) == 0)
        {
            k.s1.set (std::tan (kPi * juce::jmin (k.f1 * dark, 9000.0f) / (float) sr), 1.0f / 4.5f);
            k.s2.set (std::tan (kPi * juce::jmin (k.f2 * dark, 9000.0f) / (float) sr), 1.0f / 5.5f);
        }
        if (k.env < 1.0e-3f) continue;
        k.vib += 5.2f / (float) sr; if (k.vib > 1.0f) k.vib -= 1.0f;
        k.ph += k.f0 * (1.0f + 0.012f * fastSin (k.vib)) / (float) sr; if (k.ph > 1.0f) k.ph -= 1.0f;
        const float voiced = fastSin (k.ph) + 0.5f * fastSin (2.0f * k.ph) + 0.3f * fastSin (3.0f * k.ph) + 0.15f * fastSin (4.0f * k.ph);
        const float x = voicedMix * voiced + (1.0f - 0.5f * voicedMix) * rng.bi() * 0.9f;
        float bp1, hp1, bp2, hp2;
        k.s1.tick (x, bp1, hp1); k.s2.tick (x, bp2, hp2);
        const float v = (bp1 + 0.5f * bp2) * k.env;
        sl += v * k.pl; sr_ += v * k.pr;
    }
    float g = level * 0.06f / std::sqrt ((float) talkers);
    float ml = sl * g, mr = sr_ * g;
    if (var == 2 || dark < 1.0f)       // far away: the voices arrive smeared by the room
    {
        smear.push (0.5f * (ml + mr));
        const float d = 0.45f * smear.read (0.021f * (float) sr) + 0.35f * smear.read (0.037f * (float) sr) + 0.28f * smear.read (0.061f * (float) sr) + 0.2f * smear.read (0.089f * (float) sr);
        ml = 0.45f * ml + d; mr = 0.45f * mr + d * 0.92f;
    }
    babLp[0] += (0.22f * dark) * (ml - babLp[0]);
    babLp[1] += (0.22f * dark) * (mr - babLp[1]);
    l += babLp[0]; r += babLp[1];
}

void Life::world (float& l, float& r)
{
    // ---- vinyl
    const float v = vinylS.step (lv.vinyl);
    if (lv.vinyl > 0.0005f || v > 0.0005f)
    {
        if (! popDone) { popDone = true; popLeft = (int) (0.02 * sr); }
        for (int c = 0; c < 2; ++c)
        {
            float s = 0;
            if (rng.uni() < 0.00035f + 0.0012f * v * v) { const float u = rng.uni(); s = (rng.bi() > 0 ? 1.0f : -1.0f) * u * u * u * 0.12f; }
            s += (rng.bi() * 0.0008f) * v;      // constant dust
            s = crackHp[c].tick (s) * v * 1.6f;
            rumbleLp[c] += 0.004f * (rng.bi() - rumbleLp[c]);
            rumbleLp2[c] += 0.05f * (rumbleLp[c] - rumbleLp2[c]);
            s += rumbleLp2[c] * 0.35f * v;
            (c == 0 ? l : r) += s;
        }
        if (popLeft > 0) { const float pop = (popLeft % 5 < 2 ? 0.09f : -0.06f) * (float) popLeft / (0.02f * (float) sr) * v; l += pop; r += pop * 0.8f; --popLeft; }
    }
    // ---- world
    const float w = worldS.step (lv.worldType > 0 ? lv.world * 0.6f : 0.0f);   // ambiences are capped: they are a background
    if (w <= 0.0005f) return;
    switch (lv.worldType)
    {
        case 1: // rain on a window: a soft pink bed, a patter of tiny drops, bigger resonant drops, a distant roof rumble
        {
            static const float bedA[3] { 0.0054f, 0.0090f, 0.0135f }, rate[3] { 160.0f, 380.0f, 700.0f }, big[3] { 2.0f, 4.0f, 7.0f }, rum[3] { 0.0f, 0.0038f, 0.0082f };
            const int vr = juce::jlimit (0, 2, rainVariant());
            if (++gustCount >= 8192) { gustCount = 0; gustTarget = 0.65f + 0.7f * rng.uni(); }
            gust += (gustTarget - gust) * 3.0e-5f;
            for (int c = 0; c < 2; ++c)
            {
                const float wn = rng.bi();
                rk[c][0] = 0.99765f * rk[c][0] + wn * 0.0990460f;
                rk[c][1] = 0.96300f * rk[c][1] + wn * 0.2965164f;
                rk[c][2] = 0.57000f * rk[c][2] + wn * 1.0526913f;
                const float pink = (rk[c][0] + rk[c][1] + rk[c][2] + wn * 0.1848f) * 0.11f;
                rbp[c] += 0.05f * (pink - rbp[c]);                      // remove the low end: it is the sound of the window, not of a storm
                float s = (pink - rbp[c]) * bedA[vr] * gust;
                rrum[c] += 0.0025f * (rng.bi() - rrum[c]); rrum2[c] += 0.01f * (rrum[c] - rrum2[c]);
                s += rrum2[c] * rum[vr] * 12.0f * (0.7f + 0.3f * gust);
                (c == 0 ? l : r) += s * w;
            }
            if (rng.uni() < rate[vr] * gust / (float) sr)
                for (auto& d : patter)
                    if (! d.on)
                    {
                        const float f = 1800.0f + 5200.0f * rng.uni() * rng.uni(), om = kTwoPi * f / (float) sr, u = rng.uni();
                        d.on = true; d.c = std::cos (om); d.s = std::sin (om); d.re = 1; d.im = 0; d.env = 1;
                        d.coef = std::exp (-6.9f / ((0.0015f + 0.004f * rng.uni()) * (float) sr));
                        d.amp = std::exp (-3.0f * rng.uni()) * 0.020f; d.pl = std::cos (u * 1.5708f); d.pr = std::sin (u * 1.5708f);
                        break;
                    }
            for (auto& d : patter)
            {
                if (! d.on) continue;
                const float nre = d.re * d.c - d.im * d.s; d.im = d.re * d.s + d.im * d.c; d.re = nre;
                const float v = d.im * d.env * d.amp * w;
                d.env *= d.coef;
                if (d.env < 1.0e-3f) d.on = false;
                l += v * d.pl; r += v * d.pr;
            }
            if (rng.uni() < big[vr] * gust / (float) sr)
                spawn (700.0f + 1900.0f * rng.uni(), 0, 14.0f, 0.045f, 0.020f * w, rng.uni());       // a bigger drop on the glass
            break;
        }
        case 2: // wind
        {
            if (--wc1 <= 0) { wc1 = (int) ((5.0f + 9.0f * rng.uni()) * (float) sr); wg1t = juce::jlimit (0.10f, 1.0f, 0.12f + 1.3f * rng.uni() * rng.uni()); }
            if (--wc2 <= 0) { wc2 = (int) ((0.8f + 2.2f * rng.uni()) * (float) sr); wg2t = rng.uni(); }
            wg1 += (wg1t - wg1) * 6.0e-6f;
            wg2 += (wg2t - wg2) * 4.0e-5f;
            wg3 += (rng.uni() - wg3) * 3.0e-4f;
            wPhase += 1.0f / (float) sr;
            const float gl = juce::jlimit (0.0f, 1.0f, wg1 * (0.55f + 0.45f * wg2) * (0.75f + 0.5f * wg3));
            if ((++wCoef & 15) == 0)
            {
                const float fc = 170.0f + 1500.0f * gl * std::sqrt (gl), fa = 2600.0f + 2400.0f * gl;
                static const float base[3] { 610.0f, 930.0f, 1380.0f };
                for (int c = 0; c < 2; ++c)
                {
                    wBody[c].set (std::tan (kPi * fc * (c ? 1.07f : 1.0f) / (float) sr), 1.0f / 0.9f);
                    wAir[c].set (std::tan (kPi * fa / (float) sr), 1.0f / 0.7f);
                    for (int i = 0; i < 3; ++i)
                    {
                        const float f = base[i] * (1.0f + 0.05f * std::sin (wPhase * (0.6f + 0.2f * (float) i) + (float) i * 2.1f + (float) c * 0.9f) + 0.10f * (gl - 0.5f));
                        wWhist[c][i].set (std::tan (kPi * f / (float) sr), 1.0f / 14.0f);
                    }
                }
            }
            const float whistleAmt = ss01 (0.50f, 0.95f, gl);
            for (int c = 0; c < 2; ++c)
            {
                const float wn = rng.bi();
                wk[c][0] = 0.99765f * wk[c][0] + wn * 0.0990460f;
                wk[c][1] = 0.96300f * wk[c][1] + wn * 0.2965164f;
                wk[c][2] = 0.57000f * wk[c][2] + wn * 1.0526913f;
                const float pink = (wk[c][0] + wk[c][1] + wk[c][2] + wn * 0.1848f) * 0.11f;
                float bp, hp;
                const float body = wBody[c].tick (pink, bp, hp);
                float air; { float lp2, bp2; air = 0; lp2 = wAir[c].tick (wn, bp2, air); (void) lp2; }
                float whistle = 0;
                for (int i = 0; i < 3; ++i) { float b2, h2; wWhist[c][i].tick (wn, b2, h2); whistle += b2 * (i == 1 ? 1.0f : 0.7f); }
                wRum[c] += 0.010f * (rng.bi() - wRum[c]); wRum2[c] += 0.05f * (wRum[c] - wRum2[c]);
                const float s = body * 0.55f * (0.15f + gl) + air * 0.010f * gl * gl + whistle * 0.045f * whistleAmt * whistleAmt + wRum2[c] * 0.45f * gl;
                (c == 0 ? l : r) += s * w;
            }
            if (rng.uni() < 0.0009f * gl * gl * gl)
                spawn (3200.0f + 2600.0f * rng.uni(), 0, 2.2f, 0.07f, 0.010f * w * (0.5f + gl), rng.uni());     // leaves rustling in a strong gust
            break;
        }
        case 3: // fireplace
            for (int c = 0; c < 2; ++c)
            {
                bed[c] += 0.02f * (rng.bi() - bed[c]); bed2[c] += 0.03f * (bed[c] - bed2[c]);
                float s = bed2[c] * 0.35f;
                if (rng.uni() < 0.0009f) fireLp[c] = rng.uni() * rng.uni() * 0.25f;
                fireLp[c] *= 0.93f;
                fireLp2[c] += 0.5f * (fireLp[c] * rng.bi() - fireLp2[c]);
                (c == 0 ? l : r) += (s + fireLp2[c]) * w * 0.5f;
            }
            break;
        case 4: // dawn birds over a soft air bed
            for (int c = 0; c < 2; ++c) { bed[c] += 0.05f * (rng.bi() - bed[c]); (c == 0 ? l : r) += bed[c] * 0.004f * w; }
            for (auto& b : birds)
            {
                if (! b.on) continue;
                b.t += 1.0f;
                const float u = b.t / b.len;
                if (u >= 1.0f) { b.on = false; continue; }
                b.ph += (b.f + b.df * u) / (float) sr;
                if (b.ph > 1) b.ph -= 1;
                const float s = fastSin (b.ph) * std::sin (kPi * u) * (0.5f + 0.5f * fastSin (b.t * 90.0f / (float) sr)) * 0.05f * w;
                l += s * std::cos (b.pan * 1.5708f); r += s * std::sin (b.pan * 1.5708f);
            }
            break;
        default: break;
    }
}
}
