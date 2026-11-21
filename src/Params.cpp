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
