#pragma once
#include "Common.h"
#include "Filters.h"
#include "Space.h"
#include "Life.h"
#include "Composer.h"

// Memoria piano engine: modal synthesis, no samples.
// Every string partial is a decaying complex resonator (inharmonic frequency, two decay rates = the piano's
// double decay, unison strings slightly detuned = beating). A hammer sets the initial amplitudes (felt spectrum by
// velocity, strike-position comb); dampers, the sustain pedal (continuous, so half pedal works) and sympathetic
// resonance of the undamped strings complete the model. Bars (celesta, toy piano) use free partial ratio tables.
// Determinism: all per-note randomness comes from hash(seed, note); control decisions run on an absolute kTick grid,
// so output is identical for any host block size.
namespace gx
{
constexpr int kTick = 32, kMaxComp = 128, kVoiceSlots = 44, kPolyphony = 32, kLowKey = 21, kHighKey = 108, kGhostKeys = kHighKey - kLowKey + 1;

struct HostInfo
{
    bool hostPlaying = false, hasPosition = false, internalPlay = false, hasPpq = false;
    int64_t hostSample = 0;
    double ppq = 0, bpm = 120;
};

// Values published to the UI (relaxed atomics).
struct Telemetry
{
    std::array<std::atomic<float>, 128> keyLevel {};   // 0..1 per key: how loud each key rings (drives key lights / strings view)
    std::array<std::atomic<float>, 128> keyHit {};     // last hammer strength of the key (0..1), decays in the UI
    std::atomic<float> pedal { 0 };                    // sustain pedal 0..1
    std::atomic<int> voices { 0 }, hitCounter { 0 }, chordRoot { 0 }, songBar { 0 }, songBars { 0 };
    std::atomic<bool> handsOn { false };
    std::array<std::atomic<int>, 128> keyStamp {};     // hitCounter value of the last hammer strike of each key
};

struct PianoModel
{
    const char* name;
    float bMul, tMul, fcMul, pFast, detCents, x0, slope, thump, bodyMul, lpHz;
    int nStr;                       // 0 = by register
    const float* ratios;            // null = string; otherwise bar partial ratios
    const float* ratioAmp;          // linear amplitude per bar partial
    int numRatios;
};
const PianoModel& pianoModel (int type);
int numPianoTypes();

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset (uint32_t seed);
    void reseed (uint32_t s) { seed = s; }
    void process (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* params, const HostInfo& host);
    Telemetry tele;
    double getSampleRate() const { return sr; }
    void requestSeek (float bar) { seekReq.store (bar); }

private:
    struct Comp { float re = 0, im = 0, c = 1, s = 0, sig = 0, dsig = 0, rc = 1, rs = 0; };
    struct Voice
    {
        bool active = false, held = false, fading = false;
        int note = 60, n = 0;
        float vel = 0, att = 0, attCoef = 0, dmp = 0, dmpLast = -1, panL = 0.7f, panR = 0.7f, gain = 1, fadeG = 1, level = 0;
        float thump = 0, thumpCoef = 0.99f, thumpGain = 0;
        uint64_t age = 0;
        Svf thumpF;
        Rng rng;
        Comp comp[kMaxComp];
    };
    struct Ghost { float re[2] {}, im[2] {}, c[2] {}, s[2] {}, rc[2] {}, rs[2] {}, sig = 0; bool on = false; float f = 0; };

    void tick();
    void renderChunk (float* L, float* R, int n);
    void handle (const juce::MidiMessage& m);
    void noteOn (int note, float vel);
    void noteOff (int note);
    void setPedal (float v);
    void excite (Voice& v, int note, float vel, bool restrike);
    void exciteGhosts (int note, float vel);
    void setRates (Voice& v);
    float p (int i) const { return eff[(size_t) i]; }
    void applyMacros (const float* raw);
    int allocVoice();

    double sr = 48000.0;
    int maxBlock = 512;
    std::array<float, 64> eff {};          // parameters after the macros (Memory, Warmth, Intimacy, Time, Space)
    Life life;
    Score score;
    size_t noteIdx = 0, pedIdx = 0;
    struct PendOff { double beat; int note; };
    std::vector<PendOff> pendOffs;
    double songStart = 0;
    int songIndex = 0;
    uint32_t cfgHash = 0;
    ComposeCfg cfgNow;
    std::atomic<float> seekReq { -1.0f };               // bar to jump to (UI), -1 none
    float resumeBar = 0;
    void composeSong();
    void releaseHands();
    double beatClock = 0, bpm = 72;
    bool wasRunning = false, handsRunning = false;
    void startStopHands (const HostInfo& host);
    void tickHands();
    LifeLevels lifeLv;
    bool pedalWasDown = false;
    uint32_t seed = 1;
    int tickLeft = kTick;
    uint64_t counter = 0;
    std::array<Voice, kVoiceSlots> voices;
    std::array<Ghost, kGhostKeys> ghosts;
    int activeGhosts = 0;
    std::array<bool, 128> keyDown {};
    float pedalTarget = 0, pedalNow = 0, a4 = 440.0f;
    std::vector<float> vbuf, mixL, mixR, bodyIn;
    // body (soundboard modes), felt low-pass, room, master
    static constexpr int kModes = 10;
    Svf modeL[kModes], modeR[kModes];
    float modeG[kModes] {};
    Biquad lpF[2], toneF[2];
    float cacheTone = -1, cacheLp = -1;
    Fdn room;
    float cacheRoom = -1;
    float dcX[2] {}, dcY[2] {};
    float limG = 1.0f, limAtk = 0.05f, limRel = 0.0002f;      // output limiter
    Smooth masterS, bodyS, roomS, widthS;
    int idleTicks = 0;
};
}
