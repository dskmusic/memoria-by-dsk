// Memoria unit tests (run with: MemoriaTool test)
#include "Tests.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <set>

namespace gxtest
{
int failures = 0, checks = 0;

void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) { ++failures; std::printf ("  FAIL: %s\n", what.toRawUTF8()); }
}

Stats analyse (const juce::AudioBuffer<float>& b, int start, int len)
{
    Stats s;
    double sum = 0, sq = 0;
    const int n = len < 0 ? b.getNumSamples() - start : len;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float* d = b.getReadPointer (c, start);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (d[i])) { s.finite = false; continue; }
            s.peak = juce::jmax (s.peak, std::abs (d[i]));
            sum += d[i]; sq += (double) d[i] * d[i];
        }
    }
    const double total = (double) n * b.getNumChannels();
    s.dc = (float) std::abs (sum / juce::jmax (1.0, total));
    s.rms = (float) std::sqrt (sq / juce::jmax (1.0, total));
    return s;
}

void setParam (MemoriaProcessor& p, const juce::String& id, float plain)
{
    const int i = gx::paramIndex (id);
    jassert (i >= 0);
    if (i >= 0) p.setParamPlain (i, plain);
}

void setParam (gx::Preset& p, const juce::String& id, float plain)
{
    const int i = gx::paramIndex (id);
    jassert (i >= 0);
    if (i >= 0) p.params[(size_t) i] = gx::paramDefs()[(size_t) i].range.snapToLegalValue (plain);
}

juce::AudioBuffer<float> render (const gx::Preset& p, double seconds, double sr, int bs)
{
    juce::AudioBuffer<float> out (2, (int) (seconds * sr));
    out.clear();
    gx::renderPreset (p, out, sr, bs);
    return out;
}

static float db (float x) { return 20.0f * std::log10 (juce::jmax (1.0e-9f, x)); }

static void testParams()
{
    std::puts ("[params]");
    const auto& defs = gx::paramDefs();
    check ((int) defs.size() == gx::P::numParams, "param count");
    std::set<juce::String> ids;
    bool ok = true;
    for (int i = 0; i < (int) defs.size(); ++i)
    {
        ok &= defs[(size_t) i].index == i && defs[(size_t) i].id.isNotEmpty();
        ok &= defs[(size_t) i].range.getRange().contains (defs[(size_t) i].def) || defs[(size_t) i].range.end == defs[(size_t) i].def;
        ids.insert (defs[(size_t) i].id);
        ok &= gx::paramIndex (defs[(size_t) i].id) == i;
    }
    check (ok, "indices, defaults inside ranges, lookup");
    check ((int) ids.size() == gx::P::numParams, "unique ids");
    for (int s = 0; s < gx::scaleNames().size(); ++s) check (! gx::scaleSteps (s).empty() && gx::scaleSteps (s)[0] == 0, "scale " + gx::scaleNames()[s]);
}

static void testRender()
{
    std::puts ("[render]");
    const double sr = 48000;
    gx::Preset p;
    auto b = render (p, 30.0, sr);
    const auto all = analyse (b);
    const auto late = analyse (b, (int) (10 * sr));
    std::printf ("    default preset: rms %.1f dB, peak %.1f dB, dc %.5f\n", db (late.rms), db (all.peak), all.dc);
    check (all.finite, "finite");
    check (all.peak < 1.0f, "peak below 0 dBFS");
    check (late.rms > 0.005f, "audible after the fade-in");
    check (all.dc < 0.01f, "no DC");
    check (analyse (b, 0, 256).peak < 0.02f, "starts from silence (fade-in)");

    // every layer alone makes sound
    for (int l = 0; l < gx::kNumLayers; ++l)
    {
        gx::Preset q;
        for (int k = 0; k < gx::kNumLayers; ++k) q.params[(size_t) gx::P::layer (k, gx::P::lOn)] = k == l ? 1.0f : 0.0f;
        q.params[(size_t) gx::P::layer (l, gx::P::lPresence)] = 1.0f;
        q.params[(size_t) gx::P::layer (l, gx::P::lLevel)] = 0.8f;
        if (l == gx::lyBells) q.params[(size_t) gx::P::layer (l, gx::P::lChar)] = 0.9f;
        auto r = render (q, 20.0, sr);
        const auto st = analyse (r, (int) (6 * sr));
        std::printf ("    %-8s rms %.1f dB, peak %.1f dB\n", gx::layerIds()[l].toRawUTF8(), db (st.rms), db (st.peak));
        check (st.finite && st.rms > 0.0005f && st.peak < 1.0f, "layer alone: " + gx::layerIds()[l]);
    }

    // everything off -> silence
    gx::Preset off;
    for (int k = 0; k < gx::kNumLayers; ++k) off.params[(size_t) gx::P::layer (k, gx::P::lOn)] = 0.0f;
    check (analyse (render (off, 5.0, sr)).peak < 1.0e-6f, "all layers off is silent");
}

static void testDeterminism()
{
    std::puts ("[determinism]");
    const double sr = 44100;
    gx::Preset p;
    setParam (p, "seed", 777);
    setParam (p, "evolution", 0.9f);
    setParam (p, "chord_rate", 1.0f);
    setParam (p, "bells_char", 0.9f);
    const auto a = render (p, 25.0, sr, 512), b = render (p, 25.0, sr, 333), c = render (p, 25.0, sr, 64);
    float diff = 0, diff2 = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            diff = juce::jmax (diff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
            diff2 = juce::jmax (diff2, std::abs (a.getSample (ch, i) - c.getSample (ch, i)));
        }
    std::printf ("    max diff 512 vs 333: %g, 512 vs 64: %g\n", (double) diff, (double) diff2);
    check (diff < 1.0e-5f && diff2 < 1.0e-5f, "identical output for any block size");

    setParam (p, "seed", 778);
    const auto d = render (p, 25.0, sr, 512);
    float sep = 0;
    for (int i = 0; i < a.getNumSamples(); ++i) sep = juce::jmax (sep, std::abs (a.getSample (0, i) - d.getSample (0, i)));
    check (sep > 0.01f, "another seed sounds different");
}

static void testTransport()
{
    std::puts ("[transport]");
    const double sr = 48000;
    gx::Engine e;
    e.prepare (sr, 512);
    gx::Preset p;
    p.params[(size_t) gx::P::fadeIn] = 1.0f;
    p.params[(size_t) gx::P::fadeOut] = 2.0f;
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer none;
    gx::HostInfo hi;
    auto run = [&] (double secs) { float pk = 0; for (int k = 0; k < (int) (secs * sr / 512); ++k) { e.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi); pk = juce::jmax (pk, b.getMagnitude (0, 512)); } return pk; };
    check (run (2.0) < 1.0e-6f, "stopped: silent");
    hi.internalPlay = true;
    check (run (6.0) > 0.01f, "internal Play: sound");
    check (e.tele.running.load(), "telemetry running");
    hi.internalPlay = false;
    float tail = 1;
    for (int s = 0; s < 20 && tail > 1.0e-4f; ++s) tail = run (1.0);
    if (tail > 1.0e-4f) std::printf ("    tail after 20 s: %.1f dB\n", 20.0 * std::log10 (tail));
    check (tail < 1.0e-4f, "Stop: fades out to silence (tail < 20 s)");

    // DAW transport: playing from a position resyncs; the same position gives the same audio
    auto fromHost = [&] (int64_t start, double secs)
    {
        gx::Engine eng;
        eng.prepare (sr, 512);
        gx::HostInfo h; h.hostPlaying = true; h.hasPosition = true; h.hostSample = start;
        juce::AudioBuffer<float> out (2, (int) (secs * sr));
        for (int pos = 0; pos + 512 <= out.getNumSamples(); pos += 512)
        {
            eng.process (out.getWritePointer (0, pos), out.getWritePointer (1, pos), 512, none, p.params.data(), h);
            h.hostSample += 512;
        }
        return out;
    };
    const auto h1 = fromHost (48000 * 30, 6.0), h2 = fromHost (48000 * 30, 6.0);
    float diff = 0;
    for (int i = 0; i < h1.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (h1.getSample (0, i) - h2.getSample (0, i)));
    check (diff < 1.0e-6f && analyse (h1).rms > 0.001f, "DAW playback from a position is reproducible");

    // session timer: 1 minute -> stops by itself
    gx::Engine t;
    t.prepare (sr, 512);
    gx::Preset tp;
    tp.params[(size_t) gx::P::timer] = 1;
    tp.params[(size_t) gx::P::fadeOut] = 3;
    gx::HostInfo th; th.internalPlay = true;
    bool expired = false;
    for (int k = 0; k < (int) (70 * sr / 512) && ! expired; ++k)
    {
        t.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, tp.params.data(), th);
        expired = t.tele.timerExpired.exchange (false);
        if (expired) check (t.tele.sessionElapsed.load() > 50.0f && t.tele.sessionElapsed.load() < 60.0f, "timer fires before the end minus fade-out");
    }
    check (expired, "session timer expires");
}

// Changing the sound while it plays (preset browsing, new seed) must take effect at once, without a Stop.
static void testLiveChanges()
{
    std::puts ("[live changes]");
    const double sr = 48000;
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer none;
    gx::HostInfo hi; hi.internalPlay = true;
    auto run = [&] (gx::Engine& e, const gx::Preset& p, double secs) { for (int k = 0; k < (int) (secs * sr / 512); ++k) e.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi); };
    gx::Preset a, c;
    setParam (a, "seed", 11); setParam (a, "scale", 0); setParam (a, "octave", 2); setParam (a, "harmony", 0.6f);
    c = a;
    setParam (c, "seed", 12345); setParam (c, "scale", 1); setParam (c, "octave", 3);
    gx::Engine e1, e2;
    e1.prepare (sr, 512); e2.prepare (sr, 512);
    run (e1, a, 5.0); run (e2, a, 5.0);
    run (e1, c, 1.0); run (e2, a, 1.0);
    // B: C minor-ish chord notes one octave up (pad base = 12 * (octave + 2) + root)
    const int base = 12 * (3 + 2) + (int) c.get (gx::P::key);
    bool inRange = true, inScale = true;
    for (int i = 0; i < gx::kChordVoices; ++i)
    {
        const int n = e1.tele.chordNotes[(size_t) i].load();
        inRange &= n >= base - 3 && n <= base + 26; // voice-leading may dip a few semitones below the register base
        const auto& st = gx::scaleSteps (1);
        inScale &= std::find (st.begin(), st.end(), ((n - (int) c.get (gx::P::key)) % 12 + 12) % 12) != st.end();
    }
    check (inRange && inScale, "new scale / octave apply while playing");
    run (e1, c, 20.0); run (e2, a, 20.0);
    float diff = 0;
    for (int l = 0; l < gx::kNumLayers; ++l) diff += std::abs (e1.tele.presence[(size_t) l].load() - e2.tele.presence[(size_t) l].load());
    check (diff > 0.01f || e1.tele.chordDegree.load() != e2.tele.chordDegree.load(), "new seed changes the journey while playing");
    check (e1.tele.running.load() && e1.tele.runGain.load() > 0.99f, "no stop / fade on a live change");
}

static void testFeatures()
{
    std::puts ("[features]");
    // every noise type is finite and audible
    for (int nt = 1; nt < 6; ++nt)
    {
        gx::Preset p;
        for (int l = 0; l < gx::kNumLayers; ++l) p.params[(size_t) gx::P::layer (l, gx::P::lOn)] = 0;
        p.params[(size_t) gx::P::noiseType] = (float) nt;
        p.params[(size_t) gx::P::noiseLevel] = 0.8f;
        const auto st = analyse (render (p, 6.0), 48000 * 2);
        check (st.finite && st.rms > 0.0003f && st.peak < 1.0f, "noise type " + juce::String (nt));
    }
    // sleep mode: with a 1-minute timer the second half is clearly quieter than the start
    {
        gx::Preset p;
        p.params[(size_t) gx::P::timer] = 1; p.params[(size_t) gx::P::sleepMode] = 1;
        const auto b = render (p, 58.0);
        const float early = analyse (b, 48000 * 8, 48000 * 8).rms, late = analyse (b, 48000 * 50, 48000 * 6).rms;
        std::printf ("    sleep mode: %.1f dB -> %.1f dB\n", 20.0 * std::log10 (early + 1e-9), 20.0 * std::log10 (late + 1e-9));
        check (late < early * 0.5f, "sleep mode fades out over the timer");
    }
    // tempo sync: chords change exactly on the bar grid of the host
    {
        gx::Engine e;
        e.prepare (48000, 512);
        gx::Preset p;
        p.params[(size_t) gx::P::syncChords] = 1; p.params[(size_t) gx::P::chordBars] = 0; p.params[(size_t) gx::P::harmony] = 0.7f;
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer none;
        gx::HostInfo hi; hi.hostPlaying = true; hi.hasPosition = true; hi.hasPpq = true; hi.bpm = 120;
        int changes = 0, last = -1;
        for (int k = 0; k < (int) (48000 * 16 / 512); ++k)
        {
            hi.hostSample = (int64_t) k * 512; hi.ppq = (double) hi.hostSample / 48000.0 * 2.0;
            e.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi);
            const int d = e.tele.chordNotes[0].load() * 100 + e.tele.chordDegree.load();
            if (d != last) { ++changes; last = d; }
        }
        std::printf ("    tempo sync: %d chord changes in 8 bars\n", changes);
        check (changes >= 4 && changes <= 9, "tempo-synced chords change about once per bar");
    }
}

static void testMidi()
{
    std::puts ("[midi]");
    const double sr = 48000;
    gx::Engine e;
    e.prepare (sr, 512);
    gx::Preset p;
    juce::AudioBuffer<float> b (2, 512);
    gx::HostInfo hi;
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 62, 0.9f), 100); // D
    e.process (b.getWritePointer (0), b.getWritePointer (1), 512, m, p.params.data(), hi);
    juce::MidiBuffer none;
    float pk = 0;
    for (int k = 0; k < 100; ++k) { e.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi); pk = juce::jmax (pk, b.getMagnitude (0, 512)); }
    check (pk > 0.005f, "MIDI notes play even while stopped");
    hi.internalPlay = true;
    for (int k = 0; k < 10; ++k) e.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi);
    check (e.tele.rootPc.load() == 2, "MIDI note sets the key (D)");
}

static void testExport()
{
    std::puts ("[export]");
    gx::Preset p;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    for (int f = 0; f < 3; ++f)
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("memoria_export_test" + gx::exportFormatExtension (f));
        std::atomic<float> prog { 0 };
        const auto err = gx::exportAudio (p, file, f, 6.0, 48000.0, &prog);
        check (err.isEmpty(), "export " + gx::exportFormatExtension (f) + " " + err);
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        check (r != nullptr && std::abs ((double) r->lengthInSamples - 6.0 * 48000.0) < 4096 && r->numChannels == 2, "reads back " + gx::exportFormatExtension (f));
        check (prog.load() > 0.99f, "progress reaches 100%");
        if (r != nullptr)
        {
            juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
            r->read (&b, 0, b.getNumSamples(), 0, true, true);
            check (analyse (b).rms > 0.001f && analyse (b, b.getNumSamples() - 64).peak < 0.01f, "audible, silent at the end " + gx::exportFormatExtension (f));
        }
        r.reset();
        file.deleteFile();
    }
}

static void testStress()
{
    std::puts ("[stress]");
    const double sr = 44100;
    juce::Random rnd (99);
    bool ok = true;
    for (int k = 0; k < 6; ++k)
    {
        gx::Preset p;
        for (auto& d : gx::paramDefs()) p.params[(size_t) d.index] = d.range.convertFrom0to1 (rnd.nextFloat());
        p.params[(size_t) gx::P::master] = 6.0f;
        p.params[(size_t) gx::P::fadeIn] = 0.5f;
        const auto b = render (p, 8.0, sr, 256);
        const auto s = analyse (b);
        ok &= s.finite && s.peak <= 1.0f;
        if (! s.finite || s.peak > 1.0f) std::printf ("    random patch %d: finite %d peak %.3f\n", k, (int) s.finite, (double) s.peak);
    }
    check (ok, "random patches stay finite and below 0 dBFS");
}

static void testCpu()
{
    std::puts ("[cpu]");
    gx::Preset p;
    for (int l = 0; l < gx::kNumLayers; ++l) { p.params[(size_t) gx::P::layer (l, gx::P::lPresence)] = 1; p.params[(size_t) gx::P::layer (l, gx::P::lLevel)] = 0.7f; }
    p.params[(size_t) gx::P::density] = 1;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    render (p, 10.0, 48000.0, 512);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("    all layers at full density: %.1f%% of one core\n", ms / 100.0);
    check (ms / 100.0 < 40.0, "CPU budget");
}

int runAll()
{
    testParams();
    testRender();
    testDeterminism();
    testTransport();
    testLiveChanges();
    testFeatures();
    testMidi();
    testExport();
    testStress();
    testCpu();
    for (auto& t : extraTests()) t();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

std::vector<std::function<void()>>& extraTests() { static std::vector<std::function<void()>> v; return v; }
}
