#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Lang.h"

namespace gx::ui
{
// UI clock in seconds. MemoriaTool "video" installs a virtual clock so animations follow video time.
inline std::function<double()>& virtualClock() { static std::function<double()> f; return f; }
inline double nowSeconds() { auto& f = virtualClock(); return f ? f() : juce::Time::getMillisecondCounterHiRes() * 0.001; }
// Non-zero: generator seeds start here (reproducible video takes).
inline uint32_t& forcedSeed() { static uint32_t s = 0; return s; }

// Central look: every colour, radius and font lives here.
namespace col
{
    const juce::Colour bg        { 0xff0d1020 };  // deep night indigo
    const juce::Colour bgDeep    { 0xff080a16 };
    const juce::Colour panel     { 0xff181d33 };
    const juce::Colour panelHi   { 0xff222944 };
    const juce::Colour panelLo   { 0xff12162a };
    const juce::Colour bevelHi   { 0x26ffffff };
    const juce::Colour bevelLo   { 0x66000000 };
    const juce::Colour text      { 0xffe2e6f2 };
    const juce::Colour textDim   { 0xff8790ad };
    const juce::Colour accent    { 0xff4fe3c1 }; // aurora teal
    const juce::Colour accent2   { 0xffffcf6e }; // horizon gold
    const juce::Colour accent3   { 0xffa68cff }; // violet
    const juce::Colour danger    { 0xffff6b81 };
    const juce::Colour knobBody  { 0xff262d4a };
    const juce::Colour screen    { 0xff070915 };
    const juce::Colour phosphor  { 0xff8ff5dd };
    const juce::Colour grid      { 0xff141a33 };
    // one colour per layer (landscape, layer strips)
    const juce::Colour layer[8] { juce::Colour (0xff6f8cff), juce::Colour (0xffa68cff), juce::Colour (0xffffcf6e),
                                  juce::Colour (0xff4fe3c1), juce::Colour (0xff9fb7c9), juce::Colour (0xffffe9a8), juce::Colour (0xffff8a6b), juce::Colour (0xffff9fd6) };
}

juce::Font font (float size, bool bold = false);
juce::Font techFont (float size);   // embedded Orbitron (GENERATE button)
juce::Font monoFont (float size);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.0f, bool inset = false);
void drawScreen (juce::Graphics& g, juce::Rectangle<float> r);   // dark display with grid
void drawLed (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float level);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.5f); }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override { return font (juce::jmin (b.getProperties().contains ("small") ? 11.5f : 13.0f, h * 0.55f), true); }
    juce::Font getLabelFont (juce::Label&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parentArea) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};
}
