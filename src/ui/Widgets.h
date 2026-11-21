#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../preset/Library.h"

namespace gx::ui
{
// ---------------------------------------------------------------- settings
class Settings
{
public:
    Settings();
    float scale() const { return (float) props->getDoubleValue ("scale", 1.0); }
    void setScale (float s) { props->setValue ("scale", s); props->saveIfNeeded(); }
    int animation() const { return props->getIntValue ("animation", 0); } // 0 full, 1 reduced, 2 off
    void setAnimation (int a) { props->setValue ("animation", a); props->saveIfNeeded(); }
    bool aiMessages() const { return props->getBoolValue ("aiMessages", true); }
    void setAiMessages (bool b) { props->setValue ("aiMessages", b); props->saveIfNeeded(); }
    int historyLimit() const { return props->getIntValue ("historyLimit", 200); }
    void setHistoryLimit (int n) { props->setValue ("historyLimit", juce::jlimit (10, 2000, n)); props->saveIfNeeded(); }
    // UI language setting: "auto" (follows the OS: Spanish -> "es", anything else -> "en"), "en" or "es".
    juce::String languageSetting() const { return props->getValue ("uiLang", "auto"); }
    void setLanguageSetting (const juce::String& l) { props->setValue ("uiLang", l); props->saveIfNeeded(); }
    juce::String messageLanguage() const // effective language
    {
        if (forcedLanguage().isNotEmpty()) return forcedLanguage();
        const auto s = languageSetting();
        if (s == "en" || s == "es") return s;
        return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("es") || juce::SystemStats::getDisplayLanguage().startsWithIgnoreCase ("es") ? "es" : "en";
    }
    bool gpu() const { return props->getBoolValue ("gpu", true); }
    void setGpu (bool b) { props->setValue ("gpu", b); props->saveIfNeeded(); }
    int gpuQuality() const { return props->getIntValue ("gpuQuality", 1); }
    void setGpuQuality (int q) { props->setValue ("gpuQuality", q); props->saveIfNeeded(); }
    bool fps() const { return props->getBoolValue ("fps", false); }
    void setFps (bool b) { props->setValue ("fps", b); props->saveIfNeeded(); }
    float noteStars() const { return (float) props->getDoubleValue ("noteStars", 0.3); }
    void setNoteStars (float v) { props->setValue ("noteStars", v); props->saveIfNeeded(); }
    bool previewOnSelect() const { return props->getBoolValue ("previewChord", false); }
    void setPreviewOnSelect (bool b) { props->setValue ("previewChord", b); props->saveIfNeeded(); }
    juce::PropertiesFile& file() { return *props; }

private:
    std::unique_ptr<juce::PropertiesFile> props;
};

// Snapshot-based undo/redo (all parameters), driven by polling for changes.
class UndoStack
{
public:
    explicit UndoStack (MemoriaProcessor& p);
    void poll (bool userIsDragging);     // call ~3x per second
    bool canUndo() const { return ! undo.empty(); }
    bool canRedo() const { return ! redo.empty(); }
    void performUndo();
    void performRedo();
    void reset();                        // forget history (e.g. after loading a preset)
private:
    bool differs (const Preset& a) const;
    MemoriaProcessor& proc;
    Preset last;
    std::vector<Preset> undo, redo;
    bool applying = false;
};

struct Context
{
    MemoriaProcessor& proc;
    Settings& settings;
    Library& library;
    UndoStack& undo;
    std::function<void (int paramIndex)> startMidiLearn;
    std::function<void()> refreshAll;               // repaint/sync everything after big changes
    std::function<void (const juce::String&)> toast;
    std::function<void()> languageChanged;          // rebuild the whole UI in the new language
};

// ---------------------------------------------------------------- controls
class ParamKnob : public juce::Component
{
public:
    ParamKnob (Context& c, int paramIndex, const juce::String& label = {}, const juce::String& tooltip = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int param() const { return index; }
    void setAccent (juce::Colour c) { slider.getProperties().set ("arc", (juce::int64) c.getARGB()); slider.repaint(); }
    juce::Slider slider;
    float labelSize = 10.5f;

private:
    void showMenu();
    Context& ctx;
    int index;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (Context& c, int paramIndex, const juce::String& label = {});
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (Context& c, int paramIndex);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Panel with bevel + title. Right-click on the title copies/pastes the section.
class Section : public juce::Component
{
public:
    Section (Context& c, juce::String title, juce::String group = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (8).withTrimmedTop (20); }
    juce::String title, group;
    juce::Colour accent = col::accent;
protected:
    Context& ctx;
};

// Section with explicitly placed children (layout lambda).
struct Box : public Section
{
    using Section::Section;
    std::function<void (juce::Rectangle<int>)> layout;
    void resized() override { if (layout) layout (content()); }
    template <typename T, typename... A> T& add (A&&... a)
    {
        auto c = std::make_unique<T> (std::forward<A> (a)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }
    std::vector<std::unique_ptr<juce::Component>> owned;
};

inline void gridKnobs (std::vector<juce::Component*> ks, juce::Rectangle<int> r, int cols)
{
    if (ks.empty()) return;
    const int rows = ((int) ks.size() + cols - 1) / cols;
    const int cw = r.getWidth() / cols, ch = juce::jmin (76, r.getHeight() / rows);
    for (size_t i = 0; i < ks.size(); ++i)
        ks[i]->setBounds (r.getX() + (int) (i % (size_t) cols) * cw, r.getY() + (int) (i / (size_t) cols) * ch, cw, ch);
}

// Lays out every parameter of a group automatically (knobs for floats/ints, combos, toggles).
class AutoPanel : public Section
{
public:
    AutoPanel (Context& c, const juce::String& title, const juce::String& group, int columns = 0, std::vector<int> skip = {});
    void resized() override;
    std::vector<std::unique_ptr<juce::Component>> controls;
    int columns;
};

// Section copy/paste clipboard (field-suffix -> value)
void copySection (MemoriaProcessor& p, const juce::String& group);
bool pasteSection (MemoriaProcessor& p, const juce::String& group);
juce::String fieldOf (const juce::String& id); // "osc2_morph" -> "morph"
}
