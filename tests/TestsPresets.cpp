#include "Tests.h"
#include "preset/Library.h"

using namespace gxtest;

static void testPresets()
{
    std::puts ("[presets]");
    MemoriaProcessor p;
    setParam (p, "piano", 3);
    setParam (p, "tone", 0.8f);
    setParam (p, "hammer", 0.25f);
    setParam (p, "seed", 4242);

    auto pr = gx::Preset::capture (p);
    pr.meta.name = "Round Trip"; pr.meta.tags = { "Dark", "Wide" };
    const auto json = pr.toJson();
    gx::Preset back;
    check (gx::Preset::fromJson (json, back), "json parses");
    bool same = true;
    const auto& defs = gx::paramDefs();
    for (size_t i = 0; i < defs.size(); ++i)
        if (std::abs (back.params[i] - pr.params[i]) > 1.0e-4f * juce::jmax (1.0f, std::abs (pr.params[i]))) { same = false; std::printf ("    differs: %s\n", defs[i].id.toRawUTF8()); }
    check (same, "all params round-trip");
    check (back.meta.name == "Round Trip" && back.meta.tags.contains ("Wide"), "meta round-trip");
    check (back.similarity (pr) > 0.999f, "similarity identical");
    check (gx::Preset().similarity (pr) < 0.999f, "similarity differs");

    // forward compatibility: unknown ids ignored, missing ids default
    juce::var v;
    juce::JSON::parse (json, v);
    v.getProperty ("params", {}).getDynamicObject()->setProperty ("future_param_xyz", 1.0);
    v.getProperty ("params", {}).getDynamicObject()->removeProperty ("tone");
    auto mig = gx::Preset::fromVar (v);
    check (std::abs (mig.params[(size_t) gx::paramIndex ("tone")] - defs[(size_t) gx::paramIndex ("tone")].def) < 1e-3f, "missing param -> default");

    MemoriaProcessor q;
    back.applyTo (q);
    check ((int) q.getParam (gx::paramIndex ("seed")) == 4242, "apply: params");
}

static void testLibrary()
{
    std::puts ("[library]");
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("memoria_lib_test_" + juce::String (juce::Random::getSystemRandom().nextInt()));
    dir.deleteRecursively();
    {
        gx::Library lib (dir);
        // factory bank of 3
        juce::Array<juce::var> arr;
        const char* names[] { "Deep Sub", "Glass Bell", "Warm Pad" };
        const char* cats[] { "Dark", "Ambient", "Cinematic" };
        for (int i = 0; i < 3; ++i) { gx::Preset p; p.meta.name = names[i]; p.meta.category = cats[i]; p.meta.tags = { i == 2 ? "Warm" : "Dark" }; arr.add (p.toVar()); }
        auto* bank = new juce::DynamicObject(); bank->setProperty ("presets", arr);
        lib.setFactoryBank (juce::JSON::toString (juce::var (bank)));
        check (lib.all().size() == 3, "factory loaded");

        gx::Preset u; u.meta.name = "My Lead"; u.meta.category = "Sci-Fi"; u.meta.tags = { "Bright" }; u.meta.author = "Tester";
        check (lib.saveUser (u), "save user");
        check (lib.all().size() == 4, "user added");
        gx::PresetFilter f; f.text = "lead tester";
        check (lib.query (f).size() == 1, "search text across fields");
        f = {}; f.category = "Cinematic";
        check (lib.query (f).size() == 1, "filter category");
        f = {}; f.tags = { "Dark" };
        check (lib.query (f).size() == 2, "filter tags");
        f = {}; f.type = gx::PresetFilter::User;
        check (lib.query (f).size() == 1, "filter type");
        const int ui = lib.query (f)[0];
        check (lib.rename (ui, "Renamed Lead"), "rename");
        f.text = "renamed";
        check (lib.query (f).size() == 1, "renamed searchable");
        const int ri = lib.query (f)[0];
        lib.setFavorite (ri, true);
        lib.setRating (ri, 4);
        check (lib.duplicate (ri), "duplicate");
        f = {}; f.type = gx::PresetFilter::User;
        check (lib.query (f).size() == 2, "duplicate added");
        f = {}; f.favoritesOnly = true;
        check (lib.query (f).size() == 1, "favorites");
        auto m = lib.all()[(size_t) lib.query (f)[0]];
        m.category = "Dark"; m.folder = "Moved";
        lib.createFolder ("Moved");
        check (lib.updateMeta (lib.query (f)[0], m), "move category + folder");
        f = {}; f.folder = "Moved";
        check (lib.query (f).size() == 1, "folder filter");
        f = {}; f.sort = gx::PresetFilter::ByName;
        const auto sorted = lib.query (f);
        check (lib.all()[(size_t) sorted.front()].name == "Deep Sub", "sort by name");
        // export bank / import
        auto bankFile = dir.getChildFile ("bank.memoriabank");
        check (lib.exportBank (sorted, bankFile), "export bank");
        check (lib.importFile (bankFile) == (int) sorted.size(), "import bank");
        gx::Preset loaded;
        check (lib.load (sorted.front(), loaded) && loaded.meta.name == "Deep Sub", "load factory");
        f = {}; f.type = gx::PresetFilter::User;
        const int before = (int) lib.query (f).size();
        check (lib.remove (lib.query (f)[0]), "delete");
        check ((int) lib.query (f).size() == before - 1, "deleted");
    }
    {
        gx::Library lib2 (dir); // reload from disk (index + overlay)
        gx::PresetFilter f; f.favoritesOnly = true;
        check (lib2.query (f).size() >= 1, "favorites persisted");
    }
    dir.deleteRecursively();
}

static const bool registered = []
{
    extraTests().push_back (testPresets);
    extraTests().push_back (testLibrary);
    return true;
}();
