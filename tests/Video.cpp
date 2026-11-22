// MemoriaTool video <dir> | trailer <dir>: demo video made with the real engine and the real editor (frame by frame),
// with bilingual captions. Needs ffmpeg (found on PATH, in Program Files (x86)\ffmpeg or next to the tool).
#include "Tests.h"
#include "PluginEditor.h"
#include "gen/Generator.h"
#include <juce_audio_formats/juce_audio_formats.h>
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace
{
juce::File findFfmpeg()
{
    for (auto p : { "C:/Program Files (x86)/ffmpeg/ffmpeg.exe", "C:/Program Files/ffmpeg/ffmpeg.exe", "C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe", "/usr/local/bin/ffmpeg", "/opt/homebrew/bin/ffmpeg", "/usr/bin/ffmpeg" })
        if (juce::File (p).existsAsFile()) return juce::File (p);
    return {};
}

struct Caption
{
    juce::String en, es; double t0 = 0, t1 = 0; bool big = false;
    Caption (const char* e, const char* s, double a, double b, bool bg = false) : en (juce::String::fromUTF8 (e)), es (juce::String::fromUTF8 (s)), t0 (a), t1 (b), big (bg) {}
};

void drawCaptions (juce::Image& img, const std::vector<Caption>& caps, double t)
{
    juce::Graphics g (img);
    for (auto& c : caps)
    {
        if (t < c.t0 || t > c.t1) continue;
        const float a = (float) juce::jmin (1.0, juce::jmin ((t - c.t0) / 0.8, (c.t1 - t) / 0.8));
        const int w = img.getWidth(), h = img.getHeight();
        if (c.big)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f * a));
            g.fillRect (0, 0, w, h);
            g.setColour (gx::ui::col::accent.withAlpha (a));
            g.setFont (gx::ui::techFont (86.0f));
            g.drawText (c.en, juce::Rectangle<int> (0, h / 2 - 120, w, 100), juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (a * 0.9f));
            g.setFont (gx::ui::font (30.0f, true));
            g.drawText (c.es, juce::Rectangle<int> (0, h / 2 - 10, w, 50), juce::Justification::centred);
        }
        else
        {
            auto r = juce::Rectangle<int> (w / 2 - 440, h - 150, 880, 76);
            g.setColour (juce::Colours::black.withAlpha (0.62f * a));
            g.fillRoundedRectangle (r.toFloat(), 12.0f);
            g.setColour (juce::Colours::white.withAlpha (a));
            g.setFont (gx::ui::font (27.0f, true));
            g.drawText (c.en, r.removeFromTop (40), juce::Justification::centred);
            g.setColour (gx::ui::col::accent.withAlpha (a));
            g.setFont (gx::ui::font (20.0f));
            g.drawText (c.es, r, juce::Justification::centredTop);
        }
    }
}

// timeline: params as a function of time
using Setter = std::function<void (MemoriaProcessor&, double)>;

int makeVideo (const juce::File& outDir, const juce::String& name, double secs, const Setter& setter, const std::vector<Caption>& caps)
{
    const auto ff = findFfmpeg();
    if (! ff.existsAsFile()) { std::puts ("ffmpeg not found"); return 1; }
    outDir.createDirectory();
    const auto tmp = outDir.getChildFile ("frames_" + name);
    tmp.deleteRecursively(); tmp.createDirectory();
    gx::forcedLanguage() = "en";
    gx::ui::virtualClock() = nullptr;
    MemoriaProcessor proc;
    proc.prepareToPlay (48000.0, 500);
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<MemoriaEditor*> (base.get());
    ed->setSize (1280, 800);
    ed->showPage (0);
    constexpr double fps = 24.0;
    const int frames = (int) (secs * fps), spf = (int) (48000.0 / fps);
    juce::AudioBuffer<float> audio (2, frames * spf), blk (2, 500);
    audio.clear();
    juce::MidiBuffer midi;
    juce::PNGImageFormat png;
    for (int f = 0; f < frames; ++f)
    {
        const double t = f / fps;
        setter (proc, t);
        if (f == 0) ed->refreshAll();      // the header shows the name of the sound that is playing
        for (int pos = 0; pos < spf; pos += 500)
        {
            const int n = juce::jmin (500, spf - pos);
            juce::AudioBuffer<float> b (blk.getArrayOfWritePointers(), 2, n);
            b.clear(); midi.clear();
            proc.processBlock (b, midi);
            for (int c = 0; c < 2; ++c) audio.copyFrom (c, f * spf + pos, b, c, 0, n);
        }
        ed->advanceFrames (1.0 / fps);
        { auto& rs = ed->roomScene(); rs.setExternalBackground (gx::ui::OffscreenRoom::get().render (rs.uniforms(), rs.getWidth(), rs.getHeight())); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f).convertedToFormat (juce::Image::ARGB);
        drawCaptions (img, caps, t);
        const auto file = tmp.getChildFile (juce::String::formatted ("%05d.png", f));
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
        if (os != nullptr) png.writeImageToStream (img, *os);
        if (f % 240 == 0) std::puts (("  frame " + juce::String (f) + " / " + juce::String (frames)).toRawUTF8());
    }
    base.reset();
    // fade out the last second of audio
    for (int c = 0; c < 2; ++c) audio.applyGainRamp (c, audio.getNumSamples() - 48000, 48000, 1.0f, 0.0f);
    const auto wav = tmp.getChildFile ("audio.wav");
    gxtest::writeWav (audio, wav);
    const auto out = outDir.getChildFile (name + ".mp4");
    out.deleteFile();
    juce::ChildProcess p;
    juce::StringArray args { ff.getFullPathName(), "-y", "-loglevel", "error", "-framerate", "24", "-i", tmp.getChildFile ("%05d.png").getFullPathName(), "-i", wav.getFullPathName(),
                             "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", "-c:a", "aac", "-b:a", "256k", "-shortest", out.getFullPathName() };
    if (! p.start (args)) { std::puts ("cannot start ffmpeg"); return 1; }
    const auto log = p.readAllProcessOutput();
    p.waitForProcessToFinish (600000);
    if (log.isNotEmpty()) std::puts (log.toRawUTF8());
    tmp.deleteRecursively();
    std::puts (out.existsAsFile() ? ("wrote " + out.getFullPathName()).toRawUTF8() : "ffmpeg failed");
    return out.existsAsFile() ? 0 : 1;
}

float ramp (double t, double a, double b) { return (float) juce::jlimit (0.0, 1.0, (t - a) / (b - a)); }

void setP (MemoriaProcessor& p, const char* id, float v) { gxtest::setParam (p, id, v); }

void prepareSound (MemoriaProcessor& proc, int category, int hands, float tempo, uint32_t seed, int style = 0, int key = 9, int scale = 1)
{
    gx::GenRequest rq; rq.category = gx::categories()[category]; rq.seed = seed;
    auto pr = gx::Generator::generate (rq);
    pr.params[(size_t) gx::P::hands] = (float) hands;
    pr.params[(size_t) gx::P::handsTempo] = tempo;
    pr.params[(size_t) gx::P::memory] = 0.0f; pr.params[(size_t) gx::P::world] = 0; pr.params[(size_t) gx::P::tape] = 0; pr.params[(size_t) gx::P::vinyl] = 0;
    pr.params[(size_t) gx::P::piano] = 0; pr.params[(size_t) gx::P::handsKey] = (float) key; pr.params[(size_t) gx::P::handsScale] = (float) scale; pr.params[(size_t) gx::P::handsBars] = 1; pr.params[(size_t) gx::P::handsStyle] = (float) style; pr.params[(size_t) gx::P::handsExpress] = 0.75f;
    pr.params[(size_t) gx::P::room] = 0.3f; pr.params[(size_t) gx::P::roomType] = 0;
    pr.applyTo (proc);
    proc.setPlaying (true);
}
// the factory preset with this name (so the videos use real, curated sounds); the hands keep playing
bool applyBank (MemoriaProcessor& proc, const juce::String& name)
{
    static juce::var bank; static bool loaded = false;
    if (! loaded)
    {
        loaded = true;
#if __has_include(<BinaryData.h>)
        int size = 0; const char* d = BinaryData::getNamedResource ("factory_memoriabank", size);
        if (d != nullptr) bank = juce::JSON::parse (juce::String::fromUTF8 (d, size));
#endif
    }
    if (auto* arr = bank.getProperty ("presets", {}).getArray())
        for (auto& v : *arr)
        {
            auto pr = gx::Preset::fromVar (v);
            if (pr.meta.name != name) continue;
            pr.params[(size_t) gx::P::hands] = 1.0f;
            pr.applyTo (proc);
            proc.setPlaying (true);
            return true;
        }
    return false;
}

void setHands (MemoriaProcessor& p, int style, int seed, int key, int scale, float tempo, int form = 0)
{
    setP (p, "hands", 1.0f); setP (p, "hands_style", (float) style); setP (p, "hands_seed", (float) seed); setP (p, "hands_key", (float) key);
    setP (p, "hands_scale", (float) scale); setP (p, "hands_tempo", tempo); setP (p, "hands_form", (float) form); setP (p, "hands_bars", 1.0f);
}
}

int runVideo (const juce::File& dir, bool trailer)
{
    if (! trailer)
    {
        // 110 s: the hour of the day and the seasons behind the window, the wall clock, the instruments, the hands
        const double len = 110.0;
        std::vector<Caption> caps {
            { "MEMORIA", "A piano with a soul  ·  Un piano con alma", 1.0, 6.5, true },
            { "A physically modelled piano. No samples.", "Un piano modelado físicamente. Sin muestras.", 7.5, 14.5 },
            { "Hour of the day and season, behind your window", "Hora del día y estación del año, tras tu ventana", 16.0, 25.0 },
            { "A wall clock that follows the light", "Un reloj de pared que sigue la luz", 27.0, 35.0 },
            { "From dawn to a snowy night", "Del amanecer a una noche de nieve", 37.0, 45.0 },
            { "Eleven instruments: electric piano, vibraphone...", "Once instrumentos: piano eléctrico, vibráfono...", 46.0, 55.0 },
            { "...music box, marimba, glockenspiel and more", "...caja de música, marimba, glockenspiel y más", 56.0, 66.0 },
            { "The hands compose and play by themselves", "Las manos componen y tocan solas", 68.0, 76.0 },
            { "In many styles: romantic, jazz, cinematic, lo-fi...", "En muchos estilos: romántico, jazz, cinematográfico, lo-fi...", 77.0, 95.0 },
            { "Loops, choruses and short pieces: drag the MIDI into your DAW", "Bucles, estribillos y piezas cortas: arrastra el MIDI a tu DAW", 96.0, 104.0 },
            { "MEMORIA by DSK", "Free · VST3 / AU / Standalone · dskmusic.com  ·  Gratis", 104.5, 109.5, true } };
        Setter setter = [] (MemoriaProcessor& p, double t)
        {
            // segments: curated factory presets and hand-written song themes
            struct Seg { double from; const char* preset; int style, seed, key, scale; float tempo; };
            static const Seg segs[] {
                { 0.0, "Open Hall", 1, 3, 0, 0, 72.0f },
                { 46.0, "Rhodes Late Night", 8, 62, 0, 0, 74.0f },
                { 51.0, "Vibraphone Moon", 8, 62, 0, 0, 74.0f },
                { 56.0, "Music Box Cradle", 8, 62, 0, 0, 74.0f },
                { 61.0, "Marimba Warm Desk", 8, 62, 0, 0, 74.0f },
                { 68.0, "Hushed Keys", 2, 6, 0, 0, 70.0f },
                { 77.0, "Blue Window Seat", 7, 7, 0, 0, 84.0f },
                { 86.0, "Open Hall", 1, 1, 0, 0, 72.0f },
                { 95.0, "Patient Focus", 0, 4, 0, 0, 78.0f } };
            static int cur = -1;
            int want = 0; for (int i = 0; i < 9; ++i) if (t >= segs[i].from) want = i;
            if (want != cur) { cur = want; applyBank (p, segs[want].preset); }
            const auto& sg = segs[want];
            setHands (p, sg.style, sg.seed, sg.key, sg.scale, sg.tempo, t >= 96.0 && t < 104.0 ? 1 : 0);
            setP (p, "world", 0.0f); setP (p, "world_level", 0.0f);
            setP (p, "memory", 0.12f + 0.10f * ramp (t, 0.0, 100.0));
            // scene: auto at first, then hour and season sweep (the scene eases between looks)
            const float tod = t < 16.0 ? 0.0f : t < 24.0 ? 2.0f : t < 30.0 ? 3.0f : t < 37.0 ? 4.0f : 5.0f;
            const float season = t < 16.0 ? 0.0f : t < 24.0 ? 1.0f : t < 30.0 ? 2.0f : t < 37.0 ? 3.0f : t < 66.0 ? 4.0f : 0.0f;
            setP (p, "scene_time", t < 94.0 ? tod : 3.0f); setP (p, "scene_season", t < 94.0 ? season : 2.0f);
        };
        return makeVideo (dir, "Memoria-demo", len, setter, caps);
    }
    std::vector<Caption> caps {
        { "MEMORIA", "by DSK  ·  coming soon  ·  próximamente", 0.5, 6.0, true },
        { "A piano with a soul", "Un piano con alma", 6.5, 11.0 },
        { "Free. VST3 / AU / Standalone.", "Gratis. VST3 / AU / Standalone.", 11.5, 17.0 },
        { "MEMORIA", "dskmusic.com", 17.5, 22.0, true } };
    Setter setter = [] (MemoriaProcessor& p, double t)
    {
        if (t < 0.02) applyBank (p, "Hazy Notes");
        setHands (p, 8, 62, 0, 0, 72.0f);       // Lo-Fi Loop, a hand-written theme (ii - V - I - vi)
        setP (p, "world", 0.0f); setP (p, "world_level", 0.0f);     // no ambience
        setP (p, "memory", 0.40f + 0.30f * ramp (t, 3.0, 18.0));
        setP (p, "scene_time", 3.0f); setP (p, "scene_season", 2.0f);   // a summer afternoon outside the window
    };
    return makeVideo (dir, "Memoria-coming-soon", 22.0, setter, caps);
}
