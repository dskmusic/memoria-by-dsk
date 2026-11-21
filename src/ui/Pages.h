#pragma once
#include "Visuals.h"
#include <thread>

namespace gx::ui
{
// Container that forwards frames to every Animated descendant that is showing.
class Page : public juce::Component, public Animated
{
public:
    explicit Page (Context& c) : ctx (c) {}
    void tick (const Frame& f) override { forward (this, f); }
    virtual void refresh() {}          // after preset load / undo
    virtual void pageShown() {}
    static void forward (juce::Component* c, const Frame& f);
protected:
    Context& ctx;
};

class Logo : public juce::Component, public juce::SettableTooltipClient
{
public:
    Logo() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip ("dskmusic.com"); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { juce::URL ("https://dskmusic.com").launchInDefaultBrowser(); }
    static void drawMark (juce::Graphics&, juce::Rectangle<float>, float glow, float phase = 0.0f);
    float glow = 0, phase = 0;
};

class Header : public juce::Component, public Animated
{
public:
    Header (Context& c, std::function<void (int)> onTab);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (const Frame&) override;
    void setTab (int t);
    void setPresetName (const juce::String& n) { presetName.setText (n, juce::dontSendNotification); }
    std::function<void (int delta)> onPresetStep;
    std::function<void()> onAbout, onAB, onUndo, onRedo;
    juce::TextButton abButton { "A" };
    juce::TextButton undoButton { L ("Deshacer", "Undo") }, redoButton { L ("Rehacer", "Redo") };
private:
    Context& ctx;
    Logo logo;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton prev { "<" }, next { ">" }, about { "i" };
    juce::Label presetName;
    std::function<void (int)> tabCb;
    float energy = 0;
};

class AboutOverlay : public juce::Component
{
public:
    AboutOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { setVisible (false); }
private:
    juce::HyperlinkButton link { "dskmusic.com", juce::URL ("https://dskmusic.com") };
};

// Main page: the piano (scope, spectrum, meter) and its controls.
class PianoPage : public Page
{
public:
    explicit PianoPage (Context& c);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Scope scope;
    Spectrum spectrum;
    VuMeter vu;
    AutoPanel global, piano, space;
};

// Help: a short, friendly guide (both languages).
class HelpPage : public Page
{
public:
    explicit HelpPage (Context& c);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    struct Body : public juce::Component { void paint (juce::Graphics&) override; int layoutHeight (int width); };
    juce::Viewport view;
    Body body;
};

class PresetsPage : public Page, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit PresetsPage (Context& c);
    ~PresetsPage() override { ctx.library.onChange = nullptr; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void pageShown() override { refresh(); }
    bool keyPressed (const juce::KeyPress&) override;
    void step (int delta);                         // header < > buttons
    int rowOfPreset (const juce::String& name) const;  // row in the current filtered list, -1 if hidden
    std::function<void (const Preset&)> onLoaded;
private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void textEditorTextChanged (juce::TextEditor&) override { requery(); }
    void requery();
    void loadRow (int row);
    void showRowMenu (int row);
    void editMeta (int libIndex);
    void rebuildCategoryList();

    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::ListBox catList, subList;
    struct StringModel : juce::ListBoxModel
    {
        juce::StringArray items; int selected = 0; std::function<void (int)> onSelect;
        int getNumRows() override { return items.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool sel) override;
        void selectedRowsChanged (int row) override { selected = row; if (onSelect) onSelect (row); }
    } catModel, subModel;
    juce::OwnedArray<juce::TextButton> tagButtons;
    juce::ComboBox typeBox, sortBox, authorBox, folderBox;
    juce::ToggleButton favOnly { L ("Favoritos", "Favourites") };
    juce::TextButton saveBtn { L ("Guardar", "Save") }, saveAsBtn { L ("Guardar como", "Save as") }, importBtn { L ("Importar", "Import") }, exportBtn { L ("Exportar banco", "Export bank") },
                     prevBtn { L ("Anterior", "Previous") }, nextBtn { L ("Siguiente", "Next") }, folderBtn { L ("Nueva carpeta", "New folder") }, rescanBtn { L ("Reescanear", "Rescan") };
    juce::Label info;
    std::vector<int> shown;
    bool syncingSelection = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Preset file import / export (long audio export returns with the generative hands).
class ExportPage : public Page
{
public:
    explicit ExportPage (Context& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    bool isBusy() const { return false; }
private:
    juce::TextButton exportPreset { L ("Exportar preset .memoria", "Export .memoria preset") }, importPreset { L ("Importar preset o banco", "Import preset or bank") };
    std::unique_ptr<juce::FileChooser> chooser;
};

class SettingsPage : public Page
{
public:
    SettingsPage (Context& c, std::function<void (float)> setScale, std::function<void()> showAbout);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    juce::ComboBox scaleBox, animBox, langBox;
    juce::ToggleButton preview { L ("Al elegir un preset, tocar una nota de prueba", "Play a test chord when a preset is selected") };
    juce::TextButton openFolder { L ("Abrir carpeta de presets", "Open presets folder") }, clearLearn { L ("Borrar asignaciones MIDI Learn", "Clear MIDI Learn assignments") }, aboutBtn { L ("Acerca de Memoria", "About Memoria") };
};
}
