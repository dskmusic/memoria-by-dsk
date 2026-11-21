// MemoriaTool video <dir> | trailer <dir>: demo video made with the real engine and the real editor (frame by frame),
// with bilingual captions. Needs ffmpeg (found on PATH, in Program Files (x86)\ffmpeg or next to the tool).
#include "Tests.h"
#include "PluginEditor.h"
#include "gen/Generator.h"
#include <juce_audio_formats/juce_audio_formats.h>

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
}

int runVideo (const juce::File& dir, bool trailer)
{
    if (! trailer)
    {
        // 70 s: one sound, one scene; MEMORY goes from a present studio piano to an attic tape, the world outside changes slowly
        const double len = 70.0;
        std::vector<Caption> caps {
            { "MEMORIA", "A piano with a soul  ·  Un piano con alma", 1.0, 7.0, true },
            { "A physically modelled piano. No samples.", "Un piano modelado físicamente. Sin muestras.", 8.0, 15.0 },
            { "Turn up MEMORY...", "Sube MEMORIA...", 17.0, 24.0 },
            { "Tape, vinyl, wood and a room that remembers", "Cinta, vinilo, madera y una habitación con recuerdos", 28.0, 36.0 },
            { "Rain on the window", "Lluvia en la ventana", 38.0, 45.0 },
            { "The hands compose and play, in time with your DAW", "Las manos componen y tocan, al ritmo de tu DAW", 48.0, 56.0 },
            { "MEMORIA by DSK", "Free · VST3 / AU / Standalone · dskmusic.com  ·  Gratis", 62.0, 69.0, true } };
        Setter setter = [] (MemoriaProcessor& p, double t)
        {
            if (t < 0.02) prepareSound (p, 0, 1, 76.0f, 31);
            if (t < 0.02) { setP (p, "hands", 1.0f); setP (p, "piano", 0.0f); }
            const float m = ramp (t, 17.0, 40.0) * (1.0f - 0.7f * ramp (t, 56.0, 68.0));
            setP (p, "memory", m);
            setP (p, "world", t > 36.0 && t < 52.0 ? 1.0f : t >= 52.0 ? 3.0f : 0.0f);
            setP (p, "world_level", 0.7f);
            setP (p, "time", 0.7f);
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
        if (t < 0.02)
        {
            prepareSound (p, 0, 1, 72.0f, 21, 8, 0, 0);
            setP (p, "piano", 1.0f); setP (p, "tone", 0.30f); setP (p, "hammer", 0.30f); setP (p, "warmth", 0.7f); setP (p, "detune", 0.6f);
            setP (p, "tape", 0.45f); setP (p, "vinyl", 0.30f); setP (p, "wood", 0.3f); setP (p, "room", 0.22f); setP (p, "room_type", 0.0f);
            setP (p, "hands_density", 0.45f); setP (p, "hands_express", 0.55f); setP (p, "resonance", 0.0f);
        }
        setP (p, "memory", 0.45f + 0.35f * ramp (t, 3.0, 18.0));
        setP (p, "world", 1.0f);             // rain, very low
        setP (p, "world_level", 0.08f);
    };
    return makeVideo (dir, "Memoria-coming-soon", 22.0, setter, caps);
}
