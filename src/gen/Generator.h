#pragma once
#include "../preset/Preset.h"
#include <set>

namespace gx
{
struct GenRequest
{
    juce::String category = "Ambient";   // mood
    juce::String subcategory;            // empty = pick one
    float light = 0.5f, movement = 0.5f, density = 0.5f, space = 0.6f, mystery = 0.25f;
    uint32_t seed = 1;
    std::set<juce::String> locks;        // "harmony", "layers", "fx"
    const Preset* base = nullptr;        // source for locked sections
};

struct Validation
{
    bool ok = false;
    float rmsDb = -100, peakDb = -100, dc = 0;
    float movement = 0, width = 0, score = 0;   // interest metrics (best-of-N)
    juce::String reason;
};

// Rule-based mood generator with offline validation and level matching.
class Generator
{
public:
    // Generates a validated preset; retries with new sub-seeds until it passes (max 'maxAttempts').
    // v2: collects up to 'pickBest' valid candidates and keeps the most interesting one (movement, width, layers)
    static Preset generate (const GenRequest& req, int* attempts = nullptr, Validation* result = nullptr, int maxAttempts = 10, int pickBest = 3);
    static Preset candidate (const GenRequest& req, uint32_t seed); // one unvalidated candidate
    // Renders the preset, checks it and sets Master for the mood's target loudness.
    static Validation validate (Preset& p, bool fixLevel = true);
    static Preset mutate (const Preset& p, float amount, uint32_t seed, const std::set<juce::String>& locks = {});
    static Preset blend (const Preset& a, const Preset& b, float t, uint32_t seed);
    static juce::StringArray deriveTags (const Preset& p);
    static juce::String makeName (const juce::String& mood, Rng& rng, const juce::String& sub = {});
    static float targetRmsDb (const juce::String& mood);
    static float interestScore (const Preset& p, const Validation& v); // higher = more alive (best-of)
};

void applyLocks (Preset& dst, const Preset& src, const std::set<juce::String>& locks);
bool paramInLock (const ParamDef& d, const juce::String& lock);
}
