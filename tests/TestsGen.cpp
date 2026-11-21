#include "Tests.h"
#include "gen/Generator.h"
#include "gen/Messages.h"
#include <set>
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define GX_HAS_BINARYDATA 1
#endif

using namespace gxtest;

static void testGenerator()
{
    std::puts ("[generator]");
    std::set<juce::String> names;
    for (auto& mood : gx::categories())
    {
        gx::GenRequest r;
        r.category = mood;
        r.seed = 1234;
        gx::Validation v;
        int attempts = 0;
        auto p = gx::Generator::generate (r, &attempts, &v);
        std::printf ("    %-10s %-18s %-11s rms %.1f dB peak %.1f dB (%d attempt%s)\n", mood.toRawUTF8(), p.meta.name.toRawUTF8(), p.meta.subcategory.toRawUTF8(),
                     v.rmsDb, v.peakDb, attempts, attempts == 1 ? "" : "s");
        check (v.ok, "generates a valid " + mood + " preset (" + v.reason + ")");
        check (std::abs (v.rmsDb - gx::Generator::targetRmsDb (mood)) < 1.0f || v.peakDb > -3.0f, "level matched: " + mood);
        check (p.meta.category == mood && gx::subcategories (mood).contains (p.meta.subcategory), "mood + sub-mood set");
        check (p.meta.name.isNotEmpty() && ! p.meta.tags.isEmpty(), "name and tags");
        names.insert (p.meta.name);
        check (gx::numMessagesFor (mood, true) >= 24 && gx::numMessagesFor (mood, false) >= 24, "AI messages for " + mood);
    }
    // determinism: same request -> same preset parameters
    gx::GenRequest r; r.category = "Dark"; r.seed = 99;
    const auto a = gx::Generator::candidate (r, 99), b = gx::Generator::candidate (r, 99);
    check (a.similarity (b) > 0.999f && a.get (gx::P::seed) == b.get (gx::P::seed), "candidate is deterministic");

    // locks keep a section, mutate/blend stay valid
    auto base = gx::Generator::candidate (r, 5);
    gx::GenRequest lr; lr.category = "Sci-Fi"; lr.locks = { "harmony" }; lr.base = &base;
    const auto locked = gx::Generator::candidate (lr, 777);
    check (locked.get (gx::P::scale) == base.get (gx::P::scale) && locked.get (gx::P::key) == base.get (gx::P::key), "harmony lock");
    auto m = gx::Generator::mutate (base, 0.35f, 42);
    check (gx::Generator::validate (m).ok, "mutation validates");
    auto bl = gx::Generator::blend (base, locked, 0.5f, 7);
    check (gx::Generator::validate (bl).ok, "blend validates");
}

// Factory bank: parses, moods and names are sane, and a sample is re-validated (catches DSP regressions).
static void testFactoryBank()
{
    std::puts ("[factory bank]");
#if GX_HAS_BINARYDATA
    int size = 0;
    const char* data = BinaryData::getNamedResource ("factory_memoriabank", size);
    check (data != nullptr, "factory bank embedded");
    if (data == nullptr) return;
    juce::var v;
    check (juce::JSON::parse (juce::String::fromUTF8 (data, size), v).wasOk(), "factory bank parses");
    auto* arr = v.getProperty ("presets", {}).getArray();
    check (arr != nullptr && arr->size() >= 60, "at least 60 factory presets");
    if (arr == nullptr) return;
    std::set<juce::String> names;
    std::map<juce::String, int> perMood;
    int bad = 0, i = 0;
    for (auto& pv : *arr)
    {
        auto p = gx::Preset::fromVar (pv);
        names.insert (p.meta.name);
        perMood[p.meta.category]++;
        if (! gx::categories().contains (p.meta.category)) ++bad;
        if (i++ % 12 == 0)
        {
            const auto r = gx::Generator::validate (p, false);
            if (! r.ok || r.peakDb > 0.0f) { ++bad; std::printf ("    %s: %s peak %.1f\n", p.meta.name.toRawUTF8(), r.reason.toRawUTF8(), r.peakDb); }
        }
    }
    check ((int) names.size() == arr->size(), "unique names");
    for (auto& mood : gx::categories()) check (perMood[mood] >= 10, "at least 10 presets: " + mood);
    check (bad == 0, "factory presets valid");
#endif
}

static const bool registered = []
{
    extraTests().push_back (testGenerator);
    extraTests().push_back (testFactoryBank);
    return true;
}();
