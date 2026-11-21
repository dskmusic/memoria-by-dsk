#include "Generator.h"
#include "../preset/Render.h"
#include <map>

namespace gx
{
namespace
{
struct Range2 { float lo, hi; };

// One mood "archetype": ranges for every musical decision. Sub-moods override a few of them.
struct Arch
{
    std::vector<int> scales;                          // indices into scaleNames()
    Range2 octave { 1.6f, 2.4f }, harmony { 0.4f, 0.8f }, chordRate { 0.15f, 0.4f }, evolution { 0.25f, 0.5f },
           density { 0.4f, 0.7f }, light { 0.4f, 0.7f }, space { 0.6f, 0.9f }, warmth { 0.1f, 0.35f }, dly { 0.15f, 0.4f },
           noiseLevel { 0.15f, 0.35f }, dlyFb { 0.3f, 0.65f }, dlyTime { 250.0f, 900.0f };
    float tuning432 = 0.0f;                           // probability of A4 = 432 Hz
    float noiseChance = 0.25f;                        // probability of using the noise generator
    std::vector<int> noises { ntPink, ntBrown };      // noise colours that fit
    std::vector<int> scenes { scValley };             // landscapes that fit (first = most typical)
    bool fixedFocus = false;                          // sub-mood already decides the layer balance
    bool melody = false;                              // composed melody layer (melodic moods only)
    // per layer: level, presence, tone, character
    std::array<std::array<Range2, 4>, kNumLayers> layer {};
};

enum Sc { Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmMinor, MajPent, MinPent, Suspended, Hirajoshi, WholeTone };

Arch baseArch (const juce::String& mood)
{
    Arch a;
    //                        level            presence          tone              character
    a.layer[lyDrone]   = { { { 0.60f, 0.80f }, { 0.90f, 1.00f }, { 0.30f, 0.60f }, { 0.25f, 0.55f } } };
    a.layer[lyPad]     = { { { 0.50f, 0.75f }, { 0.60f, 1.00f }, { 0.30f, 0.60f }, { 0.30f, 0.60f } } };
    a.layer[lyTexture] = { { { 0.35f, 0.60f }, { 0.35f, 0.75f }, { 0.30f, 0.70f }, { 0.30f, 0.70f } } };
    a.layer[lyShimmer] = { { { 0.35f, 0.60f }, { 0.40f, 0.85f }, { 0.35f, 0.70f }, { 0.20f, 0.60f } } };
    a.layer[lyWind]    = { { { 0.15f, 0.40f }, { 0.20f, 0.60f }, { 0.30f, 0.70f }, { 0.30f, 0.70f } } };
    a.layer[lyBells]   = { { { 0.20f, 0.40f }, { 0.30f, 0.65f }, { 0.30f, 0.65f }, { 0.08f, 0.32f } } };
    a.layer[lyBass]    = { { { 0.25f, 0.45f }, { 0.45f, 0.85f }, { 0.20f, 0.45f }, { 0.05f, 0.35f } } };
    if (mood == "Cinematic")
    {
        a.scales = { Minor, Dorian, Lydian, HarmMinor, Minor, Major };
        a.harmony = { 0.45f, 0.9f }; a.chordRate = { 0.3f, 0.7f }; a.evolution = { 0.4f, 0.75f };
        a.density = { 0.5f, 0.8f }; a.light = { 0.3f, 0.65f }; a.space = { 0.7f, 0.95f }; a.octave = { 1.0f, 2.4f };
        a.layer[lyPad][0] = { 0.6f, 0.8f };
        a.scenes = { scValley, scUnderwater, scValley };
    }
    else if (mood == "Meditation")
    {
        a.scales = { MajPent, Suspended, Major, Lydian, MinPent };
        a.harmony = { 0.0f, 0.35f }; a.chordRate = { 0.0f, 0.2f }; a.evolution = { 0.1f, 0.35f };
        a.density = { 0.3f, 0.55f }; a.light = { 0.4f, 0.7f }; a.space = { 0.7f, 0.95f }; a.tuning432 = 0.5f;
        a.layer[lyBells] = { { { 0.15f, 0.35f }, { 0.3f, 0.6f }, { 0.2f, 0.5f }, { 0.05f, 0.25f } } };
        a.layer[lyWind][0] = { 0.1f, 0.3f };
        a.scenes = { scValley, scStarfield, scValley };
        a.noises = { ntOcean, ntPink, ntBrown }; a.noiseChance = 0.45f;
    }
    else if (mood == "Dark")
    {
        a.scales = { Minor, Phrygian, HarmMinor, Hirajoshi, MinPent };
        a.harmony = { 0.15f, 0.6f }; a.light = { 0.0f, 0.3f }; a.octave = { 1.0f, 1.6f }; a.warmth = { 0.3f, 0.6f };
        a.layer[lyPad][2] = { 0.15f, 0.4f }; a.layer[lyDrone][2] = { 0.15f, 0.45f };
        a.layer[lyWind] = { { { 0.25f, 0.5f }, { 0.3f, 0.7f }, { 0.1f, 0.4f }, { 0.4f, 0.8f } } };
        a.scenes = { scUnderwater, scValley, scNeon };
        a.noises = { ntBrown, ntPink }; a.noiseChance = 0.4f;
    }
    else if (mood == "Sci-Fi")
    {
        a.scales = { WholeTone, Lydian, Hirajoshi, Phrygian, Minor };
        a.harmony = { 0.4f, 0.9f }; a.evolution = { 0.55f, 0.9f }; a.density = { 0.5f, 0.85f };
        a.layer[lyDrone][3] = { 0.6f, 0.95f }; a.layer[lyTexture] = { { { 0.45f, 0.7f }, { 0.5f, 0.9f }, { 0.55f, 0.95f }, { 0.1f, 0.6f } } };
        a.layer[lyShimmer][3] = { 0.5f, 1.0f };
        a.scenes = { scStarfield, scUnderwater, scNeon };
    }
    else if (mood == "Ethereal") // angelic, weightless: vowel-like drones, high shimmer, bright, slow
    {
        a.scales = { Lydian, Major, MajPent, Suspended }; a.harmony = { 0.45f, 0.85f }; a.chordRate = { 0.1f, 0.4f }; a.evolution = { 0.25f, 0.5f };
        a.light = { 0.6f, 0.95f }; a.space = { 0.85f, 1.0f }; a.octave = { 2.0f, 3.0f }; a.warmth = { 0.0f, 0.2f };
        a.layer[lyDrone] = { { { 0.5f, 0.7f }, { 0.9f, 1.0f }, { 0.55f, 0.85f }, { 0.15f, 0.4f } } };
        a.layer[lyShimmer] = { { { 0.55f, 0.75f }, { 0.75f, 1.0f }, { 0.55f, 0.85f }, { 0.3f, 0.8f } } };
        a.layer[lyBells] = { { { 0.2f, 0.35f }, { 0.4f, 0.75f }, { 0.55f, 0.85f }, { 0.2f, 0.45f } } };
        a.layer[lyWind][0] = { 0.05f, 0.2f };
        a.scenes = { scAurora, scUnderwater, scValley }; a.noiseChance = 0.1f;
    }
    else if (mood == "Oceanic") // the sea: slow swells, deep pads, waves of filtered noise
    {
        a.scales = { MajPent, Dorian, Mixolydian, Lydian, Minor }; a.harmony = { 0.3f, 0.7f }; a.chordRate = { 0.05f, 0.35f }; a.evolution = { 0.25f, 0.5f };
        a.light = { 0.3f, 0.75f }; a.space = { 0.75f, 0.95f }; a.octave = { 1.0f, 2.0f };
        a.layer[lyPad] = { { { 0.55f, 0.75f }, { 0.75f, 1.0f }, { 0.25f, 0.5f }, { 0.4f, 0.7f } } };
        a.layer[lyWind] = { { { 0.2f, 0.4f }, { 0.4f, 0.8f }, { 0.15f, 0.4f }, { 0.4f, 0.7f } } };
        a.layer[lyBells][0] = { 0.1f, 0.3f };
        a.scenes = { scOcean }; a.noises = { ntOcean }; a.noiseChance = 0.95f; a.noiseLevel = { 0.3f, 0.5f };
    }
    else if (mood == "Nostalgia") // warm lo-fi memories: major 7ths, wobbly tape-like pads, vinyl crackle
    {
        a.scales = { Major, Lydian, Dorian, Mixolydian }; a.harmony = { 0.55f, 0.95f }; a.chordRate = { 0.3f, 0.6f }; a.evolution = { 0.3f, 0.55f };
        a.light = { 0.45f, 0.75f }; a.space = { 0.5f, 0.8f }; a.warmth = { 0.45f, 0.75f }; a.octave = { 1.6f, 2.4f };
        a.layer[lyPad] = { { { 0.65f, 0.85f }, { 0.85f, 1.0f }, { 0.3f, 0.55f }, { 0.65f, 0.95f } } };
        a.layer[lyShimmer][0] = { 0.15f, 0.35f };
        a.layer[lyBells] = { { { 0.25f, 0.4f }, { 0.4f, 0.75f }, { 0.25f, 0.5f }, { 0.25f, 0.5f } } };
        a.scenes = { scValley, scOcean }; a.noises = { ntVinyl }; a.noiseChance = 0.85f; a.noiseLevel = { 0.25f, 0.45f };
    }
    else if (mood == "Epic") // trailer-size: low brass-like pads, big bass, wide space, bold chords
    {
        a.scales = { Minor, Dorian, HarmMinor, Phrygian }; a.harmony = { 0.4f, 0.75f }; a.chordRate = { 0.35f, 0.7f }; a.evolution = { 0.45f, 0.75f };
        a.density = { 0.6f, 0.9f }; a.light = { 0.25f, 0.55f }; a.space = { 0.8f, 1.0f }; a.octave = { 1.0f, 1.8f }; a.warmth = { 0.3f, 0.55f };
        a.layer[lyPad] = { { { 0.7f, 0.9f }, { 0.9f, 1.0f }, { 0.35f, 0.65f }, { 0.6f, 0.9f } } };
        a.layer[lyBass] = { { { 0.5f, 0.7f }, { 0.85f, 1.0f }, { 0.3f, 0.55f }, { 0.3f, 0.6f } } };
        a.layer[lyShimmer][0] = { 0.4f, 0.6f };
        a.scenes = { scValley, scAurora, scStarfield };
    }
    else if (mood == "Forest") // green, organic, warm: soft pads, birds of bells, wind in the leaves
    {
        a.scales = { MajPent, Major, Dorian, Mixolydian }; a.harmony = { 0.3f, 0.65f }; a.chordRate = { 0.15f, 0.45f }; a.evolution = { 0.25f, 0.5f };
        a.light = { 0.45f, 0.75f }; a.space = { 0.55f, 0.8f }; a.warmth = { 0.2f, 0.45f };
        a.layer[lyBells] = { { { 0.25f, 0.4f }, { 0.5f, 0.85f }, { 0.25f, 0.55f }, { 0.35f, 0.65f } } };
        a.layer[lyWind] = { { { 0.25f, 0.45f }, { 0.55f, 0.9f }, { 0.4f, 0.7f }, { 0.25f, 0.55f } } };
        a.layer[lyTexture] = { { { 0.3f, 0.5f }, { 0.4f, 0.75f }, { 0.4f, 0.7f }, { 0.2f, 0.5f } } };
        a.scenes = { scValley }; a.noises = { ntPink, ntBrown }; a.noiseChance = 0.3f;
    }
    else if (mood == "Desert") // heat haze and dry wind: Phrygian / Hijaz-like colours, drones, sparse bells
    {
        a.scales = { Phrygian, HarmMinor, Hirajoshi, MinPent }; a.harmony = { 0.1f, 0.45f }; a.chordRate = { 0.05f, 0.35f }; a.evolution = { 0.3f, 0.55f };
        a.light = { 0.55f, 0.85f }; a.space = { 0.6f, 0.85f }; a.warmth = { 0.35f, 0.6f }; a.octave = { 1.0f, 2.0f };
        a.layer[lyDrone] = { { { 0.65f, 0.85f }, { 0.95f, 1.0f }, { 0.45f, 0.75f }, { 0.35f, 0.65f } } };
        a.layer[lyWind] = { { { 0.35f, 0.55f }, { 0.7f, 1.0f }, { 0.5f, 0.85f }, { 0.55f, 0.85f } } };
        a.layer[lyBells] = { { { 0.2f, 0.35f }, { 0.3f, 0.6f }, { 0.55f, 0.85f }, { 0.15f, 0.4f } } };
        a.scenes = { scValley, scStarfield }; a.noises = { ntBrown }; a.noiseChance = 0.3f;
    }
    else if (mood == "Sleep") // as gentle as it gets: low, dark, slow, long fades, soft noise beds
    {
        a.scales = { MajPent, Suspended, Major }; a.harmony = { 0.0f, 0.3f }; a.chordRate = { 0.0f, 0.12f }; a.evolution = { 0.05f, 0.25f };
        a.density = { 0.2f, 0.4f }; a.light = { 0.1f, 0.35f }; a.space = { 0.7f, 0.9f }; a.octave = { 1.0f, 2.0f }; a.warmth = { 0.2f, 0.4f };
        a.layer[lyBells] = { { { 0.08f, 0.2f }, { 0.2f, 0.45f }, { 0.1f, 0.35f }, { 0.03f, 0.15f } } };
        a.layer[lyShimmer][0] = { 0.2f, 0.4f };
        a.layer[lyBass][0] = { 0.15f, 0.3f };
        a.scenes = { scAurora, scOcean, scValley }; a.noises = { ntPink, ntBrown, ntOcean }; a.noiseChance = 0.75f; a.noiseLevel = { 0.2f, 0.4f };
    }
    else if (mood == "Fairytale") // storybook magic: celesta / music-box tunes over warm, moving chords
    {
        a.melody = true;
        a.scales = { Major, Lydian, MajPent, Major }; a.harmony = { 0.4f, 0.75f }; a.chordRate = { 0.62f, 0.85f }; a.evolution = { 0.25f, 0.45f };
        a.density = { 0.35f, 0.55f }; a.light = { 0.55f, 0.85f }; a.space = { 0.6f, 0.85f }; a.octave = { 2.0f, 2.6f }; a.warmth = { 0.15f, 0.35f };
        a.layer[lyMelody] = { { { 0.62f, 0.78f }, { 0.75f, 1.0f }, { 0.62f, 0.95f }, { 0.25f, 0.6f } } };
        a.layer[lyPad] = { { { 0.5f, 0.68f }, { 0.85f, 1.0f }, { 0.35f, 0.6f }, { 0.25f, 0.5f } } };
        a.layer[lyDrone][0] = { 0.35f, 0.5f };
        a.layer[lyBells] = { { { 0.08f, 0.2f }, { 0.2f, 0.45f }, { 0.4f, 0.7f }, { 0.03f, 0.15f } } };
        a.layer[lyTexture][0] = { 0.15f, 0.3f }; a.layer[lyWind][0] = { 0.0f, 0.12f };
        a.layer[lyShimmer] = { { { 0.3f, 0.5f }, { 0.5f, 0.85f }, { 0.4f, 0.7f }, { 0.1f, 0.4f } } };
        a.scenes = { scAurora, scValley, scUnderwater }; a.noiseChance = 0.0f; a.fixedFocus = true;
        a.dly = { 0.2f, 0.4f }; a.dlyFb = { 0.3f, 0.5f };
    }
    else if (mood == "Film Score") // an orchestral-feeling theme: lyrical lead, lush chords, a bass line underneath
    {
        a.melody = true;
        a.scales = { Minor, Major, Dorian, HarmMinor, Lydian }; a.harmony = { 0.5f, 0.85f }; a.chordRate = { 0.6f, 0.82f }; a.evolution = { 0.3f, 0.5f };
        a.density = { 0.45f, 0.65f }; a.light = { 0.35f, 0.7f }; a.space = { 0.7f, 0.95f }; a.octave = { 1.6f, 2.2f }; a.warmth = { 0.25f, 0.45f };
        a.layer[lyMelody] = { { { 0.62f, 0.78f }, { 0.75f, 1.0f }, { 0.1f, 0.45f }, { 0.15f, 0.5f } } };
        a.layer[lyPad] = { { { 0.62f, 0.8f }, { 0.9f, 1.0f }, { 0.3f, 0.55f }, { 0.4f, 0.7f } } };
        a.layer[lyBass] = { { { 0.4f, 0.55f }, { 0.8f, 1.0f }, { 0.25f, 0.45f }, { 0.05f, 0.25f } } };
        a.layer[lyBells] = { { { 0.05f, 0.15f }, { 0.15f, 0.35f }, { 0.3f, 0.6f }, { 0.03f, 0.12f } } };
        a.layer[lyTexture][0] = { 0.15f, 0.3f }; a.layer[lyWind][0] = { 0.0f, 0.15f };
        a.layer[lyShimmer] = { { { 0.3f, 0.5f }, { 0.5f, 0.85f }, { 0.35f, 0.6f }, { 0.1f, 0.4f } } };
        a.scenes = { scValley, scStarfield, scOcean }; a.noiseChance = 0.0f; a.fixedFocus = true;
    }
    else // Ambient
    {
        a.scales = { Major, Lydian, MajPent, Suspended, Mixolydian, Dorian };
        a.light = { 0.45f, 0.8f };
        a.scenes = { scValley, scUnderwater, scStarfield };
        a.noises = { ntPink, ntOcean }; a.noiseChance = 0.3f;
    }
    return a;
}

void applySub (Arch& a, const juce::String& sub)
{
    if (sub == "Epic") { a.scales = { Lydian, Major, Dorian }; a.light = { 0.5f, 0.7f }; a.layer[lyShimmer][0] = { 0.5f, 0.7f }; a.layer[lyShimmer][1] = { 0.7f, 1.0f }; }
    else if (sub == "Wonder") { a.scales = { Lydian, MajPent, Major }; a.light = { 0.6f, 0.85f }; a.layer[lyBells][3] = { 0.35f, 0.6f }; a.layer[lyBells][1] = { 0.5f, 0.9f }; }
    else if (sub == "Tension") { a.scales = { Phrygian, HarmMinor, Minor }; a.harmony = { 0.15f, 0.5f }; a.light = { 0.15f, 0.35f }; a.layer[lyWind][1] = { 0.4f, 0.8f }; a.layer[lyBells][0] = { 0.1f, 0.25f }; }
    else if (sub == "Drift") { a.evolution = { 0.15f, 0.4f }; a.chordRate = { 0.05f, 0.3f }; }
    else if (sub == "Glow") { a.light = { 0.65f, 0.9f }; a.layer[lyShimmer][0] = { 0.5f, 0.7f }; }
    else if (sub == "Haze") { a.light = { 0.35f, 0.55f }; a.layer[lyTexture][1] = { 0.6f, 0.95f }; a.layer[lyWind][1] = { 0.4f, 0.8f }; }
    else if (sub == "Breath") { a.layer[lyWind] = { { { 0.3f, 0.5f }, { 0.6f, 0.9f }, { 0.2f, 0.5f }, { 0.3f, 0.6f } } }; }
    else if (sub == "Healing") { a.tuning432 = 1.0f; a.layer[lyShimmer][1] = { 0.6f, 0.95f }; a.harmony = { 0.0f, 0.2f }; }
    else if (sub == "Calm") { a.evolution = { 0.05f, 0.25f }; a.density = { 0.25f, 0.45f }; }
    else if (sub == "Abyss") { a.octave = { 1.0f, 1.2f }; a.layer[lyBells][1] = { 0.0f, 0.2f }; a.layer[lyWind][1] = { 0.6f, 0.95f }; a.light = { 0.0f, 0.15f }; }
    else if (sub == "Ritual") { a.scales = { Hirajoshi, Phrygian }; a.layer[lyTexture][1] = { 0.6f, 0.95f }; a.layer[lyBells][3] = { 0.3f, 0.55f }; }
    else if (sub == "Night") { a.scales = { Minor, Dorian, MinPent }; a.light = { 0.15f, 0.35f }; }
    else if (sub == "Deep Space") { a.light = { 0.05f, 0.35f }; a.space = { 0.85f, 1.0f }; a.octave = { 1.0f, 1.8f }; }
    else if (sub == "Machine") { a.layer[lyTexture][3] = { 0.0f, 0.3f }; a.layer[lyTexture][1] = { 0.7f, 1.0f }; a.layer[lyWind][1] = { 0.0f, 0.25f }; }
    else if (sub == "Trailer") { a.chordRate = { 0.5f, 0.75f }; a.layer[lyBass][0] = { 0.6f, 0.75f }; }
    else if (sub == "Heroes") { a.scales = { Dorian, Lydian, Major }; a.light = { 0.5f, 0.7f }; }
    else if (sub == "Battlefield") { a.scales = { Phrygian, HarmMinor }; a.light = { 0.1f, 0.3f }; a.layer[lyWind][1] = { 0.5f, 0.85f }; }
    else if (sub == "Ancient Woods") { a.light = { 0.3f, 0.5f }; a.octave = { 1.0f, 1.6f }; }
    else if (sub == "Morning Dew") { a.light = { 0.65f, 0.85f }; a.layer[lyBells][3] = { 0.45f, 0.7f }; }
    else if (sub == "Fireflies") { a.light = { 0.15f, 0.35f }; a.layer[lyBells][1] = { 0.6f, 0.95f }; }
    else if (sub == "Dunes") { a.evolution = { 0.15f, 0.35f }; }
    else if (sub == "Mirage") { a.layer[lyShimmer] = { { { 0.45f, 0.65f }, { 0.7f, 1.0f }, { 0.5f, 0.8f }, { 0.4f, 0.8f } } }; }
    else if (sub == "Caravan") { a.chordRate = { 0.2f, 0.45f }; a.layer[lyBass][1] = { 0.7f, 1.0f }; }
    else if (sub == "Deep Sleep") { a.light = { 0.0f, 0.2f }; a.evolution = { 0.02f, 0.15f }; }
    else if (sub == "Lullaby") { a.scales = { MajPent, Major }; a.layer[lyBells][0] = { 0.2f, 0.3f }; a.layer[lyBells][1] = { 0.4f, 0.7f }; }
    else if (sub == "Night Rain") { a.noises = { ntPink }; a.noiseChance = 1.0f; a.noiseLevel = { 0.3f, 0.45f }; }
    else if (sub == "Angelic") { a.layer[lyDrone][2] = { 0.6f, 0.85f }; a.layer[lyShimmer][3] = { 0.0f, 0.3f }; }
    else if (sub == "Celestial") { a.scenes = { scStarfield, scAurora }; a.layer[lyBells][1] = { 0.6f, 0.9f }; a.layer[lyShimmer][3] = { 0.6f, 1.0f }; }
    else if (sub == "Dreamland") { a.evolution = { 0.45f, 0.7f }; a.layer[lyTexture][1] = { 0.6f, 0.95f }; }
    else if (sub == "Shoreline") { a.light = { 0.55f, 0.85f }; a.layer[lyBells][1] = { 0.3f, 0.6f }; }
    else if (sub == "Deep Blue") { a.light = { 0.1f, 0.35f }; a.octave = { 1.0f, 1.2f }; a.noiseLevel = { 0.2f, 0.35f }; }
    else if (sub == "Tidal") { a.evolution = { 0.45f, 0.7f }; a.noiseLevel = { 0.4f, 0.6f }; }
    else if (sub == "Lo-Fi Memory") { a.noiseLevel = { 0.35f, 0.55f }; a.layer[lyBells][3] = { 0.35f, 0.6f }; }
    else if (sub == "Tape Dream") { a.layer[lyPad][3] = { 0.85f, 1.0f }; a.evolution = { 0.2f, 0.4f }; }
    else if (sub == "Golden Hour") { a.light = { 0.6f, 0.8f }; a.scenes = { scValley }; }
    else if (sub == "Neo-Noir") // rain-soaked neon city of the future: lush analog brass pads, glassy bells, long echoes
    {
        a.scales = { Minor, Dorian, Lydian, Minor, Phrygian }; a.harmony = { 0.55f, 0.92f }; a.chordRate = { 0.25f, 0.5f };
        a.evolution = { 0.3f, 0.55f }; a.density = { 0.45f, 0.7f }; a.light = { 0.12f, 0.38f }; a.space = { 0.82f, 1.0f };
        a.warmth = { 0.35f, 0.6f }; a.octave = { 1.0f, 2.0f }; a.dly = { 0.35f, 0.55f }; a.dlyFb = { 0.5f, 0.66f }; a.dlyTime = { 480.0f, 900.0f };
        a.layer[lyPad] = { { { 0.72f, 0.88f }, { 0.85f, 1.0f }, { 0.45f, 0.7f }, { 0.72f, 1.0f } } };
        a.layer[lyDrone] = { { { 0.45f, 0.65f }, { 0.9f, 1.0f }, { 0.25f, 0.45f }, { 0.15f, 0.35f } } };
        a.layer[lyTexture] = { { { 0.2f, 0.4f }, { 0.3f, 0.6f }, { 0.3f, 0.6f }, { 0.4f, 0.7f } } };
        a.layer[lyShimmer] = { { { 0.25f, 0.45f }, { 0.4f, 0.7f }, { 0.3f, 0.55f }, { 0.0f, 0.3f } } };
        a.layer[lyWind] = { { { 0.25f, 0.42f }, { 0.55f, 0.9f }, { 0.15f, 0.35f }, { 0.25f, 0.5f } } };
        a.layer[lyBells] = { { { 0.25f, 0.42f }, { 0.4f, 0.75f }, { 0.65f, 0.95f }, { 0.25f, 0.5f } } };
        a.noises = { ntPink, ntBrown }; a.noiseChance = 0.7f; a.noiseLevel = { 0.15f, 0.3f };
        a.scenes = { scNeon }; a.fixedFocus = true;
    }
    else if (sub == "Derelict") // a dead ship drifting in deep space: metallic mutating hum, air vents, sonar pings, dread
    {
        a.scales = { Phrygian, HarmMinor, WholeTone, Hirajoshi }; a.harmony = { 0.0f, 0.28f }; a.chordRate = { 0.0f, 0.25f };
        a.evolution = { 0.35f, 0.6f }; a.density = { 0.35f, 0.6f }; a.light = { 0.0f, 0.15f }; a.space = { 0.75f, 0.95f };
        a.warmth = { 0.15f, 0.4f }; a.octave = { 1.0f, 1.3f }; a.dly = { 0.2f, 0.4f }; a.dlyTime = { 600.0f, 1400.0f };
        a.layer[lyDrone] = { { { 0.7f, 0.85f }, { 0.95f, 1.0f }, { 0.1f, 0.35f }, { 0.65f, 0.95f } } };
        a.layer[lyPad] = { { { 0.2f, 0.4f }, { 0.3f, 0.6f }, { 0.08f, 0.3f }, { 0.2f, 0.5f } } };
        a.layer[lyTexture] = { { { 0.4f, 0.6f }, { 0.5f, 0.85f }, { 0.7f, 1.0f }, { 0.08f, 0.35f } } };
        a.layer[lyShimmer] = { { { 0.2f, 0.4f }, { 0.3f, 0.6f }, { 0.1f, 0.35f }, { 0.7f, 1.0f } } };
        a.layer[lyWind] = { { { 0.35f, 0.55f }, { 0.7f, 1.0f }, { 0.08f, 0.32f }, { 0.5f, 0.8f } } };
        a.layer[lyBells] = { { { 0.15f, 0.3f }, { 0.3f, 0.6f }, { 0.8f, 1.0f }, { 0.08f, 0.28f } } };
        a.scenes = { scStarfield }; a.fixedFocus = true;
    }
    else if (sub == "Music Box") { a.layer[lyMelody][2] = { 0.82f, 1.0f }; a.layer[lyMelody][3] = { 0.35f, 0.65f }; a.light = { 0.6f, 0.85f }; }
    else if (sub == "Enchanted") { a.scales = { Lydian, Major }; a.layer[lyMelody][2] = { 0.55f, 0.75f }; a.layer[lyShimmer][1] = { 0.75f, 1.0f }; a.scenes = { scAurora, scUnderwater }; }
    else if (sub == "Once Upon") { a.scales = { Major, MajPent }; a.layer[lyMelody][3] = { 0.1f, 0.35f }; a.layer[lyMelody][2] = { 0.65f, 0.9f }; a.light = { 0.45f, 0.7f }; a.scenes = { scValley }; }
    else if (sub == "Main Theme") { a.scales = { Major, Lydian, Dorian }; a.layer[lyMelody][2] = { 0.25f, 0.45f }; a.light = { 0.5f, 0.75f }; }
    else if (sub == "Heartfelt") { a.scales = { Minor, Major, Dorian }; a.layer[lyMelody][3] = { 0.05f, 0.3f }; a.layer[lyMelody][2] = { 0.05f, 0.3f }; a.evolution = { 0.2f, 0.4f }; }
    else if (sub == "Adventure") { a.scales = { Dorian, Mixolydian, Major }; a.layer[lyMelody][3] = { 0.5f, 0.75f }; a.layer[lyBass][0] = { 0.5f, 0.65f }; a.scenes = { scValley, scStarfield, scOcean }; }
    else if (sub == "Alien") { a.scales = { WholeTone, Hirajoshi }; a.layer[lyDrone][3] = { 0.8f, 1.0f }; }
}

struct B
{
    Preset& p;
    Rng& r;
    float rr (Range2 g) { return g.lo + (g.hi - g.lo) * r.uni(); }
    void set (int i, float v) { p.params[(size_t) i] = paramDefs()[(size_t) i].range.snapToLegalValue (v); }
};
} // namespace

bool paramInLock (const ParamDef& d, const juce::String& lock)
{
    if (lock == "harmony") return d.id == "key" || d.id == "scale" || d.id == "octave" || d.id == "harmony" || d.id == "chord_rate" || d.id == "tuning";
    if (lock == "layers") return layerIds().contains (d.group);
    if (lock == "fx") return d.group == "fx" || d.id == "space";
    return false;
}

void applyLocks (Preset& dst, const Preset& src, const std::set<juce::String>& locks)
{
    for (auto& d : paramDefs())
        for (auto& l : locks)
            if (paramInLock (d, l)) dst.params[(size_t) d.index] = src.params[(size_t) d.index];
}

float Generator::targetRmsDb (const juce::String& mood)
{
    if (mood == "Meditation") return -23.0f;
    if (mood == "Cinematic") return -19.5f;
    if (mood == "Sci-Fi") return -20.5f;
    if (mood == "Dark") return -21.0f;
    if (mood == "Oceanic" || mood == "Ethereal") return -21.5f;
    if (mood == "Nostalgia") return -20.0f;
    if (mood == "Epic") return -18.5f;
    if (mood == "Sleep") return -25.0f;
    if (mood == "Fairytale") return -20.0f;
    if (mood == "Film Score") return -19.5f;
    return -21.0f;
}

Preset Generator::candidate (const GenRequest& req, uint32_t seed)
{
    Preset p;
    Rng r (seed * 2654435761u + 12345u);
    B b { p, r };
    const auto mood = categories().contains (req.category) ? req.category : juce::String ("Ambient");
    const auto& subs = subcategories (mood);
    const auto sub = req.subcategory.isNotEmpty() ? req.subcategory : subs[(int) (r.next() % (uint32_t) subs.size())];
    Arch a = baseArch (mood);
    applySub (a, sub);

    // ---- harmony
    int scale = a.scales[(size_t) (r.next() % (uint32_t) a.scales.size())];
    if (req.mystery > 0.7f && r.uni() < (req.mystery - 0.6f) * 1.2f)
    {
        const int exotic[] { Phrygian, HarmMinor, Hirajoshi, WholeTone, Lydian };
        scale = exotic[r.next() % 5];
    }
    b.set (P::scale, (float) scale);
    b.set (P::key, (float) (r.next() % 12));
    b.set (P::octave, std::round (b.rr (a.octave)));
    b.set (P::harmony, b.rr (a.harmony) + (req.mystery - 0.5f) * 0.25f);
    b.set (P::chordRate, b.rr (a.chordRate) + (req.movement - 0.5f) * 0.3f);
    b.set (P::tuning, r.uni() < a.tuning432 ? 1.0f : 0.0f);
    b.set (P::seed, (float) (1 + r.next() % 99999));
    // ---- macros (character sliders bias them)
    b.set (P::evolution, b.rr (a.evolution) + (req.movement - 0.5f) * 0.5f);
    b.set (P::density, b.rr (a.density) + (req.density - 0.5f) * 0.5f);
    b.set (P::light, b.rr (a.light) + (req.light - 0.5f) * 0.5f);
    b.set (P::space, b.rr (a.space) + (req.space - 0.5f) * 0.4f);
    b.set (P::freeze, 0);
    // ---- layers
    for (int l = 0; l < kNumLayers; ++l)
    {
        b.set (P::layer (l, P::lOn), 1);
        b.set (P::layer (l, P::lLevel), b.rr (a.layer[(size_t) l][0]));
        b.set (P::layer (l, P::lPresence), b.rr (a.layer[(size_t) l][1]));
        b.set (P::layer (l, P::lTone), b.rr (a.layer[(size_t) l][2]));
        b.set (P::layer (l, P::lChar), b.rr (a.layer[(size_t) l][3]) + (l == lyDrone ? req.mystery * 0.25f : 0.0f));
    }
    if (! a.melody) b.set (P::layer (lyMelody, P::lOn), 0);   // the melody belongs to the melodic moods only
    // variety with coherence: one featured layer leads, one or two step back (unless the sub-mood fixes the balance)
    if (! a.fixedFocus)
    {
        const int featured = 1 + (int) (r.next() % 6);
        b.set (P::layer (featured, P::lLevel), p.get (P::layer (featured, P::lLevel)) + 0.15f);
        b.set (P::layer (featured, P::lPresence), p.get (P::layer (featured, P::lPresence)) + 0.25f);
        const int quiet = (int) (r.next() % 3);
        for (int k = 0; k < quiet; ++k)
        {
            const int l = 1 + (int) (r.next() % 6);
            if (l != featured) b.set (P::layer (l, P::lPresence), p.get (P::layer (l, P::lPresence)) - 0.3f);
        }
    }
    // the drone carries the piece: never let it disappear for long
    if (p.get (P::layer (lyDrone, P::lPresence)) < 0.85f) b.set (P::layer (lyDrone, P::lPresence), 0.9f);
    // occasionally drop a layer for a sparser, more characterful landscape
    if (! a.fixedFocus && r.uni() < 0.35f) b.set (P::layer (r.uni() < 0.5f ? lyWind : lyBells, P::lOn), 0);
    // ---- fx
    b.set (P::warmth, b.rr (a.warmth));
    b.set (P::dlyMix, b.rr (a.dly));
    b.set (P::dlyTime, b.rr (a.dlyTime));
    b.set (P::dlyFb, b.rr (a.dlyFb));
    const bool useNoise = r.uni() < a.noiseChance;
    b.set (P::noiseType, useNoise ? (float) a.noises[(size_t) (r.next() % (uint32_t) a.noises.size())] : 0.0f);
    b.set (P::noiseLevel, b.rr (a.noiseLevel));
    b.set (P::noiseTone, 0.25f + 0.5f * r.uni());
    b.set (P::scene, (float) a.scenes[(size_t) (r.uni() < 0.6f ? 0 : r.next() % (uint32_t) a.scenes.size())]);
    b.set (P::dlyTone, 0.3f + r.uni() * 0.4f);
    b.set (P::revDamp, 0.25f + r.uni() * 0.4f);
    b.set (P::width, 0.9f + r.uni() * 0.4f);
    b.set (P::lowCut, mood == "Dark" ? 22.0f : 28.0f + r.uni() * 20.0f);
    // brighter landscapes keep more top end; dark ones roll it off (coherent with Light)
    b.set (P::highCut, 6000.0f + 14000.0f * juce::jlimit (0.0f, 1.0f, p.get (P::light) * 0.8f + r.uni() * 0.3f));
    b.set (P::fadeIn, mood == "Meditation" || mood == "Sleep" ? 8.0f : 3.0f + r.uni() * 4.0f);
    b.set (P::fadeOut, mood == "Meditation" || mood == "Sleep" ? 5.0f : 2.0f + r.uni() * 2.0f);
    b.set (P::sleepMode, mood == "Sleep" ? 1.0f : 0.0f);

    if (req.base != nullptr && ! req.locks.empty()) applyLocks (p, *req.base, req.locks);

    Rng nr (seed ^ 0x51u);
    p.meta.uuid = juce::Uuid().toString();
    p.meta.category = mood;
    p.meta.subcategory = sub;
    p.meta.name = makeName (mood, nr, sub);
    p.meta.author = "Memoria AI";
    p.meta.tags = deriveTags (p);
    p.meta.created = p.meta.modified = juce::Time::getCurrentTime();
    return p;
}

Validation Generator::validate (Preset& p, bool fixLevel)
{
    Validation v;
    constexpr double sr = 48000.0, secs = 22.0, from = 9.0;
    juce::AudioBuffer<float> b (2, (int) (secs * sr));
    b.clear();
    renderPreset (p, b, sr, 1024);
    double sq = 0, sum = 0;
    float peak = 0;
    bool finite = true;
    const int s0 = (int) (from * sr), n = b.getNumSamples() - s0;
    for (int c = 0; c < 2; ++c)
    {
        const float* d = b.getReadPointer (c, s0);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (d[i])) { finite = false; continue; }
            peak = juce::jmax (peak, std::abs (d[i]));
            sq += (double) d[i] * d[i]; sum += d[i];
        }
    }
    const float rms = (float) std::sqrt (sq / (2.0 * n));
    {   // movement: spread of the 0.5 s loudness (dB); width: side / mid energy
        std::vector<float> win;
        const int w = (int) (0.5 * sr);
        double mid = 0, side = 0;
        for (int st = 0; st + w <= n; st += w)
        {
            double e = 0;
            for (int i = 0; i < w; ++i)
            {
                const float l = b.getSample (0, s0 + st + i), r = b.getSample (1, s0 + st + i);
                e += 0.5 * (l * l + r * r);
                mid += 0.25 * (l + r) * (l + r); side += 0.25 * (l - r) * (l - r);
            }
            win.push_back (10.0f * std::log10 ((float) (e / w) + 1.0e-12f));
        }
        float mean = 0, var = 0;
        for (float x : win) mean += x;
        mean /= (float) juce::jmax ((size_t) 1, win.size());
        for (float x : win) var += (x - mean) * (x - mean);
        v.movement = std::sqrt (var / (float) juce::jmax ((size_t) 1, win.size()));
        v.width = (float) std::sqrt (side / (mid + 1.0e-12));
    }
    v.rmsDb = 20.0f * std::log10 (juce::jmax (1.0e-9f, rms));
    v.peakDb = 20.0f * std::log10 (juce::jmax (1.0e-9f, peak));
    v.dc = (float) std::abs (sum / (2.0 * n));
    if (! finite) { v.reason = "nan"; return v; }
    if (v.rmsDb < -60.0f) { v.reason = "silent"; return v; }
    if (v.dc > 0.02f) { v.reason = "dc"; return v; }
    if (fixLevel)
    {
        // linear gain toward the mood's loudness, peaks kept clear of the safety limiter (-2.5 dBFS)
        const float gainDb = juce::jmin (targetRmsDb (p.meta.category) - v.rmsDb, -2.5f - v.peakDb);
        const float master = p.get (P::master) + gainDb;
        if (master > 12.0f) { v.reason = "too quiet"; return v; }
        p.params[(size_t) P::master] = juce::jlimit (-48.0f, 12.0f, master);
        v.rmsDb += gainDb; v.peakDb += gainDb;
    }
    v.ok = true;
    return v;
}

Preset Generator::generate (const GenRequest& req, int* attempts, Validation* result, int maxAttempts, int pickBest)
{
    Preset best, first;
    Validation bv, fv;
    int found = 0;
    for (int k = 0; k < maxAttempts && found < pickBest; ++k)
    {
        auto p = candidate (req, req.seed + (uint32_t) k * 7919u);
        auto v = validate (p);
        if (attempts) *attempts = k + 1;
        if (k == 0) { first = p; fv = v; }
        if (! v.ok) continue;
        v.score = interestScore (p, v);
        if (found == 0 || v.score > bv.score) { best = p; bv = v; }
        ++found;
    }
    if (found == 0) { if (result) *result = fv; return first; }
    if (result) *result = bv;
    return best;
}

float Generator::interestScore (const Preset& p, const Validation& v)
{
    int layers = 0;
    for (int l = 0; l < kNumLayers; ++l) layers += p.get (P::layer (l, P::lOn)) > 0.5f;
    // alive but calm: gentle movement is rewarded, restlessness is not; width and a full ensemble help;
    // richer harmony (7ths / 9ths) and a present pad make it more musical
    const float mv = v.movement < 4.0f ? v.movement : 4.0f - 0.8f * (v.movement - 4.0f);
    return mv + 4.0f * juce::jmin (v.width, 0.8f) + 0.3f * (float) layers + 1.2f * p.get (P::harmony)
           + 1.0f * p.get (P::layer (lyPad, P::lLevel)) * p.get (P::layer (lyPad, P::lPresence));
}

Preset Generator::mutate (const Preset& src, float amount, uint32_t seed, const std::set<juce::String>& locks)
{
    Preset p = src;
    Rng rng (seed ^ 0xA5A5A5A5u);
    for (auto& d : paramDefs())
    {
        if (d.id == "master" || d.id == "seed" || d.id == "timer" || d.id == "tuning" || d.id == "midi_mode" || d.id == "freeze" || d.id.endsWith ("_on")) continue;
        bool locked = false;
        for (auto& l : locks) locked |= paramInLock (d, l);
        if (locked) continue;
        auto& v = p.params[(size_t) d.index];
        if (d.kind == pkFloat)
        {
            const float n = d.range.convertTo0to1 (v);
            const float g = (rng.uni() + rng.uni() + rng.uni() - 1.5f) * 0.25f * amount; // ~gaussian
            v = d.range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n + g));
        }
        else if ((d.kind == pkChoice || d.kind == pkInt) && d.id != "key" && rng.uni() < amount * 0.05f)
            v = juce::jlimit (d.range.start, d.range.end, v + (rng.uni() < 0.5f ? -1.0f : 1.0f));
    }
    if (rng.uni() < 0.5f) p.params[(size_t) P::seed] = (float) (1 + rng.next() % 99999); // a new journey half of the time
    p.meta.uuid = juce::Uuid().toString();
    p.meta.name = src.meta.name.upToFirstOccurrenceOf (" ~", false, false) + " ~" + juce::String (1 + (int) (rng.next() % 99));
    p.meta.factory = false;
    p.meta.file = juce::File();
    p.meta.tags = deriveTags (p);
    return p;
}

Preset Generator::blend (const Preset& a, const Preset& b, float t, uint32_t seed)
{
    Preset p = a;
    Rng rng (seed ^ 0x5EEDu);
    for (auto& d : paramDefs())
    {
        auto& v = p.params[(size_t) d.index];
        const float vb = b.params[(size_t) d.index];
        if (d.kind == pkFloat) v = d.range.convertFrom0to1 (d.range.convertTo0to1 (v) * (1 - t) + d.range.convertTo0to1 (vb) * t);
        else if (rng.uni() < t) v = vb;
    }
    p.meta.uuid = juce::Uuid().toString();
    p.meta.name = a.meta.name.upToFirstOccurrenceOf (" x ", false, false) + " x " + b.meta.name.upToFirstOccurrenceOf (" x ", false, false);
    p.meta.category = t > 0.5f ? b.meta.category : a.meta.category;
    p.meta.subcategory = t > 0.5f ? b.meta.subcategory : a.meta.subcategory;
    p.meta.factory = false;
    p.meta.file = juce::File();
    p.meta.tags = deriveTags (p);
    return p;
}

juce::StringArray Generator::deriveTags (const Preset& p)
{
    juce::StringArray t;
    auto lay = [&] (int l, int f) { return p.get (P::layer (l, f)); };
    auto amount = [&] (int l) { return lay (l, P::lOn) > 0.5f ? lay (l, P::lLevel) * lay (l, P::lPresence) : 0.0f; };
    if (p.get (P::light) < 0.3f) t.add ("Dark");
    if (p.get (P::light) > 0.7f) t.add ("Bright");
    if (p.get (P::warmth) > 0.35f || (p.get (P::light) < 0.5f && p.get (P::light) > 0.3f)) t.add ("Warm");
    if (p.get (P::light) > 0.55f && p.get (P::warmth) < 0.15f) t.add ("Cold");
    if (p.get (P::evolution) > 0.6f) t.add ("Evolving");
    if (p.get (P::evolution) < 0.25f) t.add ("Static");
    if (amount (lyShimmer) > 0.3f) t.add ("Shimmering");
    if (amount (lyTexture) > 0.3f) t.add ("Granular");
    if (amount (lyWind) > 0.2f) t.add ("Windy");
    if (amount (lyBells) > 0.15f && lay (lyBells, P::lChar) > 0.3f) t.add ("Sparkling");
    if ((int) p.get (P::octave) <= 1) t.add ("Deep");
    if (p.get (P::space) > 0.8f) t.add ("Wide");
    if (p.get (P::harmony) > 0.7f) t.add ("Lush");
    if (p.get (P::freeze) > 0.5f) t.add ("Frozen");
    if ((int) p.get (P::tuning) == 1) t.add ("432 Hz");
    if (p.meta.category == "Meditation") t.add ("Calm");
    if (p.meta.subcategory == "Tension" || p.meta.subcategory == "Ritual" || p.meta.subcategory == "Derelict") t.add ("Tense");
    if ((int) p.get (P::noiseType) == ntOcean) t.add ("Ocean");
    if ((int) p.get (P::noiseType) == ntVinyl) t.add ("Vinyl");
    if (p.meta.category == "Ethereal") t.add ("Choir");
    if (p.meta.category == "Nostalgia") t.add ("Nostalgic");
    if (lay (lyPad, P::lChar) > 0.7f && amount (lyPad) > 0.4f) t.add ("Analog");
    if (p.meta.subcategory == "Neo-Noir") t.add ("Noir");
    if (p.meta.subcategory == "Derelict") t.add ("Haunted");
    if (lay (lyMelody, P::lOn) > 0.5f) t.add ("Melodic");
    t.removeDuplicates (false);
    return t;
}

juce::String Generator::makeName (const juce::String& mood, Rng& rng, const juce::String& sub)
{
    static const std::map<juce::String, std::pair<juce::StringArray, juce::StringArray>> subWords {
        { "Neo-Noir", { { "Neon", "Acid", "Chrome", "Midnight", "Wet", "Electric", "Synthetic", "Lonely", "Burning", "Smoky", "Rain-Soaked", "Violet", "Amber", "Endless", "Replica", "Off-World", "Holo", "Fading", "Last", "Silent" },
                        { "Rain", "Skyline", "Avenue", "District", "Memories", "Tears", "Rooftops", "Alley", "Arcology", "Detective", "Boulevard", "Haze", "Billboards", "Origami", "Overpass", "Nightfall", "Android", "Signals", "Streets", "Sector 7" } } },
        { "Derelict", { { "Derelict", "Cold", "Silent", "Abandoned", "Drifting", "Dead", "Frozen", "Hollow", "Forsaken", "Dark", "Lost", "Sealed", "Corroded", "Quarantined", "Deep", "Haunted", "Black", "Sleeping", "Ruined", "Last" },
                        { "Hull", "Cargo Bay", "Hypersleep", "Vents", "Corridor", "Airlock", "Signal", "Freighter", "Bulkhead", "Reactor", "Mothership", "Egg Chamber", "Beacon", "Moon", "Colony", "Station", "Engine Room", "Crew Deck", "Transmission", "Wreck" } } },
    };
    if (auto st = subWords.find (sub); st != subWords.end())
        return st->second.first[(int) (rng.next() % (uint32_t) st->second.first.size())] + " " + st->second.second[(int) (rng.next() % (uint32_t) st->second.second.size())];

    static const std::map<juce::String, std::pair<juce::StringArray, juce::StringArray>> words {
        { "Cinematic", { { "Endless", "Burning", "Silent", "Fallen", "Golden", "Distant", "Last", "Rising", "Broken", "Eternal", "Northern", "Iron", "Crimson", "Ancient", "Lost", "Final", "Vast", "Hidden", "Sacred", "Frozen" },
                         { "Horizon", "Empire", "Kingdom", "Odyssey", "Exodus", "Citadel", "Frontier", "Monument", "Requiem", "Voyage", "Dominion", "Titans", "Legacy", "Valley", "Shores", "Covenant", "Ascent", "Dynasty", "Prophecy", "Dawn" } } },
        { "Ambient", { { "Floating", "Quiet", "Soft", "Drifting", "Weightless", "Pale", "Golden", "Hazy", "Morning", "Slow", "Velvet", "Liquid", "Tender", "Amber", "Gentle", "Faded", "Open", "Warm", "Lucid", "Still" },
                       { "Clouds", "Fields", "Tides", "Light", "Meadow", "Lagoon", "Haze", "Bloom", "Drift", "Breeze", "Shore", "Garden", "Mist", "Glow", "Rivers", "Dunes", "Echoes", "Petals", "Sky", "Waters" } } },
        { "Meditation", { { "Inner", "Calm", "Sacred", "Peaceful", "Healing", "Mindful", "Still", "Pure", "Deep", "Open", "Gentle", "Silent", "Golden", "Infinite", "Serene", "Clear", "Present", "Soft", "Whole", "Breathing" },
                          { "Breath", "Temple", "Lotus", "Silence", "Mantra", "Stillness", "Garden", "Light", "Presence", "Heart", "Sanctuary", "Ocean", "Balance", "Spirit", "Awareness", "Haven", "Rest", "Pulse", "Grace", "Unity" } } },
        { "Dark", { { "Black", "Hollow", "Drowned", "Cursed", "Abyssal", "Moonless", "Forgotten", "Silent", "Haunted", "Shadow", "Bleak", "Ashen", "Sunken", "Grim", "Endless", "Cold", "Veiled", "Restless", "Obsidian", "Midnight" },
                    { "Abyss", "Catacombs", "Ritual", "Depths", "Void", "Crypt", "Marsh", "Mire", "Omen", "Ruins", "Labyrinth", "Hollow", "Chasm", "Wraith", "Tomb", "Forest", "Night", "Well", "Altar", "Eclipse" } } },
        { "Ethereal", { { "Angelic", "Weightless", "Luminous", "Celestial", "Crystal", "Heavenly", "Floating", "Radiant", "Pure", "Silver", "Halo", "Soft", "Endless", "Gossamer", "Seraphic", "Feathered", "Glowing", "Pale", "Divine", "Opaline" },
                        { "Choir", "Wings", "Light", "Veil", "Spire", "Prism", "Grace", "Hymn", "Clouds", "Ascension", "Vespers", "Aura", "Psalm", "Lantern", "Sky", "Bloom", "Cathedral", "Mist", "Sanctum", "Dawn" } } },
        { "Oceanic", { { "Deep", "Tidal", "Salted", "Blue", "Drowned", "Coral", "Endless", "Calm", "Rolling", "Foamy", "Moonlit", "Silent", "Distant", "Abyssal", "Sunken", "Lunar", "Wandering", "Pearl", "Misty", "Open" },
                       { "Shore", "Tides", "Reef", "Harbour", "Lagoon", "Currents", "Waves", "Swell", "Undertow", "Bay", "Horizon", "Driftwood", "Lighthouse", "Kelp Forest", "Seafloor", "Tidepool", "Mariner", "Sirens", "Estuary", "Atlantic" } } },
        { "Nostalgia", { { "Faded", "Golden", "Sunday", "Polaroid", "Dusty", "Warm", "Old", "Summer", "Velvet", "Childhood", "Lost", "Sepia", "Hazy", "Sweet", "Late", "Analog", "Rewound", "Grandma's", "Sleepy", "Autumn" },
                         { "Memories", "Tapes", "Afternoon", "Letters", "Cassette", "Photographs", "Radio", "Bedroom", "Porch", "Vinyl", "Holidays", "Seaside", "Daydream", "Lullaby", "Postcards", "Home Movies", "Attic", "Sunset", "Records", "Window" } } },
        { "Epic", { { "Rising", "Fallen", "Iron", "Last", "Burning", "Eternal", "Titanic", "Crimson", "Golden", "Final", "Thundering", "Endless", "Sacred", "Mighty", "Shattered", "Glorious", "Silent", "Valiant", "Ancient", "Storm" },
                    { "Legion", "Kingdom", "Crusade", "Throne", "Siege", "Empire", "Oath", "Ascent", "Uprising", "Vanguard", "Armada", "Colossus", "Odyssey", "Reckoning", "Dynasty", "Monolith", "Requiem", "Citadel", "Exodus", "Horizon" } } },
        { "Forest", { { "Ancient", "Mossy", "Green", "Whispering", "Hidden", "Morning", "Sunlit", "Deep", "Wild", "Silver", "Old", "Quiet", "Dewy", "Emerald", "Misty", "Gentle", "Elder", "Fern", "Amber", "Twilight" },
                      { "Grove", "Canopy", "Woods", "Glade", "Clearing", "Roots", "Thicket", "Brook", "Hollow", "Pines", "Meadow", "Willows", "Fireflies", "Leaves", "Moss", "Cedars", "Trail", "Spring", "Birches", "Ferns" } } },
        { "Desert", { { "Burning", "Golden", "Endless", "Silent", "Shifting", "Scorched", "Sandy", "Ancient", "Hidden", "Crimson", "Distant", "Dry", "Wandering", "Sun-Baked", "Lonely", "Amber", "Copper", "Hollow", "Bronze", "Veiled" },
                      { "Dunes", "Mirage", "Oasis", "Caravan", "Sands", "Canyon", "Mesa", "Nomads", "Sirocco", "Ruins", "Bazaar", "Sun Temple", "Wells", "Horizon", "Salt Flats", "Kasbah", "Sphinx", "Crossing", "Winds", "Silk Road" } } },
        { "Sleep", { { "Soft", "Sleepy", "Gentle", "Velvet", "Quiet", "Drowsy", "Dreamy", "Warm", "Hushed", "Slow", "Silken", "Calm", "Starry", "Cozy", "Feather", "Distant", "Tender", "Moonlit", "Sweet", "Still" },
                     { "Lullaby", "Pillow", "Blanket", "Dreams", "Night", "Slumber", "Cradle", "Moon", "Hush", "Stars", "Rest", "Nest", "Clouds", "Breathing", "Midnight", "Haven", "Nightlight", "Sleep", "Drift", "Embrace" } } },
        { "Fairytale", { { "Enchanted", "Little", "Silver", "Sleeping", "Golden", "Moonlit", "Secret", "Wishing", "Magic", "Snowy", "Starlit", "Velvet", "Glass", "Hidden", "Faraway", "Dancing", "Painted", "Tiny", "Crystal", "Midnight" },
                         { "Music Box", "Castle", "Garden", "Lantern", "Princess", "Forest", "Carousel", "Kingdom", "Wishes", "Storybook", "Tower", "Fairies", "Snowglobe", "Unicorn", "Clockwork", "Meadow", "Dragonfly", "Ballroom", "Tales", "Moon" } } },
        { "Film Score", { { "Main", "Final", "Endless", "Brave", "Distant", "Last", "Rising", "Silent", "Golden", "Northern", "Lost", "Eternal", "Gentle", "Hopeful", "Faithful", "Wandering", "Bright", "Quiet", "Open", "Home" },
                          { "Theme", "Journey", "Horizon", "Promise", "Letters", "Farewell", "Voyage", "Homecoming", "Skies", "Overture", "Reunion", "Legend", "Shores", "Memories", "Frontier", "Prelude", "Embrace", "Ballad", "Waltz", "Finale" } } },
        { "Sci-Fi", { { "Orbital", "Quantum", "Stellar", "Ionic", "Derelict", "Alien", "Neural", "Silent", "Cryo", "Hyper", "Distant", "Signal", "Plasma", "Binary", "Cosmic", "Synthetic", "Polar", "Lunar", "Void", "Magnetic" },
                      { "Station", "Nebula", "Drift", "Colony", "Array", "Beacon", "Outpost", "Transmission", "Reactor", "Expanse", "Corridor", "Probe", "Singularity", "Hive", "Orbit", "Relay", "Horizon", "Archive", "Vessel", "Pulsar" } } },
    };
    auto it = words.find (mood);
    const auto& wd = it != words.end() ? it->second : words.at ("Ambient");
    return wd.first[(int) (rng.next() % (uint32_t) wd.first.size())] + " " + wd.second[(int) (rng.next() % (uint32_t) wd.second.size())];
}
}
