#pragma once
#include "../preset/Preset.h"

namespace gx
{
struct GenRequest
{
    juce::String category = "Lo-Fi";
    int sub = -1;            // -1 = pick one
    uint32_t seed = 1;
};

// Rule-based "memory" generator: every category has an archetype (piano, room, tape, vinyl, world, macros);
// candidates are rendered offline, checked (finite, not silent, no overs) and level-matched.
class Generator
{
public:
    static Preset candidate (const GenRequest& req);                     // one unvalidated preset
    static bool validate (Preset& p, float* rmsDb = nullptr);            // renders a phrase; sets Master so every preset has the same loudness
    static Preset generate (const GenRequest& req, int maxAttempts = 6); // candidate + validation, retries with new sub-seeds
    static juce::StringArray deriveTags (const Preset& p);
    static float targetRmsDb() { return -27.0f; }
};
}
