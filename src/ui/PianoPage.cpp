#include "Pages.h"

namespace gx::ui
{
PianoPage::PianoPage (Context& c)
    : Page (c), sceneTime (c, P::sceneTime), sceneSeason (c, P::sceneSeason), hands (c), gen (c), global (c),
      piano (c, L ("Mecánica y cuerdas", "Action & strings"), "piano"),
      macros (c, L ("Macros: el recuerdo", "Macros: the memory"), "macro"),
      life (c, L ("Capas de vida", "Layers of life"), "life"),
      space (c, L ("Caja y sala", "Body & room"), "space")
{
    scene.gpuWanted = c.settings.gpu();
    scene.quality = c.settings.gpuQuality();
    scene.setGpu (scene.gpuWanted);   // not waiting for a window: the context attaches itself as soon as there is one
    piano.accent = col::accent;
    macros.accent = col::accent;
    life.accent = col::accent2;
    space.accent = col::accent3;
    memory.setSliderStyle (juce::Slider::LinearHorizontal);
    memory.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    memory.setTooltip (L ("MEMORIA: del piano de estudio, nítido y presente, al recuerdo desgastado de una cinta encontrada en un desván",
                          "MEMORY: from a crisp, present studio piano to the worn memory of a tape found in an attic"));
    memoryAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (c.proc.apvts, "memory", memory);
    gen.onGenerated = [this] (const Preset& p) { if (onLoaded) onLoaded (p); };
    sceneTime.setTooltip (L ("Hora del día de la escena (el reloj de la pared la sigue)", "Hour of the day in the scene (the wall clock follows it)"));
    sceneSeason.setTooltip (L ("Estación del año tras la ventana", "Season outside the window"));
    for (auto* comp : std::initializer_list<juce::Component*> { &scene, &memory, &sceneTime, &sceneSeason, &hands, &gen, &global, &piano, &macros, &life, &space })
        addAndMakeVisible (comp);
}

void PianoPage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::textDim);
    g.setFont (font (11.0f, true));
    const auto b = memory.getBounds();
    g.drawText (L ("PRESENTE", "PRESENT"), b.withX (scene.getX()).withWidth (70), juce::Justification::centredLeft);
    g.drawText (L ("RECUERDO", "MEMORY"), b.withRight (scene.getRight()).withLeft (scene.getRight() - 70), juce::Justification::centredRight);
}

void PianoPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (juce::jmin (372, (r.getHeight() * 13) / 20));
    auto left = top.removeFromLeft ((int) (top.getHeight() - 58) * 16 / 10 + 8);
    auto memRow = left.removeFromBottom (28);
    auto sceneRow = left.removeFromBottom (26);
    sceneTime.setBounds (sceneRow.removeFromLeft (sceneRow.getWidth() / 2 - 3));
    sceneSeason.setBounds (sceneRow.withTrimmedLeft (6));
    scene.setBounds (left);
    memory.setBounds (memRow.withTrimmedLeft (74).withTrimmedRight (74));
    top.removeFromLeft (8);
    hands.setBounds (top);
    r.removeFromTop (8);
    const int gap = 6, n = 6;
    const int w = (r.getWidth() - gap * (n - 1));
    const int ws[n] { w * 33 / 100, w * 17 / 100, w * 13 / 100, w * 13 / 100, w * 13 / 100, 0 };
    global.setBounds (r.removeFromLeft (ws[0])); r.removeFromLeft (gap);
    piano.setBounds (r.removeFromLeft (ws[1])); r.removeFromLeft (gap);
    macros.setBounds (r.removeFromLeft (ws[2])); r.removeFromLeft (gap);
    life.setBounds (r.removeFromLeft (ws[3])); r.removeFromLeft (gap);
    space.setBounds (r.removeFromLeft (ws[4])); r.removeFromLeft (gap);
    gen.setBounds (r);
}
}
