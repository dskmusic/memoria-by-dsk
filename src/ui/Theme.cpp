#include "Theme.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define GX_HAS_FONTDATA 1
#endif

namespace gx::ui
{
juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
// Orbitron (SIL OFL 1.1, see licenses/Orbitron-OFL.txt), embedded unmodified; falls back to the UI font.
juce::Font techFont (float size)
{
#if GX_HAS_FONTDATA
    static juce::Typeface::Ptr tf = [] () -> juce::Typeface::Ptr
    {
        int n = 0;
        if (auto* d = BinaryData::getNamedResource ("Orbitron_ttf", n)) return juce::Typeface::createSystemTypefaceFor (d, (size_t) n);
        return nullptr;
    }();
    if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (size));
#endif
    return font (size, true);
}
juce::Font monoFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool inset)
{
    juce::ColourGradient grad (inset ? col::panelLo : col::panelHi, r.getX(), r.getY(),
                               inset ? col::panel : col::panelLo, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, radius);
    // bevel: light top-left, dark bottom-right
    g.setColour (inset ? col::bevelLo : col::bevelHi);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (inset ? col::bevelHi.withAlpha (0.08f) : col::bevelLo);
    juce::Path p;
    p.addRoundedRectangle (r.reduced (1.5f), radius - 1.0f);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void drawScreen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::screen);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::grid);
    const int nx = 10, ny = 6;
    for (int i = 1; i < nx; ++i) { const float x = r.getX() + r.getWidth() * (float) i / nx; g.drawVerticalLine ((int) x, r.getY() + 2, r.getBottom() - 2); }
    for (int i = 1; i < ny; ++i) { const float y = r.getY() + r.getHeight() * (float) i / ny; g.drawHorizontalLine ((int) y, r.getX() + 2, r.getRight() - 2); }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.5f);
    g.setColour (col::bevelHi.withAlpha (0.12f));
    g.drawRoundedRectangle (r.expanded (1.0f), 6.0f, 1.0f);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float rad, juce::Colour colour, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level > 0.05f)
    {
        g.setColour (colour.withAlpha (0.25f * level));
        g.fillEllipse (juce::Rectangle<float> (rad * 4.4f, rad * 4.4f).withCentre (c));
    }
    g.setColour (colour.darker (2.2f).interpolatedWith (colour.brighter (0.3f), level));
    g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.4f * level));
    g.fillEllipse (juce::Rectangle<float> (rad * 0.8f, rad * 0.8f).withCentre (c.translated (-rad * 0.3f, -rad * 0.3f)));
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, col::panelLo);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::bevelLo);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextEditor::backgroundColourId, col::bgDeep);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, col::bevelLo);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::ListBox::backgroundColourId, col::bgDeep);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, col::bgDeep);
    setColour (juce::TooltipWindow::backgroundColourId, col::bgDeep);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::ScrollBar::thumbColourId, col::panelHi);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::accent2);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getProperties()["bipolar"];
    const auto accent = s.getProperties().contains ("arc") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["arc"]) : col::accent;
    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
    g.setColour (col::bgDeep);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // value arc
    juce::Path val;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    g.setColour (s.isEnabled() ? accent : col::textDim);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // modulation ring (set by editor as property "mod" in -1..1)
    const float modAmt = (float) (double) s.getProperties().getWithDefault ("mod", 0.0);
    if (std::abs (modAmt) > 0.001f)
    {
        juce::Path mp;
        const float ma = juce::jlimit (a0, a1, ang + modAmt * (a1 - a0));
        mp.addCentredArc (c.x, c.y, r - 6.5f, r - 6.5f, 0, juce::jmin (ang, ma), juce::jmax (ang, ma), true);
        g.setColour (col::accent3);
        g.strokePath (mp, juce::PathStrokeType (2.0f));
    }
    // body
    const float br = r - 7.5f;
    juce::ColourGradient body (col::knobBody.brighter (0.35f), c.x - br * 0.6f, c.y - br * 0.6f, col::knobBody.darker (0.6f), c.x + br, c.y + br, true);
    g.setGradientFill (body);
    g.fillEllipse (juce::Rectangle<float> (br * 2, br * 2).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (juce::Rectangle<float> (br * 2, br * 2).withCentre (c), 1.0f);
    g.setColour (col::bevelHi);
    g.drawEllipse (juce::Rectangle<float> (br * 2 - 2, br * 2 - 2).withCentre (c), 0.8f);
    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.3f, -br + 1.5f, 2.6f, br * 0.55f, 1.2f);
    g.setColour (s.isEnabled() ? col::text : col::textDim);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s)
{
    const bool vert = st == juce::Slider::LinearVertical || st == juce::Slider::LinearBarVertical;
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto accent = s.getProperties().contains ("arc") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["arc"]) : col::accent;
    if (st == juce::Slider::LinearBar || st == juce::Slider::LinearBarVertical)
    {
        g.setColour (col::bgDeep);
        g.fillRoundedRectangle (r, 3.0f);
        const bool bip = s.getProperties()["bipolar"];
        juce::Rectangle<float> fill;
        if (vert)
        {
            const float mid = bip ? r.getCentreY() : r.getBottom();
            fill = juce::Rectangle<float> (r.getX() + 1, juce::jmin (pos, mid), r.getWidth() - 2, std::abs (mid - pos));
        }
        else
        {
            const float mid = bip ? r.getCentreX() : r.getX();
            fill = juce::Rectangle<float> (juce::jmin (pos, mid), r.getY() + 1, std::abs (pos - mid), r.getHeight() - 2);
        }
        g.setColour (accent.withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 2.0f);
        return;
    }
    // FL-style fader: groove + chunky handle
    auto groove = vert ? juce::Rectangle<float> (r.getCentreX() - 2.5f, r.getY() + 6, 5, r.getHeight() - 12)
                       : juce::Rectangle<float> (r.getX() + 6, r.getCentreY() - 2.5f, r.getWidth() - 12, 5);
    g.setColour (col::bgDeep);
    g.fillRoundedRectangle (groove, 2.5f);
    g.setColour (accent.withAlpha (0.7f));
    if (vert) g.fillRoundedRectangle (groove.withTop (pos), 2.5f);
    else g.fillRoundedRectangle (groove.withRight (pos), 2.5f);
    auto handle = vert ? juce::Rectangle<float> (r.getWidth() * 0.8f, 11).withCentre ({ r.getCentreX(), pos })
                       : juce::Rectangle<float> (11, r.getHeight() * 0.8f).withCentre ({ pos, r.getCentreY() });
    juce::ColourGradient hg (col::panelHi.brighter (0.4f), handle.getX(), handle.getY(), col::panelLo, handle.getX(), handle.getBottom(), false);
    g.setGradientFill (hg);
    g.fillRoundedRectangle (handle, 2.5f);
    g.setColour (accent);
    if (vert) g.drawHorizontalLine ((int) handle.getCentreY(), handle.getX() + 2, handle.getRight() - 2);
    else g.drawVerticalLine ((int) handle.getCentreX(), handle.getY() + 2, handle.getBottom() - 2);
    g.setColour (col::bevelLo);
    g.drawRoundedRectangle (handle, 2.5f, 1.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (over) base = base.brighter (0.12f);
    if (down) base = base.darker (0.2f);
    juce::ColourGradient grad (base.brighter (0.15f), r.getX(), r.getY(), base.darker (0.25f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (down ? col::bevelLo : col::bevelHi);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (14.0f, r.getHeight() - 4);
    const auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 3, r.getCentreY() });
    drawLed (g, led.getCentre(), d * 0.38f, col::accent2, b.getToggleState() ? 1.0f : (over ? 0.15f : 0.0f));
    g.setColour (b.findColour (juce::ToggleButton::textColourId));
    g.setFont (font (12.0f, true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) d + 7), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cb.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.5f);
    g.setColour (cb.hasKeyboardFocus (true) ? col::accent.withAlpha (0.6f) : col::bevelLo);
    g.drawRoundedRectangle (r, 3.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (cb.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::accent.withAlpha (0.5f));
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (col::bgDeep.withAlpha (0.96f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::accent);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (col::text);
    g.setFont (font (12.5f));
    g.drawFittedText (text, r.reduced (6, 3).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> area)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font (12.5f), tip, 0, 0);
    const int w = juce::jmin (320, (int) ga.getBoundingBox (0, -1, true).getWidth() + 16);
    const int h = 22 * juce::jmax (1, (int) std::ceil ((float) w / 320.0f));
    return juce::Rectangle<int> (pos.x + 12, pos.y + 14, w, h).constrainedWithin (area);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x + 2, ts, w - 4, tsz) : juce::Rectangle<int> (ts, y + 2, tsz, h - 4);
    g.setColour (over ? col::accent.withAlpha (0.6f) : col::panelHi);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 3.5f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? col::accent : col::bevelLo);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 3.5f, 1.0f);
}
}
