#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Single source of truth for every parameter. IDs are FROZEN once released (sessions/presets depend on them).
// Add new parameters at the END of the table only.
namespace gx
{
namespace P
{
    enum Global : int
    {
        master, seed, piano, tuning,
        tone, hammer, sustain, damper, detune, stretch, dynamics,   // the piano itself
        body, resonance, width, room, roomSize,                     // soundboard, sympathetic strings, stereo, room
        memory, warmth, intimacy, time, spaceMacro,                 // macros
        roomType, tape, vinyl, wood, world, worldLevel,             // layers of life
        hands, handsKey, handsScale, handsStyle, handsBars, handsTempo, handsDensity, handsExpress, handsSeed, handsPedal, handsEndless,   // generative hands
        handsForm,                                                  // song form (appended later: older presets get the default)
        sceneTime, sceneSeason,                                     // scene: hour of the day and season (0 = auto, from the World layer)
        numParams
    };
}

enum ParamKind { pkFloat, pkInt, pkBool, pkChoice };

struct ParamDef
{
    int index = -1;
    juce::String id, name, unit;
    ParamKind kind = pkFloat;
    juce::NormalisableRange<float> range;
    float def = 0;
    juce::StringArray choices;
    juce::String group;          // "piano", "space"... (section copy / paste)
};

const std::vector<ParamDef>& paramDefs();
int paramIndex (const juce::String& id);                     // -1 if unknown
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
const juce::StringArray& pianoNames();
const juce::StringArray& roomNames();
const juce::StringArray& worldNames();
const juce::StringArray& handsNames();
const juce::StringArray& handsScaleNames();
const juce::StringArray& handsStyleNames();
const juce::StringArray& handsBarsNames();
const juce::StringArray& handsFormNames();
const juce::StringArray& sceneTimeNames();
const juce::StringArray& sceneSeasonNames();
int handsBarsFromIndex (int index);
const juce::StringArray& keyNames();
}
