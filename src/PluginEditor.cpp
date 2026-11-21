#include "PluginEditor.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define GX_HAS_BINARYDATA 1
#endif

LibraryHolder::LibraryHolder()
{
#if GX_HAS_BINARYDATA
    int size = 0;
    if (auto* data = BinaryData::getNamedResource ("factory_memoriabank", size))
        library.setFactoryBank (juce::String::fromUTF8 (data, size));
#endif
}

MemoriaEditor::MemoriaEditor (MemoriaProcessor& p)
    : AudioProcessorEditor (p), proc (p), undo (p),
      ctx { p, settings, libHolder->library, undo, {}, {}, {} },
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      frames (p)
{
    gx::uiEnglish() = settings.messageLanguage() == "en"; // before any page builds its labels
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    ctx.toast = [this] (const juce::String& m) { toast (m); };
    ctx.refreshAll = [this] { refreshAll(); };
    ctx.startMidiLearn = [this] (int param)
    {
        proc.learnParam.store (param);
        toast (gx::L ("MIDI Learn: mueve un control de tu controlador para ", "MIDI Learn: move a control on your controller for ") + gx::paramDefs()[(size_t) param].name);
    };

    ctx.languageChanged = [this] { pendingRebuild = true; }; // rebuilt from the timer, never inside the combo callback

    addAndMakeVisible (root);
    root.addAndMakeVisible (keyboard);
    root.addChildComponent (toastLabel);
    buildUi();
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, gx::ui::col::accent.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, gx::ui::col::accent.withAlpha (0.3f));
    keyboard.setLowestVisibleKey (24);
    keyboard.setKeyWidth (18.0f);
    toastLabel.setColour (juce::Label::backgroundColourId, gx::ui::col::bgDeep.withAlpha (0.92f));
    toastLabel.setColour (juce::Label::outlineColourId, gx::ui::col::accent);
    toastLabel.setColour (juce::Label::textColourId, gx::ui::col::text);
    toastLabel.setJustificationType (juce::Justification::centred);
    toastLabel.setFont (gx::ui::font (13.0f, true));

    tooltips = std::make_unique<juce::TooltipWindow> (&root, 600);
    constrainer.setFixedAspectRatio ((double) kDesignW / kDesignH);
    constrainer.setSizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        constrainer.setFixedAspectRatio (0.0); // the Standalone window can take any shape (and be maximised)
    if (! settings.file().containsKey ("launched"))   // first launch: always 100 %
    {
        settings.setScale (1.0f);
        settings.file().setValue ("launched", true);
        settings.file().saveIfNeeded();
    }
    const float s = initialScale();
    setSize ((int) (kDesignW * s), (int) (kDesignH * s));
    header->setPresetName (proc.currentMeta.name);
    showPage (0);
    lastTime = gx::ui::nowSeconds();
    lastPresetGen = proc.presetGeneration.load();
    startTimerHz (60);
}

// Builds header + pages with the current language (gx::uiEnglish()).
void MemoriaEditor::buildUi()
{
    header = std::make_unique<gx::ui::Header> (ctx, [this] (int t) { showPage (t); });
    pianoPage = std::make_unique<gx::ui::PianoPage> (ctx);
    lifePage = std::make_unique<gx::ui::LifePage> (ctx);
    presets = std::make_unique<gx::ui::PresetsPage> (ctx);
    exportPage = std::make_unique<gx::ui::ExportPage> (ctx);
    helpPage = std::make_unique<gx::ui::HelpPage> (ctx);
    settingsPage = std::make_unique<gx::ui::SettingsPage> (ctx, [this] (float s) { setUiScale (s); }, [this] { about->setVisible (true); about->toFront (false); });
    about = std::make_unique<gx::ui::AboutOverlay>();
    pages = { pianoPage.get(), lifePage.get(), presets.get(), exportPage.get(), settingsPage.get(), helpPage.get() };
    root.addAndMakeVisible (*header);
    for (auto* pg : pages) root.addChildComponent (pg);
    root.addChildComponent (*about);
    toastLabel.toFront (false);

    header->onPresetStep = [this] (int d) { presets->step (d); };
    header->onAbout = [this] { about->setVisible (true); about->toFront (false); };
    header->onAB = [this] { toggleAB(); };
    header->onUndo = [this] { undo.performUndo(); refreshAll(); };
    header->onRedo = [this] { undo.performRedo(); refreshAll(); };
    header->abButton.setButtonText (abActive == 0 ? "A" : "B");
    presets->onLoaded = [this] (const gx::Preset& pr) { header->setPresetName (pr.meta.name); };
}

// Language switch: recreate every page so all labels, menus and tooltips change at once.
void MemoriaEditor::rebuildUi()
{
    pendingRebuild = false;
    gx::uiEnglish() = settings.messageLanguage() == "en";
    const int page = currentPage;
    pages.clear();
    header.reset(); pianoPage.reset(); lifePage.reset(); presets.reset(); exportPage.reset(); helpPage.reset(); settingsPage.reset(); about.reset();
    buildUi();
    resized();
    header->setPresetName (proc.currentMeta.name);
    showPage (page);
    toast (gx::L ("Idioma: Español", "Language: English"));
}

MemoriaEditor::~MemoriaEditor()
{
    stopTimer();
    proc.keyboardState.allNotesOff (1);
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lnf) juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MemoriaEditor::paint (juce::Graphics& g) { g.fillAll (gx::ui::col::bg); }

void MemoriaEditor::resized()
{
    // scale to fit; any extra width/height (maximised Standalone, other screen ratios) goes to the pages, which stretch
    const float s = juce::jmin ((float) getWidth() / (float) kDesignW, (float) getHeight() / (float) kDesignH);
    root.setBounds (0, 0, juce::jmax (kDesignW, (int) std::floor ((float) getWidth() / s)), juce::jmax (kDesignH, (int) std::floor ((float) getHeight() / s)));
    root.setTransform (juce::AffineTransform::scale (s));
    auto r = root.getLocalBounds();
    header->setBounds (r.removeFromTop (64));
    keyboard.setBounds (r.removeFromBottom (52).reduced (4, 2));
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 75.0f);   // all 75 white keys (MIDI 0-127) always fill the width
    for (auto* pg : pages) pg->setBounds (r);
    about->setBounds (root.getLocalBounds());
    toastLabel.setBounds (root.getLocalBounds().withSizeKeepingCentre (560, 34).withY (root.getHeight() - 110));
}

// The saved interface size, but never bigger than the screen we open on (e.g. 1280x800 on a 150 % laptop display)
float MemoriaEditor::initialScale() const
{
    float s = juce::jlimit (0.5f, 2.0f, settings.scale());
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = d->userArea;
        s = juce::jmin (s, (float) (area.getWidth() - 40) / kDesignW, (float) (area.getHeight() - 90) / kDesignH);
    }
    return juce::jmax (0.5f, s);
}

// Standalone: JUCE's window only shows minimise + close; add the maximise button, and open at the proper size once.
void MemoriaEditor::parentHierarchyChanged()
{
    if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone) return;
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
    {
        w->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
        if (! sizedInWindow)
        {
            sizedInWindow = true;
            const float s = initialScale();
            juce::Component::SafePointer<MemoriaEditor> sp (this);
            juce::MessageManager::callAsync ([sp, s]
            {
                if (sp == nullptr) return;
                sp->setSize ((int) (kDesignW * s), (int) (kDesignH * s));
                if (auto* win = sp->findParentComponentOfClass<juce::DocumentWindow>()) win->centreWithSize (win->getWidth(), win->getHeight());
            });
        }
    }
}

void MemoriaEditor::setUiScale (float s)
{
    settings.setScale (s);
    setSize ((int) (kDesignW * s), (int) (kDesignH * s));
}

void MemoriaEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (int i = 0; i < (int) pages.size(); ++i) pages[(size_t) i]->setVisible (i == currentPage);
    header->setTab (currentPage);
    pages[(size_t) currentPage]->pageShown();
}


void MemoriaEditor::toast (const juce::String& msg)
{
    toastLabel.setText (msg, juce::dontSendNotification);
    toastLabel.setVisible (true);
    toastLabel.toFront (false);
    toastUntil = gx::ui::nowSeconds() + 2.6;
}

void MemoriaEditor::refreshAll()
{
    header->setPresetName (proc.currentMeta.name);
    for (auto* pg : pages) pg->refresh();
    root.repaint();
}

void MemoriaEditor::toggleAB()
{
    abSlots[(size_t) abActive] = gx::Preset::capture (proc);
    abValid[(size_t) abActive] = true;
    abActive ^= 1;
    if (! abValid[(size_t) abActive]) { abSlots[(size_t) abActive] = abSlots[(size_t) (abActive ^ 1)]; abValid[(size_t) abActive] = true; }
    const auto meta = proc.currentMeta;
    abSlots[(size_t) abActive].applyTo (proc);
    proc.currentMeta = meta;
    header->abButton.setButtonText (abActive == 0 ? "A" : "B");
    toast (abActive == 0 ? gx::L ("Comparando: A", "Comparing: A") : gx::L ("Comparando: B (edita libremente, A se conserva)", "Comparing: B (edit freely, A is kept)"));
    refreshAll();
}

void MemoriaEditor::advanceFrames (double dt)
{
    frames.update (frame, dt);
    frame.reduced = settings.animation() == 1;
    header->tick (frame);
    pages[(size_t) currentPage]->tick (frame);
}

void MemoriaEditor::timerCallback()
{
    const double now = gx::ui::nowSeconds();
    const double dt = juce::jlimit (0.0, 0.25, now - lastTime);
    const int anim = settings.animation();
    const double interval = anim == 0 ? 0.0 : anim == 1 ? 1.0 / 15.0 : 0.5;
    if (now - slowTickAt >= interval)
    {
        slowTickAt = now;
        lastTime = now;
        advanceFrames (dt);
    }
    if (now > undoPollAt)
    {
        undoPollAt = now + 0.3;
        undo.poll (juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown());
        header->undoButton.setEnabled (undo.canUndo());
        header->redoButton.setEnabled (undo.canRedo());
        if (const int g = proc.presetGeneration.load(); g != lastPresetGen) { lastPresetGen = g; refreshAll(); }
    }
    if (toastLabel.isVisible() && now > toastUntil) toastLabel.setVisible (false);
    if (pendingRebuild && ! exportPage->isBusy()) rebuildUi(); // waits for a running generation / export
}

bool MemoriaEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z' && ! mods.isShiftDown()) { undo.performUndo(); refreshAll(); return true; }
    if (mods.isCommandDown() && (k.getKeyCode() == 'Y' || (k.getKeyCode() == 'Z' && mods.isShiftDown()))) { undo.performRedo(); refreshAll(); return true; }
    if (k.getKeyCode() == juce::KeyPress::spaceKey && ! mods.isAnyModifierKeyDown() && proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone) { proc.setPlaying (! proc.isPlayingInternal()); return true; }
    return false;
}
