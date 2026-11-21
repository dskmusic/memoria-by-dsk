#include "Pages.h"

namespace gx::ui
{
namespace
{
struct Item { const char* esTitle; const char* enTitle; const char* es; const char* en; };

const Item items[] {
    { "Bienvenido a Memoria", "Welcome to Memoria",
      "Memoria es un paisaje sonoro que nunca se repite. No hace falta tocar nada: pulsa Play, cierra los ojos y deja que evolucione solo. "
      "Ocho capas (drone, pad, textura, shimmer, viento, campanas, bajo y melodía) entran y salen por su cuenta mientras la armonía, la luz y el espacio cambian despacio.",
      "Memoria is a landscape of sound that never repeats. You don't need to play anything: press Play, close your eyes and let it evolve. "
      "Eight layers (drone, pad, texture, shimmer, wind, bells, bass and melody) come and go on their own while harmony, light and space slowly change." },
    { "1. Empieza en 10 segundos", "1. Start in 10 seconds",
      "Pulsa el gran botón Play (o la barra espaciadora en la versión Standalone). En tu DAW basta con darle a Play: Memoria le sigue. "
      "Elige un preset en PRESETS o pulsa GENERAR en el Generador para crear uno nuevo según tu estado de ánimo.",
      "Press the big Play button (or the space bar in the Standalone). In your DAW just press Play: Memoria follows it. "
      "Pick a preset in PRESETS, or press GENERATE on the Generator page to create a new one for your mood." },
    { "2. Los cuatro macros", "2. The four macros",
      "EVOLUCIÓN: lo deprisa que cambia todo. DENSIDAD: cuántas capas y detalles suenan a la vez. LUZ: de noche cerrada (oscuro, cálido) a mediodía (brillante). "
      "ESPACIO: de una sala a una catedral infinita. CONGELAR detiene el tiempo y deja el momento suspendido.",
      "EVOLUTION: how fast everything changes. DENSITY: how many layers and details play at once. LIGHT: from deep night (dark, warm) to midday (bright). "
      "SPACE: from a room to an endless cathedral. FREEZE stops time and leaves the moment hanging in the air." },
    { "3. La semilla", "3. The seed",
      "Cada semilla es un viaje distinto con el mismo sonido. La misma semilla siempre da el mismo recorrido, así que lo que exportas desde tu DAW suena exactamente igual que lo que oíste. "
      "Pulsa «Nueva semilla» para descubrir otro camino.",
      "Each seed is a different journey with the same sound. The same seed always gives the same path, so what you bounce from your DAW sounds exactly like what you heard. "
      "Press \"New seed\" to discover another path." },
    { "4. Armonía", "4. Harmony",
      "Elige tonalidad y escala. ARMONÍA va de quintas abiertas a acordes de 7ª y 9ª; ACORDES decide cada cuánto cambian. "
      "Con SINCRO TEMPO los acordes cambian cada 1-16 compases de tu DAW. Con el teclado MIDI puedes cambiar la tonalidad o tocar notas encima.",
      "Pick a key and a scale. HARMONY goes from open fifths to 7th and 9th chords; CHORDS sets how often they change. "
      "With TEMPO SYNC the chords change every 1-16 bars of your DAW. With a MIDI keyboard you can set the key or play notes on top." },
    { "5. Página Sonido", "5. Sound page",
      "Cada capa tiene NIVEL, PRESENCIA (cuánto tiempo está presente: 100 % = siempre), TONO y CARÁCTER. "
      "Abajo tienes la mezcla (calidez, anchura, cortes, delay), el generador de RUIDO (rosa, marrón, blanco, océano o vinilo) y los FUNDIDOS de Play y Stop.",
      "Every layer has LEVEL, PRESENCE (how much of the time it is there: 100 % = always), TONE and CHARACTER. "
      "Below are the mix (warmth, width, filters, delay), the NOISE generator (pink, brown, white, ocean or vinyl) and the Play / Stop FADES." },
    { "6. Paisajes y pantalla completa", "6. Landscapes and full screen",
      "Seis paisajes reaccionan a cada capa: Valle, Campo estelar, Bajo el agua, Neón, Océano y Aurora. "
      "«Pantalla completa» los muestra a toda pantalla: 1-6 cambia de paisaje, flechas = luz y evolución, F congela, N nueva semilla, R tipo de ruido, T modo relax (sin textos), H ayuda y Esc para salir.",
      "Six landscapes react to every layer: Valley, Starfield, Underwater, Neon, Ocean and Aurora. "
      "\"Full screen\" shows them full size: 1-6 change landscape, arrows = light and evolution, F freeze, N new seed, R noise type, T relax mode (no texts), H help and Esc to exit." },
    { "7. Memorias rápidas", "7. Quick memories",
      "Los botones 1-8 bajo el Play guardan sonidos para tu sesión. Un botón vacío guarda el sonido actual; uno lleno lo recupera con un fundido suave de 2 segundos. "
      "Mayús+clic vuelve a guardar y mantener pulsado un botón lo borra. Se guardan con tu proyecto.",
      "Buttons 1-8 under Play keep sounds for your session. An empty button stores the current sound; a filled one recalls it with a smooth 2-second crossfade. "
      "Shift+click stores again, and pressing and holding a button clears it. They are saved with your project." },
    { "8. Temporizador y modo Dormir", "8. Timer and Sleep mode",
      "Pon el TEMPORIZADOR en minutos y Memoria se apagará solo con un fundido. Activa MODO DORMIR y, durante ese tiempo, el paisaje se oscurece, se calma y baja de volumen poco a poco: perfecto para dormir o meditar.",
      "Set the TIMER in minutes and Memoria switches itself off with a fade. Turn on SLEEP MODE and, during that time, the landscape darkens, calms down and gets quieter little by little: perfect for sleep or meditation." },
    { "9. Exportar audio y vídeo", "9. Export audio and video",
      "En EXPORTAR puedes renderizar de 1 minuto a 3 horas en WAV, FLAC u OGG, y también un vídeo MP4 del paisaje con su sonido, listo para YouTube (el vídeo es lento de crear: tarda algo más que su duración). "
      "Para el vídeo hace falta ffmpeg (gratis): Memoria lo busca solo y, si no lo encuentra, te ofrece instalarlo o te explica cómo.",
      "On EXPORT you can render 1 minute to 3 hours to WAV, FLAC or OGG, and also an MP4 video of the landscape with its sound, ready for YouTube (video is slow to create: a bit longer than its length). "
      "Video needs ffmpeg (free): Memoria looks for it and, if it is missing, offers to install it or explains how." },
    { "10. Trucos", "10. Tips",
      "Clic derecho en cualquier mando: MIDI Learn. Doble clic: valor por defecto. A/B compara dos sonidos; Deshacer y Rehacer siempre están ahí. "
      "En Ajustes puedes cambiar el idioma (automático, inglés o español) y el tamaño de la ventana. ¡Disfruta del viaje!",
      "Right-click any knob: MIDI Learn. Double-click: default value. A/B compares two sounds; Undo and Redo are always there. "
      "In Settings you can change the language (automatic, English or Spanish) and the window size. Enjoy the journey!" },
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
