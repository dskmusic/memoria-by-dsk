#include "Pages.h"

namespace gx::ui
{
namespace
{
struct Item { const char* esTitle; const char* enTitle; const char* es; const char* en; };

const Item items[] {
    { "Bienvenido a Memoria", "Welcome to Memoria",
      "Memoria es un piano con alma. No usa muestras: cada cuerda, cada martillo y la caja de resonancia se calculan en tiempo real, "
      "así que el piano respira con tu forma de tocar: suave, el sonido es redondo; fuerte, se abre y brilla. "
      "Toca con un teclado MIDI o con el teclado de la pantalla. El pedal de sustain (CC64) funciona de verdad, también a medio pedal.",
      "Memoria is a piano with a soul. It uses no samples: every string, every hammer and the soundboard are computed in real time, "
      "so the piano breathes with the way you play: soft and it is round, hard and it opens up and shines. "
      "Play with a MIDI keyboard or the on-screen keys. The sustain pedal (CC64) really works, half-pedaling included." },
    { "El piano", "The piano",
      "PIANO elige el instrumento: cola de concierto, vertical, piano de fieltro, honky-tonk, piano de juguete, celesta, piano eléctrico o vibráfono. "
      "TONE es el brillo; HAMMER la dureza del fieltro del martillo; SUSTAIN cuánto dura la nota; DAMPERS lo rápido que cae el apagador al soltar la tecla. "
      "DETUNE desafina un poco las cuerdas de cada nota (más recuerdo, más piano viejo); STRETCH es el estiramiento natural de la afinación; DYNAMICS, cuánto cambia el sonido entre tocar suave y fuerte.",
      "PIANO picks the instrument: concert grand, upright, felt piano, honky-tonk, toy piano, celesta, electric piano or vibraphone. "
      "TONE is brightness; HAMMER the hardness of the felt; SUSTAIN how long notes ring; DAMPERS how fast the damper falls when you release a key. "
      "DETUNE slightly detunes the strings of each note (more memory, older piano); STRETCH is the natural stretch of the tuning; DYNAMICS how much the sound changes between soft and hard playing." },
    { "Caja y sala", "Body & room",
      "BODY es la madera de la caja de resonancia. RESONANCE son las cuerdas que vibran solas por simpatía cuando el pedal está pisado. "
      "WIDTH abre o cierra el estéreo (graves a la izquierda, agudos a la derecha, como oyes al piano). ROOM y ROOM SIZE añaden la sala.",
      "BODY is the wood of the soundboard. RESONANCE is the strings that ring on their own by sympathy while the pedal is down. "
      "WIDTH opens or closes the stereo image (low notes left, high notes right, as the pianist hears it). ROOM and ROOM SIZE add the room." },
    { "Presets y ajustes", "Presets & settings",
      "En PRESETS guardas y recuperas tus sonidos. A/B compara dos versiones y Deshacer / Rehacer siempre están a mano. "
      "Clic derecho en un mando: MIDI Learn. Doble clic: valor por defecto. En Ajustes cambias el idioma y el tamaño de la ventana.",
      "In PRESETS you save and recall your sounds. A/B compares two versions; Undo and Redo are always at hand. "
      "Right-click a knob: MIDI Learn. Double-click: default value. In Settings you change the language and the window size." },
};

juce::AttributedString build (float width)
{
    juce::ignoreUnused (width);
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.setLineSpacing (3.0f);
    for (auto& it : items)
    {
        as.append (L (it.esTitle, it.enTitle) + "\n", techFont (16.0f), col::accent);
        as.append (L (it.es, it.en) + "\n\n", font (14.5f), col::text);
    }
    return as;
}
} // namespace

int HelpPage::Body::layoutHeight (int width)
{
    juce::TextLayout tl;
    tl.createLayout (build ((float) width), (float) width);
    return (int) std::ceil (tl.getHeight()) + 20;
}

void HelpPage::Body::paint (juce::Graphics& g)
{
    juce::TextLayout tl;
    tl.createLayout (build ((float) getWidth()), (float) getWidth());
    tl.draw (g, getLocalBounds().toFloat());
}

HelpPage::HelpPage (Context& c) : Page (c)
{
    view.setViewedComponent (&body, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);
}

void HelpPage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (12).toFloat();
    drawPanel (g, r);
    auto lr = r.removeFromLeft (300).reduced (24);
    Logo::drawMark (g, lr.removeFromTop (110).withSizeKeepingCentre (200, 100), 0.6f, 0.2f);
    g.setColour (col::text);
    g.setFont (techFont (22.0f));
    g.drawText (L ("AYUDA", "HELP"), lr.removeFromTop (34), juce::Justification::centred);
    g.setColour (col::textDim);
    g.setFont (font (13.0f));
    g.drawFittedText (L ("Todo lo que necesitas saber, en un minuto.\n\nMemoria es gratuito y de código abierto.\ndskmusic.com",
                         "Everything you need to know, in one minute.\n\nMemoria is free and open source.\ndskmusic.com"),
                      lr.removeFromTop (120).toNearestInt(), juce::Justification::centredTop, 8);
}

void HelpPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromLeft (300);
    view.setBounds (r.reduced (8, 16));
    const int w = view.getWidth() - view.getScrollBarThickness() - 8;
    body.setSize (w, body.layoutHeight (w));
}
}
