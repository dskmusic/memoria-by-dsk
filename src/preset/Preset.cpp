#include "Preset.h"
#include "../PluginProcessor.h"
#include "../Lang.h"

namespace gx
{
const juce::StringArray& categories()
{
    static const juce::StringArray c { "Lo-Fi", "Felt", "Cinematic", "Rainy", "Vintage", "Church", "Lullaby", "Study", "Concert", "Toy" };
    return c;
}
const juce::StringArray& subcategories (const juce::String& cat)
{
    static const std::map<juce::String, juce::StringArray> m {
        { "Lo-Fi", { "Tape Dream", "Dusty Vinyl", "Bedroom" } },
        { "Felt", { "Soft", "Intimate", "Muted" } },
        { "Cinematic", { "Emotional", "Hopeful", "Tension" } },
        { "Rainy", { "Window", "Windy", "Night" } },
        { "Vintage", { "Old Upright", "Honky-Tonk", "Attic Tape" } },
        { "Church", { "Nave", "Chapel", "Cathedral" } },
        { "Lullaby", { "Music Box", "Dreaming", "Goodnight" } },
        { "Study", { "Focus", "Calm", "Morning" } },
        { "Concert", { "Hall", "Studio", "Close Mic" } },
        { "Toy", { "Toy Piano", "Celesta", "Bells" } },
    };
    static const juce::StringArray none;
    auto it = m.find (cat);
    return it == m.end() ? none : it->second;
}
juce::String categoryLabel (const juce::String& c)
{
    if (c == "Cinematic") return L ("Cine", "Cinematic");
    if (c == "Rainy") return L ("Lluvia", "Rainy");
    if (c == "Church") return L ("Iglesia", "Church");
    if (c == "Lullaby") return L ("Nana", "Lullaby");
    if (c == "Study") return L ("Estudio", "Study");
    if (c == "Concert") return L ("Concierto", "Concert");
    if (c == "Toy") return L ("Juguete", "Toy");
    return c;
}
const juce::StringArray& characterTags()
{
    static const juce::StringArray t { "Warm", "Dark", "Bright", "Soft", "Intimate", "Wide", "Dreamy", "Nostalgic", "Sad", "Hopeful",
                                       "Tape", "Vinyl", "Rain", "Aged", "Clean", "Dusty", "Detuned", "Felt", "Bell", "Lo-Fi" };
    return t;
}

Preset::Preset()
{
    for (auto& d : paramDefs()) params.push_back (d.def);
    meta.uuid = juce::Uuid().toString();
    meta.created = meta.modified = juce::Time::getCurrentTime();
}

juce::var Preset::toVar (bool includeMeta) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "memoria");
    root->setProperty ("version", kPresetFormatVersion);
    root->setProperty ("app", MEMORIA_VERSION);
    if (includeMeta)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("uuid", meta.uuid);
        m->setProperty ("name", meta.name);
        m->setProperty ("category", meta.category);
        m->setProperty ("subcategory", meta.subcategory);
        m->setProperty ("author", meta.author);
        m->setProperty ("notes", meta.notes);
        juce::Array<juce::var> tags;
        for (auto& t : meta.tags) tags.add (t);
        m->setProperty ("tags", tags);
        m->setProperty ("favorite", meta.favorite);
        m->setProperty ("rating", meta.rating);
        m->setProperty ("created", meta.created.toISO8601 (true));
        m->setProperty ("modified", meta.modified.toISO8601 (true));
        root->setProperty ("meta", juce::var (m));
    }
    const auto& defs = paramDefs();
    auto* prm = new juce::DynamicObject();
    for (size_t i = 0; i < defs.size(); ++i)
        prm->setProperty (defs[i].id, defs[i].kind == pkFloat ? juce::var (params[i]) : juce::var ((int) std::lround (params[i])));
    root->setProperty ("params", juce::var (prm));
    return juce::var (root);
}

Preset Preset::fromVar (const juce::var& v)
{
    Preset p;
    if (auto* m = v.getProperty ("meta", {}).getDynamicObject())
    {
        p.meta.uuid = m->getProperty ("uuid").toString();
        if (p.meta.uuid.isEmpty()) p.meta.uuid = juce::Uuid().toString();
        p.meta.name = m->getProperty ("name").toString();
        p.meta.category = m->getProperty ("category").toString();
        p.meta.subcategory = m->getProperty ("subcategory").toString();
        p.meta.author = m->getProperty ("author").toString();
        p.meta.notes = m->getProperty ("notes").toString();
        if (auto* t = m->getProperty ("tags").getArray()) for (auto& x : *t) p.meta.tags.add (x.toString());
        p.meta.favorite = (bool) m->getProperty ("favorite");
        p.meta.rating = (int) m->getProperty ("rating");
        p.meta.created = juce::Time::fromISO8601 (m->getProperty ("created").toString());
        p.meta.modified = juce::Time::fromISO8601 (m->getProperty ("modified").toString());
    }
    const auto& defs = paramDefs();
    if (auto* prm = v.getProperty ("params", {}).getDynamicObject())
        for (auto& nv : prm->getProperties())
        {
            const int i = paramIndex (nv.name.toString());
            if (i >= 0) p.params[(size_t) i] = defs[(size_t) i].range.snapToLegalValue ((float) (double) nv.value);
        }
    return p;
}

juce::String Preset::toJson() const { return juce::JSON::toString (toVar(), false); }

bool Preset::fromJson (const juce::String& json, Preset& out)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || v.getProperty ("format", "").toString() != "memoria") return false;
    out = fromVar (v);
    return true;
}

bool Preset::saveTo (const juce::File& f) const
{
    f.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp (f);
    if (! tmp.getFile().replaceWithText (toJson())) return false;
    return tmp.overwriteTargetFileWithTemporary();
}

bool Preset::loadFrom (const juce::File& f, Preset& out)
{
    if (! f.existsAsFile() || f.getSize() > 8 * 1024 * 1024) return false;
    if (! fromJson (f.loadFileAsString(), out)) return false;
    out.meta.file = f;
    return true;
}

Preset Preset::capture (MemoriaProcessor& proc)
{
    Preset p;
    for (int i = 0; i < P::numParams; ++i) p.params[(size_t) i] = proc.getParam (i);
    p.meta = proc.currentMeta;
    return p;
}

void Preset::applyTo (MemoriaProcessor& proc) const
{
    proc.beginPresetSwitch();
    for (int i = 0; i < P::numParams; ++i) proc.setParamPlain (i, params[(size_t) i]);
    proc.currentMeta = meta;
}

float Preset::similarity (const Preset& o) const
{
    const auto& defs = paramDefs();
    double diff = 0; int n = 0;
    for (size_t i = 0; i < defs.size(); ++i)
    {
        if (i == (size_t) P::seed || i == (size_t) P::master) continue;
        const float a = defs[i].range.convertTo0to1 (params[i]), b = defs[i].range.convertTo0to1 (o.params[i]);
        diff += std::abs (a - b) * (defs[i].kind == pkChoice || defs[i].kind == pkBool ? 2.0 : 1.0);
        ++n;
    }
    return (float) juce::jlimit (0.0, 1.0, 1.0 - 8.0 * diff / juce::jmax (1, n));
}
}
