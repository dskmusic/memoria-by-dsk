#include "Pages.h"

namespace gx::ui
{
namespace
{
juce::Label& addLabel (Box& box, const juce::String& text)
{
    auto& l = box.add<juce::Label> (juce::String(), text);
    l.setFont (font (10.5f, true));
    l.setColour (juce::Label::textColourId, col::textDim);
    l.setJustificationType (juce::Justification::centredLeft);
    return l;
}
// one row of knobs using the full height (capped), centred vertically
void rowKnobs (std::vector<juce::Component*> ks, juce::Rectangle<int> r, int maxH = 130)
{
    const int w = r.getWidth() / (int) ks.size(), h = juce::jmin (maxH, r.getHeight());
    r = r.withSizeKeepingCentre (r.getWidth(), h);
    for (auto* k : ks) k->setBounds (r.removeFromLeft (w));
}
}

// ================================================================ Landscape page
LandscapePage::LandscapePage (Context& c)
    : Page (c), play (c.proc), freeze (c, P::freeze, L ("CONGELAR", "FREEZE")),
      harmony (c, L ("Armonía", "Harmony"), "global"), macros (c, "Macros", "macro"), session (c, L ("Sesión", "Session"), "global")
{
    addAndMakeVisible (landscape);
    landscape.setGpu (c.settings.gpu());
    landscape.onClick = [this] { if (! ctx.proc.isPlayingInternal() && ! ctx.proc.isHostPlaying()) ctx.proc.setPlaying (true); };
    for (int s = 0; s < numScenes; ++s)
    {
        auto* b = sceneButtons.add (new juce::TextButton (sceneName (s)));
        b->setClickingTogglesState (false);
        b->setTooltip (L ("Paisaje visual (se guarda con el preset)", "Visual landscape (saved with the preset)"));
        b->onClick = [this, s] { ctx.proc.setParamPlain (P::scene, (float) s); };
        landscape.addAndMakeVisible (b);   // children of the landscape: an OpenGL view would hide siblings drawn over it
    }
    for (int m = 0; m < 8; ++m)
    {
        auto* b = memButtons.add (new MemButton (juce::String (m + 1)));
        b->setTooltip (L ("Memoria rápida: clic para recuperar (con fundido) o guardar si está vacía; Mayús+clic guarda de nuevo; mantener pulsado la borra",
                          "Quick memory: click to recall (with a crossfade) or store if empty; Shift+click stores again; press and hold to clear"));
        b->onLongPress = [this, m]
        {
            if (ctx.proc.memories[(size_t) m].isVoid()) return;
            ctx.proc.memories[(size_t) m] = juce::var();
            if (ctx.toast) ctx.toast (L ("Memoria ", "Memory ") + juce::String (m + 1) + L (" borrada", " cleared"));
        };
        b->onClick = [this, m]
        {
            const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
            if (mods.isShiftDown() || mods.isPopupMenu() || ctx.proc.memories[(size_t) m].isVoid()) storeMemory (m);
            else recallMemory (m);
        };
        b->getProperties().set ("small", true);
        addAndMakeVisible (b);
    }
    fullscreen.setButtonText (L ("Pantalla completa", "Full screen"));
    for (auto* b : sceneButtons) b->getProperties().set ("small", true);
    fullscreen.setTooltip (L ("Paisaje a pantalla completa (Esc para salir, H para ver los atajos)", "Full-screen landscape (Esc to exit, H for shortcuts)"));
    fullscreen.onClick = [this] { if (onFullscreen) onFullscreen(); };
    landscape.addAndMakeVisible (fullscreen);
    addAndMakeVisible (play);
    addAndMakeVisible (freeze);
    freeze.setTooltip (L ("Congela el paisaje: la evolución se detiene y la textura y el shimmer se quedan suspendidos.",
                          "Freezes the landscape: evolution stops, texture and shimmer hang in the air."));
    seedLabel.setJustificationType (juce::Justification::centred);
    seedLabel.setFont (techFont (14.0f));
    seedLabel.setColour (juce::Label::textColourId, col::accent2);
    seedLabel.setTooltip (L ("La semilla decide el recorrido del paisaje: misma semilla = mismo viaje.", "The seed decides the journey: same seed = same landscape."));
    addAndMakeVisible (seedLabel);
    for (auto* b : { &dice, &seedPrev, &seedNext }) addAndMakeVisible (b);
    auto setSeed = [this] (int s) { ctx.proc.setParamPlain (P::seed, (float) juce::jlimit (1, 99999, s)); };
    dice.setTooltip (L ("Otra semilla al azar: mismo sonido, otro viaje.", "Random seed: same sound, a new journey."));
    dice.onClick = [setSeed] { setSeed (1 + juce::Random::getSystemRandom().nextInt (99999)); };
    seedPrev.onClick = [this, setSeed] { setSeed ((int) ctx.proc.getParam (P::seed) - 1); };
    seedNext.onClick = [this, setSeed] { setSeed ((int) ctx.proc.getParam (P::seed) + 1); };

    // ---- harmony
    harmony.accent = col::accent3;
    auto& keyL = addLabel (harmony, L ("TONALIDAD", "KEY"));
    auto& scaleL = addLabel (harmony, L ("ESCALA", "SCALE"));
    auto& key = harmony.add<ParamCombo> (c, P::key);
    auto& scale = harmony.add<ParamCombo> (c, P::scale);
    auto& oct = harmony.add<ParamKnob> (c, P::octave, L ("OCTAVA", "OCTAVE"), L ("Registro del drone y del pad", "Register of the drone and pad"));
    auto& harm = harmony.add<ParamKnob> (c, P::harmony, L ("ARMONÍA", "HARMONY"), L ("De quintas abiertas (0) a acordes de 7ª y 9ª (100 %)", "From open fifths (0) to 7th and 9th chords (100 %)"));
    auto& chords = harmony.add<ParamKnob> (c, P::chordRate, L ("ACORDES", "CHORDS"), L ("Cada cuánto cambia el acorde: de ~48 s a ~6 s", "How often the chord changes: ~48 s down to ~6 s"));
    auto& sync = harmony.add<ParamToggle> (c, P::syncChords, L ("SINCRO TEMPO", "TEMPO SYNC"));
    sync.setTooltip (L ("Cambia de acorde cada N compases del tempo de tu DAW (sin DAW: 120 BPM)", "Change chords every N bars of your DAW tempo (without a DAW: 120 BPM)"));
    auto& bars = harmony.add<ParamCombo> (c, P::chordBars);
    for (auto* k : { &oct, &harm, &chords }) k->setAccent (col::accent3);
    harmony.layout = [&keyL, &scaleL, &key, &scale, &oct, &harm, &chords, &sync, &bars] (juce::Rectangle<int> r)
    {
        auto labels = r.removeFromTop (14);
        keyL.setBounds (labels.removeFromLeft (r.getWidth() * 2 / 5));
        scaleL.setBounds (labels);
        auto row = r.removeFromTop (26);
        key.setBounds (row.removeFromLeft (r.getWidth() * 2 / 5).withTrimmedRight (6));
        scale.setBounds (row);
        r.removeFromTop (4);
        auto srow = r.removeFromBottom (22);
        sync.setBounds (srow.removeFromLeft (srow.getWidth() / 2).withTrimmedLeft (4));
        bars.setBounds (srow.reduced (2, 0));
        rowKnobs ({ &oct, &harm, &chords }, r, 100);
    };
    addAndMakeVisible (harmony);

    // ---- macros
    macros.accent = col::accent;
    const juce::String names[] { L ("EVOLUCIÓN", "EVOLUTION"), L ("DENSIDAD", "DENSITY"), L ("LUZ", "LIGHT"), L ("ESPACIO", "SPACE") };
    const juce::String tips[] { L ("Velocidad a la que cambia todo: de casi inmóvil a un paisaje en continuo movimiento", "How fast everything changes: from nearly still to constantly moving"),
                                L ("Cuántas capas suenan a la vez y cuántos granos y campanas aparecen", "How many layers play at once, how many grains and bells appear"),
                                L ("De noche cerrada (oscuro, cálido) a mediodía (brillante, abierto)", "From deep night (dark, warm) to midday (bright, open)"),
                                L ("Tamaño y cantidad de reverb: de una sala a una catedral infinita", "Reverb size and amount: from a room to an endless cathedral") };
    const int ids[] { P::evolution, P::density, P::light, P::space };
    const juce::Colour cols[] { col::accent, col::accent3, col::accent2, juce::Colour (0xff6f8cff) };
    std::vector<juce::Component*> mk;
    for (int i = 0; i < 4; ++i)
    {
        auto& k = macros.add<ParamKnob> (c, ids[i], names[i], tips[i]);
        k.setAccent (cols[i]);
        k.labelSize = 12.0f;
        mk.push_back (&k);
    }
    macros.layout = [mk] (juce::Rectangle<int> r) { rowKnobs (mk, r.withTrimmedTop (4), 150); };
    addAndMakeVisible (macros);

    // ---- session
    session.accent = col::accent2;
    auto& tunL = addLabel (session, L ("AFINACIÓN", "TUNING"));
    auto& midiL = addLabel (session, "MIDI");
    auto& tun = session.add<ParamCombo> (c, P::tuning);
    auto& midi = session.add<ParamCombo> (c, P::midiMode);
    tun.setTooltip (L ("La 4 a 440 Hz (estándar) o 432 Hz", "A4 at 440 Hz (standard) or 432 Hz"));
    midi.setTooltip (L ("Teclado MIDI: cambiar la tonalidad, tocar notas encima, o ambas", "MIDI keyboard: set the key, play notes on top, or both"));
    auto& timer = session.add<ParamKnob> (c, P::timer, L ("TEMPORIZADOR", "TIMER"), L ("Minutos hasta que se apague solo con un fundido (0 = sin límite)", "Minutes until it fades out and stops by itself (0 = no limit)"));
    auto& vol = session.add<ParamKnob> (c, P::master, L ("VOLUMEN", "VOLUME"));
    auto& sleep = session.add<ParamToggle> (c, P::sleepMode, L ("MODO DORMIR", "SLEEP MODE"));
    sleep.setTooltip (L ("Con el temporizador: el paisaje se oscurece, se calma y se apaga poco a poco hasta el silencio", "With the timer: the landscape darkens, calms down and slowly fades to silence"));
    for (auto* k : { &timer, &vol }) k->setAccent (col::accent2);
    session.layout = [&tunL, &midiL, &tun, &midi, &timer, &vol, &sleep] (juce::Rectangle<int> r)
    {
        auto labels = r.removeFromTop (14);
        tunL.setBounds (labels.removeFromLeft (r.getWidth() / 2));
        midiL.setBounds (labels);
        auto row = r.removeFromTop (26);
        tun.setBounds (row.removeFromLeft (r.getWidth() / 2).withTrimmedRight (6));
        midi.setBounds (row);
        r.removeFromTop (4);
        sleep.setBounds (r.removeFromBottom (22).withTrimmedLeft (4));
        rowKnobs ({ &timer, &vol }, r, 100);
    };
    addAndMakeVisible (session);
}

void LandscapePage::storeMemory (int slot)
{
    ctx.proc.memories[(size_t) slot] = Preset::capture (ctx.proc).toVar();
    if (ctx.toast) ctx.toast (L ("Guardado en la memoria ", "Stored in memory ") + juce::String (slot + 1));
}

// Recall: float parameters glide over 2 s; switches / choices change half-way, under a short dip (no clicks)
void LandscapePage::recallMemory (int slot)
{
    const auto target = Preset::fromVar (ctx.proc.memories[(size_t) slot]);
    morph.from = Preset::capture (ctx.proc).params;
    morph.to = target.params;
    morph.start = nowSeconds();
    morph.switched = false;
    ctx.proc.currentMeta.name = target.meta.name;
    if (ctx.toast) ctx.toast (L ("Memoria ", "Memory ") + juce::String (slot + 1) + ": " + target.meta.name);
}

void LandscapePage::tick (const Frame& f)
{
    Page::tick (f);
    for (int m = 0; m < memButtons.size(); ++m)
        memButtons[m]->setColour (juce::TextButton::buttonColourId, ctx.proc.memories[(size_t) m].isVoid() ? col::panelHi : col::accent3.withAlpha (0.55f));
    if (morph.start >= 0)
    {
        const float k = (float) juce::jlimit (0.0, 1.0, (nowSeconds() - morph.start) / 2.0);
        const float e = k * k * (3.0f - 2.0f * k);
        const auto& defs = paramDefs();
        if (k >= 0.5f && ! morph.switched)
        {
            morph.switched = true;
            bool anyStep = false;
            for (auto& d : defs) anyStep |= d.kind != pkFloat && morph.from[(size_t) d.index] != morph.to[(size_t) d.index];
            if (anyStep) ctx.proc.beginPresetSwitch();
            for (auto& d : defs) if (d.kind != pkFloat) ctx.proc.setParamPlain (d.index, morph.to[(size_t) d.index]);
        }
        for (auto& d : defs)
            if (d.kind == pkFloat && d.index != P::master)
                ctx.proc.setParamPlain (d.index, d.range.convertFrom0to1 (d.range.convertTo0to1 (morph.from[(size_t) d.index]) * (1.0f - e)
                                                                          + d.range.convertTo0to1 (morph.to[(size_t) d.index]) * e));
        ctx.proc.setParamPlain (P::master, morph.from[(size_t) P::master] + (morph.to[(size_t) P::master] - morph.from[(size_t) P::master]) * e);
        if (k >= 1.0f) morph.start = -1;
    }
    for (int s = 0; s < sceneButtons.size(); ++s) sceneButtons[s]->setToggleState (s == f.scene, juce::dontSendNotification);
    if (f.seed != lastSeed) { lastSeed = f.seed; seedLabel.setText (L ("SEMILLA ", "SEED ") + juce::String (f.seed), juce::dontSendNotification); }
}

void LandscapePage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (418);
    r.removeFromLeft (330 + 8);
    drawPanel (g, r.removeFromLeft (250).toFloat());
}

void LandscapePage::resized()
{
    auto r = getLocalBounds().reduced (8);
    landscape.setBounds (r.removeFromTop (r.getHeight() - 258)); // the landscape takes all the extra height
    {
        auto bar = landscape.getLocalBounds().reduced (14, 10).removeFromBottom (22);
        fullscreen.setBounds (bar.removeFromRight (124));
        bar.removeFromRight (8);
        for (int s = sceneButtons.size(); --s >= 0;) sceneButtons[s]->setBounds (bar.removeFromRight (s == scStarfield ? 104 : 84).reduced (2, 0));
        for (auto* b : sceneButtons) b->toFront (false);
        fullscreen.toFront (false);
    }
    r.removeFromTop (8);
    harmony.setBounds (r.removeFromLeft (330));
    r.removeFromLeft (8);
    auto mid = r.removeFromLeft (250).reduced (10, 8);
    play.setBounds (mid.removeFromTop (116).withSizeKeepingCentre (116, 116));
    mid.removeFromTop (4);
    {
        auto row = mid.removeFromTop (24);
        const int bw = row.getWidth() / 8;
        for (auto* b : memButtons) b->setBounds (row.removeFromLeft (bw).reduced (1, 0));
    }
    mid.removeFromTop (6);
    auto seedRow = mid.removeFromTop (26);
    seedPrev.setBounds (seedRow.removeFromLeft (26));
    seedNext.setBounds (seedRow.removeFromRight (26));
    seedLabel.setBounds (seedRow);
    mid.removeFromTop (6);
    auto bottom = mid.removeFromTop (28);
    dice.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0));
    freeze.setBounds (bottom.reduced (6, 0));
    r.removeFromLeft (8);
    macros.setBounds (r.removeFromLeft (420));
    r.removeFromLeft (8);
    session.setBounds (r);
}

// ================================================================ Sound page
SoundPage::Strip::Strip (Context& c, int l) : Box (c, layerName (l), layerIds()[l]), layer (l)
{
    accent = col::layer[l];
    const juce::String tone[] { L ("BRILLO", "BRIGHT"), L ("FILTRO", "FILTER"), L ("TONO", "PITCH"), L ("BRILLO", "BRIGHT"), L ("COLOR", "COLOUR"), L ("CRISTAL", "GLASS"), L ("FILTRO", "FILTER"), L ("TIMBRE", "TIMBRE") };
    const juce::String chr[] { L ("MUTACIÓN", "MUTATION"), L ("ANALÓGICO", "ANALOG"), L ("GRANO", "GRAIN"), L ("QUINTA", "FIFTH"), L ("RÁFAGAS", "GUSTS"), L ("DENSIDAD", "DENSITY"), L ("GRIT", "GRIT"), L ("RITMO", "RHYTHM") };
    const juce::String toneTip[] { L ("Brillo del espectro en mutación", "Brightness of the mutating spectrum"),
                                   L ("Apertura del filtro del pad", "Pad filter opening"),
                                   L ("Grave: granos a la octava; agudo: quintas y octavas arriba", "Low: octave grains; high: fifths and octaves above"),
                                   L ("Brillo de la cola de shimmer", "Brightness of the shimmer tail"),
                                   L ("Altura del viento: de un rumor grave a un silbido", "Wind pitch: from a low rumble to a whistle"),
                                   L ("De campanas suaves a cristal brillante (índice FM)", "From soft bells to bright glass (FM index)"),
                                   L ("Apertura del filtro del bajo", "Bass filter opening"),
                                   L ("De flauta suave y ligada (bajo) a celesta y caja de música (alto)", "From a soft legato flute (low) to celesta and music box (high)") };
    const juce::String chrTip[] { L ("Velocidad y caos con que mutan los armónicos", "Speed and chaos of the harmonic mutation"),
                                  L ("Carácter analógico de los 80: desafinación, vibrato lento y apertura del filtro en cada acorde", "80s analog character: detune, slow vibrato and a filter bloom on every chord"),
                                  L ("Granos pequeños y densos o grandes y lentos", "Small dense grains or big slow ones"),
                                  L ("Mezcla del shimmer de octava con octava + quinta", "Blend of octave shimmer with octave + fifth"),
                                  L ("Intensidad de las ráfagas y resonancia", "Gust intensity and resonance"),
                                  L ("Cuántas campanas caen", "How many bells fall"),
                                  L ("Saturación y mordida del bajo", "Bass saturation and bite"),
                                  L ("Tempo y movimiento: melodías lentas y largas o más vivas", "Tempo and motion: slow, long melodies or livelier ones") };
    auto& on = add<ParamToggle> (c, P::layer (l, P::lOn), L ("ACTIVA", "ON"));
    auto& lvl = add<ParamKnob> (c, P::layer (l, P::lLevel), L ("NIVEL", "LEVEL"));
    auto& pres = add<ParamKnob> (c, P::layer (l, P::lPresence), L ("PRESENCIA", "PRESENCE"),
                                 L ("Cuánto tiempo está presente: 100 % = siempre; menos = entra y sale sola", "How much of the time it is present: 100 % = always; lower = it comes and goes by itself"));
    auto& tn = add<ParamKnob> (c, P::layer (l, P::lTone), tone[l], toneTip[l]);
    auto& ch = add<ParamKnob> (c, P::layer (l, P::lChar), chr[l], chrTip[l]);
    for (auto* k : { &lvl, &pres, &tn, &ch }) k->setAccent (col::layer[l]);
    layout = [&on, &lvl, &pres, &tn, &ch] (juce::Rectangle<int> r)
    {
        on.setBounds (r.removeFromTop (22).withTrimmedLeft (4));
        r.removeFromTop (6);
        r.removeFromBottom (22); // presence meter
        auto top = r.removeFromTop (r.getHeight() / 2);
        lvl.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (2));
        pres.setBounds (top.reduced (2));
        tn.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2));
        ch.setBounds (r.reduced (2));
    };
}

void SoundPage::Strip::paint (juce::Graphics& g)
{
    Box::paint (g);
    auto m = content().removeFromBottom (14).toFloat().reduced (4, 0);
    g.setColour (col::bgDeep);
    g.fillRoundedRectangle (m, 3);
    g.setColour (accent.withAlpha (0.35f));
    g.fillRoundedRectangle (m.withWidth (m.getWidth() * presence), 3);
    g.setColour (accent);
    g.fillRoundedRectangle (m.withWidth (m.getWidth() * juce::jmin (1.0f, energy)).withTrimmedTop (m.getHeight() * 0.6f), 2);
}

SoundPage::SoundPage (Context& c) : Page (c), fx (c, L ("Mezcla y efectos", "Mix & effects"), "fx"), noise (c, L ("Ruido", "Noise"), "fx"), fades (c, L ("Fundidos", "Fades"), "global")
{
    for (int l = 0; l < kNumLayers; ++l) addAndMakeVisible (strips.add (new Strip (c, l)));
    const std::pair<int, juce::String> fxk[] { { P::warmth, L ("CALIDEZ", "WARMTH") }, { P::width, L ("ANCHURA", "WIDTH") }, { P::lowCut, L ("CORTE GRAVE", "LOW CUT") },
                                               { P::highCut, L ("CORTE AGUDO", "HIGH CUT") }, { P::revDamp, L ("AMORTIG.", "DAMPING") }, { P::dlyTime, L ("DELAY", "DELAY") },
                                               { P::dlyFb, L ("REALIM.", "FEEDBACK") }, { P::dlyTone, L ("TONO DELAY", "DLY TONE") }, { P::dlyMix, L ("ENVÍO DELAY", "DLY SEND") } };
    std::vector<juce::Component*> ks;
    for (auto& [id, name] : fxk) ks.push_back (&fx.add<ParamKnob> (c, id, name));
    fx.layout = [ks] (juce::Rectangle<int> r) { rowKnobs (ks, r, 110); };
    addAndMakeVisible (fx);
    // noise generator: type selector + level + tone
    noise.accent = col::layer[lyWind];
    auto& nType = noise.add<ParamCombo> (c, P::noiseType);
    nType.setTooltip (L ("Ruido de fondo: rosa, marrón, blanco, olas de océano o crepitar de vinilo", "Background noise: pink, brown, white, ocean waves or vinyl crackle"));
    auto& nLvl = noise.add<ParamKnob> (c, P::noiseLevel, L ("NIVEL", "LEVEL"));
    auto& nTone = noise.add<ParamKnob> (c, P::noiseTone, L ("TONO", "TONE"), L ("Más oscuro o más brillante", "Darker or brighter"));
    noise.layout = [&nType, &nLvl, &nTone] (juce::Rectangle<int> r)
    {
        nType.setBounds (r.removeFromTop (24).reduced (4, 0));
        r.removeFromTop (6);
        rowKnobs ({ &nLvl, &nTone }, r, 100);
    };
    addAndMakeVisible (noise);
    std::vector<juce::Component*> ts;
    ts.push_back (&fades.add<ParamKnob> (c, P::fadeIn, L ("ENTRADA", "FADE IN"), L ("Fundido de entrada al pulsar Play", "Fade-in time when pressing Play")));
    ts.push_back (&fades.add<ParamKnob> (c, P::fadeOut, L ("SALIDA", "FADE OUT"), L ("Fundido al pulsar Stop o al acabar el temporizador", "Fade-out time on Stop or when the timer ends")));
    fades.layout = [ts] (juce::Rectangle<int> r) { rowKnobs (ts, r, 100); };
    addAndMakeVisible (fades);
}

void SoundPage::tick (const Frame& f)
{
    Page::tick (f);
    for (auto* s : strips)
    {
        const float p = f.presence[(size_t) s->layer], e = f.layerEnergy[(size_t) s->layer];
        if (std::abs (p - s->presence) > 0.005f || std::abs (e - s->energy) > 0.01f) { s->presence = p; s->energy = e; s->repaint(); }
    }
}

void SoundPage::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void SoundPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() - 228);
    const int w = top.getWidth() / kNumLayers;
    for (auto* s : strips) s->setBounds (top.removeFromLeft (w).reduced (3, 0));
    r.removeFromTop (8);
    fades.setBounds (r.removeFromRight (200));
    r.removeFromRight (8);
    noise.setBounds (r.removeFromRight (230));
    r.removeFromRight (8);
    fx.setBounds (r);
}

// ================================================================ Full-screen landscape
FullscreenView::FullscreenView (Context& c, std::function<void()> onClose) : ctx (c), close (std::move (onClose))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    view.setInterceptsMouseClicks (false, true);   // the picture / variation buttons stay clickable
    view.fullBleed = true;
    addAndMakeVisible (view);
    view.setGpu (c.settings.gpu());
    view.paintExtra = [this] (juce::Graphics& g) { paintHud (g); };   // shortcuts box + value toasts, over the landscape
}

void FullscreenView::tick (const Frame& f)
{
    if (lastT == 0) helpUntil = f.time + 6.0; // first frame: show the shortcuts for a few seconds
    lastT = f.time;
    view.tick (f);
    repaint();
}

void FullscreenView::nudge (int param, float delta, const juce::String& label)
{
    const auto& d = paramDefs()[(size_t) param];
    const float v = d.range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, d.range.convertTo0to1 (ctx.proc.getParam (param)) + delta));
    ctx.proc.setParamPlain (param, v);
    toastText = label + "  " + juce::String ((int) std::lround (d.range.convertTo0to1 (v) * 100)) + " %";
    toastUntil = lastT + 1.5;
}

bool FullscreenView::keyPressed (const juce::KeyPress& k)
{
    const int c = k.getKeyCode();
    const auto ch = juce::CharacterFunctions::toUpperCase (k.getTextCharacter());
    auto say = [this] (const juce::String& s) { toastText = s; toastUntil = lastT + 1.5; };
    if (c == juce::KeyPress::escapeKey) { if (close) close(); return true; }
    if (c == juce::KeyPress::spaceKey) { ctx.proc.setPlaying (! ctx.proc.isPlayingInternal()); say (ctx.proc.isPlayingInternal() ? "PLAY" : "STOP"); return true; }
    if (c == juce::KeyPress::leftKey) { nudge (P::light, -0.05f, L ("LUZ", "LIGHT")); return true; }
    if (c == juce::KeyPress::rightKey) { nudge (P::light, 0.05f, L ("LUZ", "LIGHT")); return true; }
    if (c == juce::KeyPress::upKey) { nudge (P::evolution, 0.05f, L ("EVOLUCIÓN", "EVOLUTION")); return true; }
    if (c == juce::KeyPress::downKey) { nudge (P::evolution, -0.05f, L ("EVOLUCIÓN", "EVOLUTION")); return true; }
    if (ch >= '1' && ch <= '6') { ctx.proc.setParamPlain (P::scene, (float) (ch - '1')); say (sceneName ((int) (ch - '1'))); return true; }
    switch (ch)
    {
        case 'F':
        {
            const bool on = ctx.proc.getParam (P::freeze) < 0.5f;
            ctx.proc.setParamPlain (P::freeze, on ? 1.0f : 0.0f);
            say (on ? L ("CONGELADO", "FROZEN") : L ("DESCONGELADO", "UNFROZEN"));
            return true;
        }
        case 'N': ctx.proc.setParamPlain (P::seed, (float) (1 + juce::Random::getSystemRandom().nextInt (99999))); say (L ("NUEVA SEMILLA", "NEW SEED")); return true;
        case 'A': nudge (P::density, 0.05f, L ("DENSIDAD", "DENSITY")); return true;
        case 'Z': nudge (P::density, -0.05f, L ("DENSIDAD", "DENSITY")); return true;
        case 'S': nudge (P::space, 0.05f, L ("ESPACIO", "SPACE")); return true;
        case 'X': nudge (P::space, -0.05f, L ("ESPACIO", "SPACE")); return true;
        case 'R':
        {
            const int nt = ((int) ctx.proc.getParam (P::noiseType) + 1) % 6;
            ctx.proc.setParamPlain (P::noiseType, (float) nt);
            say (L ("RUIDO: ", "NOISE: ") + paramDefs()[(size_t) P::noiseType].choices[nt].toUpperCase());
            return true;
        }
        case 'H': helpPinned = ! helpPinned; helpUntil = helpPinned ? 1.0e9 : 0.0; return true;
        case 'T': view.showOverlay = ! view.showOverlay; helpPinned = false; helpUntil = 0.0; say (view.showOverlay ? L ("TEXTOS", "TEXTS") : L ("MODO RELAX", "RELAX MODE")); return true;
        default: return false;
    }
}

void FullscreenView::paintHud (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (lastT < toastUntil)
    {
        g.setColour (juce::Colours::white.withAlpha ((float) juce::jmin (1.0, (toastUntil - lastT) * 2.0)));
        g.setFont (techFont (30.0f));
        g.drawText (toastText, b.withTrimmedTop (b.getHeight() * 0.42f).removeFromTop (40), juce::Justification::centred);
    }
    const float a = (float) juce::jlimit (0.0, 1.0, helpUntil - lastT);
    if (a <= 0.01f) return;
    auto box = juce::Rectangle<float> (b.getRight() - 360, b.getBottom() - 330, 330, 290);
    g.setColour (juce::Colours::black.withAlpha (0.55f * a));
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (col::accent.withAlpha (a));
    g.setFont (techFont (15.0f));
    auto in = box.reduced (16, 12);
    g.drawText (L ("ATAJOS", "SHORTCUTS"), in.removeFromTop (22), juce::Justification::centredLeft);
    g.setFont (font (13.0f));
    g.setColour (juce::Colours::white.withAlpha (0.9f * a));
    const juce::String lines[] { L ("Espacio: Play / Stop", "Space: Play / Stop"), L ("Esc: salir", "Esc: exit"), L ("F: congelar", "F: freeze"),
                                 L ("1 a 6: paisaje", "1 to 6: landscape"), L ("N: nueva semilla", "N: new seed"), L ("Izquierda / derecha: luz", "Left / right: light"),
                                 L ("Arriba / abajo: evolución", "Up / down: evolution"), L ("A / Z: densidad", "A / Z: density"), L ("S / X: espacio", "S / X: space"),
                                 L ("R: tipo de ruido", "R: noise type"), L ("T: modo relax (solo el paisaje)", "T: relax mode (landscape only)"), L ("H: mostrar / ocultar ayuda", "H: show / hide help") };
    for (auto& l : lines) g.drawText (l, in.removeFromTop (20), juce::Justification::centredLeft);
}
}
