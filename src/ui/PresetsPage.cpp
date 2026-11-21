#include "Pages.h"

namespace gx::ui
{
// Factory notes are stored in English; show the known factory sentences in Spanish when the UI is Spanish.
static juce::String localisedNotes (juce::String n)
{
    if (uiEnglish()) return n;
    const char* pairs[][2] {
        { "Press Play and let it evolve.", "Pulsa Play y deja que evolucione." },
        { "Tuned to A4 = 432 Hz.", "Afinado a La4 = 432 Hz." },
        { "Try the session timer for a meditation with a gentle fade-out.", "Prueba el temporizador para una meditación con fundido suave." },
        { "Fast evolution: try Freeze when you hear a moment you love.", "Evolución rápida: pulsa Congelar cuando oigas un momento que te guste." },
        { "Turn Evolution and Light to move the landscape.", "Gira Evolución y Luz para mover el paisaje." } };
    for (auto& p : pairs) n = n.replace (p[0], juce::String::fromUTF8 (p[1]));
    return n;
}

void PresetsPage::StringModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (sel) { g.setColour (col::accent.withAlpha (0.85f)); g.fillRoundedRectangle (1, 1, (float) w - 2, (float) h - 2, 3); }
    g.setColour (sel ? juce::Colours::black : col::text);
    g.setFont (font (13.0f, sel));
    g.drawText (items[row], 8, 0, w - 10, h, juce::Justification::centredLeft);
}

PresetsPage::PresetsPage (Context& c) : Page (c)
{
    catModel.items.add (L ("Todas", "All"));
    catModel.items.addArray (categories());
    catList.setModel (&catModel);
    subList.setModel (&subModel);
    catList.setRowHeight (24); subList.setRowHeight (22); list.setRowHeight (26);
    catModel.onSelect = [this] (int) { rebuildCategoryList(); requery(); };
    subModel.onSelect = [this] (int) { requery(); };
    for (auto* l : { &catList, &subList, &list }) { l->setColour (juce::ListBox::backgroundColourId, col::bgDeep); addAndMakeVisible (l); }
    search.setTextToShowWhenEmpty (L ("Buscar por nombre, tags, categoría, autor, notas...", "Search name, tags, category, author, notes..."), col::textDim);
    search.addListener (this);
    search.setFont (font (14.0f));
    addAndMakeVisible (search);
    for (auto& t : characterTags())
    {
        auto* b = tagButtons.add (new juce::TextButton (t));
        b->setClickingTogglesState (true);
        b->onClick = [this] { requery(); };
        addAndMakeVisible (b);
    }
    typeBox.addItemList ({ L ("Todos", "All"), L ("Fábrica", "Factory"), L ("Usuario", "User") }, 1);
    typeBox.setSelectedItemIndex (0, juce::dontSendNotification);
    sortBox.addItemList ({ L ("Nombre", "Name"), L ("Fecha", "Date"), L ("Categoría", "Category"), L ("Valoración", "Rating"), L ("Usos", "Uses") }, 1);
    sortBox.setSelectedItemIndex (0, juce::dontSendNotification);
    for (auto* cb : { &typeBox, &sortBox, &authorBox, &folderBox }) { cb->onChange = [this] { requery(); }; addAndMakeVisible (cb); }
    favOnly.onClick = [this] { requery(); };
    addAndMakeVisible (favOnly);
    info.setJustificationType (juce::Justification::topLeft);
    info.setFont (font (12.5f));
    addAndMakeVisible (info);

    for (auto* b : { &saveBtn, &saveAsBtn, &importBtn, &exportBtn, &prevBtn, &nextBtn, &folderBtn, &rescanBtn }) addAndMakeVisible (b);
    prevBtn.onClick = [this] { step (-1); };
    nextBtn.onClick = [this] { step (1); };
    rescanBtn.onClick = [this] { ctx.library.rescanUser(); refresh(); };
    saveBtn.onClick = [this]
    {
        const int idx = ctx.library.indexOfUuid (ctx.proc.currentMeta.uuid);
        if (idx >= 0 && ! ctx.library.all()[(size_t) idx].factory)
        {
            auto p = Preset::capture (ctx.proc);
            if (ctx.library.saveUser (p, ctx.library.all()[(size_t) idx].folder) && ctx.toast) ctx.toast (L ("Guardado: ", "Saved: ") + p.meta.name);
            refresh();
        }
        else editMeta (-1);
    };
    saveAsBtn.onClick = [this] { editMeta (-1); };
    folderBtn.onClick = [this]
    {
        auto* w = new juce::AlertWindow (L ("Nueva carpeta de usuario", "New user folder"), L ("Nombre de la carpeta:", "Folder name:"), juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", L ("Mis sonidos", "My sounds"));
        w->addButton (L ("Crear", "Create"), 1); w->addButton (L ("Cancelar", "Cancel"), 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1) ctx.library.createFolder (w->getTextEditorContents ("name"));
            refresh();
        }), true);
    };
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (L ("Importar presets / bancos", "Import presets / banks"), juce::File(), "*.memoria;*.memoriabank");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems, [this] (const juce::FileChooser& fc)
        {
            int n = 0;
            for (auto& f : fc.getResults()) n += ctx.library.importFile (f);
            if (ctx.toast) ctx.toast (juce::String (n) + L (" preset(s) importados", " preset(s) imported"));
            refresh();
        });
    };
    exportBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (L ("Exportar banco (presets filtrados)", "Export bank (filtered presets)"), Library::defaultRoot().getChildFile ("Banco.memoriabank"), "*.memoriabank");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (ctx.library.exportBank (shown, f.withFileExtension ("memoriabank")) && ctx.toast) ctx.toast (juce::String ((int) shown.size()) + L (" presets exportados", " presets exported"));
        });
    };
    ctx.library.onChange = [sp = juce::Component::SafePointer<PresetsPage> (this)] { juce::MessageManager::callAsync ([sp] { if (sp) sp->requery(); }); };
    setWantsKeyboardFocus (true);
    refresh();
}

void PresetsPage::rebuildCategoryList()
{
    subModel.items.clear();
    subModel.items.add (L ("Todas", "All"));
    if (catModel.selected > 0) subModel.items.addArray (subcategories (catModel.items[catModel.selected]));
    subModel.selected = 0;
    subList.updateContent();
    subList.selectRow (0, false, true);
    subList.repaint();
}

void PresetsPage::refresh()
{
    const auto a = authorBox.getText(), fo = folderBox.getText();
    authorBox.clear (juce::dontSendNotification);
    authorBox.addItem (L ("Todos los autores", "All authors"), 1);
    authorBox.addItemList (ctx.library.authors(), 2);
    authorBox.setText (a.isEmpty() ? L ("Todos los autores", "All authors") : a, juce::dontSendNotification);
    folderBox.clear (juce::dontSendNotification);
    folderBox.addItem (L ("Todas las carpetas", "All folders"), 1);
    folderBox.addItemList (ctx.library.userFolders(), 2);
    folderBox.setText (fo.isEmpty() ? L ("Todas las carpetas", "All folders") : fo, juce::dontSendNotification);
    if (subModel.items.isEmpty()) rebuildCategoryList();
    catList.updateContent();
    requery();
}

void PresetsPage::requery()
{
    PresetFilter f;
    f.text = search.getText();
    if (catModel.selected > 0) f.category = catModel.items[catModel.selected];
    if (subModel.selected > 0) f.subcategory = subModel.items[subModel.selected];
    for (auto* b : tagButtons) if (b->getToggleState()) f.tags.add (b->getButtonText());
    f.type = (PresetFilter::Type) juce::jmax (0, typeBox.getSelectedItemIndex());
    f.sort = (PresetFilter::Sort) juce::jmax (0, sortBox.getSelectedItemIndex());
    f.descending = f.sort == PresetFilter::ByRating || f.sort == PresetFilter::ByUses || f.sort == PresetFilter::ByDate;
    if (authorBox.getSelectedId() > 1) f.author = authorBox.getText();
    if (folderBox.getSelectedId() > 1) f.folder = folderBox.getText();
    f.favoritesOnly = favOnly.getToggleState();
    shown = ctx.library.query (f);
    list.updateContent();
    list.repaint();
    info.setText (juce::String ((int) shown.size()) + " presets", juce::dontSendNotification);
    const int cur = ctx.library.indexOfUuid (ctx.proc.currentMeta.uuid);
    syncingSelection = true; // reflect the loaded preset without reloading it
    list.deselectAllRows();
    for (int r = 0; r < (int) shown.size(); ++r)
        if (shown[(size_t) r] == cur) { list.selectRow (r, false, true); break; }
    syncingSelection = false;
}

void PresetsPage::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& m = ctx.library.all()[(size_t) shown[(size_t) row]];
    if (sel) { g.setColour (col::accent.withAlpha (0.25f)); g.fillRect (0, 0, w, h); g.setColour (col::accent); g.fillRect (0, 0, 3, h); }
    else if (row % 2) { g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRect (0, 0, w, h); }
    g.setColour (m.favorite ? col::accent : col::textDim.withAlpha (0.4f));
    g.setFont (font (14.0f));
    g.drawText (m.favorite ? juce::String::fromUTF8 ("\xe2\x98\x85") : juce::String::fromUTF8 ("\xe2\x98\x86"), 6, 0, 18, h, juce::Justification::centred);
    g.setColour (col::text);
    g.setFont (font (13.5f, true));
    g.drawText (m.name, 28, 0, w / 3, h, juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (font (11.5f));
    g.drawText (categoryLabel (m.category) + (m.subcategory.isNotEmpty() ? " / " + m.subcategory : juce::String()), 28 + w / 3, 0, w / 5, h, juce::Justification::centredLeft);
    g.drawFittedText (m.tags.joinIntoString (", "), 28 + w / 3 + w / 5, 0, w - 160 - (28 + w / 3 + w / 5), h, juce::Justification::centredLeft, 1, 0.8f);
    juce::String stars;
    for (int i = 0; i < 5; ++i) stars << (i < m.rating ? juce::String::fromUTF8 ("\xe2\x98\x85") : juce::String::fromUTF8 ("\xc2\xb7"));
    g.setColour (col::accent2);
    g.drawText (stars, w - 150, 0, 70, h, juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.drawText (m.factory ? L ("FÁBRICA", "FACTORY") : L ("USUARIO", "USER"), w - 78, 0, 72, h, juce::Justification::centredRight);
}

void PresetsPage::loadRow (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    Preset p;
    const int idx = shown[(size_t) row];
    if (! ctx.library.load (idx, p)) { if (ctx.toast) ctx.toast (L ("No se pudo cargar el preset", "Could not load the preset")); return; }
    p.applyTo (ctx.proc);
    ctx.library.markUsed (idx);
    ctx.undo.reset();
    if (onLoaded) onLoaded (p);
    const auto& m = ctx.library.all()[(size_t) idx];
    info.setText (m.name + "\n" + m.category + " / " + m.subcategory + L ("\nAutor: ", "\nAuthor: ") + m.author + "\nTags: " + m.tags.joinIntoString (", ")
                  + L ("\nUsos: ", "\nUses: ") + juce::String (m.uses) + "\n\n" + localisedNotes (m.notes), juce::dontSendNotification);
    if (ctx.settings.previewOnSelect() && ! ctx.proc.isHostPlaying()) ctx.proc.setPlaying (true);
}

void PresetsPage::selectedRowsChanged (int row) { if (! syncingSelection) loadRow (row); }
void PresetsPage::listBoxItemDoubleClicked (int row, const juce::MouseEvent&) { loadRow (row); }
void PresetsPage::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showRowMenu (row);
    else if (e.x < 26 && row >= 0 && row < (int) shown.size())
    {
        const int idx = shown[(size_t) row];
        ctx.library.setFavorite (idx, ! ctx.library.all()[(size_t) idx].favorite);
    }
}

void PresetsPage::step (int delta)
{
    if (shown.empty()) return;
    const int cur = list.getSelectedRow();
    const int next = cur < 0 ? 0 : (cur + delta + (int) shown.size()) % (int) shown.size();
    list.selectRow (next);
}

bool PresetsPage::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::upKey) { step (-1); return true; }
    if (k == juce::KeyPress::downKey) { step (1); return true; }
    return false;
}

void PresetsPage::showRowMenu (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const int idx = shown[(size_t) row];
    const auto m = ctx.library.all()[(size_t) idx];
    juce::PopupMenu menu, rate, move;
    for (int s = 0; s <= 5; ++s) rate.addItem (100 + s, s == 0 ? L ("Sin valorar", "Unrated") : juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x98\x85"), s), true, m.rating == s);
    for (int cIdx = 0; cIdx < categories().size(); ++cIdx) move.addItem (200 + cIdx, categories()[cIdx], true, m.category == categories()[cIdx]);
    menu.addItem (1, L ("Cargar", "Load"));
    menu.addItem (2, m.favorite ? L ("Quitar de favoritos", "Remove from favourites") : L ("Añadir a favoritos", "Add to favourites"));
    menu.addSubMenu (L ("Valoración", "Rating"), rate);
    menu.addSeparator();
    menu.addItem (3, L ("Editar nombre, tags y metadatos...", "Edit name, tags and metadata..."));
    menu.addSubMenu (L ("Mover a categoría", "Move to category"), move);
    menu.addItem (4, L ("Duplicar", "Duplicate"));
    menu.addItem (5, L ("Exportar preset...", "Export preset..."));
    menu.addItem (6, L ("Borrar", "Delete"), ! m.factory);
    menu.showMenuAsync ({}, [this, idx, m] (int r)
    {
        if (r == 1) { for (int i = 0; i < (int) shown.size(); ++i) if (shown[(size_t) i] == idx) loadRow (i); }
        else if (r == 2) ctx.library.setFavorite (idx, ! m.favorite);
        else if (r == 3) editMeta (idx);
        else if (r == 4) ctx.library.duplicate (idx);
        else if (r == 5)
        {
            chooser = std::make_unique<juce::FileChooser> (L ("Exportar preset", "Export preset"), Library::defaultRoot().getChildFile (m.name + ".memoria"), "*.memoria");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode, [this, idx] (const juce::FileChooser& fc)
            { if (fc.getResult() != juce::File()) ctx.library.exportPreset (idx, fc.getResult().withFileExtension ("memoria")); });
        }
        else if (r == 6)
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, L ("Borrar preset", "Delete preset"), L ("¿Borrar \"", "Delete \"") + m.name + "\"?", L ("Borrar", "Delete"), L ("Cancelar", "Cancel"), nullptr,
                juce::ModalCallbackFunction::create ([this, idx] (int ok) { if (ok) ctx.library.remove (idx); }));
        else if (r >= 100 && r <= 105) ctx.library.setRating (idx, r - 100);
        else if (r >= 200)
        {
            auto mm = m; mm.category = categories()[r - 200]; mm.subcategory = {};
            ctx.library.updateMeta (idx, mm);
        }
        refresh();
    });
}

void PresetsPage::editMeta (int libIndex)
{
    PresetMeta m = libIndex >= 0 ? ctx.library.all()[(size_t) libIndex] : ctx.proc.currentMeta;
    if (libIndex < 0 && m.factory) { m.author = L ("Usuario", "User"); }
    auto* w = new juce::AlertWindow (libIndex >= 0 ? L ("Editar preset", "Edit preset") : L ("Guardar preset", "Save preset"), "", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", m.name, L ("Nombre", "Name"));
    w->addComboBox ("cat", categories(), L ("Categoría", "Category"));
    if (auto* cb = w->getComboBoxComponent ("cat")) cb->setText (m.category, juce::dontSendNotification);
    w->addTextEditor ("sub", m.subcategory, L ("Subcategoría", "Subcategory"));
    w->addTextEditor ("tags", m.tags.joinIntoString (", "), L ("Etiquetas (separadas por comas)", "Tags (comma separated)"));
    w->addTextEditor ("author", m.author, L ("Autor", "Author"));
    w->addTextEditor ("notes", m.notes, L ("Notas", "Notes"));
    juce::StringArray folders { L ("(raíz)", "(root)") };
    folders.addArray (ctx.library.userFolders());
    w->addComboBox ("folder", folders, L ("Carpeta", "Folder"));
    if (auto* cb = w->getComboBoxComponent ("folder")) cb->setText (m.folder.isEmpty() ? L ("(raíz)", "(root)") : m.folder, juce::dontSendNotification);
    w->addComboBox ("fav", { L ("No favorito", "Not favourite"), L ("Favorito", "Favourite") }, L ("Favorito", "Favourite"));
    if (auto* cb = w->getComboBoxComponent ("fav")) cb->setSelectedItemIndex (m.favorite ? 1 : 0, juce::dontSendNotification);
    w->addButton (L ("Guardar", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (L ("Cancelar", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, libIndex, m] (int r) mutable
    {
        if (r != 1) return;
        m.name = w->getTextEditorContents ("name").trim();
        if (m.name.isEmpty()) m.name = L ("Sin nombre", "Untitled");
        m.category = w->getComboBoxComponent ("cat")->getText();
        m.subcategory = w->getTextEditorContents ("sub").trim();
        m.tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
        m.tags.trim(); m.tags.removeEmptyStrings();
        m.author = w->getTextEditorContents ("author").trim();
        m.notes = w->getTextEditorContents ("notes");
        const auto folder = w->getComboBoxComponent ("folder")->getText();
        m.folder = folder == L ("(raíz)", "(root)") ? juce::String() : folder;
        m.favorite = w->getComboBoxComponent ("fav")->getSelectedItemIndex() == 1;
        if (libIndex >= 0) ctx.library.updateMeta (libIndex, m);
        else
        {
            auto p = Preset::capture (ctx.proc);
            p.meta = m;
            p.meta.uuid = {}; // "save as" always creates a new preset
            p.meta.factory = false;
            if (ctx.library.saveUser (p, m.folder))
            {
                ctx.proc.currentMeta = p.meta;
                if (const int ni = ctx.library.indexOfUuid (p.meta.uuid); ni >= 0 && m.favorite) ctx.library.setFavorite (ni, true);
                if (ctx.toast) ctx.toast (L ("Guardado: ", "Saved: ") + p.meta.name);
                if (onLoaded) onLoaded (p);
            }
        }
        refresh();
    }), true);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (8);
    drawPanel (g, r.removeFromLeft (200).toFloat());
    r.removeFromLeft (8);
    drawPanel (g, r.removeFromRight (330).toFloat());
    g.setColour (col::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (L ("ESTADO DE ÁNIMO", "MOOD"), 20, 12, 160, 16, juce::Justification::centredLeft);
}

void PresetsPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (200).reduced (8);
    left.removeFromTop (18);
    catList.setBounds (left.removeFromTop (12 * 24 + 4));
    left.removeFromTop (6);
    subList.setBounds (left);
    r.removeFromLeft (8);
    auto right = r.removeFromRight (330).reduced (10);
    r.removeFromRight (8);
    // centre: search + filters + list
    search.setBounds (r.removeFromTop (32).reduced (0, 2));
    auto fr = r.removeFromTop (30).reduced (0, 3);
    typeBox.setBounds (fr.removeFromLeft (100).reduced (2, 0));
    authorBox.setBounds (fr.removeFromLeft (150).reduced (2, 0));
    folderBox.setBounds (fr.removeFromLeft (150).reduced (2, 0));
    sortBox.setBounds (fr.removeFromLeft (110).reduced (2, 0));
    favOnly.setBounds (fr.reduced (4, 0));
    auto tags = r.removeFromBottom (58);
    const int tw = tags.getWidth() / 10;
    for (int i = 0; i < tagButtons.size(); ++i) tagButtons[i]->setBounds (juce::Rectangle<int> (tags.getX() + (i % 10) * tw, tags.getY() + (i / 10) * 29, tw, 29).reduced (2));
    list.setBounds (r.reduced (0, 4));
    // right: info + actions
    auto nav = right.removeFromBottom (32);
    prevBtn.setBounds (nav.removeFromLeft (nav.getWidth() / 2).reduced (2));
    nextBtn.setBounds (nav.reduced (2));
    auto b2 = right.removeFromBottom (32);
    folderBtn.setBounds (b2.removeFromLeft (b2.getWidth() / 2).reduced (2)); rescanBtn.setBounds (b2.reduced (2));
    auto b1 = right.removeFromBottom (32);
    importBtn.setBounds (b1.removeFromLeft (b1.getWidth() / 2).reduced (2)); exportBtn.setBounds (b1.reduced (2));
    auto b0 = right.removeFromBottom (36);
    saveBtn.setBounds (b0.removeFromLeft (b0.getWidth() / 2).reduced (2)); saveAsBtn.setBounds (b0.reduced (2));
    info.setBounds (right.reduced (2));
}
}

namespace gx::ui
{
int PresetsPage::rowOfPreset (const juce::String& name) const
{
    for (size_t i = 0; i < shown.size(); ++i)
        if (ctx.library.all()[(size_t) shown[i]].name == name) return (int) i;
    return -1;
}
}
