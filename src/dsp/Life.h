#pragma once
#include "Common.h"
#include "Filters.h"
#include "Space.h"

// The "life" around the piano: wooden sounds (bench, key mechanism, pedal), tape (wow / flutter, saturation, dull highs,
// hiss, dropouts), vinyl (crackle, rumble, needle pop) and a distant world (rain, cafe, fireplace, birds, crowd).
// Everything is synthesised; all randomness is sequential per sample from a seeded Rng or hashed from the absolute
// time slot, so the result never depends on the host block size.
namespace gx
{
struct LifeLevels
{
    float tape = 0, vinyl = 0, wood = 0, world = 0, intimacy = 0.5f;
    int worldType = 0;   // 0 off, 1 rain, 2 cafe, 3 fireplace, 4 birds, 5 crowd
};

class Life
{
public:
    void prepare (double sampleRate);
    void reset (uint32_t seed);
    void setLevels (const LifeLevels& l);
    void noteOn (int note, float vel);      // key thud + mechanism
    void noteOff (int note, float vel);     // damper
    void pedal (bool down);
    void tick();                            // once per absolute control tick (random creaks, dropouts)
    void wood (float& l, float& r);         // added before the room: sounds that happen in the room
    void tape (float& l, float& r);         // processes the whole mix
    void world (float& l, float& r);        // added after the tape: vinyl and the outside world

private:
    struct Ev { bool on = false; float env = 0, coef = 0.99f, amp = 0, pan = 0.5f, f0 = 500, f1 = 500, t = 0; Svf f; };
    struct Bird { bool on = false; float ph = 0, f = 3000, df = 0, env = 0, t = 0, len = 4000, pan = 0.5f, delay = 0; int left = 0; };
    void spawn (float freq0, float freq1, float q, float seconds, float amp, float pan, float freqTo = -1.0f);
    void murmur (float& l, float& r, float level, float dark);
    float hashSlot (uint32_t salt) const;

    double sr = 48000.0;
    uint32_t seed = 1;
    uint64_t tickCount = 0;
    LifeLevels lv;
    Rng rng { 1 };
    // wood
    std::array<Ev, 10> ev;
    // tape
    DelayLine dl[2];
    float wowPh = 0, flPh = 0.3f, drift = 0, driftTarget = 0, tapeSat = 1, dropG = 1, dropTarget = 1;
    int dropLeft = 0;
    Biquad tapeLp[2], tapeHp[2];
    float cacheTape = -1, hissLp[2] {};
    // vinyl
    float rumbleLp[2] {}, rumbleLp2[2] {};
    Biquad crackHp[2];
    bool popDone = false;
    int popLeft = 0;
    // world
    float bed[2] {}, bed2[2] {}, mur[3][2] {}, murEnv[2] {}, murTarget = 0, fireLp[2] {}, fireLp2[2] {};
    Svf fm[3][2];
    std::array<Bird, 6> birds;
    int birdQueue = 0; float birdGap = 0;
    float rainBed[2] {};
    Smooth tapeS, vinylS, worldS;
};
}
