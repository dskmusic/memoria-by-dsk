#pragma once
#include "Common.h"
#include <vector>

// "Hands": a generative pianist for people who do not play. Chord progressions with voice-leading, arpeggios,
// emotional melodic phrases and a bass line, locked to the host tempo / transport (or an internal clock).
// Deterministic: every decision is a function of (seed, step) or of state that evolves strictly step by step.
namespace gx
{
enum HandsMode { hmOff, hmArp, hmChords, hmPhrases, hmFull };

struct HandsCfg
{
    int mode = hmOff, key = 9, scale = 1, rate = 1;    // rate: 0 = 1/4, 1 = 1/8, 2 = 1/16
    float density = 0.6f, softness = 0.5f;
    bool pedal = true;
    uint32_t seed = 1;
};

class Hands
{
public:
    struct Ev { int note, vel; bool on; };
    void reset (double startBeat);
    void configure (const HandsCfg& c) { cfg = c; }
    void advance (double beatEnd, std::vector<Ev>& out, bool& pedalDown, bool& pedalChanged);
    void allOff (std::vector<Ev>& out);
    int currentChordRoot() const { return lastRoot; }

private:
    void step (int64_t s, std::vector<Ev>& out);
    void play (std::vector<Ev>& out, int note, int vel, double lenBeats);
    int degreeNote (int degree, int octaveBase) const;
    float hf (uint32_t salt, int64_t s) const;
    struct Off { double beat; int note; };
    HandsCfg cfg;
    double last = 0;
    bool started = false;
    std::vector<Off> offs;
    int melDeg = 4, lastRoot = 0, voicingCentre = 62;
    int pedalState = 0;
    std::vector<int> voicing;
    double pedalUpAt = -1;
};
}
