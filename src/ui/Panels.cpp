#include "Pages.h"
#include "Panels.h"
#include "../preset/Song.h"
#include "../preset/Render.h"
#include "../gen/Generator.h"

namespace gx::ui
{
// ================================================================ DragTile
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const auto c = ready ? col::accent : col::textDim;
    g.setColour (col::bgDeep);
    g.fillRoundedRectangle (r, 6.0f);
    if (busy) { g.setColour (col::accent.withAlpha (0.25f)); g.fillRoundedRectangle (r.withWidth (r.getWidth() * progress), 6.0f); }
    g.setColour (c.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    // grip dots
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 2; ++j) g.fillEllipse (r.getX() + 9.0f + (float) j * 5.0f, r.getCentreY() - 8.0f + (float) i * 7.0f, 2.6f, 2.6f);
    g.setColour (ready ? col::text : col::textDim);
    g.setFont (font (12.5f, true));
    g.drawText (text, r.withTrimmedLeft (26).withTrimmedBottom (sub.isEmpty() ? 0.0f : 10.0f), juce::Justification::centredLeft);
    if (sub.isNotEmpty()) { g.setColour (col::textDim); g.setFont (font (10.0f)); g.drawText (sub, r.withTrimmedLeft (26).withTrimmedTop (16.0f), juce::Justification::centredLeft); }
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (started || ! ready || e.getDistanceFromDragStart() < 8 || ! getFile) return;
    const auto f = getFile();
    if (! f.existsAsFile()) return;
    started = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { started = false; });
}

// ================================================================ StringsView
StringsView::StringsView() { setInterceptsMouseClicks (false, false); }

void StringsView::tick (const Frame& f)
{
    phase += 1.0f / 30.0f;
    pedal = f.pedal;
    for (int k = 21; k <= 108; ++k)
    {
        level[(size_t) k] += (f.keyLevel[(size_t) k] - level[(size_t) k]) * (f.keyLevel[(size_t) k] > level[(size_t) k] ? 0.7f : 0.12f);
        if (f.keyStamp[(size_t) k] != stamp[(size_t) k])
        {
            if (! first) { hammer[(size_t) k] = 1.0f; hammerVel[(size_t) k] = juce::jlimit (0.2f, 1.0f, f.keyHit[(size_t) k]); }
            stamp[(size_t) k] = f.keyStamp[(size_t) k];
        }
        hammer[(size_t) k] *= 0.72f;
    }
    first = false;
    repaint();
}

void StringsView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // cast-iron plate
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2118), r.getX(), r.getY(), juce::Colour (0xff120e0a), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colour (0xff5a4630).withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    const auto in = r.reduced (10.0f, 12.0f);
    const float bottom = in.getBottom() - 18.0f, top = in.getY() + 8.0f;
    // tuning pins and the bridge
    for (int k = 0; k < 88; ++k)
    {
        const float t = (float) k / 87.0f, x = in.getX() + in.getWidth() * t;
        const float yTop = top + (bottom - top) * (0.05f + 0.42f * std::pow (t, 0.85f));
        g.setColour (juce::Colour (0xff8a7552).withAlpha (0.55f));
        g.fillEllipse (x - 1.3f, yTop - 5.0f, 2.6f, 2.6f);
    }
    juce::Path bridge;
    for (int k = 0; k < 88; ++k)
    {
        const float t = (float) k / 87.0f, x = in.getX() + in.getWidth() * t, y = bottom - 8.0f - 10.0f * std::sin (t * 3.0f);
        if (k == 0) bridge.startNewSubPath (x, y); else bridge.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xff6b563a).withAlpha (0.7f));
    g.strokePath (bridge, juce::PathStrokeType (2.5f));
    const bool dampersUp = pedal > 0.5f;
    for (int k = 0; k < 88; ++k)
    {
        const int note = 21 + k;
        const float t = (float) k / 87.0f, x = in.getX() + in.getWidth() * t;
        const float yTop = top + (bottom - top) * (0.05f + 0.42f * std::pow (t, 0.85f));
        const float yBot = bottom - 8.0f - 10.0f * std::sin (t * 3.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, level[(size_t) note]);
        const int nStr = note < 30 ? 1 : note < 46 ? 2 : 3;
        const float wob = lv * 1.4f * std::sin (phase * (6.0f + (float) k * 0.25f) + (float) k);
        const auto c = juce::Colour (0xff9c8a64).interpolatedWith (col::accent, lv).brighter (lv * 0.6f);
        for (int s = 0; s < nStr; ++s)
        {
            const float dx = ((float) s - (float) (nStr - 1) * 0.5f) * 1.35f;
            juce::Path p;
            p.startNewSubPath (x + dx, yTop);
            p.quadraticTo (x + dx + wob, (yTop + yBot) * 0.5f, x + dx, yBot);
            if (lv > 0.03f) { g.setColour (col::accent.withAlpha (0.18f * lv)); g.strokePath (p, juce::PathStrokeType (4.0f)); }
            g.setColour (c.withAlpha (0.55f + 0.45f * lv));
            g.strokePath (p, juce::PathStrokeType (note < 40 ? 1.8f : note < 60 ? 1.3f : 0.9f));
        }
        // damper: a felt block on the strings, lifted off while the key is down or the pedal is
        const bool lifted = dampersUp || lv > 0.02f;
        g.setColour (juce::Colour (lifted ? 0xff3a2a22 : 0xff7a2f34).withAlpha (lifted ? 0.45f : 0.9f));
        g.fillRect (x - 1.9f, yTop + (yBot - yTop) * 0.22f + (lifted ? -7.0f : 0.0f), 3.8f, 4.0f);
        // hammer: rests below the strings, flies up and strikes
        const float h = hammer[(size_t) note];
        const float hy = yBot + 7.0f - h * 11.0f;
        g.setColour (juce::Colour (0xffe9dcc0).withAlpha (0.55f + 0.4f * h));
        g.fillRoundedRectangle (x - 1.7f, hy, 3.4f, 6.0f, 1.5f);
        if (h > 0.25f) { g.setColour (juce::Colours::white.withAlpha (0.8f * h * hammerVel[(size_t) note])); g.fillEllipse (x - 5.0f * h, yBot - 5.0f * h, 10.0f * h, 10.0f * h); }
    }
    // octave labels
    g.setColour (col::textDim); g.setFont (font (9.0f));
    for (int note = 24; note <= 108; note += 12)
    {
        const float t = (float) (note - 21) / 87.0f;
        g.drawText ("C" + juce::String (note / 12 - 1), juce::Rectangle<float> (in.getX() + in.getWidth() * t - 10.0f, in.getBottom() - 12.0f, 20.0f, 11.0f), juce::Justification::centred);
    }
    // pedals
    for (int i = 0; i < 3; ++i)
    {
        const auto pr = juce::Rectangle<float> (r.getRight() - 54.0f + (float) i * 16.0f, r.getBottom() - 16.0f, 11.0f, 7.0f);
        const bool on = i == 2 && pedal > 0.3f;
        g.setColour (on ? col::accent : juce::Colour (0xff6b563a)); g.fillRoundedRectangle (pr, 2.0f);
    }
}

// ================================================================ PianoPanel
PianoPanel::PianoPanel (Context& c) : Section (c, L ("Piano", "Piano"), "global"), piano (c, P::piano), tuning (c, P::tuning), master (c, P::master)
{
    accent = col::accent;
    for (auto* comp : std::initializer_list<juce::Component*> { &piano, &tuning, &master, &strings }) addAndMakeVisible (comp);
}

void PianoPanel::resized()
{
    auto r = content();
    auto left = r.removeFromLeft (112);
    piano.setBounds (left.removeFromTop (26)); left.removeFromTop (6);
    tuning.setBounds (left.removeFromTop (26)); left.removeFromTop (10);
    master.setBounds (left.removeFromTop (80).withSizeKeepingCentre (70, 80));
    r.removeFromLeft (8);
    strings.setBounds (r);
}

// ================================================================ HandsPanel
HandsPanel::HandsPanel (Context& c)
    : Section (c, L ("MANOS: tocan solas", "HANDS: they play by themselves"), "hands"),
      mode (c, P::hands), style (c, P::handsStyle), key (c, P::handsKey), scale (c, P::handsScale), bars (c, P::handsBars),
      tempo (c, P::handsTempo), density (c, P::handsDensity), express (c, P::handsExpress), pedal (c, P::handsPedal, "PEDAL"), endless (c, P::handsEndless, L ("SIN FIN", "ENDLESS"))
{
    accent = col::accent3;
    newVar.setButtonText (L ("Nueva variación", "New variation"));
    randomize.setButtonText (L ("Canción al azar", "Random song"));
    saveMidi.setButtonText (L ("Guardar MIDI…", "Save MIDI…"));
    saveWav.setButtonText (L ("Guardar WAV…", "Save WAV…"));
    lockStyle.setButtonText (L ("Estilo", "Style")); lockKey.setButtonText (L ("Tono", "Key")); lockScale.setButtonText (L ("Escala", "Scale")); lockTempo.setButtonText ("Tempo");
    for (auto* b : { &lockStyle, &lockKey, &lockScale, &lockTempo }) b->setTooltip (L ("Bloquea este parámetro: «Canción al azar» no lo cambiará", "Lock this parameter: \"Random song\" will not change it"));
    play.setTooltip (L ("Las manos tocan con el tempo de tu DAW (o el interno). Barra espaciadora en el Standalone.", "The hands play at your DAW tempo (or the internal one). Space bar in the Standalone."));
    newVar.setTooltip (L ("Misma configuración, otra canción: cambia solo la semilla", "Same settings, a different song: only the seed changes"));
    randomize.setTooltip (L ("Estilo, tono, escala, tempo y carácter nuevos (respeta los bloqueos) y otra semilla", "New style, key, scale, tempo and character (respects the locks) and a new seed"));
    midiTile.text = L ("Arrastra el MIDI", "Drag the MIDI"); midiTile.sub = L ("a tu DAW o escritorio", "into your DAW or desktop");
    midiTile.setTooltip (L ("Arrastra esta canción como archivo MIDI a una pista de tu DAW", "Drag this song as a MIDI file onto a track of your DAW"));
    wavTile.text = L ("Preparar WAV", "Prepare WAV"); wavTile.sub = L ("clic y luego arrastra", "click, then drag"); wavTile.ready = false;
    wavTile.setTooltip (L ("Renderiza la canción con el sonido actual; cuando esté lista, arrástrala a tu DAW", "Renders the song with the current sound; when it is ready, drag it into your DAW"));
    midiTile.getFile = [this] { return midiFile(); };
    wavTile.getFile = [this] { return wavFile; };
    wavTile.onClick = [this] { if (wavState.load() == 0) prepareWav(); };
    status.setFont (font (12.0f)); status.setColour (juce::Label::textColourId, col::textDim); status.setJustificationType (juce::Justification::centredLeft);
    for (auto* comp : std::initializer_list<juce::Component*> { &mode, &style, &key, &scale, &bars, &tempo, &density, &express, &pedal, &endless, &play, &newVar, &randomize, &saveMidi, &saveWav,
                                                                 &lockStyle, &lockKey, &lockScale, &lockTempo, &midiTile, &wavTile, &status })
        addAndMakeVisible (comp);
    play.onClick = [this]
    {
        if (ctx.proc.getParam (P::hands) < 0.5f) ctx.proc.setParamPlain (P::hands, 1.0f);
        ctx.proc.setPlaying (! ctx.proc.isPlayingInternal());
    };
    newVar.onClick = [this] { randomise (false); };
    randomize.onClick = [this] { randomise (true); };
    saveMidi.onClick = [this]
    {
        refreshScore();
        const auto def = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName ("Memoria - " + handsStyleNames()[(int) ctx.proc.getParam (P::handsStyle)] + " " + juce::String ((int) ctx.proc.getParam (P::handsSeed)) + ".mid"));
        chooser = std::make_unique<juce::FileChooser> (L ("Guardar canción como MIDI", "Save song as MIDI"), def, "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (writeMidi (score, ctx.proc.getParam (P::handsTempo), f.withFileExtension ("mid")) && ctx.toast) ctx.toast (L ("MIDI guardado: ", "MIDI saved: ") + f.getFileName());
        });
    };
    saveWav.onClick = [this]
    {
        if (rendering.load()) return;
        refreshScore();
        const auto def = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName ("Memoria - " + handsStyleNames()[(int) ctx.proc.getParam (P::handsStyle)] + " " + juce::String ((int) ctx.proc.getParam (P::handsSeed)) + ".wav"));
        chooser = std::make_unique<juce::FileChooser> (L ("Guardar canción como WAV", "Save song as WAV"), def, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File() || rendering.exchange (true)) return;
            if (worker.joinable()) worker.join();
            cancel = false; renderProgress = 0;
            const auto target = f.withFileExtension ("wav");
            const auto pr = exportPreset();
            const double secs = score.totalBeats * 60.0 / juce::jmax (30.0, (double) pr.params[(size_t) P::handsTempo]) + 8.0;
            worker = std::thread ([this, pr, secs, target]
            {
                const auto err = gx::exportAudio (pr, target, efWav, secs, 48000.0, &renderProgress, &cancel);
                rendering = false;
                juce::MessageManager::callAsync ([this, err, target] { if (ctx.toast) ctx.toast (err.isEmpty() ? L ("WAV guardado: ", "WAV saved: ") + target.getFileName() : err); });
            });
        });
    };
    refreshScore();
}

HandsPanel::~HandsPanel()
{
    cancel = true;
    if (worker.joinable()) worker.join();
}

uint32_t HandsPanel::songHash() const
{
    uint32_t h = 7;
    for (int i : { (int) P::hands, (int) P::handsStyle, (int) P::handsKey, (int) P::handsScale, (int) P::handsBars, (int) P::handsSeed, (int) P::handsPedal })
        h = h * 31u + (uint32_t) (int) ctx.proc.getParam (i);
    return h * 31u + (uint32_t) (ctx.proc.getParam (P::handsDensity) * 1000.0f) + 7u * (uint32_t) (ctx.proc.getParam (P::handsExpress) * 1000.0f) + 13u * (uint32_t) (int) ctx.proc.getParam (P::handsTempo);
}

void HandsPanel::refreshScore()
{
    compose (composeCfgFromPreset (Preset::capture (ctx.proc)), score);
    scoreHash = songHash();
}

Preset HandsPanel::exportPreset() const
{
    auto p = Preset::capture (ctx.proc);
    if (p.params[(size_t) P::hands] < 0.5f) p.params[(size_t) P::hands] = 1.0f;
    p.params[(size_t) P::handsEndless] = 0.0f;
    return p;
}

juce::File HandsPanel::midiFile()
{
    refreshScore();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Memoria");
    dir.createDirectory();
    const auto f = dir.getChildFile (juce::File::createLegalFileName ("Memoria - " + handsStyleNames()[(int) ctx.proc.getParam (P::handsStyle)] + " " + juce::String ((int) ctx.proc.getParam (P::handsSeed)) + ".mid"));
    writeMidi (score, ctx.proc.getParam (P::handsTempo), f);
    return f;
}

void HandsPanel::prepareWav()
{
    if (rendering.exchange (true)) return;
    if (worker.joinable()) worker.join();
    refreshScore();
    wavState = 1; wavHash = scoreHash; cancel = false; renderProgress = 0;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Memoria");
    dir.createDirectory();
    wavFile = dir.getChildFile (juce::File::createLegalFileName ("Memoria - " + handsStyleNames()[(int) ctx.proc.getParam (P::handsStyle)] + " " + juce::String ((int) ctx.proc.getParam (P::handsSeed)) + ".wav"));
    const auto pr = exportPreset();
    const auto target = wavFile;
    const double secs = score.totalBeats * 60.0 / juce::jmax (30.0, (double) pr.params[(size_t) P::handsTempo]) + 8.0;
    worker = std::thread ([this, pr, secs, target]
    {
        const auto err = gx::exportAudio (pr, target, efWav, secs, 48000.0, &renderProgress, &cancel);
        wavState = err.isEmpty() ? 2 : 0;
        rendering = false;
    });
}

void HandsPanel::randomise (bool all)
{
    auto& r = juce::Random::getSystemRandom();
    auto set = [&] (int idx, float v) { ctx.proc.setParamPlain (idx, v); };
    set (P::handsSeed, (float) (1 + r.nextInt (99999)));
    if (ctx.proc.getParam (P::hands) < 0.5f) set (P::hands, 1.0f);
    if (! all) return;
    if (! lockStyle.getToggleState()) set (P::handsStyle, (float) r.nextInt (numSongStyles));
    const int st = juce::jlimit (0, numSongStyles - 1, (int) ctx.proc.getParam (P::handsStyle));
    if (! lockKey.getToggleState()) set (P::handsKey, (float) r.nextInt (12));
    if (! lockScale.getToggleState())
    {
        static const int sc[6][3] { { 1, 2, 1 }, { 1, 0, 1 }, { 1, 7, 0 }, { 0, 1, 2 }, { 0, 3, 0 }, { 0, 1, 0 } };
        set (P::handsScale, (float) sc[st][r.nextInt (3)]);
    }
    if (! lockTempo.getToggleState())
    {
        static const int lo[6] { 66, 60, 56, 70, 80, 62 }, hi[6] { 88, 92, 84, 104, 110, 86 };
        set (P::handsTempo, (float) (lo[st] + r.nextInt (hi[st] - lo[st] + 1)));
    }
    set (P::handsDensity, 0.45f + 0.4f * r.nextFloat());
    set (P::handsExpress, 0.5f + 0.4f * r.nextFloat());
}

void HandsPanel::seekTo (const juce::MouseEvent& e)
{
    if (score.bars <= 0 || mapArea.isEmpty()) return;
    const float bar = juce::jlimit (0.0f, (float) score.bars - 0.01f, (float) std::floor ((float) (e.x - mapArea.getX()) / (float) mapArea.getWidth() * (float) score.bars));
    markerBar = bar;
    ctx.proc.seekSong (bar);
    repaint (mapArea);
}

void HandsPanel::mouseDown (const juce::MouseEvent& e) { if (mapArea.contains (e.getPosition())) seekTo (e); else Section::mouseDown (e); }
void HandsPanel::mouseDrag (const juce::MouseEvent& e) { if (mapArea.contains (e.getPosition()) || e.mouseWasDraggedSinceMouseDown()) { if (e.getMouseDownPosition().x >= mapArea.getX() && mapArea.contains (e.getMouseDownPosition())) seekTo (e); } }

void HandsPanel::tick (const Frame& f)
{
    const bool on = ctx.proc.isPlayingInternal();
    running = (on || ctx.proc.isHostPlaying()) && ctx.proc.getParam (P::hands) > 0.5f;
    songBar = f.songBar; songBars = f.songBars;
    play.setButtonText (on ? L ("PARAR", "STOP") : "PLAY");
    play.setToggleState (on, juce::dontSendNotification);
    if (songHash() != scoreHash)
    {
        refreshScore();
        if (wavState.load() == 2 && wavHash != scoreHash) { wavState = 0; wavFile.deleteFile(); }
    }
    if (wavState.load() == 1) { wavTile.busy = true; wavTile.ready = false; wavTile.progress = renderProgress.load(); wavTile.text = L ("Renderizando… ", "Rendering… ") + juce::String ((int) (renderProgress.load() * 100)) + " %"; wavTile.sub = {}; }
    else if (wavState.load() == 2) { wavTile.busy = false; wavTile.ready = true; wavTile.text = L ("Arrastra el WAV", "Drag the WAV"); wavTile.sub = L ("listo para tu DAW", "ready for your DAW"); }
    else { wavTile.busy = false; wavTile.ready = false; wavTile.text = L ("Preparar WAV", "Prepare WAV"); wavTile.sub = L ("clic y luego arrastra", "click, then drag"); }
    const double secs = score.totalBeats * 60.0 / juce::jmax (30.0, (double) ctx.proc.getParam (P::handsTempo));
    static const char* en[] { "Intro", "Verse", "Chorus", "Bridge", "Outro", "Pre-chorus" };
    juce::String sec;
    if (running)
        for (auto& s : score.sections)
            if (songBar >= s.startBar && songBar < s.startBar + s.bars)
            { const int k = s.type == 'I' ? 0 : s.type == 'A' ? 1 : s.type == 'B' ? 2 : s.type == 'C' ? 3 : s.type == 'P' ? 5 : 4; sec = en[k]; }
    status.setText (running ? L ("Compás ", "Bar ") + juce::String (songBar + 1) + " / " + juce::String (score.bars) + "   " + sec
                            : L ("Duración: ", "Length: ") + juce::String ((int) secs / 60) + ":" + juce::String ((int) secs % 60).paddedLeft ('0', 2) + L ("   ·   pulsa PLAY", "   ·   press PLAY"),
                    juce::dontSendNotification);
    repaint (mapArea);
}

void HandsPanel::paint (juce::Graphics& g)
{
    Section::paint (g);
    auto r = mapArea.toFloat();
    if (r.isEmpty() || score.bars <= 0 || score.notes.empty()) return;
    g.setColour (col::bgDeep); g.fillRoundedRectangle (r, 5.0f);
    const float labelH = 14.0f;
    auto bandR = r.withHeight (labelH);
    auto roll = r.withTrimmedTop (labelH + 2.0f).reduced (2.0f, 3.0f);
    for (auto& s : score.sections)
    {
        const float x0 = r.getX() + r.getWidth() * (float) s.startBar / (float) score.bars, w = r.getWidth() * (float) s.bars / (float) score.bars;
        const auto c = s.type == 'B' ? col::accent : s.type == 'C' ? col::accent2 : (s.type == 'A' || s.type == 'P') ? col::accent3 : col::textDim;
        g.setColour (c.withAlpha (running ? 0.30f : 0.2f));
        g.fillRect (juce::Rectangle<float> (x0, bandR.getY(), w, labelH).reduced (1.0f, 0.0f));
        g.setColour (c.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (x0, roll.getY() - 2.0f, w, roll.getHeight() + 4.0f).reduced (1.0f, 0.0f));
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (font (9.5f, true));
        if (w > 30.0f) g.drawText (s.type == 'I' ? "INTRO" : s.type == 'A' ? L ("ESTROFA", "VERSE") : s.type == 'P' ? L ("PRE", "PRE") : s.type == 'B' ? L ("ESTRIBILLO", "CHORUS") : s.type == 'C' ? L ("PUENTE", "BRIDGE") : "OUTRO", juce::Rectangle<float> (x0, bandR.getY(), w, labelH), juce::Justification::centred);
    }
    int lo = 127, hi = 0;
    for (auto& n : score.notes) { lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); }
    const float span = (float) juce::jmax (12, hi - lo);
    for (auto& n : score.notes)
    {
        const float x = roll.getX() + roll.getWidth() * (float) (n.start / score.totalBeats), w = juce::jmin (juce::jmax (1.5f, roll.getWidth() * (float) (n.len / score.totalBeats)), roll.getRight() - x);
        const float y = roll.getBottom() - roll.getHeight() * (float) (n.note - lo) / span;
        g.setColour (col::accent.interpolatedWith (col::accent2, (float) n.vel / 127.0f).withAlpha (0.35f + 0.6f * (float) n.vel / 127.0f));
        g.fillRect (x, y - 1.0f, w, 2.0f);
    }
    if (running)
    {
        const float x = r.getX() + r.getWidth() * (float) songBar / (float) score.bars;
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
    else if (markerBar >= 0.0f)
    {
        const float x = r.getX() + r.getWidth() * markerBar / (float) score.bars;
        g.setColour (col::accent.withAlpha (0.9f)); g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
}

void HandsPanel::resized()
{
    auto r = content();
    auto row = r.removeFromTop (30);
    play.setBounds (row.removeFromLeft (92).reduced (0, 0));
    row.removeFromLeft (6);
    mode.setBounds (row.removeFromLeft (118)); row.removeFromLeft (4);
    style.setBounds (row.removeFromLeft (140)); row.removeFromLeft (4);
    key.setBounds (row.removeFromLeft (52)); row.removeFromLeft (4);
    scale.setBounds (row.removeFromLeft (126)); row.removeFromLeft (4);
    bars.setBounds (row.removeFromLeft (84));
    r.removeFromTop (6);
    auto k = r.removeFromTop (80);
    tempo.setBounds (k.removeFromLeft (66)); density.setBounds (k.removeFromLeft (66)); express.setBounds (k.removeFromLeft (66));
    k.removeFromLeft (10);
    auto tg = k.removeFromLeft (110);
    pedal.setBounds (tg.removeFromTop (28).reduced (0, 4)); endless.setBounds (tg.removeFromTop (28).reduced (0, 4));
    auto btn = k.reduced (0, 6);
    auto b1 = btn.removeFromTop (30);
    newVar.setBounds (b1.removeFromLeft (btn.getWidth() / 2 - 3)); b1.removeFromLeft (6); randomize.setBounds (b1);
    btn.removeFromTop (6);
    auto lk = btn;
    const int lw = lk.getWidth() / 4;
    lockStyle.setBounds (lk.removeFromLeft (lw)); lockKey.setBounds (lk.removeFromLeft (lw)); lockScale.setBounds (lk.removeFromLeft (lw)); lockTempo.setBounds (lk);
    r.removeFromTop (8);
    auto ex = r.removeFromBottom (44);
    status.setBounds (r.removeFromTop (20));
    r.removeFromTop (2);
    mapArea = r.withTrimmedBottom (8);
    mapArea.setHeight (juce::jmax (30, mapArea.getHeight()));
    const int w = (ex.getWidth() - 3 * 8) / 4;
    midiTile.setBounds (ex.removeFromLeft (w)); ex.removeFromLeft (8);
    saveMidi.setBounds (ex.removeFromLeft (w).reduced (0, 6)); ex.removeFromLeft (8);
    wavTile.setBounds (ex.removeFromLeft (w)); ex.removeFromLeft (8);
    saveWav.setBounds (ex.reduced (0, 6));
}

// ================================================================ SoundGenPanel
SoundGenPanel::SoundGenPanel (Context& c) : Section (c, L ("SONIDO AL AZAR", "RANDOM SOUND"), "gen")
{
    accent = col::accent2;
    category.addItem (L ("Todas las categorías", "All categories"), 1);
    for (int i = 0; i < categories().size(); ++i) category.addItem (categoryLabel (categories()[i]), i + 2);
    category.setSelectedId (1, juce::dontSendNotification);
    surprise.setButtonText (L ("Sorpréndeme", "Surprise me"));
    surprise.setTooltip (L ("Genera un sonido nuevo (piano, sala, capas, macros) de la categoría elegida", "Generates a brand-new sound (piano, room, layers, macros) in the chosen category"));
    lockPiano.setButtonText ("Piano"); lockRoom.setButtonText (L ("Sala", "Room")); lockLayers.setButtonText (L ("Capas", "Layers")); lockMacros.setButtonText ("Macros");
    for (auto* b : { &lockPiano, &lockRoom, &lockLayers, &lockMacros }) b->setTooltip (L ("Bloqueado: se conserva al generar", "Locked: kept when generating"));
    for (auto* comp : std::initializer_list<juce::Component*> { &category, &surprise, &lockPiano, &lockRoom, &lockLayers, &lockMacros }) addAndMakeVisible (comp);
    surprise.onClick = [this] { generate(); };
}

SoundGenPanel::~SoundGenPanel() { *alive = false; }

void SoundGenPanel::resized()
{
    auto r = content();
    category.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
    surprise.setBounds (r.removeFromTop (34)); r.removeFromTop (6);
    auto a = r.removeFromTop (24); lockPiano.setBounds (a.removeFromLeft (a.getWidth() / 2)); lockRoom.setBounds (a);
    auto b = r.removeFromTop (24); lockLayers.setBounds (b.removeFromLeft (b.getWidth() / 2)); lockMacros.setBounds (b);
}

void SoundGenPanel::generate()
{
    if (busy.exchange (true)) return;
    surprise.setEnabled (false);
    const int id = category.getSelectedId();
    auto& rnd = juce::Random::getSystemRandom();
    const juce::String cat = id >= 2 ? categories()[id - 2] : categories()[rnd.nextInt (categories().size())];
    const uint32_t seed = (uint32_t) rnd.nextInt();
    const Preset current = Preset::capture (ctx.proc);
    const bool lp = lockPiano.getToggleState(), lr = lockRoom.getToggleState(), ll = lockLayers.getToggleState(), lm = lockMacros.getToggleState();
    auto flag = alive;
    std::thread ([this, flag, cat, seed, current, lp, lr, ll, lm]
    {
        GenRequest rq; rq.category = cat; rq.seed = seed;
        auto p = Generator::generate (rq);
        for (auto& d : paramDefs())
        {
            const bool keep = d.group == "hands" || (lp && (d.group == "piano" || d.index == P::piano || d.index == P::tuning)) || (lr && d.group == "space")
                              || (ll && d.group == "life") || (lm && d.group == "macro");
            if (keep) p.params[(size_t) d.index] = current.params[(size_t) d.index];
        }
        p.meta.factory = false; p.meta.author = "Memoria"; p.meta.name = "Surprise: " + p.meta.name;
        juce::MessageManager::callAsync ([this, flag, p]
        {
            if (! *flag) return;
            p.applyTo (ctx.proc);
            ctx.undo.reset();
            if (onGenerated) onGenerated (p);
            if (ctx.toast) ctx.toast (L ("Sonido nuevo: ", "New sound: ") + p.meta.name);
            surprise.setEnabled (true);
            busy = false;
        });
    }).detach();
}
}
