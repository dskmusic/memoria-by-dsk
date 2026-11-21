#include "Engine.h"

namespace gx
{
namespace
{
inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
inline float hashUni (uint32_t a, uint32_t b) { return (float) (hash32 (hash32 (a * 0x9E3779B1u) ^ (b + 0x85ebca6bu)) >> 8) * (1.0f / 16777216.0f); }
inline float smoothstep (float a, float b, float x) { const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a)); return t * t * (3 - 2 * t); }
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float saw (float& ph, float inc)
{
    ph += inc;
    if (ph >= 1.0f) ph -= 1.0f;
    return 2.0f * ph - 1.0f - polyBlep (ph, inc);
}
inline float coefFor (float seconds, double rate) { return seconds <= 0 ? 0.0f : std::exp (-1.0f / (float) (seconds * rate)); }
inline void panGains (float pan, float& l, float& r) // equal power, pan -1..1
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * kPi * 0.25f;
    l = std::cos (a) * 1.41421356f; r = std::sin (a) * 1.41421356f;
}
// Scale degree (any integer, may exceed the scale length) -> semitones from the root.
inline int degreeSemis (const std::vector<int>& s, int d)
{
    const int m = (int) s.size();
    const int oct = d >= 0 ? d / m : -((-d + m - 1) / m);
    return s[(size_t) (d - oct * m)] + 12 * oct;
}
} // namespace

// ================================================================ setup
void Engine::prepare (double sampleRate, int mb)
{
    sr = sampleRate;
    maxBlock = mb;
    fftBuf.assign (1024, 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        grnBuf[c].init ((int) (sr * 5.0));
        dly[c].init ((int) (sr * 2.5));
        chorus[c].init ((int) (sr * 0.05));
        for (auto& s : shift[c]) s.prepare (sr);
    }
    shm.prepare (sr);
    rev.prepare (sr);
    for (auto* s : { &widthS, &warmthS, &dlyMixS, &revMixS, &masterS }) s->setTime (0.05f, sr);
    limRel = coefFor (0.15f, sr);
    tailCoef = coefFor (0.35f, sr);
    reset (seed);
}

// New seed while playing: new evolution curves, RNGs and chord path; audio state (voices, tails) untouched.
void Engine::reseed (uint32_t s)
{
    seed = s == 0 ? 1u : s;
    for (size_t i = 0; i < evoFreq.size(); ++i)
    {
        static const float mult[3] { 1.0f, 1.6180339f, 2.6457513f };
        evoFreq[i] = (1.0f / 120.0f) * mult[i % 3] * (0.72f + 0.56f * hashUni (seed, (uint32_t) i * 3 + 1));
        evoPhase[i] = kTwoPi * hashUni (seed, (uint32_t) i * 3 + 2);
    }
    mut.rng = Rng (seed * 7919u + 3u);
    grnRng = Rng (seed * 31u + 555u);
    windRng = Rng (seed * 13u + 31337u);
    bellRng = Rng (seed * 17u + 7u);
    noiseRng = Rng (seed * 19u + 4711u);
    chordSlot = -1; // forces a chord update (the pad glides to it)
}

void Engine::reset (uint32_t s)
{
    seed = s == 0 ? 1u : s;
    ctrlLeft = 0;
    musT = evoT = sessionT = 0;
    runGain = 0; runInc = 0;
    freezeAmt = 0;
    tailGain = 1;
    compEnv = 0; compGain = compTarget = 1; sleepGain = 1;
    onS.fill (0); dlyNow = dlySamples;
    bassPh[0] = bassPh[1] = 0; bassF.reset(); pwmPh = 0;
    silentFor = 0;
    idle = true;
    for (size_t i = 0; i < evoFreq.size(); ++i)
    {
        static const float mult[3] { 1.0f, 1.6180339f, 2.6457513f };
        evoFreq[i] = (1.0f / 120.0f) * mult[i % 3] * (0.72f + 0.56f * hashUni (seed, (uint32_t) i * 3 + 1));
        evoPhase[i] = kTwoPi * hashUni (seed, (uint32_t) i * 3 + 2);
    }
    presence.fill (0); gainNow.fill (0); gainInc.fill (0); toneEff.fill (0.5f); energyAcc.fill (0);
    keyOverride = -1; curDegree = -1; curRoot = -1; chordSlot = -1; bellSlot = -1;
    degKey = -1; degSlot = 0; degValue = 0;
    // drone
    mut = MutOsc();
    mut.rng = Rng (seed * 7919u + 3u);
    for (int k = 0; k < kGenomeHarm; ++k)
    {
        mut.dom[k] = mut.target[k] = mut.rng.uni();
        mut.speed[k] = 0.3f + mut.rng.uni();
        mut.phase[k] = mut.rng.uni();
    }
    for (int i = 0; i < 5; ++i) dronePh[i] = hashUni (seed, 100u + (uint32_t) i);
    for (auto& f : droneF) f.reset();
    droneBreath = 1;
    // pad
    for (int v = 0; v < kPadVoices; ++v)
    {
        auto& vc = pad[(size_t) v];
        vc = PadVoice();
        for (int k = 0; k < 4; ++k) vc.ph[k] = hashUni (seed, 200u + (uint32_t) (v * 4 + k));
        vc.midi = v >= kChordVoices;
        vc.attackCoef = coefFor (vc.midi ? 0.25f : 2.2f, sr);
        vc.releaseCoef = coefFor (vc.midi ? 2.5f : 3.0f, sr);
        vc.pan = vc.midi ? 0.0f : ((float) v / (kChordVoices - 1) * 2.0f - 1.0f) * 0.7f;
    }
    padAge = 0;
    // texture
    for (auto& b : grnBuf) b.clear();
    for (auto& g : grains) g = Grain();
    grnTimer = 0;
    grnRng = Rng (seed * 31u + 555u);
    // shimmer
    shm.clear();
    for (auto& c : shift) for (auto& ps : c) ps.clear();
    shmFb[0] = shmFb[1] = 0;
    for (auto& b : shmDamp) b.reset();
    // wind
    for (auto& c : pink) for (auto& v : c) v = 0;
    for (auto& c : windF) for (auto& f : c) f.reset();
    windRng = Rng (seed * 13u + 31337u);
    windGust = 0;
    noiseRng = Rng (seed * 19u + 4711u);
    for (int c = 0; c < 2; ++c)
    {
        for (auto& v : pinkN[c]) v = 0;
        brownN[c] = crackle[c] = oceanEnv[c] = 0;
        noiseF[c].reset();
        crackleHp[c].reset(); crackleHp[c].highpass (sr, 1800.0, 0.7);
        chorus[c].clear();
        shmHp[c].reset(); shmHp[c].highpass (sr, 320.0, 0.6);
    }
    chorusPh = 0;
    for (auto& q : bellQueue) q.tick = -1;
    ctrlTick = 0;
    swellT = 100.0;
    for (auto& v : mel) v = MelVoice();
    melSlot = -1; melDeg = 4; melBeats = 0;
    // bells
    for (auto& b : bells) b = Bell();
    bellRng = Rng (seed * 17u + 7u);
    // master
    for (auto& d : dly) d.clear();
    dlyLp[0] = dlyLp[1] = 0;
    rev.clear();
    for (int c = 0; c < 2; ++c) { lowCutF[c].reset(); highCutF[c].reset(); tiltF[c].reset(); dcX[c] = dcY[c] = 0; }
    limGain = 1;
    for (int i = 0; i < kChordVoices; ++i) chordNotes[(size_t) i] = 0;
}

// ================================================================ evolution / harmony
float Engine::evoNoise (int ch, double t) const
{
    const size_t b = (size_t) juce::jlimit (0, kNumLayers * 4 - 1, ch) * 3;
    const double s = 0.55 * std::sin ((double) kTwoPi * evoFreq[b] * t + evoPhase[b])
                   + 0.30 * std::sin ((double) kTwoPi * evoFreq[b + 1] * t + evoPhase[b + 1])
                   + 0.15 * std::sin ((double) kTwoPi * evoFreq[b + 2] * t + evoPhase[b + 2]);
    return (float) (0.5 + 0.5 * s);
}

// Functional harmony as a Markov chain over scale degrees (deterministic: walked from slot 0 with the seed).
// Major-like modes favour I-IV-V-vi movement; minor modes the cinematic i-VI-III-VII / i-iv family.
// Every 8 chords the progression leans back home, so it breathes in phrases instead of wandering.
int Engine::nextDegree (int from, int64_t slot) const
{
    const int sc = (int) pv (P::scale);
    const int m = (int) scaleSteps (sc).size();
    const float harmony = pv (P::harmony);
    float w[8] {};
    if (m == 7)
    {
        const bool minor = sc == 1 || sc == 2 || sc == 3 || sc == 6;
        static const float major[7][7] {   // to:  I    ii   iii  IV   V    vi   vii
                                          { 0, 0.6f, 0.8f, 3.0f, 2.2f, 2.6f, 0.2f },   // from I
                                          { 0.8f, 0, 0.4f, 1.2f, 3.0f, 0.8f, 0.3f },   // ii
                                          { 0.8f, 0.6f, 0, 1.6f, 0.6f, 2.6f, 0.2f },   // iii
                                          { 2.6f, 1.2f, 0.6f, 0, 2.4f, 1.4f, 0.2f },   // IV
                                          { 3.0f, 0.6f, 0.8f, 1.4f, 0, 2.0f, 0.2f },   // V
                                          { 1.6f, 1.6f, 1.0f, 3.0f, 1.4f, 0, 0.2f },   // vi
                                          { 3.0f, 0.4f, 1.2f, 0.6f, 0.6f, 1.0f, 0 } }; // vii
        static const float minorT[7][7] {  // to:  i    ii°  III  iv   v    VI   VII
                                          { 0, 0.2f, 1.4f, 2.4f, 1.0f, 3.0f, 2.2f },   // from i
                                          { 1.2f, 0, 0.6f, 0.8f, 2.4f, 0.6f, 0.6f },
                                          { 1.2f, 0.2f, 0, 1.6f, 0.6f, 2.0f, 2.6f },   // III -> VII / VI
                                          { 2.6f, 0.4f, 0.8f, 0, 1.6f, 1.6f, 1.6f },   // iv
                                          { 3.0f, 0.2f, 0.6f, 1.2f, 0, 1.6f, 0.6f },   // v
                                          { 1.4f, 0.4f, 2.6f, 2.0f, 0.6f, 0, 2.4f },   // VI -> III / VII / iv
                                          { 2.8f, 0.2f, 2.0f, 0.8f, 0.4f, 1.6f, 0 } }; // VII -> i / III
        for (int d = 0; d < 7; ++d) w[d] = (minor ? minorT : major)[from][d];
        // colour chords (ii, iii, vii / ii°) only with more Harmony
        const float colour = 0.3f + 0.9f * harmony;
        if (! minor) { w[1] *= colour; w[2] *= colour; w[6] *= colour * 0.5f; } else { w[1] *= colour * 0.5f; w[4] *= colour; }
    }
    else
    {
        for (int d = 0; d < m; ++d) w[d] = d == from ? 0.0f : (d == 0 ? 2.5f : 1.0f);
    }
    if (slot % 8 == 0 && from != 0) w[0] += 4.0f;          // phrase ends at home
    float total = 0;
    for (int d = 0; d < m; ++d) total += w[d];
    float u = hashUni (seed ^ 0xC0FFEEu, (uint32_t) (slot * 7 + 1)) * total;
    for (int d = 0; d < m; ++d) { u -= w[d]; if (u <= 0 && w[d] > 0) return d; }
    return 0;
}

int Engine::chordDegreeFor (int64_t slot) const
{
    if (pv (P::harmony) < 0.12f || slot <= 0) return 0;
    const int key = (int) pv (P::scale) * 1000 + (int) (pv (P::harmony) * 10.0f);
    if (slot < degSlot || key != degKey || seed != degSeed) { degSlot = 0; degValue = 0; degKey = key; degSeed = seed; }
    while (degSlot < slot) { ++degSlot; degValue = nextDegree (degValue, degSlot); }
    return degValue;
}

void Engine::updateChord (bool force)
{
    int64_t slot;
    if (pv (P::syncChords) > 0.5f)
    {
        static const double bars[] { 1, 2, 4, 8, 16 };
        const double beatsPerChord = 4.0 * bars[juce::jlimit (0, 4, (int) pv (P::chordBars))];
        const double beats = hostHasPpq ? hostPpq + (double) blockPos / sr * hostBpm / 60.0 : musT * 2.0; // 120 bpm without a DAW
        slot = (int64_t) std::floor (beats / beatsPerChord + 1.0e-6);
    }
    else
    {
        const double period = 6.0 * std::pow (8.0, 1.0 - (double) pv (P::chordRate));
        slot = (int64_t) std::floor (musT / period);
    }
    rootPc = keyOverride >= 0 ? keyOverride : (int) pv (P::key);
    const int scaleNow = (int) pv (P::scale), octNow = (int) pv (P::octave);
    const float harmNow = pv (P::harmony);
    if (! force && slot == chordSlot && rootPc == curRoot && scaleNow == curScale && octNow == curOct && harmNow == curHarm) return;
    if (! force && slot != chordSlot) swellT = 0; // new chord: the analog pad blooms
    chordSlot = slot;
    curRoot = rootPc; curScale = scaleNow; curOct = octNow; curHarm = harmNow;
    const auto& s = scaleSteps ((int) pv (P::scale));
    const float harmony = pv (P::harmony);
    const int d = chordDegreeFor (slot);
    curDegree = d;
    int tones[5];
    int k = 0;
    if (harmony < 0.25f) { tones[k++] = degreeSemis (s, d); tones[k++] = degreeSemis (s, d) + 7; }  // open fifths
    else
    {
        tones[k++] = degreeSemis (s, d);
        tones[k++] = degreeSemis (s, d + 2);
        tones[k++] = degreeSemis (s, d + 4);
        if (harmony > 0.45f) tones[k++] = degreeSemis (s, d + 6);
        if (harmony > 0.80f) tones[k++] = degreeSemis (s, d + 8);
    }
    const int base = 12 * ((int) pv (P::octave) + 2) + rootPc;
    const bool lead = ! force && chordNotes[0] > 0;
    if (! lead)
        for (int i = 0; i < kChordVoices; ++i)
        {
            int n = base + tones[i % k] + 12 * (i / k);
            while (n > base + 26) n -= 12;   // keep the pad inside ~2 octaves
            chordNotes[(size_t) i] = n;
        }
    else
    {
        // voice-leading: every chord tone goes to the voice that can reach it with the smallest step,
        // the remaining voices double the nearest chord tone. Smooth, no jumps, always inside the register.
        auto nearestOf = [&] (int tone, int prev)
        {
            int bestN = base + tone, bestD = 1000;
            for (int o = -1; o <= 3; ++o)
            {
                const int n = base + tone + 12 * o;
                if (n < base - 3 || n > base + 26) continue;
                if (std::abs (n - prev) < bestD) { bestD = std::abs (n - prev); bestN = n; }
            }
            return bestN;
        };
        std::array<int, kChordVoices> prev = chordNotes, next {};
        std::array<bool, kChordVoices> used {};
        for (int t = 0; t < juce::jmin (k, kChordVoices); ++t)
        {
            int bv = -1, bd = 1000;
            for (int v = 0; v < kChordVoices; ++v)
                if (! used[(size_t) v]) { const int d = std::abs (nearestOf (tones[t], prev[(size_t) v]) - prev[(size_t) v]); if (d < bd) { bd = d; bv = v; } }
            used[(size_t) bv] = true;
            next[(size_t) bv] = nearestOf (tones[t], prev[(size_t) bv]);
        }
        for (int v = 0; v < kChordVoices; ++v)
            if (! used[(size_t) v])
            {
                int bn = prev[(size_t) v], bd = 1000;
                for (int t = 0; t < k; ++t) { const int n = nearestOf (tones[t], prev[(size_t) v]); if (std::abs (n - prev[(size_t) v]) < bd) { bd = std::abs (n - prev[(size_t) v]); bn = n; } }
                next[(size_t) v] = bn;
            }
        chordNotes = next;
    }
    std::sort (chordNotes.begin(), chordNotes.end());
    for (int i = 0; i < kChordVoices; ++i) pad[(size_t) i].target = (float) chordNotes[(size_t) i];
    {
        const int low = 12 * ((int) pv (P::octave) + 1) + rootPc;
        int bn = low + degreeSemis (s, d);
        while (bn > low + 7) bn -= 12;
        while (bn < low - 5) bn += 12;
        bassTarget = (float) bn;
        if (force) bassNote = bassTarget;
    }
    if (force) for (int i = 0; i < kChordVoices; ++i) pad[(size_t) i].note = pad[(size_t) i].target;
    tele.rootPc.store (rootPc);
    tele.chordDegree.store (d);
    for (int i = 0; i < kChordVoices; ++i) tele.chordNotes[(size_t) i].store (chordNotes[(size_t) i]);
}

// ================================================================ drone: spectral mutation (from Genesix)
void Engine::rebuildMutation (float f0, float morph, float chaos, float rate, float tilt)
{
    auto& m = mut;
    const auto& arch = genomeArchetypes();
    const int maxK = juce::jlimit (1, kGenomeHarm - 1, (int) ((float) sr * 0.45f / juce::jmax (1.0f, f0)));
    const float pos = juce::jlimit (0.0f, 1.0f, morph) * (kNumArchetypes - 1);
    const int a = juce::jmin (kNumArchetypes - 2, (int) pos);
    const float t = pos - (float) a;
    const float dt = (float) kMutPeriod / (float) sr;
    const float r = 0.03f + rate * rate * 6.0f;
    std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
    for (int k = 1; k <= maxK; ++k)
    {
        m.dom[k] += (m.target[k] - m.dom[k]) * juce::jmin (1.0f, m.speed[k] * r * dt);
        if (std::abs (m.target[k] - m.dom[k]) < 0.03f || m.rng.uni() < r * dt * 0.25f) m.target[k] = m.rng.uni();
        const float cross = juce::jlimit (0.0f, 1.0f, t + (m.dom[k] - 0.5f) * (0.6f + chaos));
        const float base = arch[(size_t) a][(size_t) k] + (arch[(size_t) a + 1][(size_t) k] - arch[(size_t) a][(size_t) k]) * cross;
        const float gene = juce::jmax (0.0f, 1.0f + chaos * 2.5f * (m.dom[k] - 0.5f));
        const float amp = base * gene * std::pow ((float) k, -tilt);
        m.phase[k] = wrap01 (m.phase[k] + chaos * 0.01f * (m.dom[k] - 0.5f));
        const float ph = m.phase[k] * kTwoPi;
        fftBuf[(size_t) k * 2] = amp * std::sin (ph);
        fftBuf[(size_t) k * 2 + 1] = -amp * std::cos (ph);
    }
    mutFft.performRealOnlyInverseTransform (fftBuf.data());
    float peak = 1.0e-6f;
    for (int i = 0; i < kMutTable; ++i) peak = juce::jmax (peak, std::abs (fftBuf[(size_t) i]));
    const int dst = 1 - m.cur;
    for (int i = 0; i < kMutTable; ++i) m.table[dst][i] = fftBuf[(size_t) i] / peak;
    m.table[dst][kMutTable] = m.table[dst][0];
    m.cur = dst;
    m.counter = 0;
}

// ================================================================ events
// A bell event is a short phrase: 1-4 notes walking the scale from a chord tone (motif-like, never random noise).
void Engine::triggerBell (int64_t slot)
{
    const auto& s = scaleSteps ((int) pv (P::scale));
    const uint32_t h = (uint32_t) slot;
    const int base = 12 * ((int) pv (P::octave) + 4) + rootPc;
    int deg = curDegree + 2 * (int) (hashUni (seed, h * 5 + 1) * 3.0f);           // a chord tone
    const float pan = (hashUni (seed, h * 5 + 2) * 2.0f - 1.0f) * 0.8f;
    const int len = 1 + (int) (std::pow (hashUni (seed, h * 5 + 3), 1.6f) * 4.0f);  // mostly short phrases
    const int dir = hashUni (seed, h * 5 + 4) < 0.5f ? -1 : 1;
    const float stepTime = 0.22f + 0.45f * hashUni (seed, h * 5 + 5);             // seconds between notes
    for (int i = 0; i < len; ++i)
    {
        const int note = base + degreeSemis (s, deg) + (hashUni (seed, h * 7 + (uint32_t) i) < 0.25f ? 12 : 0);
        const float vel = 1.0f - 0.18f * (float) i;
        if (i == 0) playBell (note, vel, pan);
        else
            for (auto& q : bellQueue)
                if (q.tick < 0)
                {
                    q.tick = ctrlTick + (int64_t) ((float) i * stepTime * (float) sr / kCtrl);
                    q.note = note; q.vel = vel; q.pan = juce::jlimit (-0.9f, 0.9f, pan + 0.12f * (float) i * (float) dir);
                    break;
                }
        deg += dir * (1 + (hashUni (seed, h * 11 + (uint32_t) i) < 0.3f ? 1 : 0));
    }
}

// The melody is composed, not random: 8-bar periods (A A' B cadence / A A' C final cadence) on an eighth-note
// grid. A keeps its rhythm and contour for 4 periods (a recognisable theme) and is re-harmonised by the chords:
// strong-beat and long notes land on chord tones, the final note goes home. Presence = how many periods play,
// Character = tempo and how busy the rhythm is. Every choice is a (seed, position) hash: deterministic.
void Engine::melodyTick()
{
    double beats;
    double bpm = 60.0 + 36.0 * lp (lyMelody, P::lChar);
    if (pv (P::syncChords) > 0.5f && hostHasPpq)
    {
        bpm = hostBpm;
        beats = hostPpq + (double) blockPos / sr * hostBpm / 60.0;
        if (bpm > 110.0) { bpm *= 0.5; beats *= 0.5; }   // half time: stays lyrical at dance tempos
    }
    else beats = melBeats;
    const int64_t e = (int64_t) std::floor (beats * 2.0 + 1.0e-6);
    if (e == melSlot) return;
    const bool fresh = melSlot < 0;
    melSlot = e;
    if (fresh || ! running || lp (lyMelody, P::lOn) < 0.5f || e < 0) return;
    const float ch = lp (lyMelody, P::lChar);
    const int periodLen = ch < 0.45f ? 10 : 8;          // calmer melodies breathe two bars between periods
    const int64_t bar = e / 8, period = bar / periodLen;
    const int pos = (int) (e % 8), bi = (int) (bar % periodLen);
    if (bi >= 8) return;
    if (period > 0 && hashUni (seed ^ 0x3E10u, (uint32_t) period) > 0.2f + 0.8f * lp (lyMelody, P::lPresence)) return;
    const uint32_t theme = (uint32_t) (period / 4), var = (uint32_t) (period % 4);
    const int key = bi == 4 ? 0 : bi == 5 ? 1 : bi;   // bars 4-5 bring back the motif of bars 0-1
    static const int pats[9][5] { { 2, 2, 2, 2, 0 }, { 4, 2, 2, 0, 0 }, { 3, 1, 4, 0, 0 }, { 2, 2, 4, 0, 0 }, { 1, 1, 2, 4, 0 },
                                  { 6, 2, 0, 0, 0 }, { 2, 1, 1, 4, 0 }, { 4, 4, 0, 0, 0 }, { 8, 0, 0, 0, 0 } };
    static const int slow[5] { 7, 5, 1, 3, 8 }, busy[6] { 0, 4, 6, 2, 3, 1 };
    const uint32_t hk = theme * 64u + (uint32_t) key + (key >= 2 ? var * 8u : 0u);   // A fixed per theme, B / C vary
    int pat;
    if (bi == 3) pat = hashUni (seed ^ 0x51u, hk) < 0.5f ? 7 : 5;                    // half cadence
    else if (bi == 7) pat = hashUni (seed ^ 0x52u, hk) < 0.5f ? 3 : 7;               // ends on a long note
    else pat = hashUni (seed ^ 0x53u, hk) < 0.15f + 0.7f * ch ? busy[(int) (hashUni (seed ^ 0x54u, hk) * 5.999f)]
                                                              : slow[(int) (hashUni (seed ^ 0x55u, hk) * 4.999f)];
    int t = 0, idx = -1, dur = 0;
    for (int k = 0; k < 5 && pats[pat][k] > 0; ++k)
    {
        if (t == pos) { idx = k; dur = pats[pat][k]; break; }
        t += pats[pat][k];
    }
    if (idx < 0) return;
    const auto& s = scaleSteps ((int) pv (P::scale));
    const int m = (int) s.size();
    const int cd = juce::jmax (0, curDegree);
    static const int steps[8] { -2, -1, -1, 1, 1, 2, 0, -1 };
    int deg = melDeg + steps[(int) (hashUni (seed ^ 0x56u, hk * 8u + (uint32_t) idx) * 7.999f)];
    auto nearest = [&] (int target, bool rootOnly)
    {
        int best = target, bd = 1000;
        const int nTones = rootOnly ? 1 : (pv (P::harmony) > 0.45f ? 4 : 3);
        for (int o = -3; o <= 3; ++o)
            for (int k = 0; k < nTones; ++k)
            {
                const int c = (rootOnly ? 0 : cd + 2 * k) + o * m;
                if (std::abs (c - target) < bd) { bd = std::abs (c - target); best = c; }
            }
        return best;
    };
    const bool last = t + dur == 8;
    if (bi == 7 && last)
    {
        const int r = ((0 - cd) % m + m) % m;                 // is the tonic in this chord?
        deg = nearest (deg, r == 0 || r == 2 || r == 4 ? true : false);
        if (! (r == 0 || r == 2 || r == 4)) deg = nearest (deg, false);
    }
    else if (idx == 0 || dur >= 4) deg = nearest (deg, false);
    while (deg > m + 5) deg -= m;
    while (deg < 0) deg += m;
    melDeg = deg;
    const int base = 60 + rootPc - (rootPc >= 7 ? 12 : 0) + (lp (lyMelody, P::lTone) > 0.6f ? 12 : 0);
    const float vel = (idx == 0 ? 1.0f : 0.8f) * (bi == 2 || bi == 6 ? 1.06f : 1.0f) * (0.92f + 0.08f * hashUni (seed ^ 0x57u, (uint32_t) e));
    playMelody (base + degreeSemis (s, deg), vel, (float) (dur * 30.0 / bpm) * 0.93f);
}

void Engine::playMelody (int note, float vel, float durSec)
{
    const float tone = lp (lyMelody, P::lTone);
    MelVoice* v = nullptr;
    for (auto& x : mel) if (! x.on) { v = &x; break; }
    if (v == nullptr) { v = &mel[0]; for (auto& x : mel) if (x.env < v->env) v = &x; }   // steal the quietest: env and phase continue (no click)
    if (! v->on) { v->ph = v->mph = 0; v->env = 0; }
    const float f = noteToHz ((float) note, a4);
    const bool pluck = tone >= 0.5f;
    v->on = v->gate = true;
    v->pluck = pluck;
    v->inc = f / (float) sr;
    v->minc = f * (pluck ? (tone > 0.8f ? 4.0f : 3.0f) : 1.0f) / (float) sr;
    v->idx = pluck ? 1.0f + 2.2f * (tone - 0.5f) : 0.2f + 0.8f * tone;
    v->idxCoef = pluck ? coefFor (0.3f, sr) : 1.0f;
    v->peak = (pluck ? 0.19f : 0.15f) * vel;
    v->att = 1.0f - coefFor (pluck ? 0.002f : 0.03f + 0.06f * (0.5f - tone), sr);
    v->decay = pluck ? coefFor (0.9f + 1.4f * (1.0f - tone), sr) : coefFor (8.0f, sr);
    v->rel = coefFor (pluck ? 0.45f : 0.16f, sr);
    v->vib = 0; v->vibInc = 5.1f / (float) sr; v->vibDepth = pluck ? 0.0f : 0.0045f; v->age = 0;
    v->offTick = ctrlTick + (int64_t) (durSec * (float) sr / kCtrl);
    panGains ((float) ((note % 12) - 6) / 6.0f * 0.25f, v->panL, v->panR);
}

void Engine::playBell (int note, float vel, float pan)
{
    const float tone = toneEff[lyBells];
    Bell* b = nullptr;
    for (auto& x : bells) if (! x.on) { b = &x; break; }
    if (b == nullptr) { b = &bells[0]; for (auto& x : bells) if (x.amp < b->amp) b = &x; }
    const float f = noteToHz ((float) note, a4);
    const float ratio = tone < 0.5f ? 2.0f + tone * 2.0f : 3.5f + (tone - 0.5f) * 7.0f;
    b->on = true;
    b->ph = b->mph = 0;
    b->inc = f / (float) sr;
    b->minc = f * ratio / (float) sr;
    b->idx = 0.6f + tone * 3.0f;
    b->idxCoef = coefFor (0.25f + 0.6f * (1.0f - tone), sr);
    b->amp = (0.18f + 0.12f * bellRng.uni()) * vel;
    b->ampCoef = coefFor (1.6f + 3.5f * pv (P::space), sr);
    panGains (pan, b->panL, b->panR);
    tele.bellCount.fetch_add (1);
    tele.lastBellNote.store (note);
    tele.lastBellPan.store (pan);
}

void Engine::spawnGrain()
{
    Grain* g = nullptr;
    for (auto& x : grains) if (! x.on) { g = &x; break; }
    if (g == nullptr) return;
    static const float ratios[6] { 1.0f, 2.0f, 1.5f, 0.5f, 3.0f, 4.0f };
    const float tone = grnTone;
    const int maxIdx = juce::jlimit (1, 6, 2 + (int) (tone * 4.5f));
    const int idx = juce::jmin (maxIdx - 1, (int) (std::pow (grnRng.uni(), 1.6f - tone) * (float) maxIdx));
    float inc = ratios[idx];
    if (grnRng.uni() < 0.12f * grnChar) inc = -inc; // occasional reversed grain
    g->len = grnLen * (0.7f + 0.6f * grnRng.uni());
    g->inc = inc;
    const float bufLen = (float) grnBuf[0].mask;
    float pos = 0.05f * (float) sr + grnRng.uni() * (float) sr * (0.5f + 2.5f * grnChar);
    const float drift = (grnRecord ? 1.0f : 0.0f) - inc; // read-position change per sample (relative to the write head)
    if (drift < 0.0f) pos = juce::jmax (pos, g->len * -drift + 64.0f);
    else pos = juce::jmin (pos, bufLen - g->len * drift - 64.0f);
    g->pos = juce::jlimit (4.0f, bufLen - 4.0f, pos);
    g->t = 0;
    panGains ((grnRng.uni() * 2.0f - 1.0f) * 0.85f, g->panL, g->panR);
    g->on = true;
}

void Engine::handleMidi (const juce::MidiMessage& m)
{
    const int mode = (int) pv (P::midiMode);
    if (mode == mmOff) return;
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        if (mode == mmKey || mode == mmKeyPlay) keyOverride = note % 12;
        if (mode == mmPlay || mode == mmKeyPlay)
        {
            PadVoice* v = nullptr;
            for (int i = kChordVoices; i < kPadVoices; ++i) if (pad[(size_t) i].midiNote == note) v = &pad[(size_t) i];
            if (v == nullptr) for (int i = kChordVoices; i < kPadVoices; ++i) if (pad[(size_t) i].midiNote < 0 && pad[(size_t) i].gain < 0.002f) { v = &pad[(size_t) i]; break; }
            if (v == nullptr) { v = &pad[kChordVoices]; for (int i = kChordVoices; i < kPadVoices; ++i) if (pad[(size_t) i].age < v->age) v = &pad[(size_t) i]; }
            if (v->gain < 0.01f) v->note = (float) note;
            v->target = (float) note;
            v->midiNote = note;
            v->gainTarget = 0.35f + 0.65f * m.getFloatVelocity();
            v->age = ++padAge;
        }
    }
    else if (m.isNoteOff())
    {
        for (int i = kChordVoices; i < kPadVoices; ++i)
            if (pad[(size_t) i].midiNote == m.getNoteNumber()) { pad[(size_t) i].midiNote = -1; pad[(size_t) i].gainTarget = 0; }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (int i = kChordVoices; i < kPadVoices; ++i) { pad[(size_t) i].midiNote = -1; pad[(size_t) i].gainTarget = 0; }
    }
}

// ================================================================ control tick
void Engine::control()
{
    const float dt = (float) kCtrl / (float) sr;
    const bool frozen = pv (P::freeze) > 0.5f;
    {   // Sleep mode: over the session timer the landscape darkens, thins out, slows down, then fades to silence
        const int timerMin = (int) pv (P::timer);
        float k = 0;
        if (pv (P::sleepMode) > 0.5f && timerMin > 0 && internalOnly) k = juce::jlimit (0.0f, 1.0f, (float) (sessionT / (timerMin * 60.0)));
        effLight = pv (P::light) * (1.0f - 0.75f * k);
        effDensity = pv (P::density) * (1.0f - 0.6f * k);
        effEvo = pv (P::evolution) * (1.0f - 0.7f * k);
        sleepGain = 1.0f - smoothstep (0.55f, 1.0f, k);
    }
    const float evoRate = 0.15f * std::pow (30.0f, effEvo);
    if (running && ! frozen) { musT += dt; evoT += dt * evoRate; melBeats += dt * (60.0 + 36.0 * lp (lyMelody, P::lChar)) / 60.0; }
    // Freeze is smoothed (~0.4 s): feedback and reverb times glide instead of jumping (no click)
    freezeAmt += ((frozen ? 1.0f : 0.0f) - freezeAmt) * (1.0f - coefFor (0.4f, sr / kCtrl));
    const float fz = freezeAmt;
    if (running) sessionT += dt;
    ++ctrlTick;
    for (auto& v : mel) if (v.gate && v.offTick <= ctrlTick) v.gate = false;
    for (auto& q : bellQueue)
        if (q.tick >= 0 && q.tick <= ctrlTick) { if (running) playBell (q.note, q.vel, q.pan); q.tick = -1; }
    a4 = (int) pv (P::tuning) == 1 ? 432.0f : 440.0f;
    lightMul = std::pow (2.0f, (effLight - 0.5f) * 3.0f);
    updateChord (curDegree < 0);

    // ---- layer presence (smooth gates over slow incommensurate noise) and evolving tone
    const float density = effDensity;
    const float evoDepth = 0.35f + 0.65f * effEvo;
    const float gainCoef = coefFor (1.2f, sr / kCtrl);
    for (int l = 0; l < kNumLayers; ++l)
    {
        const float pres = juce::jlimit (0.0f, 1.0f, lp (l, P::lPresence) + (density - 0.5f) * 0.5f);
        float gate;
        if (pres >= 0.99f) gate = 1.0f;
        else if (pres <= 0.01f) gate = 0.0f;
        else
        {
            const float th = 1.0f - pres;
            gate = smoothstep (th - 0.14f, th + 0.14f, evoNoise (l * 4, evoT));
        }
        presence[(size_t) l] = gate + gainCoef * (presence[(size_t) l] - gate);
        onS[(size_t) l] += ((lp (l, P::lOn) > 0.5f ? 1.0f : 0.0f) - onS[(size_t) l]) * 0.08f; // ~15 ms ramp when a layer is switched
        const float on = onS[(size_t) l];
        const float lvl = lp (l, P::lLevel);
        const float target = on * lvl * lvl * (l == lyMelody ? 1.0f : presence[(size_t) l]) * (l == lyDrone ? droneBreath : 1.0f);
        gainInc[(size_t) l] = (target - gainNow[(size_t) l]) / (float) kCtrl;
        toneEff[(size_t) l] = juce::jlimit (0.0f, 1.0f, lp (l, P::lTone) + (evoNoise (l * 4 + 1, evoT) - 0.5f) * 0.5f * evoDepth
                                                        + (effLight - 0.5f) * 0.3f);
        tele.presence[(size_t) l].store (presence[(size_t) l] * on);
    }

    const int oct = (int) pv (P::octave);
    // ---- drone
    {
        const float droneNote = (float) (12 * (oct + 1) + rootPc);
        const float f0 = noteToHz (droneNote, a4);
        const float ch = lp (lyDrone, P::lChar);
        const float tone = toneEff[lyDrone];
        if (mut.counter >= kMutPeriod)
            rebuildMutation (f0 * 1.5f, 0.1f + 0.8f * evoNoise (lyDrone * 4 + 2, evoT), ch, 0.2f + 0.8f * ch, (1.0f - tone) * 0.9f);
        const float det = 0.0008f + ch * 0.0025f;
        droneInc[0] = f0 * (1 - det) / (float) sr;
        droneInc[1] = f0 * (1 + det) / (float) sr;
        droneInc[2] = f0 * 1.4983f * (1 + det * 0.7f) / (float) sr;
        droneInc[3] = f0 * 1.4983f * (1 - det * 0.7f) / (float) sr;
        droneInc[4] = f0 * 0.5f / (float) sr;
        const float cut = juce::jlimit (80.0f, (float) sr * 0.45f, 220.0f * std::pow (2.0f, tone * 6.0f) * lightMul);
        for (auto& f : droneF) f.set (std::tan (kPi * cut / (float) sr), 1.3f);
        droneBreath = 0.75f + 0.25f * evoNoise (lyDrone * 4 + 3, evoT * 3.0);
    }
    // ---- pad
    {
        const float ch = lp (lyPad, P::lChar);
        const float glide = coefFor (1.5f + 2.5f * (1.0f - pv (P::chordRate)), sr / kCtrl);
        const float cents = 3.0f + ch * 20.0f;
        // "analog" character: slow per-voice vibrato drift + filter bloom on every chord change (80s poly-synth feel)
        swellT += (double) kCtrl / sr;
        const float swell = (float) ((1.0 - std::exp (-swellT / 1.4)) * std::exp (-swellT / 7.0));
        const float vibDepth = ch * ch * 9.0f;
        const float tone = toneEff[lyPad];
        for (int v = 0; v < kPadVoices; ++v)
        {
            auto& pvc = pad[(size_t) v];
            pvc.note = pvc.target + glide * (pvc.note - pvc.target);
            if (! pvc.midi) pvc.gainTarget = 1.0f;
            const float f = noteToHz (pvc.note, a4);
            pvc.vib += (0.17f + 0.031f * (float) v) * (float) kCtrl / (float) sr;
            if (pvc.vib >= 1.0f) pvc.vib -= 1.0f;
            const float vb = vibDepth * fastSin (pvc.vib);
            const float d[4] { -cents + vb, cents * 0.15f + vb, cents + vb, -1200.0f + vb };
            for (int k = 0; k < 4; ++k) pvc.inc[k] = juce::jmin (0.45f, f * std::exp2 (d[k] / 1200.0f) / (float) sr);
            const float wob = evoNoise (lyPad * 4 + 2, evoT * 2.0 + v * 17.0);
            const float cut = juce::jlimit (60.0f, (float) sr * 0.45f, f * (1.5f + 10.0f * tone * tone) * lightMul * (0.75f + 0.5f * wob) * (1.0f + swell * ch * 2.5f));
            for (auto& flt : pvc.f) flt.set (std::tan (kPi * cut / (float) sr), 1.5f);
        }
    }
    // ---- bass: chord root, sine + a little saw, low-pass; Character = grit
    {
        const float glide = coefFor (0.9f, sr / kCtrl);
        bassNote = bassTarget + glide * (bassNote - bassTarget);
        const float f = noteToHz (bassNote, a4);
        bassInc[0] = f / (float) sr;
        bassInc[1] = f * 1.003f / (float) sr;
        const float tone = toneEff[lyBass];
        const float cut = juce::jlimit (40.0f, 3000.0f, f * (1.5f + 10.0f * tone * tone) * std::sqrt (lightMul));
        bassF.set (std::tan (kPi * cut / (float) sr), 1.2f);
        bassDrive = 1.0f + 4.0f * lp (lyBass, P::lChar);
    }
    // ---- glue compressor (2:1 above -18 dBFS, slow): computed here, applied per sample
    {
        const float lvl = 10.0f * std::log10 (compEnv + 1.0e-10f);
        const float over = juce::jmax (0.0f, lvl + 18.0f);
        compTarget = dbToGain (-over * 0.5f);
    }
    // ---- texture
    {
        grnTone = toneEff[lyTexture];
        grnChar = lp (lyTexture, P::lChar);
        grnLen = (0.08f + 0.6f * grnChar) * (float) sr;
        grnInterval = (0.03f + 0.22f * grnChar) * (float) sr * (1.4f - density * 0.8f);
        grnRecord = ! frozen;
    }
    // ---- shimmer
    {
        shmBlend = lp (lyShimmer, P::lChar);
        const float space = pv (P::space);
        shm.set (0.6f + 0.4f * space, 0.25f + 0.5f * (1.0f - toneEff[lyShimmer]), 1.0f + 2.0f * fz);
        shmGainFb = 0.35f + 0.3f * space + 0.2f * fz;
        for (auto& b : shmDamp) b.lowpass (sr, 2500.0 + 9000.0 * toneEff[lyShimmer] * juce::jmin (1.5f, lightMul));
    }
    // ---- wind
    {
        const float tone = toneEff[lyWind], ch = lp (lyWind, P::lChar);
        const float gust = evoNoise (lyWind * 4 + 2, musT * 9.0);
        windGust = 0.25f + 0.75f * std::pow (gust, 1.0f + ch * 2.0f);
        const float c1 = juce::jlimit (60.0f, (float) sr * 0.4f, 250.0f * std::pow (2.0f, tone * 4.0f) * lightMul * (0.6f + 0.8f * gust));
        const float k = 1.6f - 1.4f * ch;
        for (int c = 0; c < 2; ++c)
        {
            windF[c][0].set (std::tan (kPi * c1 * (c ? 1.07f : 0.94f) / (float) sr), k);
            windF[c][1].set (std::tan (kPi * juce::jmin ((float) sr * 0.4f, c1 * 2.6f) / (float) sr), k * 1.3f);
        }
    }
    // ---- noise generator: tone filter + ocean swell envelope (two out-of-phase waves)
    {
        const int nt = (int) pv (P::noiseType);
        const float tone = pv (P::noiseTone);
        for (int c = 0; c < 2; ++c)
        {
            const double period = 8.5 + 2.5 * hashUni (seed, 900u + (uint32_t) c);
            const float x = (float) (musT / period - std::floor (musT / period) + (c ? 0.37 : 0.0));
            const float xx = x - std::floor (x);
            oceanEnv[c] = xx < 0.38f ? smoothstep (0.0f, 0.38f, xx) : std::pow (1.0f - (xx - 0.38f) / 0.62f, 1.7f);
            float cut = 120.0f * std::pow (2.0f, tone * 7.0f);
            if (nt == ntOcean) cut = 180.0f + (500.0f + 3500.0f * tone) * oceanEnv[c];
            if (nt == ntVinyl) cut = 2500.0f + 6000.0f * tone;
            noiseF[c].set (std::tan (kPi * juce::jlimit (40.0f, (float) sr * 0.45f, cut * lightMul) / (float) sr), 1.4f);
        }
    }
    // ---- bells: one chance per 1/8 s slot of musical time
    {
        const int64_t slot = (int64_t) std::floor (musT * 8.0);
        if (slot != bellSlot)
        {
            const bool fresh = bellSlot < 0;
            bellSlot = slot;
            const float ch = lp (lyBells, P::lChar);
            const float prob = (0.015f + 0.3f * ch * ch) * (0.3f + 1.4f * density) * presence[lyBells];
            if (! fresh && running && lp (lyBells, P::lOn) > 0.5f && hashUni (seed ^ 0xB0B0u, (uint32_t) slot) < prob) triggerBell (slot);
        }
    }
    melodyTick();
    // ---- master section (cached coefficient updates)
    {
        const float lc = pv (P::lowCut), hc = pv (P::highCut), tilt = std::round ((effLight - 0.5f) * 8.0f * 20.0f) / 20.0f;
        if (lc != cacheLow) { cacheLow = lc; for (auto& b : lowCutF) b.highpass (sr, lc); }
        if (hc != cacheHigh) { cacheHigh = hc; for (auto& b : highCutF) b.lowpass (sr, hc); }
        if (tilt != cacheTilt) { cacheTilt = tilt; for (auto& b : tiltF) b.shelf (sr, 2500.0, tilt, true); }
        const float space = pv (P::space), damp = pv (P::revDamp), fr = std::round (fz * 50.0f) / 50.0f;
        if (space != cacheRev || damp != cacheDamp || fr != cacheFreeze)
        {
            cacheRev = space; cacheDamp = damp; cacheFreeze = fr;
            rev.set (0.35f + 0.75f * space, damp, 1.0f + 1.5f * fr);
        }
        dlySamples = juce::jlimit (16.0f, (float) dly[0].mask - 16.0f, pv (P::dlyTime) * 0.001f * (float) sr);
        if (ctrlTick <= 1) dlyNow = dlySamples;
    }
    tele.evoTime.store ((float) evoT);
    tele.musTime.store ((float) musT);
}

// ================================================================ audio
void Engine::renderChunk (float* L, float* R, int n)
{
    const float sources = runGain > 0.0f || running ? 1.0f : 0.0f;
    const float shimIn = 1.0f - 0.75f * freezeAmt;
    const float dlyFb = pv (P::dlyFb), dlyToneC = 0.15f + 0.8f * pv (P::dlyTone);
    const float runTarget = running ? 1.0f : 0.0f;
    const float fadeIn = juce::jmax (0.05f, pv (P::fadeIn)), fadeOut = juce::jmax (0.05f, pv (P::fadeOut));
    runInc = running ? 1.0f / (fadeIn * (float) sr) : -1.0f / (fadeOut * (float) sr);
    const auto& tbl = mut.table;
    const float droneOn = lp (lyDrone, P::lOn), padOnF = lp (lyPad, P::lOn);
    // texture and shimmer listen to drone + pad even when those layers are muted in the mix
    const bool feed = (lp (lyTexture, P::lOn) > 0.5f && gainNow[lyTexture] > 1.0e-5f) || (lp (lyShimmer, P::lOn) > 0.5f && gainNow[lyShimmer] > 1.0e-5f);
    const float windCh = lp (lyWind, P::lChar);
    bool midiAlive = false;
    for (int v = kChordVoices; v < kPadVoices; ++v) midiAlive |= pad[(size_t) v].gain > 1.0e-4f || pad[(size_t) v].gainTarget > 0;
    const int noiseType = (int) pv (P::noiseType);
    const float noiseLvl = pv (P::noiseLevel) * pv (P::noiseLevel) * 0.6f;

    for (int i = 0; i < n; ++i)
    {
        runGain = juce::jlimit (0.0f, 1.0f, runGain + runInc);
        if ((runInc > 0 && runGain >= runTarget) || (runInc < 0 && runGain <= runTarget)) runGain = runTarget;
        for (int l = 0; l < kNumLayers; ++l) gainNow[(size_t) l] += gainInc[(size_t) l];
        const float rg = runGain * runGain * (3.0f - 2.0f * runGain); // S-curve fade

        float dl = 0, dr = 0, padL = 0, padR = 0, midiL = 0, midiR = 0, feedL = 0, feedR = 0, bsl = 0;
        pwmPh += 0.13f / (float) sr; if (pwmPh >= 1.0f) pwmPh -= 1.0f;
        const float duty = 0.5f + 0.32f * fastSin (pwmPh);
        if (sources > 0 && lp (lyBass, P::lOn) > 0.5f && gainNow[lyBass] > 1.0e-5f)
        {
            float sw = 0;
            for (int k = 0; k < 2; ++k) { bassPh[k] += bassInc[k]; if (bassPh[k] >= 1.0f) bassPh[k] -= 1.0f; }
            sw = 2.0f * bassPh[1] - 1.0f - polyBlep (bassPh[1], bassInc[1]);
            float bp, hp;
            const float y = bassF.tick (fastSin (bassPh[0]) + 0.35f * sw, bp, hp);
            bsl = fastTanh (y * bassDrive) / std::sqrt (bassDrive) * gainNow[lyBass] * 0.42f;
        }

        // ---- drone
        if (sources > 0 && ((droneOn > 0.5f && gainNow[lyDrone] > 1.0e-5f) || feed))
        {
            const float xf = juce::jmin (1.0f, (float) mut.counter / (float) kMutPeriod);
            auto rd = [&] (float ph)
            {
                const float x = ph * kMutTable;
                const int k = (int) x;
                const float t = x - (float) k;
                const float a = tbl[1 - mut.cur][k] + t * (tbl[1 - mut.cur][k + 1] - tbl[1 - mut.cur][k]);
                const float b = tbl[mut.cur][k] + t * (tbl[mut.cur][k + 1] - tbl[mut.cur][k]);
                return a + xf * (b - a);
            };
            for (int k = 0; k < 5; ++k) { dronePh[k] += droneInc[k]; if (dronePh[k] >= 1.0f) dronePh[k] -= 1.0f; }
            float xl = rd (dronePh[0]) + 0.45f * rd (dronePh[2]);
            float xr = rd (dronePh[1]) + 0.45f * rd (dronePh[3]);
            const float sub = 0.4f * fastSin (dronePh[4]);
            float bp, hp;
            xl = droneF[0].tick (xl, bp, hp) + sub;
            xr = droneF[1].tick (xr, bp, hp) + sub;
            xl *= 0.32f; xr *= 0.32f;
            feedL = xl * 0.6f; feedR = xr * 0.6f;
            const float g = gainNow[lyDrone] * droneOn;
            dl = xl * g; dr = xr * g;
        }
        ++mut.counter;
        // ---- pad (chord voices + MIDI voices)
        for (int v = 0; v < kPadVoices; ++v)
        {
            auto& pvc = pad[(size_t) v];
            const float tgt = pvc.midi ? pvc.gainTarget : pvc.gainTarget * (sources > 0 && (padOnF > 0.5f || feed) ? 1.0f : 0.0f);
            pvc.gain = tgt + (tgt > pvc.gain ? pvc.attackCoef : pvc.releaseCoef) * (pvc.gain - tgt);
            if (pvc.gain < 1.0e-5f) continue;
            const float s0 = saw (pvc.ph[0], pvc.inc[0]), s1 = saw (pvc.ph[1], pvc.inc[1]), s2 = saw (pvc.ph[2], pvc.inc[2]);
            const float sh = pvc.ph[1] + duty >= 1.0f ? pvc.ph[1] + duty - 1.0f : pvc.ph[1] + duty;   // PWM: saw minus a shifted saw = pulse
            const float pulse = s1 - (2.0f * sh - 1.0f - polyBlep (sh, pvc.inc[1]));
            pvc.ph[3] += pvc.inc[3]; if (pvc.ph[3] >= 1.0f) pvc.ph[3] -= 1.0f;
            const float sub = 0.5f * fastSin (pvc.ph[3]);
            float bp, hp;
            const float l = pvc.f[0].tick (s0 + 0.45f * s1 + 0.35f * pulse, bp, hp) + sub;
            const float r = pvc.f[1].tick (s2 + 0.45f * s1 - 0.35f * pulse, bp, hp) + sub;
            float pl, pr;
            panGains (pvc.pan, pl, pr);
            if (pvc.midi) { midiL += l * pl * pvc.gain; midiR += r * pr * pvc.gain; }
            else { padL += l * pl * pvc.gain; padR += r * pr * pvc.gain; }
        }
        {   // ensemble chorus on the chord voices: two slowly modulated taps per side, cross-mixed (string-machine width)
            chorusPh += 0.37f / (float) sr;
            if (chorusPh >= 1.0f) chorusPh -= 1.0f;
            chorus[0].push (padL); chorus[1].push (padR);
            const float ms = 0.001f * (float) sr;
            const float aL = chorus[0].read (ms * (11.0f + 3.0f * fastSin (chorusPh))), bL = chorus[0].read (ms * (17.0f + 4.0f * fastSin (chorusPh + 0.33f)));
            const float aR = chorus[1].read (ms * (13.0f + 3.0f * fastSin (chorusPh + 0.5f))), bR = chorus[1].read (ms * (19.0f + 4.0f * fastSin (chorusPh + 0.83f)));
            const float dL = padL, dR = padR;
            padL = dL * 0.62f + (aL + bR) * 0.38f;
            padR = dR * 0.62f + (aR + bL) * 0.38f;
        }
        padL *= 0.075f; padR *= 0.075f;
        feedL += padL * 0.6f; feedR += padR * 0.6f;
        const float padG = gainNow[lyPad] * (padOnF > 0.5f ? 1.0f : 0.0f);
        padL *= padG; padR *= padG;
        midiL *= 0.09f; midiR *= 0.09f;

        // ---- texture: granular cloud over drone + pad (+ MIDI)
        float tl = 0, tr = 0;
        if (grnRecord) { grnBuf[0].push (feedL + midiL); grnBuf[1].push (feedR + midiR); }
        if (sources > 0 && lp (lyTexture, P::lOn) > 0.5f && gainNow[lyTexture] > 1.0e-5f)
        {
            grnTimer -= 1.0f;
            if (grnTimer <= 0) { grnTimer = grnInterval * (0.6f + 0.8f * grnRng.uni()); spawnGrain(); }
        }
        int active = 0;
        for (auto& g : grains)
        {
            if (! g.on) continue;
            ++active;
            const float env = 0.5f - 0.5f * std::cos (kTwoPi * g.t / g.len);
            const float d = juce::jlimit (2.0f, (float) grnBuf[0].mask - 4.0f, g.pos);
            tl += grnBuf[0].read (d) * env * g.panL;
            tr += grnBuf[1].read (d) * env * g.panR;
            g.pos += (grnRecord ? 1.0f : 0.0f) - g.inc; // frozen buffer: the write head stops, grains keep moving
            g.t += 1.0f;
            if (g.t >= g.len) g.on = false;
        }
        {
            const float tg = gainNow[lyTexture] * 2.2f / std::sqrt ((float) juce::jmax (4, active));
            tl *= tg; tr *= tg;
        }

        // ---- wind
        float wl = 0, wr = 0;
        if (sources > 0 && lp (lyWind, P::lOn) > 0.5f && gainNow[lyWind] > 1.0e-5f)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float w = windRng.bi();
                auto& pk = pink[c];
                pk[0] = 0.99765f * pk[0] + w * 0.0990460f;
                pk[1] = 0.96300f * pk[1] + w * 0.2965164f;
                pk[2] = 0.57000f * pk[2] + w * 1.0526913f;
                const float pn = (pk[0] + pk[1] + pk[2] + w * 0.1848f) * 0.18f;
                float bp1, bp2, hp;
                windF[c][0].tick (pn, bp1, hp);
                windF[c][1].tick (pn, bp2, hp);
                const float y = (bp1 + 0.55f * bp2) * (1.0f + windCh * 1.5f);
                (c == 0 ? wl : wr) = y;
            }
            const float g = gainNow[lyWind] * windGust * 0.9f;
            wl *= g; wr *= g;
        }

        // ---- noise generator
        float rl = 0, rr = 0;
        if (sources > 0 && noiseType != ntOff && noiseLvl > 0.0001f)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float w = noiseRng.bi();
                auto& pk = pinkN[c];
                pk[0] = 0.99765f * pk[0] + w * 0.0990460f;
                pk[1] = 0.96300f * pk[1] + w * 0.2965164f;
                pk[2] = 0.57000f * pk[2] + w * 1.0526913f;
                const float pink = (pk[0] + pk[1] + pk[2] + w * 0.1848f) * 0.2f;
                brownN[c] = 0.996f * brownN[c] + 0.035f * w;
                const float brown = brownN[c] * 2.2f;
                float x = 0;
                switch (noiseType)
                {
                    case ntPink: x = pink; break;
                    case ntBrown: x = brown; break;
                    case ntWhite: x = w * 0.3f; break;
                    case ntOcean: x = (0.6f * pink + 0.7f * brown) * (0.08f + oceanEnv[c]); break;
                    case ntVinyl:
                    {
                        if (noiseRng.uni() < 0.00035f) crackle[c] = 0.4f + 0.6f * noiseRng.uni();
                        const float cr = crackleHp[c].tick (noiseRng.bi() * crackle[c]);
                        crackle[c] *= 0.86f;
                        x = cr * 0.9f + brown * 0.15f + pink * 0.05f;
                        break;
                    }
                    default: break;
                }
                float bp, hp;
                const float y = noiseF[c].tick (x, bp, hp);
                (c == 0 ? rl : rr) = (noiseType == ntVinyl ? x * 0.5f + y * 0.5f : y) * noiseLvl;
            }
        }

        // ---- bells
        float bl = 0, br = 0;
        for (auto& b : bells)
        {
            if (! b.on) continue;
            b.mph += b.minc; if (b.mph >= 1.0f) b.mph -= 1.0f;
            b.ph += b.inc; if (b.ph >= 1.0f) b.ph -= 1.0f;
            const float y = fastSin (b.ph + b.idx * 0.159f * fastSin (b.mph)) * b.amp;
            b.idx *= b.idxCoef;
            b.amp *= b.ampCoef;
            bl += y * b.panL; br += y * b.panR;
            if (b.amp < 1.0e-5f) b.on = false;
        }
        {
            const float g = juce::jmax (gainNow[lyBells], 0.0f) * 0.9f;
            bl *= g; br *= g;
        }

        // ---- melody
        float ml = 0, mr = 0;
        for (auto& v : mel)
        {
            if (! v.on) continue;
            if (v.gate) { v.env += v.att * (v.peak - v.env); v.peak *= v.decay; }
            else v.env *= v.rel;
            v.vib += v.vibInc; if (v.vib >= 1.0f) v.vib -= 1.0f;
            const float fm = 1.0f + v.vibDepth * juce::jmin (1.0f, v.age * 2.5e-5f) * fastSin (v.vib);   // vibrato grows in
            v.age += 1.0f;
            v.mph += v.minc * fm; if (v.mph >= 1.0f) v.mph -= 1.0f;
            v.ph += v.inc * fm; if (v.ph >= 1.0f) v.ph -= 1.0f;
            const float y = fastSin (v.ph + v.idx * 0.159f * fastSin (v.mph)) * v.env;
            v.idx *= v.idxCoef;
            ml += y * v.panL; mr += y * v.panR;
            if (! v.gate && v.env < 1.0e-5f) v.on = false;
        }
        {
            const float g = juce::jmax (gainNow[lyMelody], 0.0f);
            ml *= g; mr *= g;
        }

        // ---- shimmer: pitch-shifted feedback reverb fed by the tonal layers
        float sl = 0, sr2 = 0;
        {
            const float send = gainNow[lyShimmer] * (lp (lyShimmer, P::lOn) > 0.5f ? 1.0f : 0.0f);
            const float inL = shmHp[0].tick ((feedL + tl * 0.5f) * send * shimIn * rg + midiL * send * shimIn); // no mud in the shimmer
            const float inR = shmHp[1].tick ((feedR + tr * 0.5f) * send * shimIn * rg + midiR * send * shimIn);
            if (send > 1.0e-5f || std::abs (shmFb[0]) + std::abs (shmFb[1]) > 1.0e-6f)
            {
                const float oct0 = shift[0][0].process (shmFb[0], 2.0f), oct1 = shift[1][0].process (shmFb[1], 2.0f);
                const float fif0 = shift[0][1].process (shmFb[0], 3.0f), fif1 = shift[1][1].process (shmFb[1], 3.0f);
                const float fl = oct0 + shmBlend * (fif0 - oct0), fr = oct1 + shmBlend * (fif1 - oct1);
                float l, r;
                shm.process (inL * 2.0f + fl * shmGainFb, inR * 2.0f + fr * shmGainFb, l, r);
                shmFb[0] = shmDamp[0].tick (fastTanh (l));
                shmFb[1] = shmDamp[1].tick (fastTanh (r));
                sl = l * 0.9f; sr2 = r * 0.9f;
            }
        }

        // energy per layer (UI)
        energyAcc[lyDrone] += dl * dl + dr * dr; energyAcc[lyPad] += padL * padL + padR * padR;
        energyAcc[lyTexture] += tl * tl + tr * tr; energyAcc[lyShimmer] += sl * sl + sr2 * sr2;
        energyAcc[lyWind] += wl * wl + wr * wr + rl * rl + rr * rr; energyAcc[lyBells] += bl * bl + br * br;
        energyAcc[lyBass] += 2.0f * bsl * bsl; energyAcc[lyMelody] += ml * ml + mr * mr;

        // ---- mix: layers (faded by the transport) + MIDI voices
        float xl = (dl + padL + tl + wl + bl + rl + bsl + ml) * rg + sl + midiL;
        float xr = (dr + padR + tr + wr + br + rr + bsl + mr) * rg + sr2 + midiR;

        // ---- delay (bells + texture send), cross feedback
        {
            const float mix = dlyMixS.step (pv (P::dlyMix));
            const float inL = (bl + tl * 0.4f + ml * 0.6f) * rg * mix, inR = (br + tr * 0.4f + mr * 0.6f) * rg * mix;
            dlyNow += 0.00005f * (dlySamples - dlyNow);   // delay time glides (no clicks when it changes)
            const float yl = dly[0].read (dlyNow), yr = dly[1].read (dlyNow * 1.333f);
            dlyLp[0] += dlyToneC * (yl - dlyLp[0]);
            dlyLp[1] += dlyToneC * (yr - dlyLp[1]);
            dly[0].push (fastTanh (inL + dlyLp[1] * dlyFb));
            dly[1].push (fastTanh (inR + dlyLp[0] * dlyFb));
            xl += yl * 0.8f; xr += yr * 0.8f;
        }
        // ---- warmth (gentle tape-like saturation)
        {
            const float w = warmthS.step (pv (P::warmth));
            if (w > 0.001f)
            {
                const float drive = 1.0f + w * 3.0f;
                xl = xl + w * (fastTanh (xl * drive) / drive * 1.2f - xl);
                xr = xr + w * (fastTanh (xr * drive) / drive * 1.2f - xr);
            }
        }
        // ---- reverb (Space)
        {
            const float mix = revMixS.step (0.12f + 0.68f * pv (P::space));
            float rl, rr;
            rev.process (xl, xr, rl, rr);
            xl = xl * (1.0f - mix * 0.55f) + rl * mix * 1.1f;
            xr = xr * (1.0f - mix * 0.55f) + rr * mix * 1.1f;
        }
        // ---- tone + width
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? xl : xr;
            x = tiltF[c].tick (highCutF[c].tick (lowCutF[c].tick (x)));
            const float y = x - dcX[c] + 0.9995f * dcY[c]; // DC blocker
            dcX[c] = x; dcY[c] = y; x = y;
        }
        {
            const float w = widthS.step (pv (P::width));
            const float m = 0.5f * (xl + xr), s = 0.5f * (xl - xr) * w;
            xl = m + s; xr = m - s;
        }
        // ---- after Stop: once the sources have faded, the reverb / shimmer / delay tails close in ~1.5 s (clear, not endless)
        {
            const bool open = running || runGain > 0.0f || midiAlive;
            tailGain = open ? juce::jmin (1.0f, tailGain + 1.0f / (0.05f * (float) sr)) : tailGain * tailCoef;
            xl *= tailGain; xr *= tailGain;
        }
        // ---- master + safety limiter
        const float mg = masterS.step (dbToGain (pv (P::master)) * sleepGain);
        xl *= mg; xr *= mg;
        {
            const float e = 0.5f * (xl * xl + xr * xr);
            compEnv += (e > compEnv ? 0.0015f : 0.00012f) * (e - compEnv);
            compGain += 0.0004f * (compTarget - compGain);
            xl *= compGain; xr *= compGain;
        }
        const float pk = juce::jmax (std::abs (xl), std::abs (xr));
        const float want = pk > 0.89f ? 0.89f / pk : 1.0f;
        limGain = want < limGain ? want : want + limRel * (limGain - want);
        xl = fastTanh (xl * limGain * 1.05f) * 0.95f;
        xr = fastTanh (xr * limGain * 1.05f) * 0.95f;
        if (! std::isfinite (xl) || ! std::isfinite (xr)) { xl = xr = 0; }
        L[i] = xl; R[i] = xr;
    }
}

void Engine::process (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* params, const HostInfo& host)
{
    p = params;
    hostHasPpq = host.hostPlaying && host.hasPpq;
    hostPpq = host.ppq;
    hostBpm = host.bpm > 1.0 ? host.bpm : 120.0;
    internalOnly = host.internalPlay && ! host.hostPlaying;
    const uint32_t s = (uint32_t) pv (P::seed);
    // ---- transport: DAW playback resyncs (fresh, reproducible start); internal Play keeps flowing
    if (host.hostPlaying)
    {
        const int64_t pos = host.hasPosition ? host.hostSample : expectedHost;
        if (! wasHost || std::llabs (pos - expectedHost) > 64 || s != seed)
        {
            reset (s);
            musT = (double) juce::jmax ((int64_t) 0, pos) / sr;
            evoT = musT * (double) (0.15f * std::pow (30.0f, pv (P::evolution)));
        }
        expectedHost = pos + n;
    }
    else if (s != seed)
    {
        if (! running && runGain <= 0.0f) reset (s);
        else reseed (s); // live: a new journey without cutting the sound
    }
    if (host.internalPlay && ! wasInternal && ! host.hostPlaying) sessionT = 0;
    wasHost = host.hostPlaying;
    wasInternal = host.internalPlay;
    running = host.hostPlaying || host.internalPlay;

    // ---- idle: nothing playing and every tail has died -> output silence, skip the DSP
    bool midiPending = ! midi.isEmpty();
    bool voices = false;
    for (int i = kChordVoices; i < kPadVoices; ++i) voices |= pad[(size_t) i].gain > 1.0e-5f || pad[(size_t) i].gainTarget > 0;
    if (! running && runGain <= 0.0f && ! voices && ! midiPending && silentFor > 8.0)
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        tele.running.store (false);
        tele.runGain.store (0);
        return;
    }

    int pos = 0;
    auto it = midi.begin();
    while (pos < n)
    {
        while (it != midi.end() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        if (ctrlLeft == 0)
        {
            blockPos = pos;
            control();
            ctrlLeft = kCtrl;
            for (int l = 0; l < kNumLayers; ++l)
            {
                const float e = std::sqrt (energyAcc[(size_t) l] / (2.0f * kCtrl));
                tele.energy[(size_t) l].store (e);
                energyAcc[(size_t) l] = 0;
            }
        }
        int chunk = juce::jmin (n - pos, ctrlLeft);
        if (it != midi.end()) chunk = juce::jmin (chunk, juce::jmax (1, (*it).samplePosition - pos));
        renderChunk (L + pos, R + pos, chunk);
        ctrlLeft -= chunk;
        pos += chunk;
    }
    for (; it != midi.end(); ++it) handleMidi ((*it).getMessage());

    float peak = 0;
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    silentFor = peak > 1.0e-5f || running ? 0.0 : silentFor + (double) n / sr;

    tele.running.store (running);
    tele.runGain.store (runGain);
    tele.sessionElapsed.store ((float) sessionT);
    const int timerMin = (int) pv (P::timer);
    if (timerMin > 0 && host.internalPlay && ! host.hostPlaying && sessionT >= timerMin * 60.0 - pv (P::fadeOut))
        tele.timerExpired.store (true);
}
}
