#include "Messages.h"
#include <map>

namespace gx
{
namespace
{
using List = std::vector<const char*>;

const List genericEs {
    "Dibujando el horizonte…", "Sembrando la semilla del paisaje…", "Esperando a que amanezca…", "Midiendo la distancia a las montañas…",
    "Dejando que el drone respire…", "Mutando armónicos muy despacio…", "Calculando cuándo entra cada capa…", "Moviendo nubes de sitio…",
    "Afinando el silencio…", "Colgando campanas en el viento…", "Granulando un recuerdo…", "Poniendo la reverb a la altura del cielo…",
    "Buscando una tonalidad que abrace…", "Encendiendo las estrellas una a una…", "Ralentizando el tiempo…", "Barriendo la niebla del valle…",
    "Haciendo que nada se repita…", "Comprobando que el infinito cabe en la mezcla…", "Trenzando quintas y octavas…", "Escuchando el eco de una catedral…",
    "Puliendo la aurora…", "Dejando que la luz cambie sola…", "Ordenando el cosmos por capas…", "Pasando la validación de calidad… ✔" };

const List genericEn {
    "Drawing the horizon…", "Planting the seed of the landscape…", "Waiting for the sunrise…", "Measuring the distance to the mountains…",
    "Letting the drone breathe…", "Mutating harmonics very slowly…", "Working out when each layer comes in…", "Moving clouds around…",
    "Tuning the silence…", "Hanging bells in the wind…", "Granulating a memory…", "Raising the reverb to the height of the sky…",
    "Looking for a key that embraces…", "Switching the stars on one by one…", "Slowing down time…", "Sweeping the fog out of the valley…",
    "Making sure nothing ever repeats…", "Checking that infinity fits in the mix…", "Weaving fifths and octaves…", "Listening to the echo of a cathedral…",
    "Polishing the aurora…", "Letting the light change by itself…", "Sorting the cosmos into layers…", "Passing quality control… ✔" };

const std::map<juce::String, std::pair<List, List>> byMood {
    { "Cinematic", { { "Levantando un imperio de sonido…", "Abriendo plano general…", "Preparando la escena final…", "Llamando a los títulos de crédito…", "Poniendo el sol detrás de la montaña…", "Dando épica a cada acorde…" },
                     { "Raising an empire of sound…", "Cutting to a wide shot…", "Setting up the final scene…", "Cueing the end credits…", "Putting the sun behind the mountain…", "Giving every chord an epic weight…" } } },
    { "Ambient", { { "Flotando sobre el agua…", "Desenfocando los bordes…", "Calentando la luz de la tarde…", "Dejando correr el río…", "Abriendo las ventanas…", "Haciendo sitio al aire…" },
                   { "Floating over the water…", "Softening the edges…", "Warming the afternoon light…", "Letting the river flow…", "Opening the windows…", "Making room for the air…" } } },
    { "Meditation", { { "Inspirando… espirando…", "Encendiendo una vela imaginaria…", "Bajando el ritmo del corazón…", "Afinando a 432 Hz si hace falta…", "Soltando lo que no suena…", "Volviendo al presente…" },
                      { "Breathing in… breathing out…", "Lighting an imaginary candle…", "Slowing the heartbeat…", "Tuning to 432 Hz if needed…", "Letting go of what doesn't sound…", "Coming back to the present…" } } },
    { "Dark", { { "Apagando la última luz…", "Bajando a la cripta…", "Despertando algo antiguo…", "Llenando el pozo de niebla…", "Escuchando pasos en la oscuridad…", "Sellando el ritual…" },
                { "Switching off the last light…", "Going down into the crypt…", "Waking something ancient…", "Filling the well with fog…", "Hearing footsteps in the dark…", "Sealing the ritual…" } } },
    { "Ethereal", { { "Afinando un coro de luz…", "Abriendo las nubes…", "Dejando que el sonido flote…", "Encendiendo un halo…", "Subiendo hacia la luz…", "Puliendo cada armónico de cristal…" },
                    { "Tuning a choir of light…", "Parting the clouds…", "Letting the sound float…", "Lighting a halo…", "Rising towards the light…", "Polishing every crystal harmonic…" } } },
    { "Oceanic", { { "Midiendo la marea…", "Contando olas…", "Bajando al fondo del mar…", "Encendiendo el faro…", "Dejando que la espuma respire…", "Siguiendo las corrientes…" },
                   { "Measuring the tide…", "Counting waves…", "Diving to the seafloor…", "Lighting the lighthouse…", "Letting the foam breathe…", "Following the currents…" } } },
    { "Nostalgia", { { "Rebobinando una cinta vieja…", "Quitando el polvo al vinilo…", "Buscando una foto de verano…", "Calentando la radio del salón…", "Recordando una tarde dorada…", "Desafinando la cinta con cariño…" },
                     { "Rewinding an old tape…", "Dusting off the vinyl…", "Looking for a summer photo…", "Warming up the living-room radio…", "Remembering a golden afternoon…", "Wobbling the tape lovingly…" } } },
    { "Epic", { { "Afinando cien metales graves…", "Levantando el telón del tráiler…", "Reuniendo al ejército de cuerdas…", "Encendiendo las antorchas…", "Midiendo el tamaño de la batalla…", "Preparando el crescendo final…" },
                { "Tuning a hundred low brasses…", "Raising the trailer curtain…", "Gathering the army of strings…", "Lighting the torches…", "Measuring the size of the battle…", "Preparing the final crescendo…" } } },
    { "Forest", { { "Escuchando a los pájaros…", "Dejando que el viento mueva las hojas…", "Buscando musgo blando…", "Siguiendo el arroyo…", "Contando luciérnagas…", "Abriendo un claro de luz…" },
                  { "Listening to the birds…", "Letting the wind move the leaves…", "Looking for soft moss…", "Following the brook…", "Counting fireflies…", "Opening a clearing of light…" } } },
    { "Desert", { { "Levantando dunas…", "Calentando el horizonte…", "Dibujando un espejismo…", "Guiando la caravana…", "Escuchando el viento seco…", "Buscando el oasis…" },
                  { "Raising dunes…", "Heating the horizon…", "Drawing a mirage…", "Guiding the caravan…", "Listening to the dry wind…", "Looking for the oasis…" } } },
    { "Sleep", { { "Bajando las luces…", "Mullendo la almohada…", "Respirando más despacio…", "Arropando cada nota…", "Apagando el ruido del día…", "Preparando dulces sueños…" },
                 { "Dimming the lights…", "Fluffing the pillow…", "Breathing more slowly…", "Tucking in every note…", "Switching off the noise of the day…", "Preparing sweet dreams…" } } },
    { "Fairytale", { { "Abriendo el libro de cuentos…", "Dando cuerda a la caja de música…", "Despertando a las hadas…", "Encendiendo las linternas del castillo…", "Escribiendo la melodía…", "Había una vez…" },
                     { "Opening the storybook…", "Winding up the music box…", "Waking the fairies…", "Lighting the castle lanterns…", "Writing the tune…", "Once upon a time…" } } },
    { "Film Score", { { "Componiendo el tema principal…", "Afinando la orquesta…", "Escribiendo la partitura…", "Buscando la escena…", "Dando forma a la melodía…", "Preparando los créditos…" },
                      { "Composing the main theme…", "Tuning the orchestra…", "Writing the score…", "Finding the scene…", "Shaping the melody…", "Rolling the credits…" } } },
    { "Sci-Fi", { { "Estableciendo órbita…", "Recibiendo una señal lejana…", "Calibrando el reactor…", "Despresurizando la estación…", "Traduciendo un idioma alienígena…", "Saltando al hiperespacio…" },
                  { "Establishing orbit…", "Receiving a distant signal…", "Calibrating the reactor…", "Depressurising the station…", "Translating an alien language…", "Jumping to hyperspace…" } } },
};

const std::map<juce::String, std::pair<List, List>> finals {
    { "Cinematic", { { "Tu banda sonora está lista. Pulsa Play.", "Escena lista: acción." }, { "Your soundtrack is ready. Press Play.", "Scene ready: action." } } },
    { "Ambient", { { "Tu paisaje está listo: déjalo correr.", "Listo. Nunca sonará igual dos veces." }, { "Your landscape is ready: let it flow.", "Done. It will never sound the same twice." } } },
    { "Meditation", { { "Listo. Respira y escucha.", "Tu espacio de calma está listo." }, { "Ready. Breathe and listen.", "Your calm space is ready." } } },
    { "Dark", { { "La oscuridad está lista.", "Listo. No mires atrás." }, { "The darkness is ready.", "Done. Don't look back." } } },
    { "Ethereal", { { "Listo. Deja que te eleve.", "La luz está lista." }, { "Ready. Let it lift you.", "The light is ready." } } },
    { "Oceanic", { { "El mar está listo. Escucha.", "Listo: marea alta." }, { "The sea is ready. Listen.", "Done: high tide." } } },
    { "Nostalgia", { { "Listo. Pulsa Play y recuerda.", "Tu cinta está lista." }, { "Done. Press Play and remember.", "Your tape is ready." } } },
    { "Epic", { { "Listo para la batalla.", "Tu tráiler empieza ahora." }, { "Ready for battle.", "Your trailer starts now." } } },
    { "Forest", { { "El bosque te espera.", "Listo: respira hondo." }, { "The forest is waiting.", "Done: breathe in deep." } } },
    { "Desert", { { "El desierto está listo.", "Listo: sigue la caravana." }, { "The desert is ready.", "Done: follow the caravan." } } },
    { "Sleep", { { "Listo. Que descanses.", "Buenas noches." }, { "Done. Rest well.", "Good night." } } },
    { "Fairytale", { { "Y vivieron felices…", "El cuento está listo." }, { "And they lived happily…", "The story is ready." } } },
    { "Film Score", { { "Tema listo: luces, cámara…", "La banda sonora está lista." }, { "Theme ready: lights, camera…", "The score is ready." } } },
    { "Sci-Fi", { { "Transmisión establecida.", "Listo para el despegue." }, { "Transmission established.", "Ready for lift-off." } } },
};
} // namespace

juce::StringArray aiMessages (const juce::String& category, bool english, Rng& rng, int count, juce::String& lastShown)
{
    std::vector<const char*> pool (english ? genericEn.begin() : genericEs.begin(), english ? genericEn.end() : genericEs.end());
    if (auto it = byMood.find (category); it != byMood.end())
    {
        const auto& l = english ? it->second.second : it->second.first;
        pool.insert (pool.end(), l.begin(), l.end());
        pool.insert (pool.end(), l.begin(), l.end()); // weight mood lines double
    }
    juce::StringArray out;
    int guard = 0;
    while (out.size() < count && guard++ < 200)
    {
        const auto s = juce::String::fromUTF8 (pool[(size_t) (rng.next() % (uint32_t) pool.size())]);
        if (s == lastShown || out.contains (s)) continue; // never repeat back-to-back
        out.add (s);
        lastShown = s;
    }
    if (auto it = finals.find (category); it != finals.end())
    {
        const auto& f = english ? it->second.second : it->second.first;
        out.add (juce::String::fromUTF8 (f[(size_t) (rng.next() % (uint32_t) f.size())]));
    }
    else out.add (english ? "Done!" : juce::String::fromUTF8 ("¡Listo!"));
    return out;
}

int numMessagesFor (const juce::String& category, bool english)
{
    int n = (int) (english ? genericEn.size() : genericEs.size());
    if (auto it = byMood.find (category); it != byMood.end()) n += (int) (english ? it->second.second.size() : it->second.first.size());
    return n;
}
}
