#include "Library.h"

namespace gx
{
juce::File Library::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DSK Music").getChildFile ("Memoria");
}

Library::Library (juce::File r) : root (std::move (r))
{
    userDir().createDirectory();
    loadOverlay();
    rescanUser();
}

void Library::changed() { if (onChange) onChange(); }

void Library::loadOverlay()
{
    overlay.clear();
    juce::var v;
    if (juce::JSON::parse (root.getChildFile ("library.json").loadFileAsString(), v).wasOk())
        if (auto* o = v.getProperty ("presets", {}).getDynamicObject())
            for (auto& nv : o->getProperties())
                overlay[nv.name.toString()] = { (bool) nv.value.getProperty ("fav", false), (int) nv.value.getProperty ("rating", 0), (int) nv.value.getProperty ("uses", 0) };
}

void Library::saveOverlay() const
{
    auto* o = new juce::DynamicObject();
    for (auto& [k, ov] : overlay)
    {
        if (! ov.favorite && ov.rating == 0 && ov.uses == 0) continue;
        auto* e = new juce::DynamicObject();
        e->setProperty ("fav", ov.favorite); e->setProperty ("rating", ov.rating); e->setProperty ("uses", ov.uses);
        o->setProperty (k, juce::var (e));
    }
    auto* r = new juce::DynamicObject();
    r->setProperty ("version", 1);
    r->setProperty ("presets", juce::var (o));
    root.createDirectory();
    root.getChildFile ("library.json").replaceWithText (juce::JSON::toString (juce::var (r)));
}

void Library::applyOverlay (PresetMeta& m) const
{
    auto it = overlay.find (m.uuid);
    if (it == overlay.end()) return;
    m.favorite = it->second.favorite || m.favorite;
    m.rating = juce::jmax (m.rating, it->second.rating);
    m.uses = it->second.uses;
}

void Library::setFactoryBank (const juce::String& bankJson)
{
    factoryData.clear();
    juce::var v;
    if (juce::JSON::parse (bankJson, v).wasOk())
        if (auto* arr = v.getProperty ("presets", {}).getArray())
            for (auto& p : *arr) factoryData.push_back (p);
    rescanUser();
}

static PresetMeta metaFromVar (const juce::var& v)
{
    // parse meta only (cheap): reuse Preset::fromVar on a stripped copy
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("format", "memoria");
    o->setProperty ("meta", v.getProperty ("meta", {}));
    return Preset::fromVar (juce::var (o.get())).meta;
}

void Library::rescanUser()
{
    entries.clear();
    for (auto& f : factoryData)
    {
        auto m = metaFromVar (f);
        m.factory = true;
        applyOverlay (m);
        entries.push_back (m);
    }
    // cached index: path -> (mtime, meta)
    std::map<juce::String, std::pair<juce::int64, juce::var>> cache;
    juce::var idx;
    if (juce::JSON::parse (root.getChildFile ("index.json").loadFileAsString(), idx).wasOk())
        if (auto* arr = idx.getProperty ("entries", {}).getArray())
            for (auto& e : *arr) cache[e.getProperty ("path", "").toString()] = { (juce::int64) e.getProperty ("mtime", 0), e };

    for (const auto& entry : juce::RangedDirectoryIterator (userDir(), true, "*.memoria", juce::File::findFiles))
    {
        const auto f = entry.getFile();
        const auto rel = f.getRelativePathFrom (userDir());
        PresetMeta m;
        auto it = cache.find (rel);
        if (it != cache.end() && it->second.first == f.getLastModificationTime().toMilliseconds())
            m = metaFromVar (it->second.second);
        else
        {
            juce::var v;
            if (juce::JSON::parse (f.loadFileAsString(), v).failed() || v.getProperty ("format", "") != "memoria") continue;
            m = metaFromVar (v);
        }
        m.file = f;
        m.factory = false;
        m.folder = f.getParentDirectory() == userDir() ? juce::String() : f.getParentDirectory().getRelativePathFrom (userDir()).replaceCharacter ('\\', '/');
        if (m.name.isEmpty()) m.name = f.getFileNameWithoutExtension();
        applyOverlay (m);
        entries.push_back (m);
    }
    saveIndex();
}

void Library::saveIndex() const
{
    juce::Array<juce::var> arr;
    for (auto& m : entries)
    {
        if (m.factory) continue;
        Preset tmp; tmp.meta = m;
        auto full = tmp.toVar();
        auto* e = new juce::DynamicObject();
        e->setProperty ("path", m.file.getRelativePathFrom (userDir()));
        e->setProperty ("mtime", m.file.getLastModificationTime().toMilliseconds());
        e->setProperty ("meta", full.getProperty ("meta", {}));
        arr.add (juce::var (e));
    }
    auto* r = new juce::DynamicObject();
    r->setProperty ("version", 1);
    r->setProperty ("entries", arr);
    root.getChildFile ("index.json").replaceWithText (juce::JSON::toString (juce::var (r), true));
}

std::vector<int> Library::query (const PresetFilter& f) const
{
    std::vector<int> out;
    const auto words = juce::StringArray::fromTokens (f.text.toLowerCase(), " ", "\"");
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& m = entries[(size_t) i];
        if (f.type == PresetFilter::Factory && ! m.factory) continue;
        if (f.type == PresetFilter::User && m.factory) continue;
        if (f.favoritesOnly && ! m.favorite) continue;
        if (f.category.isNotEmpty() && m.category != f.category) continue;
        if (f.subcategory.isNotEmpty() && m.subcategory != f.subcategory) continue;
        if (f.author.isNotEmpty() && m.author != f.author) continue;
        if (f.folder.isNotEmpty() && m.folder != f.folder) continue;
        bool tagsOk = true;
        for (auto& t : f.tags) tagsOk &= m.tags.contains (t, true);
        if (! tagsOk) continue;
        if (! words.isEmpty())
        {
            const auto hay = (m.name + " " + m.category + " " + m.subcategory + " " + m.author + " " + m.notes + " " + m.tags.joinIntoString (" ")).toLowerCase();
            bool all = true;
            for (auto& w : words) if (w.isNotEmpty()) all &= hay.contains (w);
            if (! all) continue;
        }
        out.push_back (i);
    }
    auto key = [this, &f] (int a, int b)
    {
        const auto& A = entries[(size_t) a]; const auto& B = entries[(size_t) b];
        int c = 0;
        switch (f.sort)
        {
            case PresetFilter::ByDate: c = A.modified < B.modified ? -1 : (A.modified > B.modified ? 1 : 0); break;
            case PresetFilter::ByCategory: c = A.category.compareIgnoreCase (B.category); if (c == 0) c = A.subcategory.compareIgnoreCase (B.subcategory); break;
            case PresetFilter::ByRating: c = A.rating - B.rating; break;
            case PresetFilter::ByUses: c = A.uses - B.uses; break;
            default: break;
        }
        if (c == 0) c = A.name.compareNatural (B.name);
        return f.descending ? c > 0 : c < 0;
    };
    std::stable_sort (out.begin(), out.end(), key);
    return out;
}

bool Library::load (int index, Preset& out) const
{
    if (index < 0 || index >= (int) entries.size()) return false;
    const auto& m = entries[(size_t) index];
    bool ok;
    if (m.factory)
    {
        int fi = 0;
        for (int i = 0; i < index; ++i) fi += entries[(size_t) i].factory ? 1 : 0;
        ok = fi < (int) factoryData.size();
        if (ok) out = Preset::fromVar (factoryData[(size_t) fi]);
    }
    else ok = Preset::loadFrom (m.file, out);
    if (ok) { auto keepFile = out.meta.file; out.meta = m; if (keepFile != juce::File()) out.meta.file = keepFile; }
    return ok;
}

int Library::indexOfUuid (const juce::String& uuid) const
{
    for (int i = 0; i < (int) entries.size(); ++i) if (entries[(size_t) i].uuid == uuid) return i;
    return -1;
}

juce::StringArray Library::authors() const
{
    juce::StringArray a;
    for (auto& m : entries) a.addIfNotAlreadyThere (m.author);
    a.sortNatural();
    return a;
}

juce::StringArray Library::userFolders() const
{
    juce::StringArray a;
    for (const auto& e : juce::RangedDirectoryIterator (userDir(), true, "*", juce::File::findDirectories))
        a.add (e.getFile().getRelativePathFrom (userDir()).replaceCharacter ('\\', '/'));
    a.sortNatural();
    return a;
}

juce::File Library::uniqueFileFor (const juce::String& name, const juce::String& folder) const
{
    auto dir = folder.isEmpty() ? userDir() : userDir().getChildFile (folder);
    const auto safe = juce::File::createLegalFileName (name.isEmpty() ? "Untitled" : name);
    return dir.getNonexistentChildFile (safe, ".memoria", false);
}

bool Library::saveUser (Preset& p, const juce::String& folder)
{
    p.meta.modified = juce::Time::getCurrentTime();
    if (p.meta.factory || p.meta.uuid.isEmpty()) { p.meta.uuid = juce::Uuid().toString(); p.meta.created = p.meta.modified; }
    p.meta.factory = false;
    const int existing = indexOfUuid (p.meta.uuid);
    juce::File f = (existing >= 0 && ! entries[(size_t) existing].factory) ? entries[(size_t) existing].file : uniqueFileFor (p.meta.name, folder);
    if (! p.saveTo (f)) return false;
    p.meta.file = f;
    rescanUser();
    changed();
    return true;
}

bool Library::rename (int index, const juce::String& newName)
{
    Preset p;
    if (! load (index, p) || entries[(size_t) index].factory || newName.trim().isEmpty()) return false;
    p.meta.name = newName.trim();
    const auto oldFile = entries[(size_t) index].file;
    const auto nf = uniqueFileFor (p.meta.name, entries[(size_t) index].folder);
    if (! p.saveTo (nf)) return false;
    if (oldFile != nf) oldFile.deleteFile();
    rescanUser();
    changed();
    return true;
}

bool Library::duplicate (int index)
{
    Preset p;
    if (! load (index, p)) return false;
    const auto folder = entries[(size_t) index].factory ? juce::String() : entries[(size_t) index].folder;
    p.meta.uuid = {};
    p.meta.name << " Copy";
    p.meta.favorite = false; p.meta.rating = 0; p.meta.uses = 0;
    p.meta.file = juce::File();
    p.meta.factory = false;
    return saveUser (p, folder);
}

bool Library::remove (int index)
{
    if (index < 0 || index >= (int) entries.size() || entries[(size_t) index].factory) return false;
    entries[(size_t) index].file.moveToTrash() || entries[(size_t) index].file.deleteFile();
    overlay.erase (entries[(size_t) index].uuid);
    saveOverlay();
    rescanUser();
    changed();
    return true;
}

bool Library::updateMeta (int index, const PresetMeta& m)
{
    if (index < 0 || index >= (int) entries.size()) return false;
    Preset p;
    if (! load (index, p)) return false;
    const bool wasFactory = entries[(size_t) index].factory;
    const auto oldFile = entries[(size_t) index].file;
    const auto oldFolder = entries[(size_t) index].folder;
    p.meta.name = m.name; p.meta.category = m.category; p.meta.subcategory = m.subcategory;
    p.meta.tags = m.tags; p.meta.author = m.author; p.meta.notes = m.notes;
    p.meta.favorite = m.favorite; p.meta.rating = m.rating;
    if (wasFactory) { p.meta.uuid = {}; return saveUser (p, m.folder); } // factory edits become a user copy
    if (m.folder != oldFolder)
    {
        const auto nf = uniqueFileFor (p.meta.name, m.folder);
        if (! p.saveTo (nf)) return false;
        oldFile.deleteFile();
        rescanUser(); changed();
        return true;
    }
    p.meta.modified = juce::Time::getCurrentTime();
    if (! p.saveTo (oldFile)) return false;
    rescanUser(); changed();
    return true;
}

void Library::setFavorite (int index, bool fav)
{
    if (index < 0 || index >= (int) entries.size()) return;
    entries[(size_t) index].favorite = fav;
    overlay[entries[(size_t) index].uuid].favorite = fav;
    saveOverlay(); changed();
}

void Library::setRating (int index, int stars)
{
    if (index < 0 || index >= (int) entries.size()) return;
    entries[(size_t) index].rating = juce::jlimit (0, 5, stars);
    overlay[entries[(size_t) index].uuid].rating = entries[(size_t) index].rating;
    saveOverlay(); changed();
}

void Library::markUsed (int index)
{
    if (index < 0 || index >= (int) entries.size()) return;
    entries[(size_t) index].uses++;
    overlay[entries[(size_t) index].uuid].uses = entries[(size_t) index].uses;
    saveOverlay();
}

bool Library::createFolder (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    const bool ok = userDir().getChildFile (clean).createDirectory().wasOk();
    changed();
    return ok;
}

int Library::importFile (const juce::File& f)
{
    juce::var v;
    if (f.getSize() > 256 * 1024 * 1024 || juce::JSON::parse (f.loadFileAsString(), v).failed()) return 0;
    int count = 0;
    auto importOne = [&] (const juce::var& pv, const juce::String& folder)
    {
        if (pv.getProperty ("format", "") != "memoria") return;
        Preset p = Preset::fromVar (pv);
        if (indexOfUuid (p.meta.uuid) >= 0) p.meta.uuid = {}; // keep both
        p.meta.file = juce::File();
        p.meta.factory = false;
        p.meta.modified = juce::Time::getCurrentTime();
        p.meta.factory = false;
        const auto fl = uniqueFileFor (p.meta.name, folder);
        if (p.saveTo (fl)) ++count;
    };
    if (auto* arr = v.getProperty ("presets", {}).getArray())
    {
        const auto folder = juce::File::createLegalFileName (v.getProperty ("name", f.getFileNameWithoutExtension()).toString());
        userDir().getChildFile (folder).createDirectory();
        for (auto& pv : *arr) importOne (pv, folder);
    }
    else importOne (v, {});
    rescanUser();
    changed();
    return count;
}

bool Library::exportPreset (int index, const juce::File& f) const
{
    Preset p;
    return load (index, p) && p.saveTo (f);
}

bool Library::exportBank (const std::vector<int>& indices, const juce::File& f) const
{
    juce::Array<juce::var> arr;
    for (int i : indices) { Preset p; if (load (i, p)) arr.add (p.toVar()); }
    auto* r = new juce::DynamicObject();
    r->setProperty ("format", "memoriabank");
    r->setProperty ("version", kPresetFormatVersion);
    r->setProperty ("name", f.getFileNameWithoutExtension());
    r->setProperty ("presets", arr);
    return f.replaceWithText (juce::JSON::toString (juce::var (r), true));
}
}
