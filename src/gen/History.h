#pragma once
#include "../preset/Preset.h"

namespace gx
{
// Browser-like history of generated presets. In memory only: it lives as long as the plugin
// instance (current session) and is never written to disk.
class GenHistory
{
public:
    explicit GenHistory (int limit) : maxSize (juce::jmax (1, limit)) {}
    void setLimit (int l) { maxSize = juce::jmax (1, l); trim(); }
    void push (const Preset& p)
    {
        if (pos + 1 < (int) items.size()) items.erase (items.begin() + pos + 1, items.end()); // cut forward branch
        items.push_back (p);
        trim();
        pos = (int) items.size() - 1;
    }
    bool canBack() const { return pos > 0; }
    bool canForward() const { return pos + 1 < (int) items.size(); }
    const Preset* back() { if (! canBack()) return nullptr; --pos; return &items[(size_t) pos]; }
    const Preset* forward() { if (! canForward()) return nullptr; ++pos; return &items[(size_t) pos]; }
    const Preset* current() const { return pos >= 0 && pos < (int) items.size() ? &items[(size_t) pos] : nullptr; }
    const Preset* previous() const { return pos > 0 ? &items[(size_t) pos - 1] : nullptr; }
    int index() const { return pos; }
    int size() const { return (int) items.size(); }

private:
    void trim()
    {
        while ((int) items.size() > maxSize) { items.erase (items.begin()); --pos; }
        pos = juce::jlimit (-1, (int) items.size() - 1, pos);
    }
    int maxSize;
    std::vector<Preset> items;
    int pos = -1;
};
}
