#include "Piano.h"

namespace gx
{
namespace
{
const float kBarR[] { 1.0f, 2.756f, 5.404f, 8.933f, 13.344f, 18.64f, 24.81f };
const float kToyA[] { 1.0f, 0.55f, 0.30f, 0.18f, 0.10f, 0.06f, 0.035f };
const float kCelA[] { 1.0f, 0.16f, 0.05f, 0.016f, 0.007f, 0.003f, 0.0015f };

//                         name       B    T     fc    pFast det   x0     slope thump body  lp     nStr  ratios amps  n
const PianoModel kModels[] {
    { "Grand",   1.0f, 1.00f, 1.00f, 0.50f, 0.6f, 0.125f, 0.90f, 1.0f, 1.0f, 20000.0f, 0, nullptr, nullptr, 0 },
    { "Upright", 1.4f, 0.55f, 0.80f, 0.62f, 1.3f, 0.115f, 1.05f, 1.8f, 1.7f, 14000.0f, 0, nullptr, nullptr, 0 },
    { "Felt",    1.0f, 0.65f, 0.28f, 0.45f, 0.7f, 0.125f, 1.00f, 0.5f, 1.0f,  2600.0f, 0, nullptr, nullptr, 0 },
    { "Honky",   1.8f, 0.80f, 1.15f, 0.55f, 9.0f, 0.110f, 0.85f, 1.3f, 1.3f, 12000.0f, 3, nullptr, nullptr, 0 },
    { "Toy",     1.0f, 0.10f, 1.60f, 0.50f, 0.0f, 0.100f, 0.00f, 1.6f, 0.6f, 16000.0f, 1, kBarR, kToyA, 7 },
    { "Celesta", 1.0f, 0.30f, 0.90f, 0.50f, 0.0f, 0.100f, 0.00f, 0.2f, 0.5f, 16000.0f, 1, kBarR, kCelA, 7 },
};

inline uint32_t hash32 (uint32_t a, uint32_t b = 0, uint32_t c = 0)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (c + 0x165667B1u) * 0xC2B2AE35u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
inline float hashUni (uint32_t a, uint32_t b = 0, uint32_t c = 0) { return (float) (hash32 (a, b, c) >> 8) * (1.0f / 16777216.0f); }

// string inharmonicity coefficient by note (grows steeply in the treble: short, stiff strings)
float inharmB (int n)
{
    return n < 45 ? 0.00022f + 0.000012f * (float) (45 - n) : 0.00025f * std::exp (0.0545f * (float) (n - 45));
}
// stretch tuning: bass flat, treble sharp (cents)
float stretchCents (int n)
{
    return n < 60 ? -0.085f * std::pow ((float) (60 - n), 1.6f) : 0.045f * std::pow ((float) (n - 60), 1.7f);
}
inline float rateToFactor (float sigma, float sr) { const float x = sigma / sr; return 1.0f - x + 0.5f * x * x; }
}

const PianoModel& pianoModel (int t) { return kModels[juce::jlimit (0, numPianoTypes() - 1, t)]; }
int numPianoTypes() { return (int) (sizeof (kModels) / sizeof (kModels[0])); }

void Engine::prepare (double sampleRate, int mb)
{
    sr = sampleRate;
    life.prepare (sr);
    maxBlock = juce::jmax (mb, 16);
    vbuf.assign ((size_t) maxBlock, 0.0f); mixL = bodyIn = mixR = vbuf;
    room.prepare (sr);
    for (auto* s : { &masterS, &bodyS, &roomS, &widthS }) s->setTime (0.05f, sr);
    // soundboard modes: a handful of broad wooden resonances, slightly different left/right
    static const float f[kModes] { 62, 94, 128, 171, 233, 312, 417, 560, 760, 1050 };
    static const float q[kModes] { 9, 11, 14, 14, 16, 18, 20, 22, 24, 26 };
    for (int i = 0; i < kModes; ++i)
    {
        const float g = std::tan (juce::MathConstants<float>::pi * f[i] / (float) sr), gr = std::tan (juce::MathConstants<float>::pi * f[i] * 1.043f / (float) sr);
        modeL[i].set (g, 1.0f / q[i]);
        modeR[i].set (gr, 1.0f / q[i]);
        modeG[i] = 0.5f / std::sqrt ((float) (i + 2));
    }
    reset (seed);
}

void Engine::reset (uint32_t s)
{
    seed = s;
    for (auto& v : voices) v = Voice();
    for (auto& g : ghosts) g = Ghost();
    activeGhosts = 0;
    keyDown.fill (false);
    pedalTarget = pedalNow = 0;
    tickLeft = kTick; counter = 0;
    room.clear();
    life.reset (seed);
    pedalWasDown = false;
    for (auto& m : modeL) m.reset();
    for (auto& m : modeR) m.reset();
    for (auto& b : lpF) b.reset();
    for (auto& b : toneF) b.reset();
    cacheLp = cacheTone = cacheRoom = -1;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
    idleTicks = 0;
    for (auto& k : tele.keyLevel) k.store (0.0f);
    tele.voices.store (0);
    tele.pedal.store (0.0f);
    masterS.v = bodyS.v = roomS.v = widthS.v = 0.0f; masterS.v = -1.0f;
}

int Engine::allocVoice()
{
    int live = 0;
    for (auto& v : voices) if (v.active && ! v.fading) ++live;
    if (live >= kPolyphony)
    {
        int best = -1; float bestScore = 1.0e30f;
        for (int i = 0; i < kVoiceSlots; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.active || v.fading) continue;
            const float score = (v.held ? 1000.0f : 0.0f) + v.level * 100.0f - (float) (counter - v.age) * 1.0e-6f;
            if (score < bestScore) { bestScore = score; best = i; }
        }
        if (best >= 0) { voices[(size_t) best].fading = true; voices[(size_t) best].fadeG = 1.0f; }
    }
    for (int i = 0; i < kVoiceSlots; ++i) if (! voices[(size_t) i].active) return i;
    // pool exhausted by fading voices: cut the quietest one
    int q = 0; float ql = 1.0e30f;
    for (int i = 0; i < kVoiceSlots; ++i) if (voices[(size_t) i].level < ql) { ql = voices[(size_t) i].level; q = i; }
    return q;
}

// (re)computes the per-sample rotation of every component from the damper state
void Engine::setRates (Voice& v)
{
    const float fs = (float) sr;
    for (int i = 0; i < v.n; ++i)
    {
        auto& c = v.comp[i];
        const float r = rateToFactor (c.sig + v.dmp * v.dmp * c.dsig, fs);
        c.rc = r * c.c; c.rs = r * c.s;
    }
    v.dmpLast = v.dmp;
}

void Engine::excite (Voice& v, int note, float vel, bool restrike)
{
    const auto& m = pianoModel ((int) p (P::piano));
    const bool bar = m.ratios != nullptr;
    const float fs = (float) sr;
    const float sustainMul = std::exp2 ((p (P::sustain) - 0.5f) * 2.2f);
    const float dampMul = 0.4f + 1.2f * p (P::damper);
    const float stretch = p (P::stretch) * 1.6f;
    const float dyn = p (P::dynamics);
    const float tone = p (P::tone), hammer = p (P::hammer);

    const float cents = bar ? 0.0f : stretchCents (note) * stretch;
    const float f0 = a4 * std::exp2 (((float) note - 69.0f + cents * 0.01f) / 12.0f);
    const float B = bar ? 0.0f : inharmB (note) * m.bMul * stretch;
    const int nStr = m.nStr > 0 ? (m.nStr == 3 && note < 30 ? 1 : m.nStr) : (note < 30 ? 1 : note < 46 ? 2 : 3);
    const int nc = juce::jmax (2, nStr);

    // hammer: felt hardness (spectral cut-off) rises steeply with velocity
    const float ampDb = (vel - 1.0f) * (18.0f + 36.0f * dyn);
    const float ampLin = std::pow (10.0f, ampDb * 0.05f);
    const float fc = 450.0f * std::exp2 (vel * 3.7f) * m.fcMul * std::exp2 ((tone - 0.5f) * 2.4f) * std::exp2 ((0.5f - hammer) * 2.0f);
    const float x0 = m.x0 * (0.93f + 0.14f * hashUni (seed, (uint32_t) note, 7));

    int K = bar ? m.numRatios : 60;
    const float nyq = 0.45f * fs;
    struct P1 { float f, a; };
    std::array<P1, 64> parts;
    int np = 0;
    float sumSq = 0;
    for (int k = 1; k <= K && np < 64; ++k)
    {
        const float fk = bar ? f0 * m.ratios[k - 1] : (float) k * f0 * std::sqrt (1.0f + B * (float) (k * k));
        if (fk > nyq) break;
        if (np * nc + nc > kMaxComp) break;
        float a;
        if (bar) a = m.ratioAmp[k - 1] * std::pow (1.0f + (fk / fc) * (fk / fc), -0.8f);
        else
        {
            const float comb = 0.15f + std::abs (std::sin (juce::MathConstants<float>::pi * (float) k * x0));
            a = comb * std::pow ((float) k, -m.slope) * std::pow (1.0f + (fk / fc) * (fk / fc), -0.8f);
        }
        a /= 1.0f + (70.0f / fk) * (70.0f / fk);      // the soundboard does not radiate the very low end
        parts[(size_t) np++] = { fk, a };
        sumSq += a * a;
    }
    if (np == 0) { v.active = false; return; }
    const float norm = 0.24f * ampLin * 1.4142f / std::sqrt (sumSq) * std::pow (10.0f, std::min (12.0f, 0.26f * (float) juce::jmax (0, note - 64)) * 0.05f);

    const float T1 = 30.0f * std::exp (-0.022f * (float) (note - 21)) * m.tMul * sustainMul;
    const float sig1 = 6.9078f / juce::jmax (0.05f, T1);
    const float detCents = (0.15f + 1.6f * p (P::detune)) * m.detCents;

    // one extra fast component per partial gives the prompt sound; the others are the (detuned) unison strings
    int ci = restrike ? 0 : 0;
    const int base = restrike ? v.n : 0;
    if (restrike && base + np * nc > kMaxComp) { v.n = 0; }   // not enough room: the old tail is replaced (voice keeps ringing from the new strike)
    ci = restrike ? v.n : 0;
    const float pf = m.pFast;
    for (int k = 0; k < np; ++k)
    {
        const float fk = parts[(size_t) k].f, a = parts[(size_t) k].a * norm;
        const float lossSlow = 1.1f * std::pow (fk * 0.001f, 1.6f) / juce::jmax (0.3f, m.tMul > 0.9f ? 1.0f : m.tMul * 1.4f);
        const float sigSlow = sig1 + lossSlow;
        const float sigFast = sigSlow * 3.5f + 1.6f / juce::jmax (0.15f, m.tMul);
        const float sigD = (14.0f + 5.0f * std::sqrt (fk * 0.01f)) * dampMul;
        for (int j = 0; j < nc; ++j)
        {
            const float detune = j == 0 ? 0.0f : ((j & 1) ? 1.0f : -1.0f) * detCents * (0.5f + hashUni (seed, (uint32_t) note * 64u + (uint32_t) j, (uint32_t) k) * 0.9f);
            const float w = juce::MathConstants<float>::twoPi * fk * std::exp2 (detune / 1200.0f) / fs;
            auto& c = v.comp[ci++];
            const bool fast = j == 0;
            const float amp = fast ? a * pf : a * (1.0f - pf) * 2.0f / (float) (nc - 1);
            if (restrike) { c.re += amp; }
            else { c.re = amp; c.im = 0; }
            c.c = std::cos (w); c.s = std::sin (w);
            c.sig = fast ? sigFast : sigSlow;
            c.dsig = sigD;
        }
    }
    v.n = ci;

    v.thumpGain = 0.20f * m.thump * vel * vel * ampLin * (0.5f + 0.5f * hammer);
    v.thump = 1.0f;
    v.thumpCoef = std::exp (-1.0f / (0.012f * fs));
    const float tc = juce::jlimit (90.0f, 900.0f, f0 * 1.5f);
    v.thumpF.set (std::tan (juce::MathConstants<float>::pi * tc / fs), 1.4f);
    v.att = 0.0f;
    v.attCoef = 1.0f - std::exp (-1.0f / ((0.0006f + (1.0f - vel) * 0.0022f) * fs));
}

void Engine::exciteGhosts (int note, float vel)
{
    const float res = p (P::resonance);
    if (res <= 0.001f || pedalNow < 0.08f) return;
    const float fn = a4 * std::exp2 (((float) note - 69.0f) / 12.0f);
    const float fs = (float) sr;
    for (int m = kLowKey; m <= kHighKey; ++m)
    {
        if (m == note) continue;
        auto& g = ghosts[(size_t) (m - kLowKey)];
        const float fm = a4 * std::exp2 (((float) m - 69.0f) / 12.0f);
        float couple = 0;
        for (int i = 1; i <= 4; ++i)
            for (int j = 1; j <= 4; ++j)
            {
                const float ratio = (float) j * fn / ((float) i * fm);
                const float dev = std::abs (std::log2 (ratio)) * 1200.0f;     // cents
                if (dev < 14.0f) couple = juce::jmax (couple, (1.0f - dev / 14.0f) / (float) (i * j));
            }
        if (couple <= 0.0f) continue;
        const float amp = couple * res * 0.045f * vel;
        if (! g.on)
        {
            g.on = true; g.f = fm;
            for (int h = 0; h < 2; ++h)
            {
                const float w = juce::MathConstants<float>::twoPi * fm * (float) (h + 1) / fs * (h ? std::sqrt (1.0f + 0.0004f * 4.0f) : 1.0f);
                g.c[h] = std::cos (w); g.s[h] = std::sin (w); g.re[h] = g.im[h] = 0;
            }
            ++activeGhosts;
        }
        g.re[0] += amp; g.re[1] += amp * 0.4f;
    }
}

void Engine::noteOn (int note, float vel)
{
    note = juce::jlimit (0, 127, note);
    keyDown[(size_t) note] = true;
    life.noteOn (note, vel);
    tele.keyHit[(size_t) note].store (vel);
    tele.hitCounter.fetch_add (1);
    auto& v = voices[(size_t) allocVoice()];
    const int slot = (int) (&v - voices.data());
    (void) slot;
    v = Voice();
    v.active = true; v.held = true; v.note = note; v.vel = vel; v.age = counter;
    v.rng = Rng (hash32 (seed, (uint32_t) note, (uint32_t) counter));
    const float pan = juce::jlimit (0.0f, 1.0f, (float) (note - kLowKey) / (float) (kHighKey - kLowKey));
    const float th = (0.5f + (pan - 0.5f) * 0.9f * widthS.v) * juce::MathConstants<float>::halfPi;
    v.panL = std::cos (th); v.panR = std::sin (th);
    excite (v, note, vel, false);
    setRates (v);
    exciteGhosts (note, vel);
}

void Engine::noteOff (int note)
{
    keyDown[(size_t) juce::jlimit (0, 127, note)] = false;
    for (auto& v : voices) if (v.active && v.note == note && v.held) { v.held = false; life.noteOff (note, v.vel); }
}

void Engine::setPedal (float v)
{
    const float x = juce::jlimit (0.0f, 1.0f, v);
    const float u = juce::jlimit (0.0f, 1.0f, (x - 0.08f) / 0.52f);   // the dampers are fully off the strings at ~60 % of the travel
    pedalTarget = u * u * (3.0f - 2.0f * u);
    if ((x > 0.5f) != pedalWasDown) { pedalWasDown = x > 0.5f; life.pedal (pedalWasDown); }
}

void Engine::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), juce::jlimit (0.01f, 1.0f, m.getFloatVelocity()));
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 64) setPedal ((float) m.getControllerValue() / 127.0f);
        else if (cc == 123 || cc == 120) { for (int n = 0; n < 128; ++n) noteOff (n); if (cc == 120) for (auto& v : voices) v.fading = v.active; }
    }
}

// once per absolute tick: pedal and dampers, culling of dead components, telemetry
void Engine::tick()
{
    life.tick();
    pedalNow += (pedalTarget - pedalNow) * (pedalTarget > pedalNow ? 0.35f : 0.12f);
    if (std::abs (pedalTarget - pedalNow) < 1.0e-4f) pedalNow = pedalTarget;
    const float damperTau = 0.020f + 0.055f * (1.0f - p (P::damper)) ;           // how fast the felt comes down
    const float fall = 1.0f - std::exp (-(float) kTick / ((float) sr * damperTau));
    int live = 0;
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const bool undamped = v.held || v.note >= 93;
        const float target = undamped ? 0.0f : 1.0f - pedalNow;
        v.dmp += (target - v.dmp) * (target > v.dmp ? fall : 0.4f);
        if (std::abs (v.dmp - v.dmpLast) > 0.004f || (v.dmp == 0.0f && v.dmpLast != 0.0f)) setRates (v);
        float e = 0;
        for (int i = 0; i < v.n;)
        {
            auto& c = v.comp[i];
            const float m2 = c.re * c.re + c.im * c.im;
            if (m2 < 1.0e-16f) { c = v.comp[--v.n]; continue; }
            e += m2;
            ++i;
        }
        v.level = std::sqrt (e);
        if (v.n == 0 || (v.level < 3.0e-6f && ! v.held)) v.active = false;
        else ++live;
        v.age += 0;
    }
    // telemetry: loudest voice per key
    for (int k = 0; k < 128; ++k) tele.keyLevel[(size_t) k].store (tele.keyLevel[(size_t) k].load() * 0.0f);
    for (auto& v : voices)
        if (v.active) { auto& kl = tele.keyLevel[(size_t) v.note]; kl.store (juce::jmax (kl.load(), juce::jmin (1.0f, v.level * 2.5f))); }
    tele.voices.store (live);
    tele.pedal.store (pedalNow);

    // ghosts: sympathetic strings follow the pedal (fully damped when it is down on the strings)
    if (activeGhosts > 0)
    {
        const float sig = 0.8f + (1.0f - pedalNow) * 40.0f;
        const float r = rateToFactor (sig, (float) sr);
        for (auto& g : ghosts)
        {
            if (! g.on) continue;
            for (int h = 0; h < 2; ++h) { g.rc[h] = r * g.c[h]; g.rs[h] = r * g.s[h]; }
            if (g.re[0] * g.re[0] + g.im[0] * g.im[0] < 1.0e-14f) { g.on = false; --activeGhosts; }
        }
    }
}

void Engine::renderChunk (float* L, float* R, int n)
{
    std::fill (mixL.begin(), mixL.begin() + n, 0.0f);
    std::fill (mixR.begin(), mixR.begin() + n, 0.0f);
    float* vb = vbuf.data();
    for (auto& v : voices)
    {
        if (! v.active) continue;
        std::fill (vb, vb + n, 0.0f);
        for (int i = 0; i < v.n; ++i)
        {
            auto& c = v.comp[i];
            float x = c.re, y = c.im;
            const float rc = c.rc, rs = c.rs;
            for (int t = 0; t < n; ++t)
            {
                const float nx = x * rc - y * rs;
                y = x * rs + y * rc;
                x = nx;
                vb[t] += y;
            }
            c.re = x; c.im = y;
        }
        const float tg = v.thumpGain;
        for (int t = 0; t < n; ++t)
        {
            v.att += (1.0f - v.att) * v.attCoef;
            float s = vb[t] * v.att;
            if (v.thump > 1.0e-4f)
            {
                float bp, hp;
                s += v.thumpF.tick (v.rng.bi() * v.thump * tg, bp, hp);
                v.thump *= v.thumpCoef;
            }
            if (v.fading) { v.fadeG -= 1.0f / 192.0f; if (v.fadeG <= 0.0f) { v.fadeG = 0.0f; v.active = false; } s *= v.fadeG; }
            mixL[(size_t) t] += s * v.panL;
            mixR[(size_t) t] += s * v.panR;
        }
    }
    if (activeGhosts > 0)
    {
        for (auto& g : ghosts)
        {
            if (! g.on) continue;
            const float pan = juce::jlimit (0.0f, 1.0f, (g.f > 0 ? (std::log2 (g.f / 27.5f) / 7.0f) : 0.5f));
            const float gl = std::cos (pan * juce::MathConstants<float>::halfPi), gr = std::sin (pan * juce::MathConstants<float>::halfPi);
            float x0 = g.re[0], y0 = g.im[0], x1 = g.re[1], y1 = g.im[1];
            for (int t = 0; t < n; ++t)
            {
                const float nx0 = x0 * g.rc[0] - y0 * g.rs[0]; y0 = x0 * g.rs[0] + y0 * g.rc[0]; x0 = nx0;
                const float nx1 = x1 * g.rc[1] - y1 * g.rs[1]; y1 = x1 * g.rs[1] + y1 * g.rc[1]; x1 = nx1;
                const float s = y0 + y1;
                mixL[(size_t) t] += s * gl; mixR[(size_t) t] += s * gr;
            }
            g.re[0] = x0; g.im[0] = y0; g.re[1] = x1; g.im[1] = y1;
        }
    }
    // soundboard modes + felt low-pass + room + master
    const auto& m = pianoModel ((int) p (P::piano));
    const float bodyAmt = p (P::body) * m.bodyMul, roomAmt = p (P::room), master = dbToGain (p (P::master));
    const float lpTarget = juce::jlimit (1500.0f, 20000.0f, m.lpHz * std::exp2 ((p (P::tone) - 0.5f) * 1.2f));
    if (std::abs (lpTarget - cacheLp) > 1.0f) { cacheLp = lpTarget; for (auto& b : lpF) b.lowpass (sr, lpTarget, 0.62); }
    const float toneDb = (p (P::tone) - 0.5f) * 7.0f;
    if (std::abs (toneDb - cacheTone) > 0.01f) { cacheTone = toneDb; for (auto& b : toneF) b.shelf (sr, 3500.0, toneDb, true); }
    const float rs = p (P::roomSize);
    const int rt = juce::jlimit (0, 4, (int) p (P::roomType));
    const float rKey = rs + (float) rt * 10.0f;
    if (std::abs (rKey - cacheRoom) > 0.002f)
    {
        cacheRoom = rKey;
        //                                      living church studio cabin hall
        static const float size[] { 0.10f, 0.75f, 0.06f, 0.14f, 0.95f }, damp[] { 0.55f, 0.35f, 0.7f, 0.25f, 0.4f }, rtS[] { 0.8f, 1.1f, 0.6f, 0.9f, 1.0f };
        room.set (size[rt] + 0.25f * (rs - 0.5f) * (1.0f - 0.3f * size[rt]), damp[rt], rtS[rt]);
    }
    for (int t = 0; t < n; ++t)
    {
        const float bAmt = bodyS.step (bodyAmt), rMix = roomS.step (roomAmt), mg = masterS.step (master);
        const float dryL = mixL[(size_t) t], dryR = mixR[(size_t) t];
        float wl = dryL, wr = dryR;
        life.wood (wl, wr);
        const float in = 0.5f * (wl + wr);
        float bl = 0, br = 0;
        for (int i = 0; i < kModes; ++i)
        {
            float bp, hp;
            modeL[i].tick (in, bp, hp); bl += bp * modeG[i];
            modeR[i].tick (in, bp, hp); br += bp * modeG[i];
        }
        float l = wl + bl * bAmt * 0.6f, r = wr + br * bAmt * 0.6f;
        l = toneF[0].tick (lpF[0].tick (l)); r = toneF[1].tick (lpF[1].tick (r));
        float rl = 0, rr = 0;
        room.process (l, r, rl, rr);
        l += rl * rMix * 0.5f; r += rr * rMix * 0.5f;
        life.tape (l, r);
        life.world (l, r);
        l *= mg; r *= mg;
        // DC blocker + gentle protection against overs
        const float yl = l - dcX[0] + 0.9995f * dcY[0]; dcX[0] = l; dcY[0] = yl;
        const float yr = r - dcX[1] + 0.9995f * dcY[1]; dcX[1] = r; dcY[1] = yr;
        L[t] = juce::jlimit (-1.0f, 1.0f, std::abs (yl) < 0.85f ? yl : std::copysign (0.85f + 0.15f * fastTanh ((std::abs (yl) - 0.85f) / 0.15f), yl));
        R[t] = juce::jlimit (-1.0f, 1.0f, std::abs (yr) < 0.85f ? yr : std::copysign (0.85f + 0.15f * fastTanh ((std::abs (yr) - 0.85f) / 0.15f), yr));
    }
}

void Engine::applyMacros (const float* raw)
{
    using namespace P;
    for (int i = 0; i < numParams; ++i) eff[(size_t) i] = raw[i];
    auto add = [&] (int i, float d, float lo = 0.0f, float hi = 1.0f) { eff[(size_t) i] = juce::jlimit (lo, hi, eff[(size_t) i] + d); };
    const float m = raw[memory], w = raw[warmth], in = raw[intimacy], t = raw[time], s = raw[spaceMacro] - 0.5f;
    // Memory: from a present studio piano (0) to a tape found in an attic (1): everything moves together, musically
    add (tape, 0.95f * m * (0.45f + 0.55f * t));
    add (vinyl, 0.55f * m * m * (0.3f + 0.7f * t));
    add (P::room, 0.22f * m);
    add (tone, -0.32f * m - 0.18f * t * m);
    add (detune, 0.30f * m + 0.45f * t);
    add (sustain, -0.12f * m);
    add (body, 0.15f * m);
    add (wood, 0.35f * m * t);
    // Warmth: darker, rounder, more wood
    add (tone, -0.28f * w);
    add (hammer, -0.25f * w);
    add (body, 0.3f * w);
    // Intimacy: close microphone (dry, detailed, mechanical noise) .. far (room, soft)
    add (P::room, -0.35f * (in - 0.5f) * 2.0f * 0.5f);
    add (body, 0.25f * (in - 0.5f));
    add (tone, 0.12f * (in - 0.5f));
    add (wood, 0.5f * (in - 0.5f) * (raw[wood] > 0.01f || m > 0.01f ? 1.0f : 0.0f));
    // Time: how much the piano has aged
    add (stretch, 0.2f * (t - 0.5f));
    add (damper, -0.3f * (t - 0.5f));
    // Space: bigger and wetter room
    add (P::room, 0.5f * s);
    add (roomSize, 0.5f * s);
    lifeLv.tape = eff[tape]; lifeLv.vinyl = eff[vinyl]; lifeLv.wood = eff[wood];
    lifeLv.world = eff[worldLevel]; lifeLv.worldType = (int) eff[world]; lifeLv.intimacy = in;
    life.setLevels (lifeLv);
}

void Engine::process (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* params, const HostInfo&)
{
    applyMacros (params);
    a4 = p (P::tuning) > 0.5f ? 432.0f : 440.0f;
    widthS.v = p (P::width);
    if (masterS.v < 0.0f) { masterS.v = dbToGain (p (P::master)); bodyS.v = p (P::body); roomS.v = p (P::room); }
    auto it = midi.begin();
    int pos = 0;
    while (pos < n)
    {
        while (it != midi.end() && (*it).samplePosition <= pos) { handle ((*it).getMessage()); ++it; }
        int end = n;
        if (it != midi.end()) end = juce::jmin (end, juce::jmax (pos + 1, (*it).samplePosition));
        end = juce::jmin (end, pos + tickLeft);
        const int len = end - pos;
        renderChunk (L + pos, R + pos, len);
        counter += (uint64_t) len;
        tickLeft -= len;
        if (tickLeft <= 0) { tick(); tickLeft = kTick; }
        pos = end;
    }
}
}
