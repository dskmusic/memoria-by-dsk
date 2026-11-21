// Memoria unit tests (run with: MemoriaTool test)
#include "Tests.h"
#include "preset/Song.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include <algorithm>
#include <chrono>
#include "gen/Generator.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define GXT_BIN 1
#endif

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

juce::AudioBuffer<float> render (const gx::Preset& p, const Song& s, double seconds, double sr, int bs)
{
    juce::AudioBuffer<float> out (2, (int) (seconds * sr));
    out.clear();
    gx::renderMidi (p, s.buf, out, sr, bs);
    return out;
}

float rmsDb (const juce::AudioBuffer<float>& b, double t0, double t1, double sr)
{
    const int a = juce::jlimit (0, b.getNumSamples(), (int) (t0 * sr)), e = juce::jlimit (a, b.getNumSamples(), (int) (t1 * sr));
    return juce::Decibels::gainToDecibels (analyse (b, a, e - a).rms, -120.0f);
}

float centroidHz (const juce::AudioBuffer<float>& b, double t0, double sr)
{
    constexpr int N = 4096;
    juce::dsp::FFT fft (12);
    std::vector<float> d (2 * N, 0.0f);
    const int a = juce::jlimit (0, juce::jmax (0, b.getNumSamples() - N), (int) (t0 * sr));
    for (int i = 0; i < N; ++i)
        d[(size_t) i] = 0.5f * (b.getSample (0, a + i) + b.getSample (1, a + i)) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / N));
    fft.performFrequencyOnlyForwardTransform (d.data());
    double num = 0, den = 0;
    for (int i = 1; i < N / 2; ++i) { const double f = i * sr / N; num += f * d[(size_t) i]; den += d[(size_t) i]; }
    return den > 0 ? (float) (num / den) : 0.0f;
}

// a click = a sample whose second difference towers over the recent history, outside the first 8 ms after every note-on
int countClicks (const juce::AudioBuffer<float>& b, const std::vector<double>& onsets, double sr)
{
    int clicks = 0;
    const int n = b.getNumSamples();
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float* x = b.getReadPointer (c);
        double hist = 1.0e-5;
        for (int i = 2; i < n; ++i)
        {
            const double d2 = std::abs ((double) x[i] - 2.0 * x[i - 1] + x[i - 2]);
            bool skip = false;
            for (double o : onsets) if ((double) i / sr >= o && (double) i / sr < o + 0.008) { skip = true; break; }
            if (! skip && d2 > 14.0 * hist && d2 > 4.0e-4) { ++clicks; if (clicks < 8) std::printf ("      click ch%d at %.4f s (d2 %.5f, hist %.6f)\n", c, (double) i / sr, d2, hist); }
            hist = juce::jmax (1.0e-5, hist * 0.999 + d2 * 0.001, d2 * 0.01);
        }
    }
    return clicks;
}

void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return;
    if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

std::vector<std::function<void()>>& extraTests() { static std::vector<std::function<void()>> v; return v; }

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
    check (gx::pianoNames().size() == gx::numPianoTypes(), "piano names match models");
}

static void testSound()
{
    std::puts ("[sound]");
    gx::Preset p;
    {   // silence without notes
        Song s;
        const auto b = render (p, s, 1.0);
        const auto st = analyse (b);
        check (st.finite && st.peak < 1.0e-6f, "silent without notes");
    }
    for (int type = 0; type < gx::numPianoTypes(); ++type)
    {
        auto q = p; setParam (q, "piano", (float) type);
        Song s;
        s.note (0.1, 1.5, 48, 90); s.note (0.1, 1.5, 60, 90); s.note (0.1, 1.5, 76, 90);
        const auto b = render (q, s, 5.0);
        const auto st = analyse (b);
        const juce::String nm = gx::pianoNames()[type];
        std::printf ("    %-14s peak %.3f  rms %.1f dB\n", nm.toRawUTF8(), st.peak, rmsDb (b, 0.15, 0.5));
        check (st.finite, nm + ": finite");
        check (st.peak < 1.0f && st.peak > 0.02f, nm + ": level sane (peak " + juce::String (st.peak, 3) + ")");
        check (st.dc < 1.0e-3f, nm + ": no DC");
        check (rmsDb (b, 0.15, 0.5) > rmsDb (b, 3.0, 3.5) + 6.0f, nm + ": decays");
    }
    {   // velocity: louder AND brighter
        float lastDb = -200, lastC = 0; bool mono = true;
        for (int v : { 25, 55, 85, 120 })
        {
            Song s; s.note (0.05, 1.0, 60, v);
            const auto b = render (p, s, 1.5);
            const float d = rmsDb (b, 0.06, 0.4), c = centroidHz (b, 0.07);
            std::printf ("    C4 vel %3d: %6.1f dB  centroid %6.0f Hz\n", v, d, c);
            mono &= d > lastDb && c > lastC;
            lastDb = d; lastC = c;
        }
        check (mono, "velocity raises level and brightness");
    }
    {   // pedal keeps the notes ringing
        Song a, b2, hp;
        a.note (0.1, 0.3, 60, 80);
        b2.note (0.1, 0.3, 60, 80); b2.pedal (0.0, 127); b2.pedal (2.5, 0);
        hp.note (0.1, 0.3, 60, 80); hp.pedal (0.0, 52);
        const auto ra = render (p, a, 3.0), rb = render (p, b2, 3.0), rh = render (p, hp, 3.0);
        const float da = rmsDb (ra, 1.2, 1.6), db = rmsDb (rb, 1.2, 1.6), dh = rmsDb (rh, 1.2, 1.6);
        std::printf ("    note released at 0.4 s, level at 1.4 s: no pedal %.1f dB, half pedal %.1f dB, full pedal %.1f dB\n", da, dh, db);
        check (db > da + 10.0f, "sustain pedal keeps the note ringing");
        check (dh > da + 2.0f && dh < db + 0.1f, "half pedal sits in between");
    }
    {   // loudness across the keyboard stays within a few dB
        float lo = 1000, hi = -1000;
        for (int n = 28; n <= 100; n += 6)
        {
            Song s; s.note (0.05, 1.0, n, 80);
            const auto b = render (p, s, 1.0);
            const float d = rmsDb (b, 0.06, 0.45);
            std::printf ("      note %3d: %6.1f dB\n", n, d);
            lo = juce::jmin (lo, d); hi = juce::jmax (hi, d);
        }
        std::printf ("    level spread over the keyboard: %.1f dB\n", hi - lo);
        check (hi - lo < 9.0f, "level is even across the keyboard");
    }
}

static void testDeterminism()
{
    std::puts ("[determinism]");
    gx::Preset p;
    Song s;
    for (int i = 0; i < 12; ++i) s.note (0.2 + i * 0.37, 0.9, 48 + (i * 5) % 30, 40 + (i * 13) % 80);
    s.pedal (1.0, 127); s.pedal (3.0, 0);
    const auto ref = render (p, s, 6.0, 48000.0, 512);
    for (int bs : { 333, 64, 1024 })
    {
        const auto other = render (p, s, 6.0, 48000.0, bs);
        bool same = true;
        for (int c = 0; c < 2 && same; ++c)
            for (int i = 0; i < ref.getNumSamples(); ++i)
                if (ref.getSample (c, i) != other.getSample (c, i)) { same = false; break; }
        check (same, "bit-identical at block size " + juce::String (bs));
    }
}

static void testClicks()
{
    std::puts ("[clicks]");
    gx::Preset p;
    Song s;
    for (int i = 0; i < 20; ++i) s.note (0.1 + i * 0.22, 0.6, 40 + (i * 7) % 40, 50 + (i * 11) % 70);
    s.pedal (0.5, 127); s.pedal (2.0, 0); s.pedal (2.4, 90); s.pedal (3.3, 0);
    for (int i = 0; i < 6; ++i) s.note (4.8 + i * 0.05, 0.4, 60, 100);       // re-strikes of a ringing note
    const auto b = render (p, s, 7.0);
    const int clicks = countClicks (b, s.onsets);
    check (clicks == 0, "no clicks (" + juce::String (clicks) + ")");
}

static void testStress()
{
    std::puts ("[stress]");
    gx::Preset p;
    Song s;
    for (int i = 0; i < 90; ++i) s.note (0.02 * i, 3.0, 21 + (i * 3) % 88, 127);
    s.pedal (0.0, 127);
    const auto t0 = std::chrono::steady_clock::now();
    const auto b = render (p, s, 10.0);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const auto st = analyse (b);
    std::printf ("    90 notes with pedal, 10 s of audio rendered in %.2f s (%.1f x realtime)\n", secs, 10.0 / secs);
    std::printf ("    overload peak %.4f finite %d\n", st.peak, (int) st.finite);
    check (st.finite && st.peak <= 1.0f, "polyphony overload stays finite and bounded");
    check (secs < 5.0, "CPU: faster than 2x realtime even at full overload");
}

static juce::AudioBuffer<float> renderHands (const gx::Preset& p, double secs, int bs)
{
    juce::AudioBuffer<float> out (2, (int) (secs * 48000.0));
    out.clear();
    juce::MidiBuffer none;
    gx::renderMidi (p, none, out, 48000.0, bs, nullptr, true);
    return out;
}

static void testHands()
{
    std::puts ("[hands / composer]");
    int bad = 0;
    for (int style = 0; style < gx::numSongStyles; ++style)
        for (int scale = 0; scale < 8; ++scale)
            for (int key = 0; key < 12; key += 3)
                for (int bars : { 16, 32, 64 })
                {
                    gx::ComposeCfg c; c.style = style; c.scale = scale; c.key = key; c.bars = bars; c.seed = (uint32_t) (style * 131 + scale * 17 + key + bars);
                    gx::Score s, s2; gx::compose (c, s); gx::compose (c, s2);
                    bool ok = s.notes.size() > (size_t) bars * 2 && s.notes.size() == s2.notes.size() && s.bars == bars;
                    for (size_t i = 0; i < s.notes.size(); ++i)
                    {
                        const auto& n = s.notes[i];
                        ok &= n.note >= 21 && n.note <= 108 && n.vel >= 1 && n.vel <= 127 && n.len > 0 && n.start >= 0 && n.start < s.totalBeats;
                        if (i > 0) ok &= s.notes[i - 1].start <= n.start;
                        if (i < s2.notes.size()) ok &= n.note == s2.notes[i].note && n.start == s2.notes[i].start;
                    }
                    if (! ok) ++bad;
                }
    check (bad == 0, "composer: every style / scale / key / length gives a valid, deterministic score (" + juce::String (bad) + " bad)");
    for (int style = 0; style < gx::numSongStyles; ++style)
    {
        gx::ComposeCfg c; c.style = style; c.bars = 32; c.seed = 11 + (uint32_t) style;
        gx::Score s; gx::compose (c, s);
        int lo = 127, hi = 0; double vsum = 0;
        for (auto& n : s.notes) { lo = std::min (lo, n.note); hi = std::max (hi, n.note); vsum += n.vel; }
        std::puts (("    " + gx::handsStyleNames()[style] + ": " + juce::String ((int) s.notes.size()) + " notes, range " + juce::String (lo) + "-" + juce::String (hi) + ", mean velocity " + juce::String (vsum / (double) s.notes.size(), 0) + ", sections " + juce::String ((int) s.sections.size())).toRawUTF8());
        check (s.sections.size() >= 4 && s.sections.front().type == 'I' && s.sections.back().type == 'O', gx::handsStyleNames()[style] + ": song form (intro ... outro)");
        gx::ComposeCfg c2 = c; c2.seed += 1; gx::Score s3; gx::compose (c2, s3);
        bool differs = s3.notes.size() != s.notes.size();
        for (size_t i = 0; i < std::min (s.notes.size(), s3.notes.size()) && ! differs; ++i) differs = s.notes[i].note != s3.notes[i].note;
        check (differs, gx::handsStyleNames()[style] + ": a new seed is a different song");
    }
    // audio: every style plays, deterministic at any block size, bounded
    for (int style = 0; style < gx::numSongStyles; ++style)
    {
        gx::Preset p; setParam (p, "hands", 1.0f); setParam (p, "hands_style", (float) style); setParam (p, "hands_tempo", 110.0f); setParam (p, "hands_bars", 0.0f);
        const auto a = renderHands (p, 20.0, 512);
        const float d = rmsDb (a, 2.0, 20.0);
        std::puts (("    audio " + gx::handsStyleNames()[style] + ": " + juce::String (d, 1) + " dB, peak " + juce::String (analyse (a).peak, 2)).toRawUTF8());
        check (analyse (a).finite && analyse (a).peak < 1.0f && d > -45.0f, gx::handsStyleNames()[style] + ": plays, bounded");
        const auto b = renderHands (p, 20.0, 77);
        bool same = true;
        for (int i = 0; i < a.getNumSamples() && same; ++i) same = a.getSample (0, i) == b.getSample (0, i);
        check (same, gx::handsStyleNames()[style] + ": identical at any block size");
    }
}

static void testGenerator()
{
    std::puts ("[generator]");
    for (auto& cat : gx::categories())
    {
        gx::GenRequest rq; rq.category = cat; rq.seed = 4242;
        float rms = 0;
        auto p = gx::Generator::generate (rq);
        const bool ok = gx::Generator::validate (p, &rms);
        std::puts (("    " + cat + " / " + p.meta.name + ": rms " + juce::String (rms, 1) + " dB").toRawUTF8());
        check (ok && std::abs (rms - gx::Generator::targetRmsDb()) < 2.0f, cat + ": valid and level-matched");
        check (p.meta.category == cat && p.meta.name.isNotEmpty(), cat + ": metadata");
    }
#if GXT_BIN
    int size = 0;
    const char* data = BinaryData::getNamedResource ("factory_memoriabank", size);
    check (data != nullptr, "factory bank embedded");
    if (data != nullptr)
    {
        const juce::var bank = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        auto* arr = bank.getProperty ("presets", {}).getArray();
        check (arr != nullptr && arr->size() >= 150, "factory bank has 150+ presets");
        if (arr != nullptr)
        {
            std::set<juce::String> names;
            for (auto& v : *arr) names.insert (gx::Preset::fromVar (v).meta.name);
            check ((int) names.size() == arr->size(), "factory names are unique");
            for (int i = 0; i < arr->size(); i += juce::jmax (1, arr->size() / 5))
            {
                auto p = gx::Preset::fromVar ((*arr)[i]);
                float rms = 0;
                check (gx::Generator::validate (p, &rms) && std::abs (rms - gx::Generator::targetRmsDb()) < 2.0f, "factory sample " + p.meta.name + " valid and level-matched");
            }
        }
    }
#endif
}

static void testLife()
{
    std::puts ("[life]");
    gx::Preset clean;
    Song s;
    for (int i = 0; i < 10; ++i) s.note (0.2 + i * 0.5, 0.8, 55 + (i * 4) % 20, 70);
    s.pedal (0.0, 127); s.pedal (4.0, 0);
    const auto a = render (clean, s, 7.0);
    auto mem = clean; setParam (mem, "memory", 1.0f);
    const auto b = render (mem, s, 7.0);
    const float ca = centroidHz (a, 1.0), cb = centroidHz (b, 1.0);
    std::puts (("    Memory 0 -> 1: centroid " + juce::String (ca, 0) + " -> " + juce::String (cb, 0) + " Hz, level " + juce::String (rmsDb (a, 0.5, 4.5), 1) + " -> " + juce::String (rmsDb (b, 0.5, 4.5), 1) + " dB").toRawUTF8());
    double diff = 0, ref = 0; for (int i = 0; i < a.getNumSamples(); ++i) { const double d = a.getSample (0, i) - b.getSample (0, i); diff += d * d; ref += (double) a.getSample (0, i) * a.getSample (0, i); }
    check (diff > 0.2 * ref, "Memory transforms the sound");
    check (analyse (b).finite && analyse (b).peak < 1.0f, "Memory at 100 % is clean and bounded");
    check (std::abs (rmsDb (a, 0.5, 4.5) - rmsDb (b, 0.5, 4.5)) < 6.0f, "Memory keeps the level similar");
    for (int w = 1; w < gx::worldNames().size(); ++w)
    {
        auto q = clean; setParam (q, "world", (float) w); setParam (q, "world_level", 0.8f);
        Song none;
        const auto r = render (q, none, 6.0);
        const float d = rmsDb (r, 1.0, 6.0);
        std::puts (("    world " + gx::worldNames()[w] + ": " + juce::String (d, 1) + " dB").toRawUTF8());
        check (analyse (r).finite && analyse (r).peak < 0.5f, "world " + gx::worldNames()[w] + " bounded");
        if (w != 4) check (d > -70.0f, "world " + gx::worldNames()[w] + " audible");
    }
    // every layer at maximum: finite, no clicks, identical for any block size
    auto full = clean;
    for (const char* id : { "memory", "tape", "vinyl", "wood", "warmth", "time" }) setParam (full, id, 1.0f);
    setParam (full, "world", 1.0f); setParam (full, "world_level", 1.0f);
    for (int rt = 0; rt < 5; ++rt)
    {
        setParam (full, "room_type", (float) rt);
        const auto x = render (full, s, 7.0), y = render (full, s, 7.0, 48000.0, 77);
        bool same = true;
        for (int i = 0; i < x.getNumSamples() && same; ++i) same = x.getSample (0, i) == y.getSample (0, i) && x.getSample (1, i) == y.getSample (1, i);
        check (analyse (x).finite && analyse (x).peak < 1.0f, "all layers max, room " + gx::roomNames()[rt] + ": bounded");
        check (same, "all layers max, room " + gx::roomNames()[rt] + ": identical at any block size");
    }
}

static void testPresetSwitch()
{
    std::puts ("[preset switching through the real processor]");
#if GXT_BIN
    int size = 0;
    const char* data = BinaryData::getNamedResource ("factory_memoriabank", size);
    if (data == nullptr) return;
    const juce::var bank = juce::JSON::parse (juce::String::fromUTF8 (data, size));
    auto* arr = bank.getProperty ("presets", {}).getArray();
    if (arr == nullptr || arr->size() < 50) return;
    MemoriaProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> blk (2, 512);
    auto run = [&] (int blocks, bool notes)
    {
        juce::AudioBuffer<float> all (2, blocks * 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            if (notes && b == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); }
            blk.clear();
            proc.processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, b * 512, blk, c, 0, 512);
        }
        return all;
    };
    float lo = 1.0e9f, hi = 0; int mismatches = 0;
    for (int k = 0; k < 10; ++k)
    {
        const auto pr = gx::Preset::fromVar ((*arr)[k * (arr->size() / 10)]);
        pr.applyTo (proc);
        run (60, false);                                    // let the click-free switch finish
        const auto got = run (90, true);
        Song s; s.note (0.0, 1.0, 60, 100); s.note (0.0, 1.0, 64, 100);
        const auto want = render (pr, s, 90.0 * 512.0 / 48000.0);
        const float cg = centroidHz (got, 0.1), cw = centroidHz (want, 0.1);
        bool paramsOk = true;
        for (int i = 0; i < gx::P::numParams; ++i) paramsOk &= std::abs (proc.getParam (i) - pr.params[(size_t) i]) < 1.0e-3f * juce::jmax (1.0f, std::abs (pr.params[(size_t) i]));
        std::puts (("    " + pr.meta.name + ": piano " + juce::String ((int) pr.params[(size_t) gx::P::piano]) + ", memory " + juce::String (pr.params[(size_t) gx::P::memory], 2)
                    + " | plugin centroid " + juce::String (cg, 0) + " Hz, direct " + juce::String (cw, 0) + " Hz, rms " + juce::String (rmsDb (got, 0.1, 0.9), 1) + " dB | params applied: " + (paramsOk ? "yes" : "NO")).toRawUTF8());
        const bool noisy = pr.params[(size_t) gx::P::world] > 0.5f || pr.params[(size_t) gx::P::tape] > 0.15f || pr.params[(size_t) gx::P::vinyl] > 0.15f || pr.params[(size_t) gx::P::memory] > 0.4f;
        const float tol = pr.params[(size_t) gx::P::piano] >= 4.0f ? 0.7f : 0.25f;      // bell-like bars decay fast: the analysis window is timing-sensitive
        if ((! noisy && std::abs (cg - cw) > tol * cw) || ! paramsOk) ++mismatches;
        lo = juce::jmin (lo, cg); hi = juce::jmax (hi, cg);
    }
    check (mismatches == 0, "the plugin plays what the preset says (" + juce::String (mismatches) + " mismatches)");
    check (hi > lo * 1.3f, "presets sound different from each other (centroid spread " + juce::String (lo, 0) + " - " + juce::String (hi, 0) + " Hz)");
#endif
}

static void testHostSilence()
{
    std::puts ("[silence when the DAW plays]");
    gx::Preset p;                                   // Hands mode is Full Song by default
    gx::Engine eng;
    eng.prepare (48000.0, 512);
    eng.reset (1);
    gx::HostInfo hi;
    hi.hostPlaying = true; hi.hasPosition = true; hi.hasPpq = true; hi.bpm = 120;       // the DAW is playing, nothing else
    juce::MidiBuffer none;
    juce::AudioBuffer<float> b (2, 512);
    float peak = 0;
    for (int blk = 0; blk < 400; ++blk)             // about 4 s of DAW playback
    {
        hi.ppq = (double) blk * 512.0 / 48000.0 * 2.0;
        b.clear();
        eng.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi);
        peak = juce::jmax (peak, b.getMagnitude (0, 512));
    }
    check (peak < 1.0e-6f, "a loaded plugin is silent while the DAW plays (peak " + juce::String (peak, 6) + ")");
    // with the instrument's own PLAY the hands do play
    eng.reset (1);
    hi.hostPlaying = false; hi.internalPlay = true;
    peak = 0;
    for (int blk = 0; blk < 400; ++blk) { b.clear(); eng.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, p.params.data(), hi); peak = juce::jmax (peak, b.getMagnitude (0, 512)); }
    check (peak > 0.01f, "the hands play with the instrument's own PLAY");
}

static void testMusicLint()
{
    std::puts ("[music lint: the songs follow the rules of the songbook]");
    double chordTone = 0, topViol = 0, clash = 0, velGap = 0; int runs = 0, worstPoly = 0;
    for (int style : { 0, 1, 2, 3, 4, 5, 7, 8, 9 })
        for (uint32_t seed = 1; seed <= 30; ++seed)
            for (int scale : { 0, 1 })
            {
                gx::ComposeCfg c; c.style = style; c.seed = seed * 17u + (uint32_t) style; c.bars = 32; c.key = 0; c.scale = scale;
                gx::Score s; gx::compose (c, s);
                // the melody = the highest note of every onset that is flagged as melody
                std::vector<const gx::NoteEv*> mel, acc;
                for (auto& n : s.notes) (n.voice == 1 ? mel : acc).push_back (&n);
                std::vector<const gx::NoteEv*> top;
                for (size_t i = 0; i < mel.size(); ++i)
                {
                    bool highest = true;
                    for (size_t j = 0; j < mel.size(); ++j) if (j != i && std::abs (mel[j]->start - mel[i]->start) < 0.04 && mel[j]->note > mel[i]->note) highest = false;
                    if (highest && mel[i]->vel > 25) top.push_back (mel[i]);
                }
                int strong = 0, strongOk = 0, tv = 0, cl = 0;
                for (auto* n : top)
                {
                    const double bar = std::floor (n->start / s.beatsPerBar), pos = n->start - bar * s.beatsPerBar;
                    const bool isStrong = pos < 0.15 || (s.beatsPerBar == 4 && std::abs (pos - 2.0) < 0.15);
                    unsigned mask = 0;
                    for (auto& cm : s.chordMasks) if (cm.first <= n->start + 0.2) mask = cm.second;
                    if (isStrong && n->len >= 0.9) { ++strong; if (mask & (1u << (unsigned) (n->note % 12))) ++strongOk; }
                    for (auto* a : acc)
                    {
                        if (a->start > n->start + 0.02 || a->start + a->len < n->start + 0.05) continue;     // not sounding at the onset
                        if (a->note >= n->note) { ++tv; break; }
                    }
                    for (auto* a : acc)
                    {
                        if (a->start > n->start + 0.02 || a->start + a->len < n->start + 0.05) continue;
                        const int d = n->note - a->note;
                        if (d > 0 && d < 12 && (d == 1 || d == 11)) { ++cl; break; }
                    }
                }
                std::vector<std::pair<double, int>> ev;
                for (auto& n : s.notes) { ev.push_back ({ n.start, 1 }); ev.push_back ({ n.start + n.len, -1 }); }
                std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.first < b.first || (a.first == b.first && a.second < b.second); });
                int cur = 0, mx = 0; for (auto& e : ev) { cur += e.second; mx = std::max (mx, cur); }
                double mv = 0, av = 0; for (auto* n : top) mv += n->vel; for (auto* a : acc) av += a->vel;
                mv /= std::max<size_t> (1, top.size()); av /= std::max<size_t> (1, acc.size());
                chordTone += strong ? (double) strongOk / strong : 1.0; topViol += (double) tv / std::max<size_t> (1, top.size()); clash += (double) cl / std::max<size_t> (1, top.size());
                velGap += mv - av; worstPoly = std::max (worstPoly, mx); ++runs;
            }
    chordTone /= runs; topViol /= runs; clash /= runs; velGap /= runs;
    std::puts (("    strong-beat chord tones " + juce::String (chordTone * 100, 1) + " %, melody not on top " + juce::String (topViol * 100, 1) + " %, m2/M7 clashes " + juce::String (clash * 100, 1) + " %, melody - accompaniment velocity " + juce::String (velGap, 1) + ", max notes sounding " + juce::String (worstPoly)).toRawUTF8());
    check (chordTone >= 0.90, "long strong-beat melody notes are chord tones (" + juce::String (chordTone * 100, 1) + " %)");
    check (topViol <= 0.03, "the melody is the top voice (" + juce::String (topViol * 100, 1) + " % violations)");
    check (clash <= 0.04, "no harsh m2 / M7 against the accompaniment (" + juce::String (clash * 100, 1) + " %)");
    check (velGap >= 6.0 && velGap <= 30.0, "the melody sits above the accompaniment in velocity (" + juce::String (velGap, 1) + ")");
    check (worstPoly <= 18, "polyphony stays playable (" + juce::String (worstPoly) + " notes at once)");
}

static void testSongForms()
{
    std::puts ("[song forms]");
    int bad = 0;
    for (int form : { 1, 2, 3 })
        for (int style : { 0, 1, 2, 3, 5, 7, 8, 9 })
            for (uint32_t seed = 1; seed <= 4; ++seed)
            {
                gx::ComposeCfg c; c.form = form; c.style = style; c.seed = seed * 13u; c.key = 3; c.scale = 1;
                gx::Score s; gx::compose (c, s);
                if (s.notes.empty()) { ++bad; continue; }
                double lastEnd = 0, first = 1e9;
                for (auto& n : s.notes) { lastEnd = std::max (lastEnd, n.start + n.len); first = std::min (first, n.start); }
                const double expect = form == 3 ? 22.0 * s.beatsPerBar : 8.0 * s.beatsPerBar;
                if (form != 3) { if (std::abs (s.totalBeats - expect) > 1e-6 || lastEnd > s.totalBeats + 1e-6) ++bad; if (c.pedal && ! s.pedal.empty() && s.pedal.back().val != 0) ++bad; }
                else if (s.bars != 22) ++bad;
                if (first > 1.5) ++bad;
            }
    check (bad == 0, "loop / chorus / short forms end on the grid with everything released (" + juce::String (bad) + " problems)");
}

static void testGainStaging()
{
    std::puts ("[gain staging]");
    gx::Preset p;
    Song chord; chord.pedal (0.0, 127);
    for (int n : { 45, 52, 57, 64, 69, 72 }) chord.note (0.0, 3.0, n, 110);
    gx::Engine eng; eng.prepare (48000.0, 512); eng.reset (1); eng.probing = true;
    gx::HostInfo hi; juce::AudioBuffer<float> b (2, 512);
    for (int pos = 0; pos < 5 * 48000; pos += 512)
    {
        juce::MidiBuffer blk; blk.addEvents (chord.buf, pos, 512, -pos);
        eng.process (b.getWritePointer (0), b.getWritePointer (1), 512, blk, p.params.data(), hi);
    }
    const double pct = 100.0 * (double) eng.probe.limN / (double) std::max<int64_t> (1, eng.probe.n);
    std::puts (("    6-note ff chord with pedal, default sound: internal peak " + juce::String (juce::Decibels::gainToDecibels (eng.probe.tape, -99.0f), 1) + " dBFS, limiter active " + juce::String (pct, 3) + " % of samples").toRawUTF8());
    check (pct < 0.1, "the limiter stays idle for a loud chord (" + juce::String (pct, 3) + " %)");
    for (int style = 0; style < gx::numSongStyles; ++style)
    {
        gx::Preset q; q.params[(size_t) gx::P::hands] = 1; q.params[(size_t) gx::P::handsStyle] = (float) style; q.params[(size_t) gx::P::handsBars] = 1;
        gx::Engine e2; e2.prepare (48000.0, 512); e2.reset (1); e2.probing = true;
        gx::HostInfo h2; h2.internalPlay = true; juce::MidiBuffer none;
        for (int blk = 0; blk < 48000 * 20 / 512; ++blk) e2.process (b.getWritePointer (0), b.getWritePointer (1), 512, none, q.params.data(), h2);
        const double pc = 100.0 * (double) e2.probe.limN / (double) std::max<int64_t> (1, e2.probe.n);
        check (pc < 0.1, "hands " + gx::handsStyleNames()[style] + ": limiter idle (" + juce::String (pc, 3) + " %)");
    }
}

int runAll()
{
    testParams();
    testSound();
    testDeterminism();
    testClicks();
    testStress();
    testLife();
    testGenerator();
    testHands();
    testPresetSwitch();
    testHostSilence();
    testMusicLint();
    testGainStaging();
    testSongForms();
    for (auto& t : extraTests()) t();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
}
