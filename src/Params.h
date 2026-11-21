#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Single source of truth for every parameter. IDs are FROZEN once released (sessions/presets depend on them).
namespace gx
{
constexpr int kNumLayers = 8;
enum Layer { lyDrone, lyPad, lyTexture, lyShimmer, lyWind, lyBells, lyBass, lyMelody };   // new layers go at the end (IDs frozen)

namespace P
{
    enum Global : int
    {
        master, seed, key, scale, octave, harmony, chordRate,
        evolution, density, light, space, freeze,
        tuning, midiMode, fadeIn, fadeOut, timer,
        width, warmth, lowCut, highCut, revDamp, dlyTime, dlyFb, dlyTone, dlyMix,
        scene, noiseType, noiseLevel, noiseTone, syncChords, chordBars, sleepMode,
        numGlobal
    };
    enum LayerField : int { lOn, lLevel, lPresence, lTone, lChar, numLayerFields };

    constexpr int layerBase = numGlobal;
    constexpr int layer (int l, int f) { return layerBase + l * numLayerFields + f; }
    constexpr int numParams = layerBase + kNumLayers * numLayerFields;
}

enum ParamKind { pkFloat, pkInt, pkBool, pkChoice };
enum MidiMode { mmOff, mmKey, mmPlay, mmKeyPlay };
enum Scene { scValley, scStarfield, scUnderwater, scNeon, scOcean, scAurora, numScenes };
enum NoiseType { ntOff, ntPink, ntBrown, ntWhite, ntOcean, ntVinyl };

struct ParamDef
{
    int index = -1;
    juce::String id, name, unit;
    ParamKind kind = pkFloat;
    juce::NormalisableRange<float> range;
    float def = 0;
    juce::StringArray choices;
    juce::String group;          // "global", "macro", "fx", "drone", "pad"... (generator, section copy)
};

const std::vector<ParamDef>& paramDefs();
int paramIndex (const juce::String& id);                     // -1 if unknown
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

const juce::StringArray& layerIds();                         // "drone", "pad", ...
juce::String layerName (int layer);                          // localized (UI)
juce::String sceneName (int scene);                          // localized (UI)
const juce::StringArray& keyNames();
const juce::StringArray& scaleNames();
const std::vector<int>& scaleSteps (int scale);              // semitones from the root, ascending, < 12
}
