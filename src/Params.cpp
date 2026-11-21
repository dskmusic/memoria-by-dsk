#include "Params.h"
#include "Lang.h"
#include <map>

namespace gx
{
using Range = juce::NormalisableRange<float>;

static Range lin (float lo, float hi, float step = 0.0f) { return Range (lo, hi, step); }

const juce::StringArray& pianoNames()
{
    static const juce::StringArray n { "Concert Grand", "Upright", "Felt Piano", "Honky-Tonk", "Toy Piano", "Celesta" };
    return n;
}

const juce::StringArray& roomNames()
{
    static const juce::StringArray n { "Living Room", "Church", "Studio", "Wooden Cabin", "Empty Concert Hall" };
    return n;
}
const juce::StringArray& worldNames()
{
    static const juce::StringArray n { "Off", "Rain", "Cafe", "Fireplace", "Dawn Birds", "Distant Audience" };
    return n;
}

const juce::StringArray& handsNames()
{
    static const juce::StringArray n { "Off", "Arpeggios", "Chords", "Phrases", "Full" };
    return n;
}
const juce::StringArray& handsScaleNames()
{
    static const juce::StringArray n { "Major", "Minor", "Dorian", "Lydian", "Minor Pentatonic", "Major Pentatonic", "Mixolydian", "Harmonic Minor" };
    return n;
}
const juce::StringArray& keyNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

namespace
{
struct Builder
{
    std::vector<ParamDef> defs;
    juce::String group;
    Builder() { defs.resize ((size_t) P::numParams); }
    void add (int idx, juce::String id, juce::String name, Range r, float def, juce::String unit = {})
    {
        auto& d = defs[(size_t) idx];
        d.index = idx; d.id = id; d.name = name; d.range = r; d.def = def; d.unit = unit; d.kind = pkFloat; d.group = group;
    }
    void addInt (int idx, juce::String id, juce::String name, int lo, int hi, int def, juce::String unit = {})
    {
        add (idx, id, name, lin ((float) lo, (float) hi, 1.0f), (float) def, unit);
        defs[(size_t) idx].kind = pkInt;
    }
    void addBool (int idx, juce::String id, juce::String name, bool def)
    {
        add (idx, id, name, lin (0, 1, 1), def ? 1.0f : 0.0f);
        defs[(size_t) idx].kind = pkBool;
    }
    void addChoice (int idx, juce::String id, juce::String name, juce::StringArray choices, int def)
    {
        add (idx, id, name, lin (0, (float) (choices.size() - 1), 1), (float) def);
        defs[(size_t) idx].kind = pkChoice;
        defs[(size_t) idx].choices = choices;
    }
};

std::vector<ParamDef> build()
{
    using namespace P;
    Builder b;
    b.group = "global";
    b.add (master, "master", "Master", lin (-48, 12), -6, "dB");
    b.addInt (seed, "seed", "Seed", 1, 99999, 1);
    b.addChoice (piano, "piano", "Piano", pianoNames(), 0);
    b.addChoice (tuning, "tuning", "Tuning", { "440 Hz", "432 Hz" }, 0);
    b.group = "piano";
    b.add (tone, "tone", "Tone", lin (0, 1), 0.5f);
    b.add (hammer, "hammer", "Hammer", lin (0, 1), 0.5f);
    b.add (sustain, "sustain", "Sustain", lin (0, 1), 0.5f);
    b.add (damper, "damper", "Dampers", lin (0, 1), 0.5f);
    b.add (detune, "detune", "Detune", lin (0, 1), 0.4f);
    b.add (stretch, "stretch", "Stretch", lin (0, 1), 0.6f);
    b.add (dynamics, "dynamics", "Dynamics", lin (0, 1), 0.5f);
    b.group = "space";
    b.add (body, "body", "Body", lin (0, 1), 0.5f);
    b.add (resonance, "resonance", "Resonance", lin (0, 1), 0.5f);
    b.add (width, "width", "Width", lin (0, 1.5f), 1.0f);
    b.add (room, "room", "Room", lin (0, 1), 0.25f);
    b.add (roomSize, "room_size", "Room Size", lin (0, 1), 0.4f);
    b.addChoice (roomType, "room_type", "Room Type", roomNames(), 0);
    b.group = "macro";
    b.add (memory, "memory", "Memory", lin (0, 1), 0.0f);
    b.add (warmth, "warmth", "Warmth", lin (0, 1), 0.0f);
    b.add (intimacy, "intimacy", "Intimacy", lin (0, 1), 0.5f);
    b.add (time, "time", "Time", lin (0, 1), 0.5f);
    b.add (spaceMacro, "space_macro", "Space", lin (0, 1), 0.5f);
    b.group = "life";
    b.add (tape, "tape", "Tape", lin (0, 1), 0.0f);
    b.add (vinyl, "vinyl", "Vinyl", lin (0, 1), 0.0f);
    b.add (wood, "wood", "Wood", lin (0, 1), 0.0f);
    b.addChoice (world, "world", "World", worldNames(), 0);
    b.add (worldLevel, "world_level", "World Level", lin (0, 1), 0.5f);
    b.group = "hands";
    b.addChoice (hands, "hands", "Hands", handsNames(), 0);
    b.addChoice (handsKey, "hands_key", "Key", keyNames(), 9);
    b.addChoice (handsScale, "hands_scale", "Scale", handsScaleNames(), 1);
    b.addChoice (handsRate, "hands_rate", "Rate", { "1/4", "1/8", "1/16" }, 1);
    b.add (handsDensity, "hands_density", "Density", lin (0, 1), 0.6f);
    b.add (handsTempo, "hands_tempo", "Tempo", lin (40, 140, 1), 72, "bpm");
    b.add (handsSoft, "hands_soft", "Softness", lin (0, 1), 0.5f);
    b.addBool (handsPedal, "hands_pedal", "Pedal", true);
    return b.defs;
}
} // namespace

const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> d = build();
    return d;
}

int paramIndex (const juce::String& id)
{
    static const std::map<juce::String, int> m = []
    {
        std::map<juce::String, int> r;
        for (auto& d : paramDefs()) r[d.id] = d.index;
        return r;
    }();
    auto it = m.find (id);
    return it == m.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : paramDefs())
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case pkFloat:
                l.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, d.range, d.def,
                         juce::AudioParameterFloatAttributes().withLabel (d.unit)));
                break;
            case pkInt:
                l.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.range.start, (int) d.range.end, (int) d.def,
                         juce::AudioParameterIntAttributes().withLabel (d.unit)));
                break;
            case pkBool: l.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f)); break;
            case pkChoice: l.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choices, (int) d.def)); break;
        }
    }
    return l;
}
}
