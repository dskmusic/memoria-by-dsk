#include "Params.h"
#include "Lang.h"
#include <map>

namespace gx
{
using Range = juce::NormalisableRange<float>;

static Range logRange (float lo, float hi)
{
    const float ratio = hi / lo;
    return Range (lo, hi,
        [lo, ratio] (float, float, float x) { return lo * std::pow (ratio, x); },
        [lo, ratio] (float, float, float v) { return std::log (v / lo) / std::log (ratio); },
        [] (float, float, float v) { return v; });
}
static Range lin (float lo, float hi, float step = 0.0f) { return Range (lo, hi, step); }
static Range skewed (float lo, float hi, float centre) { Range r (lo, hi); r.setSkewForCentre (centre); return r; }

const juce::StringArray& layerIds()
{
    static const juce::StringArray n { "drone", "pad", "texture", "shimmer", "wind", "bells", "bass", "melody" };
    return n;
}
juce::String layerName (int l)
{
    switch (l)
    {
        case lyDrone: return "DRONE";
        case lyPad: return "PAD";
        case lyTexture: return L ("TEXTURA", "TEXTURE");
        case lyShimmer: return "SHIMMER";
        case lyWind: return L ("VIENTO", "WIND");
        case lyBells: return L ("CAMPANAS", "BELLS");
        case lyBass: return L ("BAJO", "BASS");
        case lyMelody: return L ("MELODÍA", "MELODY");
        default: return {};
    }
}
juce::String sceneName (int s)
{
    switch (s)
    {
        case scValley: return L ("Valle", "Valley");
        case scStarfield: return L ("Campo estelar", "Starfield");
        case scUnderwater: return L ("Bajo el agua", "Underwater");
        case scNeon: return L ("Neón", "Neon Rain");
        case scOcean: return L ("Océano", "Ocean");
        case scAurora: return L ("Aurora", "Aurora");
        default: return {};
    }
}
const juce::StringArray& keyNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}
const juce::StringArray& scaleNames()
{
    static const juce::StringArray n { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Harmonic Minor",
                                       "Major Pentatonic", "Minor Pentatonic", "Suspended", "Hirajoshi", "Whole Tone" };
    return n;
}
const std::vector<int>& scaleSteps (int s)
{
    static const std::vector<std::vector<int>> v {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 2, 5, 7, 10 }, { 0, 2, 3, 7, 8 }, { 0, 2, 4, 6, 8, 10 } };
    return v[(size_t) juce::jlimit (0, (int) v.size() - 1, s)];
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
    b.addChoice (key, "key", "Key", keyNames(), 9);
    b.addChoice (scale, "scale", "Scale", scaleNames(), 1);
    b.addInt (octave, "octave", "Octave", 1, 4, 2);
    b.add (harmony, "harmony", "Harmony", lin (0, 1), 0.5f);
    b.add (chordRate, "chord_rate", "Chord Motion", lin (0, 1), 0.4f);
    b.addChoice (tuning, "tuning", "Tuning", { "440 Hz", "432 Hz" }, 0);
    b.addBool (syncChords, "sync_chords", "Sync Chords to Tempo", false);
    b.addChoice (chordBars, "chord_bars", "Chord Length", { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2);
    b.addBool (sleepMode, "sleep_mode", "Sleep Mode", false);
    b.addChoice (midiMode, "midi_mode", "MIDI Mode", { "Off", "Set Key", "Play Notes", "Key + Play" }, 3);
    b.add (fadeIn, "fade_in", "Fade In", skewed (0.1f, 30, 4), 4, "s");
    b.add (fadeOut, "fade_out", "Fade Out", skewed (0.1f, 60, 4), 3, "s");
    b.addInt (timer, "timer", "Session Timer", 0, 180, 0, "min");
    b.group = "macro";
    b.add (evolution, "evolution", "Evolution", lin (0, 1), 0.5f);
    b.add (density, "density", "Density", lin (0, 1), 0.5f);
    b.add (light, "light", "Light", lin (0, 1), 0.5f);
    b.add (space, "space", "Space", lin (0, 1), 0.6f);
    b.addBool (freeze, "freeze", "Freeze", false);
    b.group = "fx";
    b.add (width, "width", "Width", lin (0, 1.5f), 1.0f);
    b.add (warmth, "warmth", "Warmth", lin (0, 1), 0.2f);
    b.add (lowCut, "low_cut", "Low Cut", logRange (20, 500), 30, "Hz");
    b.add (highCut, "high_cut", "High Cut", logRange (1000, 20000), 16000, "Hz");
    b.add (revDamp, "rev_damp", "Reverb Damping", lin (0, 1), 0.4f);
    b.add (dlyTime, "dly_time", "Delay Time", skewed (20, 2000, 450), 450, "ms");
    b.add (dlyFb, "dly_fb", "Delay Feedback", lin (0, 0.95f), 0.45f);
    b.add (dlyTone, "dly_tone", "Delay Tone", lin (0, 1), 0.5f);
    b.add (dlyMix, "dly_mix", "Delay Send", lin (0, 1), 0.3f);
    b.addChoice (noiseType, "noise_type", "Noise Type", { "Off", "Pink", "Brown", "White", "Ocean", "Vinyl" }, 0);
    b.add (noiseLevel, "noise_level", "Noise Level", lin (0, 1), 0.3f);
    b.add (noiseTone, "noise_tone", "Noise Tone", lin (0, 1), 0.5f);
    b.group = "global";
    b.addChoice (scene, "scene", "Scene", { "Valley", "Starfield", "Underwater", "Neon Rain", "Ocean", "Aurora" }, 0);

    //                      level presence tone  char
    const float defs[kNumLayers][4] { { 0.70f, 1.00f, 0.50f, 0.40f },   // drone
                                      { 0.60f, 0.80f, 0.50f, 0.50f },   // pad
                                      { 0.50f, 0.55f, 0.50f, 0.50f },   // texture
                                      { 0.45f, 0.60f, 0.50f, 0.40f },   // shimmer
                                      { 0.30f, 0.40f, 0.50f, 0.50f },   // wind
                                      { 0.35f, 0.50f, 0.50f, 0.35f },   // bells
                                      { 0.45f, 0.70f, 0.40f, 0.30f },   // bass
                                      { 0.60f, 0.80f, 0.70f, 0.50f } }; // melody (off by default: older presets keep their sound)
    const char* names[kNumLayers] { "Drone", "Pad", "Texture", "Shimmer", "Wind", "Bells", "Bass", "Melody" };
    for (int l = 0; l < kNumLayers; ++l)
    {
        const juce::String id = layerIds()[l], nm = names[l];
        b.group = id;
        b.addBool (layer (l, lOn), id + "_on", nm + " On", l != lyMelody);
        b.add (layer (l, lLevel), id + "_level", nm + " Level", lin (0, 1), defs[l][0]);
        b.add (layer (l, lPresence), id + "_presence", nm + " Presence", lin (0, 1), defs[l][1]);
        b.add (layer (l, lTone), id + "_tone", nm + " Tone", lin (0, 1), defs[l][2]);
        b.add (layer (l, lChar), id + "_char", nm + " Character", lin (0, 1), defs[l][3]);
    }
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
