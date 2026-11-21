#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "ui/Pages.h"

// Library shared by every plugin instance in the process (one index.json writer).
struct LibraryHolder
{
    LibraryHolder();
    gx::Library library;
};

class MemoriaEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit MemoriaEditor (MemoriaProcessor&);
    ~MemoriaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    float initialScale() const;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kDesignW = 1280, kDesignH = 800;
    void showPage (int index);
    void setUiScale (float s);
    void toast (const juce::String& msg);
    void advanceFrames (double dt);   // used by the screenshot tool
    void rebuildUi();                 // language switch (also exercised by MemoriaTool screenshot)
    void videoFrame() { stopTimer(); timerCallback(); } // MemoriaTool video: one UI frame on the virtual clock

private:
    void timerCallback() override;
    void refreshAll();
    void toggleAB();
    void buildUi();

    struct Root : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (gx::ui::col::bg); }
    };

    MemoriaProcessor& proc;
    gx::ui::LookAndFeel lnf;
    juce::SharedResourcePointer<LibraryHolder> libHolder;
    gx::ui::Settings settings;
    gx::ui::UndoStack undo;
    gx::ui::Context ctx;
    Root root;
    std::unique_ptr<gx::ui::Header> header;
    std::unique_ptr<gx::ui::PianoPage> pianoPage;
    std::unique_ptr<gx::ui::LifePage> lifePage;
    std::unique_ptr<gx::ui::PresetsPage> presets;
    std::unique_ptr<gx::ui::ExportPage> exportPage;
    std::unique_ptr<gx::ui::HelpPage> helpPage;
    std::unique_ptr<gx::ui::SettingsPage> settingsPage;
    std::unique_ptr<gx::ui::AboutOverlay> about;
    juce::MidiKeyboardComponent keyboard;
    juce::Label toastLabel;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::vector<gx::ui::Page*> pages;
    int currentPage = 0;
    gx::ui::FrameSource frames;
    gx::ui::Frame frame;
    double lastTime = 0, toastUntil = 0, undoPollAt = 0, slowTickAt = 0;
    int lastPresetGen = -1;
    std::array<gx::Preset, 2> abSlots;
    std::array<bool, 2> abValid { false, false };
    int abActive = 0;
    bool pendingRebuild = false, sizedInWindow = false;
    juce::ComponentBoundsConstrainer constrainer;
};
