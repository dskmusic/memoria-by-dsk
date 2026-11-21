#include "Pages.h"
#include "../preset/Render.h"

namespace gx::ui
{
void Page::forward (juce::Component* c, const Frame& f)
{
    for (auto* ch : c->getChildren())
    {
        if (! ch->isVisible()) continue;
        if (auto* a = dynamic_cast<Animated*> (ch)) a->tick (f);
        else forward (ch, f);
    }
}

// ---------------------------------------------------------------- Logo: piano keys whose right edge dissolves into dust
void Logo::drawMark (juce::Graphics& g, juce::Rectangle<float> r, float glow, float phase)
{
    const float h = juce::jmin (r.getHeight(), r.getWidth() * 0.7f), w = h * 1.35f;
    const auto o = r.getCentre() - juce::Point<float> (w * 0.5f, h * 0.5f);
    const float kw = w / 3.0f, gap = h * 0.04f;
    if (glow > 0.01f)
    {
        const auto c = r.getCentre();
        g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.22f * glow), c.x, c.y, col::accent.withAlpha (0.0f), c.x + w * 0.75f, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (w * 1.5f, h * 1.5f).withCentre (c));
    }
    // two white keys + the black one between them; the right-hand key fades out
    juce::ColourGradient grad (juce::Colours::white, o.x, o.y, col::accent, o.x + w, o.y + h, false);
    for (int i = 0; i < 2; ++i)
    {
        const auto k = juce::Rectangle<float> (o.x + (float) i * kw, o.y, kw - gap, h);
        juce::Path path; path.addRoundedRectangle (k.getX(), k.getY(), k.getWidth(), k.getHeight(), h * 0.07f, h * 0.07f, false, false, true, true);
        g.setGradientFill (grad);
        g.strokePath (path, juce::PathStrokeType (juce::jmax (1.4f, h * 0.06f)));
    }
    const auto bk = juce::Rectangle<float> (o.x + kw - h * 0.11f, o.y, h * 0.22f, h * 0.6f);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bk, h * 0.04f);
    // third key: only its left edge survives, the rest becomes drifting particles
    const auto k3 = juce::Rectangle<float> (o.x + 2.0f * kw, o.y, kw - gap, h);
    g.setGradientFill (juce::ColourGradient (col::accent, k3.getX(), 0, col::accent.withAlpha (0.0f), k3.getRight(), 0, false));
    g.fillRect (k3.getX(), k3.getY(), juce::jmax (1.4f, h * 0.06f), h);
    g.fillRect (k3.getX(), k3.getY(), k3.getWidth() * 0.55f, juce::jmax (1.4f, h * 0.06f));
    g.fillRect (k3.getX(), k3.getBottom() - juce::jmax (1.4f, h * 0.06f), k3.getWidth() * 0.55f, juce::jmax (1.4f, h * 0.06f));
    for (int i = 0; i < 16; ++i)
    {
        const float u = std::fmod ((float) i * 0.6180339f + phase * (0.15f + 0.05f * (float) (i % 3)), 1.0f);
        const float x = k3.getX() + k3.getWidth() * (0.35f + 0.9f * u), y = o.y + h * (0.1f + 0.8f * std::fmod ((float) i * 0.381966f, 1.0f)) - u * h * 0.12f;
        g.setColour (col::accent.withAlpha ((1.0f - u) * 0.85f));
        g.fillEllipse (juce::Rectangle<float> (h * 0.07f * (1.0f - 0.5f * u), h * 0.07f * (1.0f - 0.5f * u)).withCentre ({ x, y }));
    }
}

void Logo::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawMark (g, r.removeFromLeft (r.getHeight() * 1.6f).reduced (4, 10), glow, phase);
    r.removeFromLeft (8);
    juce::GlyphArrangement ga;
    ga.addLineOfText (techFont (21.0f), "MEMORIA", r.getX(), r.getCentreY() + 8.0f);
    juce::Path text;
    ga.createPath (text);
    const auto tb = text.getBounds();
    g.setColour (col::accent.withAlpha (0.12f + 0.25f * glow));
    g.strokePath (text, juce::PathStrokeType (3.0f));
    juce::ColourGradient grad (juce::Colours::white, tb.getX(), tb.getY(), col::accent, tb.getRight(), tb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (text);
    g.setColour (col::textDim);
    g.setFont (font (12.0f, true));
    g.drawText ("by DSK", juce::Rectangle<float> (tb.getRight() + 7, tb.getBottom() - 14, 60, 16), juce::Justification::bottomLeft);
}

// ---------------------------------------------------------------- Header
Header::Header (Context& c, std::function<void (int)> onTab) : ctx (c), tabCb (std::move (onTab))
{
    addAndMakeVisible (logo);
    const juce::String names[] { "MEMORIA", "PRESETS", L ("EXPORTAR", "EXPORT"), L ("AJUSTES", "SETTINGS"), L ("AYUDA", "HELP") };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (false);
        b->getProperties().set ("small", true);
        b->onClick = [this, i] { setTab (i); if (tabCb) tabCb (i); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &prev, &next, &about, &abButton, &undoButton, &redoButton }) addAndMakeVisible (b);
    prev.setTooltip (L ("Preset anterior", "Previous preset"));
    next.setTooltip (L ("Preset siguiente", "Next preset"));
    abButton.setTooltip (L ("Comparar A/B", "Compare A/B"));
    undoButton.setTooltip (L ("Deshacer (Ctrl+Z)", "Undo (Ctrl+Z)"));
    redoButton.setTooltip (L ("Rehacer (Ctrl+Y)", "Redo (Ctrl+Y)"));
    prev.onClick = [this] { if (onPresetStep) onPresetStep (-1); };
    next.onClick = [this] { if (onPresetStep) onPresetStep (1); };
    about.onClick = [this] { if (onAbout) onAbout(); };
    abButton.onClick = [this] { if (onAB) onAB(); };
    undoButton.onClick = [this] { if (onUndo) onUndo(); };
    redoButton.onClick = [this] { if (onRedo) onRedo(); };
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (font (14.0f, true));
    presetName.setColour (juce::Label::backgroundColourId, col::bgDeep);
    presetName.setColour (juce::Label::textColourId, col::accent);
    addAndMakeVisible (presetName);
    setTab (0);
}

void Header::setTab (int t)
{
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (i == t, juce::dontSendNotification);
}

void Header::tick (const Frame& f)
{
    logo.glow = f.energy;
    logo.phase = (float) std::fmod (f.time * 0.08, 1.0);
    logo.repaint();
}

void Header::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::bg, 0, r.getBottom(), false));
    g.fillRect (r);
    juce::ColourGradient line (col::accent.withAlpha (0.0f), 0, 0, col::accent.withAlpha (0.0f), r.getWidth(), 0, false);
    line.addColour (0.3, col::accent.withAlpha (0.8f));
    line.addColour (0.7, col::accent2.withAlpha (0.8f));
    g.setGradientFill (line);
    g.fillRect (r.removeFromBottom (2));
}

void Header::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromBottom (2);
    logo.setBounds (r.removeFromLeft (232));
    r.removeFromLeft (8);
    auto tabArea = r.removeFromLeft (520);
    const int tw = tabArea.getWidth() / tabs.size();
    for (auto* t : tabs) t->setBounds (tabArea.removeFromLeft (tw).reduced (2, 8));
    r.removeFromLeft (8);
    about.setBounds (r.removeFromRight (28).reduced (1, 10));
    redoButton.setBounds (r.removeFromRight (62).reduced (2, 10));
    undoButton.setBounds (r.removeFromRight (66).reduced (2, 10));
    abButton.setBounds (r.removeFromRight (30).reduced (2, 10));
    prev.setBounds (r.removeFromLeft (22).reduced (1, 10));
    next.setBounds (r.removeFromRight (22).reduced (1, 10));
    presetName.setBounds (r.reduced (2, 10));
}

// ---------------------------------------------------------------- About
AboutOverlay::AboutOverlay()
{
    link.setFont (font (16.0f, true), false);
    link.setColour (juce::HyperlinkButton::textColourId, col::accent);
    addAndMakeVisible (link);
}

void AboutOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.7f));
    auto box = getLocalBounds().withSizeKeepingCentre (540, 380).toFloat();
    drawPanel (g, box, 10);
    auto in = box.reduced (28);
    Logo::drawMark (g, in.removeFromTop (80).withSizeKeepingCentre (150, 70), 0.6f, 0.2f);
    g.setColour (col::accent);
    g.setFont (techFont (28.0f));
    g.drawText ("MEMORIA", in.removeFromTop (40), juce::Justification::centred);
    g.setColour (col::textDim);
    g.setFont (font (14.0f, true));
    g.drawText ("by DSK  -  version " MEMORIA_VERSION, in.removeFromTop (22), juce::Justification::centred);
    g.setColour (col::text);
    g.setFont (font (13.0f));
    g.drawFittedText (L ("Un piano con alma: modelado físico y capas de recuerdo.\nFreeware de DSK Music. Motor y diseño: DSK Music.\n"
                         "Construido con JUCE (GPLv3) y el VST3 SDK (MIT). Licencia: GNU GPL v3.",
                         "A piano with a soul: physical modelling and layers of memory.\nFreeware by DSK Music. Engine and design: DSK Music.\n"
                         "Built with JUCE (GPLv3) and the VST3 SDK (MIT). License: GNU GPL v3."),
                      in.removeFromTop (96).toNearestInt(), juce::Justification::centred, 6);
    g.setColour (col::textDim);
    g.setFont (font (11.0f));
    g.drawText (L ("Clic para cerrar", "Click to close"), box.removeFromBottom (24), juce::Justification::centred);
}

void AboutOverlay::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre (540, 380);
    link.setBounds (box.withTrimmedTop (300).withHeight (28).withSizeKeepingCentre (200, 28));
}

// ---------------------------------------------------------------- Export page
ExportPage::ExportPage (Context& c) : Page (c)
{
    const int mins[] { 1, 5, 10, 15, 30, 45, 60, 90, 120, 180 };
    for (int m : mins) duration.addItem (juce::String (m) + " min", m);
    duration.setSelectedId (10, juce::dontSendNotification);
    format.addItemList ({ "WAV 24-bit", "FLAC 24-bit", "OGG Vorbis" }, 1);
    format.setSelectedItemIndex (0, juce::dontSendNotification);
    rate.addItem ("44.1 kHz", 44100);
    rate.addItem ("48 kHz", 48000);
    rate.setSelectedId (48000, juce::dontSendNotification);
    for (auto* comp : std::initializer_list<juce::Component*> { &duration, &format, &rate, &exportAudio, &cancel, &exportPreset, &importPreset })
        addAndMakeVisible (comp);
    cancel.setEnabled (false);
    exportAudio.onClick = [this] { start(); };
    cancel.onClick = [this] { cancelFlag = true; };
    exportPreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (L ("Exportar preset", "Export preset"), Library::defaultRoot().getChildFile (ctx.proc.currentMeta.name + ".memoria"), "*.memoria");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            auto p = Preset::capture (ctx.proc);
            if (p.saveTo (f.withFileExtension ("memoria")) && ctx.toast) ctx.toast (L ("Exportado: ", "Exported: ") + f.getFileName());
        });
    };
    importPreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (L ("Importar preset o banco", "Import preset or bank"), Library::defaultRoot(), "*.memoria;*.memoriabank");
        chooser->launchAsync (juce::FileBrowserComponent::openMode, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            const int n = ctx.library.importFile (f);
            if (ctx.toast) ctx.toast (juce::String (n) + L (" preset(s) importados a la biblioteca", " preset(s) imported into the library"));
        });
    };
    status = L ("Las manos tocan solas con el sonido actual y lo guardan en un archivo largo: ideal para vídeos, estudio o dormir. Activa las manos en la pestaña MEMORIA.",
                "The hands play by themselves with the current sound and save it to a long file: ideal for videos, studying or sleeping. Turn the hands on in the MEMORIA tab.");
}

ExportPage::~ExportPage()
{
    cancelFlag = true;
    if (worker.joinable()) worker.join();
}

void ExportPage::start()
{
    if (busy.load()) return;
    if (ctx.proc.getParam (P::hands) < 0.5f) { if (ctx.toast) ctx.toast (L ("Activa las manos (pestaña MEMORIA) para exportar", "Turn the hands on (MEMORIA tab) to export")); return; }
    const auto ext = exportFormatExtension (format.getSelectedItemIndex());
    const auto name = ctx.proc.currentMeta.name.isNotEmpty() ? ctx.proc.currentMeta.name : juce::String ("Memoria");
    const auto def = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName (name + " - " + juce::String (duration.getSelectedId()) + " min" + ext));
    chooser = std::make_unique<juce::FileChooser> (L ("Guardar audio", "Save audio"), def, "*" + ext);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, ext] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        target = f.withFileExtension (ext);
        if (worker.joinable()) worker.join();
        busy = true; done = false; cancelFlag = false; progress = 0;
        exportAudio.setEnabled (false);
        cancel.setEnabled (true);
        startedAt = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto preset = Preset::capture (ctx.proc);
        const int fmt = format.getSelectedItemIndex();
        const double secs = duration.getSelectedId() * 60.0, sr = rate.getSelectedId();
        worker = std::thread ([this, preset, fmt, secs, sr] { result = gx::exportAudio (preset, target, fmt, secs, sr, &progress, &cancelFlag); done = true; });
        startTimerHz (10);
    });
}

void ExportPage::timerCallback()
{
    if (done.load())
    {
        done = false; busy = false;
        if (worker.joinable()) worker.join();
        exportAudio.setEnabled (true); cancel.setEnabled (false);
        stopTimer();
        if (result.isEmpty())
        {
            status = L ("Listo: ", "Done: ") + target.getFullPathName();
            if (ctx.toast) ctx.toast (L ("Audio exportado: ", "Audio exported: ") + target.getFileName());
            target.revealToUser();
        }
        else status = result == "cancelled" ? L ("Exportación cancelada.", "Export cancelled.") : L ("Error: ", "Error: ") + result;
    }
    else if (busy.load())
    {
        const double el = juce::Time::getMillisecondCounterHiRes() * 0.001 - startedAt;
        const float p = progress.load();
        juce::String eta;
        if (p > 0.02f) { const int left = (int) (el / p * (1.0 - p)); eta = "  -  " + L ("quedan ", "") + juce::String (left / 60) + ":" + juce::String (left % 60).paddedLeft ('0', 2) + L ("", " left"); }
        status = L ("Renderizando… ", "Rendering… ") + juce::String ((int) (p * 100)) + " %" + eta;
    }
    repaint();
}

void ExportPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (12);
    auto panel = [&] (int hgt, const juce::String& title)
    {
        auto p = r.removeFromTop (hgt).toFloat();
        r.removeFromTop (10);
        drawPanel (g, p);
        auto in = p.reduced (24, 14);
        g.setColour (col::text);
        g.setFont (techFont (16.0f));
        g.drawText (title, in.removeFromTop (24), juce::Justification::centredLeft);
        return in;
    };
    auto in = panel (210, L ("EXPORTAR AUDIO", "EXPORT AUDIO"));
    g.setColour (col::textDim);
    g.setFont (font (12.5f));
    g.drawFittedText (status, in.removeFromTop (44).toNearestInt(), juce::Justification::topLeft, 3);
    const auto bar = in.removeFromBottom (14);
    g.setColour (col::bgDeep); g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (col::accent); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress.load()), 4.0f);
    panel (120, "PRESETS");
}

void ExportPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto in = r.removeFromTop (210).reduced (24, 14).withTrimmedTop (74);
    auto row = in.removeFromTop (32);
    duration.setBounds (row.removeFromLeft (130).reduced (2)); format.setBounds (row.removeFromLeft (150).reduced (2)); rate.setBounds (row.removeFromLeft (110).reduced (2));
    row.removeFromLeft (10);
    exportAudio.setBounds (row.removeFromLeft (170).reduced (2)); cancel.setBounds (row.removeFromLeft (110).reduced (2));
    r.removeFromTop (10);
    auto low = r.removeFromTop (120).reduced (24, 14).withTrimmedTop (30);
    exportPreset.setBounds (low.removeFromLeft (240).removeFromTop (32));
    low.removeFromLeft (10);
    importPreset.setBounds (low.removeFromLeft (220).removeFromTop (32));
}

// ---------------------------------------------------------------- Settings page
SettingsPage::SettingsPage (Context& c, std::function<void (float)> setScale, std::function<void()> showAbout) : Page (c)
{
    const int scales[] { 50, 75, 90, 100, 110, 125, 150, 175, 200 };
    for (int i = 0; i < 9; ++i) scaleBox.addItem (juce::String (scales[i]) + " %", scales[i]);
    scaleBox.setSelectedId ((int) std::lround (ctx.settings.scale() * 100), juce::dontSendNotification);
    if (scaleBox.getSelectedId() == 0) scaleBox.setSelectedId (100, juce::dontSendNotification);
    scaleBox.onChange = [this, setScale] { setScale ((float) scaleBox.getSelectedId() / 100.0f); };
    animBox.addItemList ({ L ("Completas (60 fps)", "Full (60 fps)"), L ("Reducidas (15 fps)", "Reduced (15 fps)"), L ("Desactivadas", "Off") }, 1);
    animBox.setSelectedItemIndex (ctx.settings.animation(), juce::dontSendNotification);
    animBox.onChange = [this] { ctx.settings.setAnimation (animBox.getSelectedItemIndex()); };
    langBox.addItemList ({ L ("Automático (idioma del sistema)", "Automatic (system language)"), "English", juce::String::fromUTF8 ("Español") }, 1);
    const auto ls = ctx.settings.languageSetting();
    langBox.setSelectedItemIndex (ls == "en" ? 1 : ls == "es" ? 2 : 0, juce::dontSendNotification);
    langBox.onChange = [this]
    {
        const int i = langBox.getSelectedItemIndex();
        ctx.settings.setLanguageSetting (i == 1 ? "en" : i == 2 ? "es" : "auto");
        if (ctx.languageChanged) ctx.languageChanged();
    };
    gpuToggle.setToggleState (ctx.settings.gpu() && gpuStatus().load() >= 0, juce::dontSendNotification);
    gpuToggle.setEnabled (gpuStatus().load() >= 0);
    gpuToggle.setTooltip (L ("La habitación con shaders en la tarjeta gráfica. Desactívalo si tu equipo va justo.", "The room rendered with shaders on the graphics card. Turn it off on a slow computer."));
    gpuToggle.onClick = [this] { ctx.settings.setGpu (gpuToggle.getToggleState()); if (ctx.languageChanged) ctx.languageChanged(); };
    qualityBox.addItemList ({ L ("Calidad de escena: baja", "Scene quality: low"), L ("Calidad de escena: media", "Scene quality: medium"), L ("Calidad de escena: alta", "Scene quality: high") }, 1);
    qualityBox.setSelectedItemIndex (ctx.settings.gpuQuality(), juce::dontSendNotification);
    qualityBox.onChange = [this] { ctx.settings.setGpuQuality (qualityBox.getSelectedItemIndex()); if (ctx.languageChanged) ctx.languageChanged(); };
    preview.setToggleState (ctx.settings.previewOnSelect(), juce::dontSendNotification);
    preview.onClick = [this] { ctx.settings.setPreviewOnSelect (preview.getToggleState()); };
    openFolder.onClick = [] { Library::defaultRoot().startAsProcess(); };
    clearLearn.onClick = [this] { ctx.proc.clearMidiLearn(); if (ctx.toast) ctx.toast (L ("Asignaciones MIDI Learn borradas", "MIDI Learn assignments cleared")); };
    aboutBtn.onClick = std::move (showAbout);
    for (auto* comp : std::initializer_list<juce::Component*> { &scaleBox, &animBox, &langBox, &gpuToggle, &qualityBox, &preview, &openFolder, &clearLearn, &aboutBtn })
        addAndMakeVisible (comp);
}

void SettingsPage::paint (juce::Graphics& g)
{
    const auto gpuLine = L ("Escena GPU: ", "GPU scene: ") + (gpuStatus().load() < 0 ? L ("no disponible (se usa la escena por CPU) - ", "not available (the CPU scene is used) - ") : juce::String()) + getGpuInfo();
    auto r = getLocalBounds().reduced (12).removeFromLeft (680).toFloat();
    drawPanel (g, r);
    g.setColour (col::text);
    g.setFont (font (13.0f, true));
    const juce::String labels[] { L ("Tamaño de la interfaz", "Interface size"), L ("Animaciones", "Animations"), "Idioma / Language" };
    auto rows = getLocalBounds().reduced (28, 28).removeFromLeft (640);
    for (auto& l : labels) g.drawText (l, rows.removeFromTop (40), juce::Justification::centredLeft);
    rows.removeFromTop (40);
    g.setColour (col::textDim);
    g.setFont (font (12.0f));
    g.setColour (col::textDim); g.setFont (font (11.5f));
    g.drawFittedText (gpuLine, juce::Rectangle<int> (28, getHeight() - 120, 640, 40), juce::Justification::topLeft, 2);
    g.setColour (col::textDim); g.setFont (font (12.0f));
    g.drawFittedText (L ("Consejo: clic derecho en cualquier mando para asignarlo a tu controlador (MIDI Learn). El pedal de sustain (CC64) admite medio pedal.",
                         "Tip: right-click any knob to map it to your controller (MIDI Learn). The sustain pedal (CC64) supports half-pedaling."),
                      rows.removeFromTop (60).translated (0, 40), juce::Justification::topLeft, 2);
}

void SettingsPage::resized()
{
    auto rows = getLocalBounds().reduced (28, 28).removeFromLeft (640);
    auto row = [&] { auto x = rows.removeFromTop (40); return x.withTrimmedLeft (260).reduced (0, 7); };
    scaleBox.setBounds (row().withWidth (160));
    animBox.setBounds (row().withWidth (220));
    langBox.setBounds (row().withWidth (260));
    gpuToggle.setBounds (rows.removeFromTop (40).reduced (0, 8));
    qualityBox.setBounds (rows.removeFromTop (40).reduced (0, 6).withWidth (280));
    preview.setBounds (rows.removeFromTop (40).reduced (0, 8));
    rows.removeFromTop (60);
    auto b = rows.removeFromTop (34);
    openFolder.setBounds (b.removeFromLeft (200).reduced (2));
    clearLearn.setBounds (b.removeFromLeft (250).reduced (2));
    aboutBtn.setBounds (b.removeFromLeft (170).reduced (2));
}
}
