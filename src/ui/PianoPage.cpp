#include "Pages.h"

namespace gx::ui
{
PianoPage::PianoPage (Context& c)
    : Page (c), global (c, L ("Piano", "Piano"), "global", 0, { P::seed }),
      piano (c, L ("Mecánica y cuerdas", "Action & strings"), "piano"), space (c, L ("Caja y sala", "Body & room"), "space")
{
    piano.accent = col::accent;
    space.accent = col::accent3;
    memory.setSliderStyle (juce::Slider::LinearHorizontal);
    memory.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    memory.setTooltip (L ("MEMORIA: del piano de estudio, nítido y presente, al recuerdo desgastado de una cinta encontrada en un desván",
                          "MEMORY: from a crisp, present studio piano to the worn memory of a tape found in an attic"));
    memoryAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (c.proc.apvts, "memory", memory);
    for (auto* comp : std::initializer_list<juce::Component*> { &scene, &memory, &scope, &spectrum, &vu, &global, &piano, &space })
        addAndMakeVisible (comp);
}

void PianoPage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::textDim);
    g.setFont (font (11.0f, true));
    auto b = memory.getBounds();
    g.drawText (L ("PRESENTE", "PRESENT"), b.withX (scene.getX()).withWidth (70).translated (0, 0), juce::Justification::centredLeft);
    g.drawText (L ("RECUERDO", "MEMORY"), b.withRight (scene.getRight()).withLeft (scene.getRight() - 70), juce::Justification::centredRight);
}

void PianoPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (juce::jmin (350, (r.getHeight() * 11) / 20));
    auto left = top.removeFromLeft ((int) (top.getHeight() - 30) * 16 / 10 + 8);
    auto memRow = left.removeFromBottom (28);
    scene.setBounds (left);
    memory.setBounds (memRow.withTrimmedLeft (74).withTrimmedRight (74));
    top.removeFromLeft (8);
    vu.setBounds (top.removeFromRight (56));
    top.removeFromRight (8);
    scope.setBounds (top.removeFromTop (top.getHeight() / 2 - 4));
    top.removeFromTop (8);
    spectrum.setBounds (top);
    r.removeFromTop (8);
    global.setBounds (r.removeFromLeft (300));
    r.removeFromLeft (8);
    piano.setBounds (r.removeFromLeft ((r.getWidth() * 3) / 5 - 4));
    r.removeFromLeft (8);
    space.setBounds (r);
}
}
