#include "Pages.h"

namespace gx::ui
{
LifePage::LifePage (Context& c)
    : Page (c), macros (c, L ("Macros: el recuerdo", "Macros: the memory"), "macro"), life (c, L ("Capas de vida", "Layers of life"), "life"),
      hands (c, L ("Manos: toca solo", "Hands: plays by itself"), "hands")
{
    macros.accent = col::accent;
    life.accent = col::accent2;
    hands.accent = col::accent3;
    play.setClickingTogglesState (false);
    play.setTooltip (L ("Las manos tocan con el tempo de tu DAW (o el interno). Barra espaciadora en el Standalone.", "The hands play at your DAW tempo (or the internal one). Space bar in the Standalone."));
    play.onClick = [this] { ctx.proc.setPlaying (! ctx.proc.isPlayingInternal()); };
    for (auto* comp : std::initializer_list<juce::Component*> { &macros, &life, &hands, &play }) addAndMakeVisible (comp);
}

void LifePage::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void LifePage::tick (const Frame& f)
{
    Page::tick (f);
    const bool on = ctx.proc.isPlayingInternal();
    play.setButtonText (on ? "STOP" : "PLAY");
    play.setToggleState (on, juce::dontSendNotification);
}

void LifePage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    macros.setBounds (r.removeFromTop (160));
    r.removeFromTop (8);
    life.setBounds (r.removeFromTop (190));
    r.removeFromTop (8);
    auto h = r.removeFromTop (220);
    hands.setBounds (h.withTrimmedLeft (0));
    play.setBounds (h.removeFromRight (110).removeFromTop (34).translated (-12, 8));
}
}
