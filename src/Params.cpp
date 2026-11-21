#include "Params.h"
#include "Lang.h"
#include <map>

namespace gx
{
using Range = juce::NormalisableRange<float>;

static Range lin (float lo, float hi, float step = 0.0f) { return Range (lo, hi, step); }

const juce::StringArray& pianoNames()
{
    static const juce::StringArray n { "Concert Grand", "Upright", "Felt Piano", "Honky-Tonk", "Toy Piano", "Celesta", "Electric Piano", "Vibraphone", "Glockenspiel", "Music Box", "Marimba" };
    return n;
}

const juce::StringArray& roomNames()
{
    static const juce::StringArray n { "Living Room", "Church", "Studio", "Wooden Cabin", "Empty Concert Hall" };
    return n;
}
const juce::StringArray& worldNames()
{
    static const juce::StringArray n { "No ambience", "Rain", "Wind", "Fireplace", "Dawn Birds" };
    return n;
}

const juce::StringArray& handsNames()
{
    static const juce::StringArray n { "Off", "Full Song", "Accompaniment", "Melody" };
    return n;
}
const juce::StringArray& handsScaleNames()
{
    static const juce::StringArray n { "Major", "Minor", "Dorian", "Lydian", "Minor Pentatonic", "Major Pentatonic", "Mixolydian", "Harmonic Minor" };
    return n;
}
const juce::StringArray& handsStyleNames()
{
    static const juce::StringArray n { "Lo-Fi Chill", "Cinematic", "Romantic", "Minimal", "Lullaby (Waltz)", "Power Ballad", "Classical (Canon)", "Jazz Ballad", "Lo-Fi Loop", "Nocturne (Chopin-style)" };
    return n;
}
const juce::StringArray& handsBarsNames()
{
    static const juce::StringArray n { "16 bars", "32 bars", "48 bars", "64 bars", "96 bars", "128 bars" };
    return n;
}
int handsBarsFromIndex (int i) { static const int b[] { 16, 32, 48, 64, 96, 128 }; return b[juce::jlimit (0, 5, i)]; }
const juce::StringArray& handsFormNames()
{
    static const juce::StringArray n { "Full song", "Loop (8 bars)", "Chorus (8 bars)", "Short piece" };
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
    b.add (master, "master", "Master", lin (-48, 12), -9, "dB");
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
    b.add (resonance, "resonance", "Resonance", lin (0, 1), 0.3f);
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
    b.add (worldLevel, "world_level", "World Level", lin (0, 1), 0.4f);
    b.group = "hands";
    b.addChoice (hands, "hands", "Hands", handsNames(), 1);
    b.addChoice (handsStyle, "hands_style", "Style", handsStyleNames(), 0);
    b.addChoice (handsKey, "hands_key", "Key", keyNames(), 9);
    b.addChoice (handsScale, "hands_scale", "Scale", handsScaleNames(), 1);
    b.addChoice (handsBars, "hands_bars", "Length", handsBarsNames(), 1);
    b.add (handsTempo, "hands_tempo", "Tempo", lin (40, 160, 1), 76, "bpm");
    b.add (handsDensity, "hands_density", "Density", lin (0, 1), 0.6f);
    b.add (handsExpress, "hands_express", "Expression", lin (0, 1), 0.65f);
    b.addInt (handsSeed, "hands_seed", "Song Seed", 1, 99999, 1);
    b.addBool (handsPedal, "hands_pedal", "Pedal", true);
    b.addBool (handsEndless, "hands_endless", "Endless", true);
    b.addChoice (handsForm, "hands_form", "Form", handsFormNames(), 0);
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
