#pragma once
#include "Widgets.h"
#include "Visuals.h"
#include "../dsp/Composer.h"
#include <thread>

namespace gx::ui
{
// A tile you can drag out of the plugin into the DAW / desktop (the file is made when the drag starts).
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragTile() { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent& e) override { started = false; if (e.mouseWasClicked() && onClick) onClick(); }
    std::function<juce::File()> getFile;       // null / missing file = nothing to drag yet
    std::function<void()> onClick;
    juce::String text, sub;
    bool ready = true, busy = false;
    float progress = 0;
private:
    bool started = false;
};

// HANDS: the generative pianist. Play, song parameters, regenerate (new variation / randomise unlocked parameters),
// song map with playhead, and export (MIDI / WAV, as files or dragged into the DAW).
class HandsPanel : public Section, public Animated
{
public:
    explicit HandsPanel (Context& c);
    ~HandsPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick (const Frame&) override;

private:
    void refreshScore();
    void randomise (bool all);
    juce::File midiFile();
    void prepareWav();
    Preset exportPreset() const;
    uint32_t songHash() const;

    ParamCombo mode, style, key, scale, bars;
    ParamKnob tempo, density, express;
    ParamToggle pedal, endless;
    juce::TextButton play { "PLAY" }, newVar, randomize, saveMidi, saveWav;
    juce::ToggleButton lockStyle, lockKey, lockScale, lockTempo;
    DragTile midiTile, wavTile;
    juce::Label status;
    Score score;
    uint32_t scoreHash = 0;
    int songBar = 0, songBars = 0;
    bool running = false;
    juce::Rectangle<int> mapArea;
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread worker;
    std::atomic<bool> rendering { false }, cancel { false };
    std::atomic<float> renderProgress { 0 };
    std::atomic<int> wavState { 0 };            // 0 nothing, 1 rendering, 2 ready
    juce::File wavFile;
    uint32_t wavHash = 0;
};

// SOUND GENERATOR: a new random sound (piano, room, layers, macros) in a category, with locks.
class SoundGenPanel : public Section
{
public:
    explicit SoundGenPanel (Context& c);
    ~SoundGenPanel() override;
    void resized() override;
    std::function<void (const Preset&)> onGenerated;

private:
    void generate();
    juce::ComboBox category;
    juce::TextButton surprise;
    juce::ToggleButton lockPiano, lockRoom, lockLayers, lockMacros;
    std::atomic<bool> busy { false };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
}
