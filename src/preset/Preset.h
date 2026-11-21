#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "../dsp/Common.h"

class MemoriaProcessor;

namespace gx
{
constexpr int kPresetFormatVersion = 1;

struct PresetMeta
{
    juce::String uuid, name = "Init", category = "Ambient", subcategory, author = "DSK Music", notes;
    juce::StringArray tags;
    bool favorite = false, factory = false;
    int rating = 0, uses = 0;
    juce::Time created, modified;
    juce::String folder;      // user sub-folder (relative), empty = root
    juce::File file;          // user preset file (empty for factory presets)
};

// Full preset content: metadata + every parameter (plain values, indexed like paramDefs()).
struct Preset
{
    PresetMeta meta;
    std::vector<float> params;

    Preset();                                   // = init patch (all defaults)
    juce::var toVar (bool includeMeta = true) const;
    static Preset fromVar (const juce::var& v); // ignores unknown ids, missing ones keep their default
    juce::String toJson() const;
    static bool fromJson (const juce::String& json, Preset& out);
    bool saveTo (const juce::File& f) const;
    static bool loadFrom (const juce::File& f, Preset& out);

    static Preset capture (MemoriaProcessor& p);
    void applyTo (MemoriaProcessor& p) const;

    float similarity (const Preset& other) const; // 0..1 (1 = identical), duplicate detection
    float get (int index) const { return params[(size_t) index]; }
};

// Moods (categories) shared by the generator, the browser and the factory bank.
const juce::StringArray& categories();
const juce::StringArray& subcategories (const juce::String& category);
const juce::StringArray& characterTags();
juce::String categoryLabel (const juce::String& category); // localized
}
