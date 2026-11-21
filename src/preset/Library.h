#pragma once
#include "Preset.h"
#include <set>

namespace gx
{
struct PresetFilter
{
    juce::String text, category, subcategory, author, folder;
    juce::StringArray tags;           // all must match
    enum Type { All, Factory, User } type = All;
    bool favoritesOnly = false;
    enum Sort { ByName, ByDate, ByCategory, ByRating, ByUses } sort = ByName;
    bool descending = false;
};

// Preset database: factory bank (embedded) + user folder. User-side metadata (favorites, ratings,
// use counts, also for factory presets) lives in library.json; user preset metadata is cached in
// index.json so thousands of presets load without parsing every file.
class Library
{
public:
    explicit Library (juce::File rootDir = defaultRoot());
    static juce::File defaultRoot();

    juce::File userDir() const { return root.getChildFile ("Presets"); }
    juce::File rootDir() const { return root; }

    void setFactoryBank (const juce::String& bankJson); // array of presets
    void rescanUser();

    const std::vector<PresetMeta>& all() const { return entries; }
    std::vector<int> query (const PresetFilter& f) const;          // indices into all()
    bool load (int index, Preset& out) const;
    int indexOfUuid (const juce::String& uuid) const;
    juce::StringArray authors() const;
    juce::StringArray userFolders() const;

    // mutations (persisted immediately)
    bool saveUser (Preset& p, const juce::String& folder = {}); // new or overwrite (by uuid)
    bool rename (int index, const juce::String& newName);
    bool duplicate (int index);
    bool remove (int index);
    bool updateMeta (int index, const PresetMeta& m);           // category/tags/notes/author/folder
    void setFavorite (int index, bool fav);
    void setRating (int index, int stars);
    void markUsed (int index);
    bool createFolder (const juce::String& name);

    // import / export
    int importFile (const juce::File& f);                         // .memoria or .memoriabank -> count
    bool exportPreset (int index, const juce::File& f) const;
    bool exportBank (const std::vector<int>& indices, const juce::File& f) const;

    std::function<void()> onChange;

private:
    void loadOverlay();
    void saveOverlay() const;
    void saveIndex() const;
    void applyOverlay (PresetMeta& m) const;
    void changed();
    juce::File uniqueFileFor (const juce::String& name, const juce::String& folder) const;

    juce::File root;
    std::vector<PresetMeta> entries;
    std::vector<juce::var> factoryData;   // parsed factory presets (same order as factory entries)
    struct Overlay { bool favorite = false; int rating = 0, uses = 0; };
    std::map<juce::String, Overlay> overlay;
};
}
