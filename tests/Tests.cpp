// Memoria unit tests (run with: MemoriaTool test)
#include "Tests.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include <chrono>

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

int runAll()
{
    testParams();
    testSound();
    testDeterminism();
    testClicks();
    testStress();
    for (auto& t : extraTests()) t();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
}
