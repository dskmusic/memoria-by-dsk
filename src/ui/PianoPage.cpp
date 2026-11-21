#include "Pages.h"

namespace gx::ui
{
PianoPage::PianoPage (Context& c)
    : Page (c), global (c, L ("Piano", "Piano"), "global", 0, { P::seed }),
      piano (c, L ("Mecánica y cuerdas", "Action & strings"), "piano"), space (c, L ("Caja y sala", "Body & room"), "space")
{
    piano.accent = col::accent;
    space.accent = col::accent3;
    for (auto* comp : std::initializer_list<juce::Component*> { &scope, &spectrum, &vu, &global, &piano, &space })
        addAndMakeVisible (comp);
}

void PianoPage::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void PianoPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (juce::jmin (230, r.getHeight() / 3));
    vu.setBounds (top.removeFromRight (70));
    top.removeFromRight (8);
    scope.setBounds (top.removeFromLeft (top.getWidth() / 2 - 4));
    top.removeFromLeft (8);
    spectrum.setBounds (top);
    r.removeFromTop (8);
    global.setBounds (r.removeFromLeft (300));
    r.removeFromLeft (8);
    piano.setBounds (r.removeFromLeft ((r.getWidth() * 3) / 5 - 4));
    r.removeFromLeft (8);
    space.setBounds (r);
}
}
