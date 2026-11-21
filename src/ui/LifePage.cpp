#include "Pages.h"

namespace gx::ui
{
LifePage::LifePage (Context& c)
    : Page (c), macros (c, L ("Macros: el recuerdo", "Macros: the memory"), "macro"), life (c, L ("Capas de vida", "Layers of life"), "life")
{
    macros.accent = col::accent;
    life.accent = col::accent2;
    addAndMakeVisible (macros);
    addAndMakeVisible (life);
}

void LifePage::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void LifePage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    macros.setBounds (r.removeFromTop (190));
    r.removeFromTop (8);
    life.setBounds (r.removeFromTop (230));
}
}
