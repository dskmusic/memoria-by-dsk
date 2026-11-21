#include "Tests.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif
#include "preset/Song.h"
#include "PluginEditor.h"
#include "gen/Generator.h"
#include <set>
#include <juce_dsp/juce_dsp.h>

// Organ detector: after the keys are released a piano decays; an organ / drone keeps a steady, tonal sound.
// Tonal = a few bins stand far above the median of the 100 Hz - 1.5 kHz band; steady = it barely drops between 2.0 s and 3.5 s.
struct BandStat { float levelDb = -120, crestDb = 0; };
static BandStat bandStat (const juce::AudioBuffer<float>& b, double t)
{
    constexpr int N = 8192;
    juce::dsp::FFT fft (13);
    std::vector<float> d (2 * N, 0.0f);
    const int a = juce::jlimit (0, juce::jmax (0, b.getNumSamples() - N), (int) (t * 48000.0));
    for (int i = 0; i < N; ++i) d[(size_t) i] = 0.5f * (b.getSample (0, a + i) + b.getSample (1, a + i)) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / N));
    fft.performFrequencyOnlyForwardTransform (d.data());
    std::vector<float> band;
    double e = 0;
    for (int i = (int) (100.0 * N / 48000.0); i < (int) (1500.0 * N / 48000.0); ++i) { band.push_back (d[(size_t) i]); e += (double) d[(size_t) i] * d[(size_t) i]; }
    std::vector<float> sorted = band; std::sort (sorted.begin(), sorted.end());
    BandStat s;
    s.levelDb = 10.0f * (float) std::log10 (std::max (1.0e-12, e / (double) band.size() / ((double) N * N * 0.0625)));
    s.crestDb = 20.0f * (float) std::log10 (std::max (1.0e-9f, sorted.back()) / std::max (1.0e-9f, sorted[sorted.size() / 2]));
    return s;
}
static bool presetIsOrganLike (const gx::Preset& p, juce::String* info = nullptr)
{
    gxtest::Song s;
    for (int n : { 57, 64, 69, 72 }) s.note (0.0, 1.4, n, 72);
    const auto b = gxtest::render (p, s, 4.1);
    const auto s20 = bandStat (b, 2.0), s35 = bandStat (b, 3.5), s05 = bandStat (b, 0.2);
    const float slope = s35.levelDb - s20.levelDb;
    const bool bad = s35.crestDb > 17.0f && slope > -5.0f && s35.levelDb > s05.levelDb - 20.0f;
    if (info) *info = "level " + juce::String (s05.levelDb, 0) + " -> " + juce::String (s20.levelDb, 0) + " -> " + juce::String (s35.levelDb, 0) + " dB (slope " + juce::String (slope, 1) + "), tonal crest at 3.5 s " + juce::String (s35.crestDb, 0) + " dB";
    return bad;
}

static gx::Engine::Probe probeRender (const gx::Preset& p, const gxtest::Song& song, double secs, bool hands)
{
    gx::Engine eng;
    eng.prepare (48000.0, 512);
    eng.reset ((uint32_t) p.get (gx::P::seed));
    eng.probing = true;
    gx::HostInfo hi; hi.internalPlay = hands;
    juce::AudioBuffer<float> out (2, 512);
    juce::MidiBuffer block;
    const int total = (int) (secs * 48000.0);
    for (int pos = 0; pos < total; pos += 512)
    {
        block.clear();
        block.addEvents (song.buf, pos, juce::jmin (512, total - pos), -pos);
        eng.process (out.getWritePointer (0), out.getWritePointer (1), juce::jmin (512, total - pos), block, p.params.data(), hi);
    }
    return eng.probe;
}
static juce::String probeLine (const gx::Engine::Probe& q)
{
    auto db = [] (float x) { return juce::String (juce::Decibels::gainToDecibels (x, -99.0f), 1); };
    return "voices " + db (q.voices) + "  body " + db (q.body) + "  room " + db (q.room) + "  tape-in " + db (q.tape) + " dBFS | limiter>1dB " + juce::String (100.0 * (double) q.limN / (double) juce::jmax ((int64_t) 1, q.n), 2) + " %  clip-knee " + juce::String (100.0 * (double) q.clipN / (double) juce::jmax ((int64_t) 1, q.n), 2) + " %";
}

int runVideo (const juce::File& dir, bool trailer);   // Video.cpp

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
    if (cmd == "factory")
    {
        // MemoriaTool factory <out.memoriabank> [perSubCategory=10] : generates, validates and level-matches the factory bank
        const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "resources/factory.memoriabank");
        const int per = argc > 3 ? juce::String (argv[3]).getIntValue() : 7;
        juce::Array<juce::var> arr;
        std::set<juce::String> names;
        int failed = 0;
        std::vector<gx::Preset> accepted;
        for (auto& cat : gx::categories())
            for (int sub = 0; sub < 3; ++sub)
                for (int k = 0; k < per; ++k)
                {
                    gx::GenRequest rq; rq.category = cat; rq.sub = sub; rq.seed = (uint32_t) (1000 + 97 * k + 13 * sub + 7919 * gx::categories().indexOf (cat));
                    float rms = 0;
                    gx::Preset p; bool ok = false;
                    for (int attempt = 0; attempt < 14 && ! ok; ++attempt)
                    {
                        auto r2 = rq; r2.seed = rq.seed + (uint32_t) attempt * 15485863u;
                        p = gx::Generator::generate (r2, 8);
                        if (! gx::Generator::validate (p, &rms)) continue;
                        if (presetIsOrganLike (p)) continue;                                    // sustained, tonal drone: reject
                        bool dup = false;
                        for (auto& q : accepted) if (q.meta.category == p.meta.category && q.similarity (p) > 0.90f) { dup = true; break; }
                        if (dup) continue;
                        ok = true;
                    }
                    if (! ok) { ++failed; continue; }
                    accepted.push_back (p);
                    juce::String nm = p.meta.name; int n = 2;
                    while (names.count (nm)) nm = p.meta.name + " " + juce::String (n++);
                    names.insert (nm); p.meta.name = nm;
                    arr.add (p.toVar());
                    std::puts ((cat + " / " + p.meta.subcategory + " / " + nm + "  rms " + juce::String (rms, 1) + " dB").toRawUTF8());
                }
        auto* bank = new juce::DynamicObject();
        bank->setProperty ("format", "memoriabank"); bank->setProperty ("version", 1); bank->setProperty ("presets", arr);
        out.getParentDirectory().createDirectory();
        out.replaceWithText (juce::JSON::toString (juce::var (bank), true));
        std::puts (("wrote " + juce::String (arr.size()) + " presets (" + juce::String (failed) + " failed) to " + out.getFullPathName()).toRawUTF8());
        return 0;
    }
    if (cmd == "addbank")
    {
        // MemoriaTool addbank <bank>: appends curated Electric Piano and Vibraphone presets (levelled, organ-audited) to an existing bank
        const auto f = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "resources/factory.memoriabank");
        const juce::var bank = juce::JSON::parse (f.loadFileAsString());
        juce::Array<juce::var> arr = *bank.getProperty ("presets", {}).getArray();
        for (int i = arr.size() - 1; i >= 0; --i) { const auto pp = gx::Preset::fromVar (arr[i]); if ((int) pp.get (gx::P::piano) >= 6) arr.remove (i); }
        struct Want { int piano; const char* cat; const char* name; };
        const Want wants[] { { 6, "Lo-Fi", "Rhodes Late Night" }, { 6, "Lo-Fi", "Rhodes Dusty Tape" }, { 6, "Study", "Rhodes Quiet Desk" }, { 6, "Cinematic", "Rhodes Slow Film" }, { 6, "Rainy", "Rhodes Window Rain" }, { 6, "Vintage", "Rhodes Old Club" },
                             { 7, "Lullaby", "Vibraphone Moon" }, { 7, "Study", "Vibraphone Calm" }, { 7, "Cinematic", "Vibraphone Glass Hall" }, { 7, "Lo-Fi", "Vibraphone Smoky Room" }, { 7, "Rainy", "Vibraphone After Rain" }, { 7, "Concert", "Vibraphone Stage" } };
        int added = 0;
        for (auto& w : wants)
        {
            bool ok = false; gx::Preset p; float rms = 0;
            for (int attempt = 0; attempt < 30 && ! ok; ++attempt)
            {
                gx::GenRequest rq; rq.category = w.cat; rq.sub = attempt % 3; rq.seed = (uint32_t) (5000 + 31 * added + 977 * attempt);
                p = gx::Generator::candidate (rq);
                p.params[(size_t) gx::P::piano] = (float) w.piano;
                p.params[(size_t) gx::P::hammer] = juce::jlimit (0.2f, 0.6f, p.params[(size_t) gx::P::hammer]);
                p.params[(size_t) gx::P::resonance] = 0.15f;
                if (! gx::Generator::validate (p, &rms)) continue;
                if (presetIsOrganLike (p)) continue;
                ok = true;
            }
            if (! ok) { std::puts (("skipped " + juce::String (w.name)).toRawUTF8()); continue; }
            p.meta.name = w.name; p.meta.category = w.cat;
            arr.add (p.toVar()); ++added;
            std::puts ((juce::String (w.name) + "  rms " + juce::String (rms, 1) + " dB").toRawUTF8());
        }
        auto* nb = new juce::DynamicObject();
        nb->setProperty ("format", "memoriabank"); nb->setProperty ("version", 1); nb->setProperty ("presets", arr);
        f.replaceWithText (juce::JSON::toString (juce::var (nb), true));
        std::puts (("bank now has " + juce::String (arr.size()) + " presets").toRawUTF8());
        return 0;
    }
    if (cmd == "hands")
    {
        // MemoriaTool hands <dir> [seconds]: every song style, for listening (WAV + MIDI)
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "hands-demo");
        dir.createDirectory();
        const double secs = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 90.0;
        const char* cats[] { "Lo-Fi", "Cinematic", "Concert", "Study", "Lullaby", "Felt", "Concert", "Vintage", "Lo-Fi", "Church" };
        const double tempos[] { 78, 84, 70, 92, 96, 66, 84, 78, 76, 62 };
        const int keys[] { 9, 2, 3, 0, 7, 4, 2, 5, 0, 3 }, scales[] { 1, 1, 0, 2, 0, 1, 0, 0, 0, 1 };
        const int takes = argc > 4 ? juce::String (argv[4]).getIntValue() : 1;     // several different songs per style
        for (int style = 0; style < gx::numSongStyles; ++style)
          for (int take = 0; take < takes; ++take)
        {
            gx::GenRequest rq; rq.category = cats[style]; rq.seed = 77 + (uint32_t) style;
            auto p = gx::Generator::generate (rq);
            auto& q = p.params;
            q[(size_t) gx::P::hands] = 1; q[(size_t) gx::P::handsStyle] = (float) style; q[(size_t) gx::P::handsTempo] = (float) tempos[style];
            q[(size_t) gx::P::handsKey] = (float) keys[style]; q[(size_t) gx::P::handsScale] = (float) scales[style]; q[(size_t) gx::P::handsBars] = 3; q[(size_t) gx::P::handsSeed] = (float) (10 + style + 37 * take);
            const auto base = gx::handsStyleNames()[style].retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-") + (takes > 1 ? " take " + juce::String (take + 1) : juce::String()) + " - " + p.meta.name;
            const auto err = gx::exportAudio (p, dir.getChildFile (base + ".wav"), gx::efWav, secs, 48000.0);
            gx::Score sc; gx::compose (gx::composeCfgFromPreset (p), sc);
            gx::writeMidi (sc, tempos[style], dir.getChildFile (base + ".mid"));
            std::puts ((err.isEmpty() ? base : err).toRawUTF8());
        }
        return 0;
    }
    if (cmd == "video" || cmd == "trailer") return runVideo (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "dist"), cmd == "trailer");
    if (cmd == "icon")
    {
        // MemoriaTool icon <dir> : branding/memoria-icon.png (512), memoria-icon-small.png (128) and memoria.ico
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "branding");
        dir.createDirectory();
        auto make = [] (int size)
        {
            juce::Image img (juce::Image::ARGB, size, size, true);
            juce::Graphics g (img);
            const auto r = juce::Rectangle<float> ((float) size, (float) size).reduced (size * 0.03f);
            g.setGradientFill (juce::ColourGradient (gx::ui::col::panelHi, r.getX(), r.getY(), gx::ui::col::bgDeep, r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, size * 0.2f);
            g.setColour (gx::ui::col::accent.withAlpha (0.5f));
            g.drawRoundedRectangle (r, size * 0.2f, juce::jmax (1.0f, size * 0.012f));
            gx::ui::Logo::drawMark (g, r.reduced (size * 0.12f), 0.8f, 0.35f);
            return img;
        };
        auto pngBytes = [] (const juce::Image& img) { juce::MemoryOutputStream m; juce::PNGImageFormat().writeImageToStream (img, m); return m.getMemoryBlock(); };
        auto save = [&] (const juce::String& name, const juce::MemoryBlock& b) { const auto f = dir.getChildFile (name); f.deleteFile(); f.replaceWithData (b.getData(), b.getSize()); };
        save ("memoria-icon.png", pngBytes (make (512)));
        save ("memoria-icon-small.png", pngBytes (make (128)));
        const int sizes[] { 256, 64, 48, 32, 16 };
        juce::MemoryOutputStream ico;
        ico.writeShort (0); ico.writeShort (1); ico.writeShort ((short) 5);
        std::vector<juce::MemoryBlock> data;
        for (int s : sizes) data.push_back (pngBytes (make (s)));
        int offset = 6 + 16 * 5;
        for (size_t i = 0; i < 5; ++i)
        {
            ico.writeByte ((char) (sizes[i] == 256 ? 0 : sizes[i])); ico.writeByte ((char) (sizes[i] == 256 ? 0 : sizes[i]));
            ico.writeByte (0); ico.writeByte (0); ico.writeShort (1); ico.writeShort (32);
            ico.writeInt ((int) data[i].getSize()); ico.writeInt (offset);
            offset += (int) data[i].getSize();
        }
        for (auto& d : data) ico.write (d.getData(), d.getSize());
        save ("memoria.ico", ico.getMemoryBlock());
        std::puts (("icons written to " + dir.getFullPathName()).toRawUTF8());
        return 0;
    }
    if (cmd == "rain")
    {
        // MemoriaTool rain <dir>: three rain versions, alone and over a piano, for listening
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "rain");
        dir.createDirectory();
        const char* names[] { "A_drizzle", "B_steady", "C_heavy" };
        for (int v = 0; v < 3; ++v)
        {
            gx::rainVariant() = v;
            gx::Preset p; gxtest::setParam (p, "world", 1.0f); gxtest::setParam (p, "world_level", 0.7f); gxtest::setParam (p, "wood", 0.3f);
            juce::MidiBuffer none;
            juce::AudioBuffer<float> a (2, 48000 * 20);
            a.clear(); gx::renderMidi (p, none, a, 48000.0, 512);
            gxtest::writeWav (a, dir.getChildFile (juce::String (names[v]) + "_rain_alone.wav"));
            gxtest::Song s; s.pedal (0.0, 127);
            const int ch[4][3] { { 45, 57, 64 }, { 41, 53, 60 }, { 36, 55, 64 }, { 43, 55, 62 } };
            for (int c = 0; c < 4; ++c) for (int n : ch[c]) s.note (0.5 + c * 6.0, 5.5, n + 12 * (c % 2), 62);
            for (int i = 0; i < 12; ++i) s.note (1.0 + i * 1.9, 1.6, 72 + (i * 5) % 9, 70);
            juce::AudioBuffer<float> b (2, 48000 * 26);
            b.clear(); gx::renderMidi (p, s.buf, b, 48000.0, 512);
            gxtest::writeWav (b, dir.getChildFile (juce::String (names[v]) + "_rain_with_piano.wav"));
            std::printf ("%s: rain alone %.1f dB RMS%c", names[v], gxtest::rmsDb (a, 2.0, 20.0), 10);
        }
        return 0;
    }
    if (cmd == "scene")
    {
        // MemoriaTool scene <dir>: the GPU room for review (several worlds and memory levels)
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "scene");
        if (argc > 3) gx::ui::gpuForceTier() = juce::String (argv[3]).getIntValue();
        dir.createDirectory();
        struct C { const char* n; float mem, age; int world; } cs[] { { "golden_clean", 0.0f, 0.5f, 0 }, { "rain", 0.3f, 0.5f, 1 }, { "cafe", 0.2f, 0.5f, 2 }, { "fire_night", 0.5f, 0.6f, 3 }, { "dawn_old", 1.0f, 0.9f, 4 }, { "audience", 0.0f, 0.5f, 5 } };
        for (auto& c : cs)
        {
            gx::ui::RoomUniforms u; u.time = 12.3f; u.memory = c.mem; u.age = c.age; u.world = (float) c.world; u.energy = 0.3f; u.pedal = 1.0f; u.scale = 1.0f; for (int k : { 27, 31, 34, 39 }) u.keys[k] = 0.9f;
            auto img = gx::ui::OffscreenRoom::get().render (u, 1056, 660);
            if (img.isNull()) { std::puts ("GPU render failed"); return 1; }
            juce::PNGImageFormat png;
            const auto f = dir.getChildFile (juce::String (c.n) + ".png");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (os != nullptr) png.writeImageToStream (img, *os);
        }
        std::puts (("scenes in " + dir.getFullPathName() + "   [" + gx::ui::getGpuInfo() + "]").toRawUTF8());
        return 0;
    }
    if (cmd == "audit")
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource ("factory_memoriabank", size);
        const juce::var bank = data ? juce::JSON::parse (juce::String::fromUTF8 (data, size)) : juce::var();
        auto* arr = bank.getProperty ("presets", {}).getArray();
        if (arr == nullptr) return 1;
        int flagged = 0;
        for (auto& v : *arr)
        {
            const auto p = gx::Preset::fromVar (v);
            juce::String info;
            const bool bad = presetIsOrganLike (p, &info);
            if (bad) { ++flagged; std::puts (("ORGAN-LIKE " + p.meta.category + " / " + p.meta.name + " (piano " + juce::String ((int) p.params[(size_t) gx::P::piano]) + "): " + info).toRawUTF8()); }
        }
        std::puts (("flagged " + juce::String (flagged) + " of " + juce::String (arr->size())).toRawUTF8());
        return 0;
    }
    if (cmd == "gainstage")
    {
        // MemoriaTool gainstage: where does the signal get hot? Default sound, then every factory preset (ff chord with pedal), then every Hands style
        gxtest::Song chord; chord.pedal (0.0, 127);
        for (int n : { 45, 52, 57, 64, 69, 72 }) chord.note (0.0, 3.0, n, 100);
        for (int n : { 36, 48, 55, 60, 67, 76 }) chord.note (3.0, 3.0, n, 118);
        std::puts (("default sound, 12-note ff chords with pedal:  " + probeLine (probeRender (gx::Preset(), chord, 8.0, false))).toRawUTF8());
        int size = 0;
        const char* data = BinaryData::getNamedResource ("factory_memoriabank", size);
        const juce::var bank = data ? juce::JSON::parse (juce::String::fromUTF8 (data, size)) : juce::var();
        if (auto* arr = bank.getProperty ("presets", {}).getArray())
        {
            float worstVoices = -99, worstTape = -99; int worstLim = 0; double worstLimPct = 0; juce::String worstName;
            for (auto& v : *arr)
            {
                const auto p = gx::Preset::fromVar (v);
                const auto q = probeRender (p, chord, 8.0, false);
                const float dv = juce::Decibels::gainToDecibels (q.voices, -99.0f), dt = juce::Decibels::gainToDecibels (q.tape, -99.0f);
                const double lp = 100.0 * (double) q.limN / (double) juce::jmax ((int64_t) 1, q.n);
                worstVoices = juce::jmax (worstVoices, dv); worstTape = juce::jmax (worstTape, dt);
                if (lp > worstLimPct) { worstLimPct = lp; worstName = p.meta.name; }
                (void) worstLim;
            }
            std::puts (("factory bank (" + juce::String (arr->size()) + "): hottest voice bus " + juce::String (worstVoices, 1) + " dBFS, hottest tape input " + juce::String (worstTape, 1) + " dBFS, most limiting: " + worstName + " " + juce::String (worstLimPct, 2) + " % of samples").toRawUTF8());
        }
        for (int style = 0; style < gx::numSongStyles; ++style)
        {
            gx::Preset p; p.params[(size_t) gx::P::hands] = 1; p.params[(size_t) gx::P::handsStyle] = (float) style; p.params[(size_t) gx::P::handsTempo] = 90; p.params[(size_t) gx::P::handsBars] = 1;
            std::puts (("hands " + gx::handsStyleNames()[style] + ": " + probeLine (probeRender (p, gxtest::Song(), 24.0, true))).toRawUTF8());
        }
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
        const char* names[] { "piano", "presets", "export", "settings", "help" };
        for (int page = 0; page < 5; ++page)
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
            if (page == 0) { auto& rs = ed->roomScene(); rs.setExternalBackground (gx::ui::OffscreenRoom::get().render (rs.uniforms(), rs.getWidth(), rs.getHeight())); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            const auto f = dir.getChildFile (juce::String (names[page]) + ".png");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (os != nullptr) png.writeImageToStream (img, *os);
        }
        // the Memory macro and the World layer change the picture: a few combinations of the piano page
        struct Combo { const char* name; float memory; int world; } combos[] { { "scene_clean_golden", 0.0f, 0 }, { "scene_rain_half", 0.5f, 1 }, { "scene_cafe", 0.2f, 2 }, { "scene_fire_old", 1.0f, 3 }, { "scene_dawn_old", 0.8f, 4 } };
        for (auto& c : combos)
        {
            gxtest::setParam (proc, "memory", c.memory);
            gxtest::setParam (proc, "world", (float) c.world);
            ed->showPage (0);
            for (int f = 0; f < 60; ++f) { midi.clear(); buf.clear(); proc.processBlock (buf, midi); ed->advanceFrames (1.0 / 24.0); }
            { auto& rs = ed->roomScene(); rs.setExternalBackground (gx::ui::OffscreenRoom::get().render (rs.uniforms(), rs.getWidth(), rs.getHeight())); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            const auto f = dir.getChildFile (juce::String (c.name) + ".png");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (os != nullptr) png.writeImageToStream (img, *os);
        }
        base.reset();        std::printf ("screenshots in %s\n", dir.getFullPathName().toRawUTF8());
        return 0;
    }
    std::fprintf (stderr, "usage: MemoriaTool test | piano <dir> | screenshot <dir> [en|es]\n");
    return 2;
}
