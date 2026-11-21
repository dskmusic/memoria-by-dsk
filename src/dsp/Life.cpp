#include "Life.h"

namespace gx
{
namespace
{
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
    if (lv.wood <= 0.001f) return;
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
    const float h = 0.0035f * a * a;
    hissLp[0] += 0.3f * (rng.bi() - hissLp[0]); hissLp[1] += 0.3f * (rng.bi() - hissLp[1]);
    l += (rng.bi() - hissLp[0] * 0.6f) * h; r += (rng.bi() - hissLp[1] * 0.6f) * h;
}

void Life::murmur (float& l, float& r, float level, float dark)
{
    // distant voices: noise through speech-like formants, with slow random syllable-rate swells
    static const float fr[3] { 420.0f, 1100.0f, 2300.0f };
    const float kq = dark < 1.0f ? 0.2001f : 0.2f;
    if ((rng.next() & 4095) == 0) murTarget = rng.uni();
    murEnv[0] += (murTarget - murEnv[0]) * 4.0e-5f;
    murEnv[1] += (murEnv[0] - murEnv[1]) * 4.0e-4f;
    const float env = 0.3f + 0.9f * murEnv[1];
    for (int c = 0; c < 2; ++c)
    {
        float s = 0;
        for (int k = 0; k < 3; ++k)
        {
            if (fm[k][c].k != kq) fm[k][c].set (std::tan (kPi * fr[k] * dark / (float) sr), kq);
            float bp, hp;
            fm[k][c].tick (rng.bi(), bp, hp);
            s += bp * (k == 0 ? 1.0f : k == 1 ? 0.7f : 0.35f);
        }
        (c == 0 ? l : r) += s * level * env * 0.22f;
    }
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
            s += (rng.bi() * 0.0018f) * v;      // constant dust
            s = crackHp[c].tick (s) * v * 1.6f;
            rumbleLp[c] += 0.004f * (rng.bi() - rumbleLp[c]);
            rumbleLp2[c] += 0.05f * (rumbleLp[c] - rumbleLp2[c]);
            s += rumbleLp2[c] * 0.35f * v;
            (c == 0 ? l : r) += s;
        }
        if (popLeft > 0) { const float pop = (popLeft % 5 < 2 ? 0.09f : -0.06f) * (float) popLeft / (0.02f * (float) sr) * v; l += pop; r += pop * 0.8f; --popLeft; }
    }
    // ---- world
    const float w = worldS.step (lv.worldType > 0 ? lv.world : 0.0f);
    if (w <= 0.0005f) return;
    switch (lv.worldType)
    {
        case 1: // rain on the window
            for (int c = 0; c < 2; ++c)
            {
                bed[c] += 0.35f * (rng.bi() - bed[c]); bed2[c] += 0.02f * (bed[c] - bed2[c]);
                float s = (bed[c] - bed2[c] * 1.0f) * 0.045f;
                if (rng.uni() < 0.0016f) rainBed[c] = (0.3f + rng.uni()) * 0.12f;
                rainBed[c] *= 0.985f;
                s += rainBed[c] * rng.bi();
                (c == 0 ? l : r) += s * w;
            }
            if (rng.uni() < 0.00006f) spawn (320.0f + 200.0f * rng.uni(), 0, 3.0f, 0.03f, 0.05f * w, rng.uni());   // heavy drop on the sill
            break;
        case 2: murmur (l, r, 0.9f * w, 1.0f);
            if (rng.uni() < 0.000012f) spawn (3200.0f + 1800.0f * rng.uni(), 0, 25.0f, 0.16f, 0.07f * w, rng.uni());   // cup clink
            break;
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
        case 5: murmur (l, r, 0.55f * w, 0.7f); break;   // distant audience
        default: break;
    }
}
}
