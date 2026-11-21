#pragma once
#include "Common.h"
#include "Filters.h"
#include "Space.h"
#include "Genome.h"
#include <juce_dsp/juce_dsp.h>

// Memoria engine: six self-playing layers whose presence, timbre and harmony evolve slowly.
// Determinism: every control decision happens on an absolute 32-sample tick grid and every random
// choice comes from (seed, time slot) hashes or RNGs reseeded on reset, so the output does not depend
// on the host block size and a DAW bounce matches playback (the engine resets when the transport starts
// or jumps).
namespace gx
{
constexpr int kCtrl = 32, kMutPeriod = 1024, kMutTable = 512;
constexpr int kChordVoices = 5, kMidiVoices = 6, kPadVoices = kChordVoices + kMidiVoices, kGrains = 40, kBellVoices = 12;

struct HostInfo
{
    bool hostPlaying = false, hasPosition = false, internalPlay = false, hasPpq = false;
    int64_t hostSample = 0;
    double ppq = 0, bpm = 120;
};

// Values published to the UI (relaxed atomics, written once per control tick / block).
struct Telemetry
{
    std::array<std::atomic<float>, kNumLayers> presence {}, energy {};
    std::array<std::atomic<int>, kChordVoices> chordNotes {};
    std::atomic<int> rootPc { 9 }, chordDegree { 0 }, bellCount { 0 }, lastBellNote { 72 };
    std::atomic<float> runGain { 0 }, evoTime { 0 }, musTime { 0 }, sessionElapsed { 0 }, lastBellPan { 0 }, energyAll { 0 };
    std::atomic<bool> running { false }, timerExpired { false };
};

struct MutOsc
{
    float table[2][kMutTable + 1] {};
    int cur = 0, counter = kMutPeriod;
    float dom[kGenomeHarm] {}, target[kGenomeHarm] {}, speed[kGenomeHarm] {}, phase[kGenomeHarm] {};
    Rng rng { 99 };
};

struct PadVoice
{
    float note = 60, target = 60, gain = 0, gainTarget = 0, attackCoef = 0, releaseCoef = 0, pan = 0;
    float ph[4] {}, inc[4] {}, vib = 0;
    Svf f[2];
    bool midi = false;
    int midiNote = -1, age = 0;
};

struct Grain { bool on = false; float pos = 0, inc = 1, len = 0, t = 0, panL = 1, panR = 1; };

struct Bell
{
    bool on = false;
    float ph = 0, mph = 0, inc = 0, minc = 0, idx = 0, idxCoef = 0, amp = 0, ampCoef = 0, panL = 1, panR = 1;
};

// melody voice: FM, from a breathy flute-like tone (Tone low, legato) to celesta / music box (Tone high, plucked)
struct MelVoice
{
    bool on = false, gate = false, pluck = false;
    float ph = 0, mph = 0, inc = 0, minc = 0, idx = 0, idxCoef = 1, env = 0, peak = 0, decay = 1, att = 0.01f, rel = 0.999f;
    float vib = 0, vibInc = 0, vibDepth = 0, age = 0, panL = 1, panR = 1;
    int64_t offTick = 0;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset (uint32_t seed);                    // full state reset (fresh, deterministic)
    void reseed (uint32_t seed);                   // live seed change: keeps voices and tails
    void process (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* params, const HostInfo& host);
    Telemetry tele;
    double getSampleRate() const { return sr; }

private:
    void control();                                // once per absolute kCtrl tick
    void renderChunk (float* L, float* R, int n);
    void handleMidi (const juce::MidiMessage& m);
    void updateChord (bool force);
    int chordDegreeFor (int64_t slot) const;
    int nextDegree (int from, int64_t slot) const;
    mutable int64_t degSlot = 0; mutable int degValue = 0, degKey = -1; mutable uint32_t degSeed = 0;
    float evoNoise (int channel, double t) const;  // 0..1, smooth, never-repeating (incommensurate sines)
    float pv (int index) const { return p[index]; }
    float lp (int layer, int field) const { return p[P::layer (layer, field)]; }
    void rebuildMutation (float f0, float morph, float chaos, float rate, float tilt);
    void triggerBell (int64_t slot);
    void playBell (int note, float vel, float pan);
    void spawnGrain();

    double sr = 48000.0;
    int maxBlock = 512;
    const float* p = nullptr;
    uint32_t seed = 1;

    // clocks
    int ctrlLeft = 0;                 // samples left in the current control tick (0 = tick due)
    double musT = 0, evoT = 0;        // musical time (seconds) and evolution time (frozen by Freeze)
    double sessionT = 0;
    int64_t expectedHost = 0;
    bool wasHost = false, wasInternal = false, running = false, idle = true;
    float runGain = 0, runInc = 0, freezeAmt = 0, tailGain = 1, tailCoef = 0.99f, dlyNow = 20000;
    std::array<float, kNumLayers> onS {};
    // effective macros (Sleep mode lowers them over the session), glue compressor
    float effLight = 0.5f, effDensity = 0.5f, effEvo = 0.5f, sleepGain = 1.0f, compEnv = 0, compGain = 1, compTarget = 1;
    // tempo sync of the chord changes
    double hostPpq = 0, hostBpm = 120; bool hostHasPpq = false, internalOnly = false; int blockPos = 0;
    // bass layer
    float bassPh[2] {}, bassNote = 36, bassTarget = 36, bassInc[2] {}, bassDrive = 1;
    Svf bassF;
    // pad PWM
    float pwmPh = 0;
    double silentFor = 0;

    // evolution
    std::array<float, kNumLayers * 4 * 3> evoFreq {}, evoPhase {};
    std::array<float, kNumLayers> presence {}, gainNow {}, gainInc {}, toneEff {}, energyAcc {};

    // harmony
    int keyOverride = -1, rootPc = 9, curDegree = -1, curRoot = -1, curScale = -1, curOct = -1;
    float curHarm = -1.0f;
    int64_t chordSlot = -1, bellSlot = -1;
    std::array<int, kChordVoices> chordNotes {};
    float a4 = 440.0f, lightMul = 1.0f;

    // drone
    MutOsc mut;
    juce::dsp::FFT mutFft { 9 };
    std::vector<float> fftBuf;
    float dronePh[5] {}, droneInc[5] {};
    Svf droneF[2];
    float droneBreath = 1;

    // pad
    std::array<PadVoice, kPadVoices> pad;
    int padAge = 0;

    // texture (granular)
    DelayLine grnBuf[2];
    std::array<Grain, kGrains> grains {};
    float grnTimer = 0, grnInterval = 2000, grnLen = 8000, grnTone = 0.5f, grnChar = 0.5f;
    bool grnRecord = true;
    Rng grnRng { 555 };

    // shimmer
    Fdn shm;
    PitchShifter shift[2][2];
    float shmFb[2] {}, shmBlend = 0, shmGainFb = 0.5f;
    Biquad shmDamp[2];

    // wind
    float pink[2][3] {};
    Svf windF[2][2];
    Rng windRng { 31337 };
    float windGust = 0;
    // noise generator (pink / brown / white / ocean / vinyl)
    Rng noiseRng { 4711 };
    float pinkN[2][3] {}, brownN[2] {}, crackle[2] {}, oceanEnv[2] {}, noiseCut = 1000;
    Svf noiseF[2];
    Biquad crackleHp[2];
    // pad ensemble chorus
    DelayLine chorus[2];
    float chorusPh = 0;
    // bell phrases: notes queued on the control-tick clock (deterministic)
    struct PendingBell { int64_t tick = -1; int note = 0; float vel = 0, pan = 0; };
    std::array<PendingBell, 16> bellQueue {};
    int64_t ctrlTick = 0;
    Biquad shmHp[2];
    double swellT = 100.0;               // seconds since the last chord change (pad filter bloom)

    // melody: composed phrases (motif, answer, return, cadence) on an eighth-note grid
    std::array<MelVoice, 8> mel {};
    int64_t melSlot = -1;
    int melDeg = 4;
    double melBeats = 0;
    void melodyTick();
    void playMelody (int note, float vel, float durSec);
    // bells
    std::array<Bell, kBellVoices> bells {};
    Rng bellRng { 7 };

    // master
    DelayLine dly[2];
    float dlyLp[2] {}, dlySamples = 20000;
    Fdn rev;
    Biquad lowCutF[2], highCutF[2], tiltF[2];
    float cacheLow = -1, cacheHigh = -1, cacheTilt = -100, cacheRev = -1, cacheDamp = -1, cacheFreeze = -1;
    float dcX[2] {}, dcY[2] {}, limGain = 1, limRel = 0.999f, masterNow = 0.5f;
    Smooth widthS, warmthS, dlyMixS, revMixS, masterS;
};
}
