#pragma once
#include "Visuals.h"
#include "VideoExport.h"
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

// Round Play/Stop button: internal transport (the DAW transport also starts the engine).
class PlayButton : public juce::Component, public juce::SettableTooltipClient, public Animated
{
public:
    explicit PlayButton (MemoriaProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick (const Frame&) override;
private:
    MemoriaProcessor& proc;
    bool on = false, host = false;
    float level = 0, pulse = 0;
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
    PlayButton play;
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

// Main page: the living landscape, the transport, the four macros, harmony and session controls.
class LandscapePage : public Page
{
public:
    explicit LandscapePage (Context& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick (const Frame&) override;
    std::function<void()> onFullscreen;
private:
    Landscape landscape;
    juce::OwnedArray<juce::TextButton> sceneButtons;
    juce::TextButton fullscreen;
    PlayButton play;
    ParamToggle freeze;
    // memory slot button: click = recall / store, hold 0.7 s = clear
    struct MemButton : public juce::TextButton, private juce::Timer
    {
        using juce::TextButton::TextButton;
        std::function<void()> onLongPress;
        bool consumed = false;
        void mouseDown (const juce::MouseEvent& e) override { consumed = false; startTimer (700); juce::TextButton::mouseDown (e); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            stopTimer();
            if (consumed) { setState (juce::Button::buttonNormal); return; } // the long press already acted: no click
            juce::TextButton::mouseUp (e);
        }
        void timerCallback() override { stopTimer(); if (isDown()) { consumed = true; if (onLongPress) onLongPress(); } }
    };
    juce::OwnedArray<MemButton> memButtons;
    void recallMemory (int slot);
    void storeMemory (int slot);
    struct Morph { std::vector<float> from, to; double start = -1; bool switched = false; } morph;
    juce::Label seedLabel;
    juce::TextButton dice { L ("Nueva semilla", "New seed") }, seedPrev { "<" }, seedNext { ">" };
    Box harmony, macros, session;
    int lastSeed = -1;
};

// The landscape alone, full screen (kiosk window). Esc closes; the keyboard plays the macros.
class FullscreenView : public juce::Component, public Animated
{
public:
    FullscreenView (Context& c, std::function<void()> onClose);
    void tick (const Frame&) override;
    void resized() override { view.setBounds (getLocalBounds()); }
    void paintHud (juce::Graphics&);
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override { helpUntil = juce::jmax (helpUntil, lastT + 2.5); }
private:
    void nudge (int param, float delta, const juce::String& label);
    Context& ctx;
    std::function<void()> close;
    Landscape view;
    double lastT = 0, helpUntil = 6.0, toastUntil = 0;
    bool helpPinned = false;
    juce::String toastText;
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

// Layer strips + master effects.
class SoundPage : public Page
{
public:
    explicit SoundPage (Context& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick (const Frame&) override;
private:
    struct Strip : public Box
    {
        Strip (Context& c, int layer);
        void paint (juce::Graphics&) override;
        int layer;
        float presence = 0, energy = 0;
    };
    juce::OwnedArray<Strip> strips;
    Box fx, noise, fades;
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

// Long audio export (WAV / FLAC / OGG) on a background thread + preset file import/export.
class ExportPage : public Page, private juce::Timer
{
public:
    explicit ExportPage (Context& c);
    ~ExportPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    bool isBusy() const { return busy.load() || video.running(); }
private:
    void timerCallback() override;
    void start();
    void startVideo();
    void updateFfmpegStatus();
    void installFfmpeg();
    juce::ComboBox duration, format, rate;
    juce::TextButton exportAudio { L ("Exportar audio…", "Export audio…") }, cancel { L ("Cancelar", "Cancel") };
    juce::TextButton exportPreset { L ("Exportar preset .memoria", "Export .memoria preset") }, importPreset { L ("Importar preset o banco", "Import preset or bank") };
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread worker;
    std::atomic<bool> busy { false }, cancelFlag { false }, done { false };
    std::atomic<float> progress { 0 };
    juce::String result, status;
    juce::File target;
    double startedAt = 0;
    // video
    juce::ComboBox vDuration, vRes, vScene;
    juce::TextButton exportVideo { L ("Exportar vídeo…", "Export video…") }, cancelVideo { L ("Cancelar", "Cancel") },
                     ffmpegInstall { L ("Instalar ffmpeg", "Install ffmpeg") }, ffmpegChoose { L ("Elegir ffmpeg…", "Choose ffmpeg…") };
    VideoExporter video;
    juce::File ffmpeg, videoTarget;
    juce::String videoStatus;
    std::unique_ptr<juce::ChildProcess> installer;
    double videoStart = 0;
};

class SettingsPage : public Page
{
public:
    SettingsPage (Context& c, std::function<void (float)> setScale, std::function<void()> showAbout);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    juce::ComboBox scaleBox, animBox, langBox;
    juce::ToggleButton gpuToggle { L ("Paisajes en alta calidad (tarjeta gráfica)", "High-quality landscapes (graphics card)") };
    juce::ToggleButton fpsToggle { L ("Mostrar FPS en el paisaje", "Show FPS on the landscape") };
    juce::ToggleButton aiMsgs { L ("Mensajes de la IA al generar", "AI messages while generating") }, preview { L ("Al elegir un preset, empezar a sonar", "Start playing when a preset is selected") };
    juce::Slider historyLimit, starAmount;
    juce::TextButton openFolder { L ("Abrir carpeta de presets", "Open presets folder") }, clearLearn { L ("Borrar asignaciones MIDI Learn", "Clear MIDI Learn assignments") }, aboutBtn { L ("Acerca de Memoria", "About Memoria") };
};
}
