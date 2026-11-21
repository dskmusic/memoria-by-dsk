#include "Tests.h"
#include "PluginEditor.h"

// MemoriaTool test | piano <dir> | screenshot <dir> [en|es]
//   piano: renders listening / analysis material (velocity sweep, scale, chords with pedal, staccato) as 24-bit WAV files.
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String cmd = argc > 1 ? argv[1] : "test";
    if (cmd == "test") return gxtest::runAll();
    if (cmd == "piano")
    {
        using namespace gxtest;
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "piano-test");
        dir.createDirectory();
        for (int type = 0; type < gx::numPianoTypes(); ++type)
        {
            gx::Preset p; setParam (p, "piano", (float) type);
            const juce::String nm = gx::pianoNames()[type].replace (" ", "_");
            Song vel; for (int i = 0; i < 4; ++i) vel.note (0.2 + i * 3.0, 2.6, 60, 25 + i * 32);
            writeWav (render (p, vel, 13.0), dir.getChildFile (nm + "_velocity_C4.wav"));
            Song sc; const int up[] { 0, 2, 4, 5, 7, 9, 11, 12 };
            for (int i = 0; i < 8; ++i) sc.note (0.2 + i * 0.4, 0.5, 48 + up[i], 80);
            for (int i = 0; i < 8; ++i) sc.note (4.0 + i * 0.4, 0.5, 72 + up[i], 80);
            writeWav (render (p, sc, 9.0), dir.getChildFile (nm + "_scale.wav"));
            Song ch; ch.pedal (0.0, 127);
            const int chords[4][4] { { 45, 52, 57, 60 }, { 41, 48, 53, 57 }, { 36, 48, 55, 64 }, { 43, 50, 59, 62 } };
            for (int c = 0; c < 4; ++c) { for (int n : chords[c]) ch.note (0.2 + c * 2.2, 2.0, n, 70); if (c == 1) { ch.pedal (4.3, 0); ch.pedal (4.45, 127); } }
            writeWav (render (p, ch, 12.0), dir.getChildFile (nm + "_chords_pedal.wav"));
            Song st; for (int i = 0; i < 12; ++i) st.note (0.2 + i * 0.25, 0.08, 64 + (i % 3) * 3, 95);
            writeWav (render (p, st, 5.0), dir.getChildFile (nm + "_staccato.wav"));
        }
        {   // decay profile of the concert grand (what a recording of a real piano would also show: fast prompt decay, then a long slow one)
            gx::Preset p;
            for (int n : { 36, 60, 84 })
            {
                Song s; s.note (0.0, 30.0, n, 80);
                const auto b = render (p, s, 12.0);
                std::printf ("  note %d decay (dB rel. 0.05-0.15 s):", n);
                const float ref = rmsDb (b, 0.05, 0.15);
                for (double t : { 0.5, 1.0, 2.0, 4.0, 8.0 }) std::printf ("  %.1fs %.0f", t, rmsDb (b, t, t + 0.1) - ref);
                std::puts ("");
            }
        }
        std::printf ("written to %s\n", dir.getFullPathName().toRawUTF8());
        return 0;
    }
    if (cmd == "screenshot")
    {
        // every page of the real editor, rendered offscreen with a chord ringing (so scope / spectrum / keys are alive)
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "screens");
        dir.createDirectory();
        gx::forcedLanguage() = argc > 3 ? argv[3] : "en";
        MemoriaProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* ed = dynamic_cast<MemoriaEditor*> (base.get());
        ed->setSize (1280, 800);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        const char* names[] { "piano", "life", "presets", "export", "settings", "help" };
        for (int page = 0; page < 6; ++page)
        {
            ed->showPage (page);
            for (int f = 0; f < 90; ++f)
            {
                midi.clear();
                if (page == 0 && f == 0) { for (int n : { 45, 52, 57, 64, 69 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 85), 0); midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0); }
                for (int k = 0; k < 2; ++k) { buf.clear(); proc.processBlock (buf, midi); midi.clear(); }
                ed->advanceFrames (1.0 / 60.0);
                if (f == 5) std::printf ("page %d frame 5: block peak %.4f proc.rms %.5f voices %d\n", page, buf.getMagnitude (0, 512), proc.rms.load(), proc.telemetry().voices.load());
            }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            const auto f = dir.getChildFile (juce::String (names[page]) + ".png");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (os != nullptr) png.writeImageToStream (img, *os);
        }
        base.reset();
        std::printf ("screenshots in %s\n", dir.getFullPathName().toRawUTF8());
        return 0;
    }
    std::fprintf (stderr, "usage: MemoriaTool test | piano <dir> | screenshot <dir> [en|es]\n");
    return 2;
}
