#include "GeneratorPage.h"
#include "../gen/Messages.h"

namespace gx::ui
{
static double nowSec() { return nowSeconds(); }

// ================================================================ GenerateButton
void GenerateButton::burst()
{
    const auto c = getLocalBounds().toFloat().getCentre();
    for (int i = 0; i < 70; ++i)
    {
        const float a = rng.nextFloat() * kTwoPi, s = 60.0f + rng.nextFloat() * 260.0f;
        particles.push_back ({ c, { std::cos (a) * s, std::sin (a) * s }, 1.0f, rng.nextBool() ? col::accent : (rng.nextBool() ? col::accent2 : col::accent3) });
    }
    glow = 1.0f;
}

void GenerateButton::animate (float dt, float energy, bool busy)
{
    level += (energy - level) * juce::jmin (1.0f, dt * 8.0f);
    working = busy;
    spin += dt * (busy ? 4.0f : 0.35f + energy * 1.5f);
    pulse += dt * 2.0f;
    glow = juce::jmax (0.0f, glow - dt * 1.5f);
    for (auto& p : particles) { p.p += p.v * dt; p.v *= 0.96f; p.life -= dt * 1.1f; }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (auto& p) { return p.life <= 0; }), particles.end());
    repaint();
}

void GenerateButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.36f;
    const float act = juce::jlimit (0.0f, 1.0f, level * 1.6f + (working ? 0.5f : 0.0f) + glow * 0.5f);
    // ---- backdrop: reactive radial glow, dot grid, guide rings and orbiting markers
    {
        // everything lives inside an ellipse that fades to zero before the component edges (no visible box)
        const float ex = b.getWidth() * 0.5f - 2.0f, ey = b.getHeight() * 0.5f - 2.0f;
        auto edist = [&] (float x, float y) { const float dx = (x - c.x) / ex, dy = (y - c.y) / ey; return std::sqrt (dx * dx + dy * dy); };
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::scale (ex / ey, 1.0f, c.x, c.y));          // circle -> ellipse
            juce::ColourGradient halo (col::accent.withAlpha (0.10f + 0.16f * act), c.x, c.y, col::accent.withAlpha (0.0f), c.x + ey, c.y, true);
            halo.addColour (0.55, col::accent.withAlpha ((0.10f + 0.16f * act) * 0.35f));
            g.setGradientFill (halo);
            g.fillEllipse (juce::Rectangle<float> (ey * 2, ey * 2).withCentre (c));
        }
        const float step = 13.0f;
        for (float y = std::fmod (c.y, step); y < b.getBottom(); y += step)
            for (float x = std::fmod (c.x, step); x < b.getRight(); x += step)
            {
                const float dn = edist (x, y), d = juce::Point<float> (x, y).getDistanceFrom (c);
                if (d < r * 1.38f || dn >= 1.0f) continue;
                const float wave = 0.5f + 0.5f * std::sin (d * 0.06f - spin * 3.0f);   // ripple travelling outwards
                const float fade = juce::jmin (1.0f, (1.0f - dn) * 3.0f);
                const float a = fade * (0.08f + 0.24f * act * wave);
                g.setColour ((wave > 0.8f && act > 0.2f ? col::accent : col::textDim).withAlpha (a));
                g.fillEllipse (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
            }
        // orbit rings: wider than tall so they never touch the top/bottom
        const float ry = juce::jmin (r * 1.34f, ey - 3.0f);
        for (float k : { 1.55f, 1.95f })
        {
            g.setColour (col::textDim.withAlpha (k < 1.7f ? 0.16f : 0.10f));
            g.drawEllipse (juce::Rectangle<float> (r * 2 * k, ry * 2 * (k < 1.7f ? 0.94f : 1.0f)).withCentre (c), 1.0f);
        }
        // crosshair rails fading out to the sides
        for (int side : { -1, 1 })
        {
            juce::ColourGradient rail (col::textDim.withAlpha (0.22f), c.x + side * r * 1.95f, c.y, col::textDim.withAlpha (0.0f), c.x + side * ex, c.y, false);
            g.setGradientFill (rail);
            g.fillRect (juce::Rectangle<float> (c.x + juce::jmin (side * r * 1.95f, side * ex), c.y - 0.5f, ex - r * 1.95f, 1.0f));
        }
        // counter-rotating arc segments on the inner orbit
        for (int i = 0; i < 3; ++i)
        {
            const float a0 = -spin * 0.6f + (float) i * kTwoPi / 3.0f;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r * 1.55f, ry * 0.94f, 0, a0, a0 + 0.55f, true);
            g.setColour (col::accent2.withAlpha (0.25f + 0.45f * act));
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // markers orbiting on the outer ring
        for (int i = 0; i < 6; ++i)
        {
            const float a = spin * 0.45f + (float) i * kTwoPi / 6.0f;
            const auto p = c + juce::Point<float> (std::cos (a) * r * 1.95f, std::sin (a) * ry);
            g.setColour ((i % 2 ? col::accent3 : col::accent).withAlpha (0.35f + 0.5f * act));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (p));
        }
    }
    // outer rotating ring of ticks
    for (int i = 0; i < 48; ++i)
    {
        const float a = spin + (float) i / 48.0f * kTwoPi;
        const float r0 = r * 1.18f, r1 = r * (i % 4 == 0 ? 1.32f : 1.25f);
        g.setColour ((i % 8 == 0 ? col::accent : col::textDim).withAlpha (working ? 0.9f : 0.45f));
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 4 == 0 ? 2.0f : 1.0f);
    }
    // glow
    const float pul = 0.5f + 0.5f * std::sin (pulse);
    g.setColour (col::accent.withAlpha (0.10f + 0.12f * pul + 0.4f * glow + (working ? 0.2f : 0.0f)));
    g.fillEllipse (juce::Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c));
    // body
    juce::ColourGradient body (col::accent.brighter (pressed ? 0.0f : 0.35f), c.x, c.y - r, col::accent.darker (0.6f), c.x, c.y + r, false);
    g.setGradientFill (body);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (juce::Rectangle<float> (r * 2 - 3, r * 2 - 3).withCentre (c), 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 2.0f);
    // inner arc spinner while working
    if (working)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0, spin * 1.7f, spin * 1.7f + 2.2f, true);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // label: tech font, shrunk to fit well inside the circle in any language, faux-bold via a thin outline
    {
        const auto text = working ? L ("CREANDO…", "CREATING…") : L ("GENERAR", "GENERATE");
        auto f = techFont (r * 0.30f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, text, 0, 0);
        const float maxW = r * 1.5f, w = ga.getBoundingBox (0, -1, true).getWidth();
        if (w > maxW) { f = techFont (r * 0.30f * maxW / w); ga.clear(); ga.addLineOfText (f, text, 0, 0); }
        const auto bb = ga.getBoundingBox (0, -1, true);
        juce::Path tp;
        ga.createPath (tp);
        tp.applyTransform (juce::AffineTransform::translation (c.x - bb.getCentreX(), c.y - bb.getCentreY()));
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillPath (tp);
        g.strokePath (tp, juce::PathStrokeType (r * 0.012f + 0.6f));
    }
    for (auto& p : particles)
    {
        g.setColour (p.c.withAlpha (juce::jlimit (0.0f, 1.0f, p.life)));
        g.fillEllipse (juce::Rectangle<float> (4, 4).withCentre (p.p));
    }
}

// ================================================================ AiConsole
void AiConsole::start (const juce::StringArray& l, double duration)
{
    lines = l;
    shown.clear();
    lineIdx = 0; typed = 0;
    t0 = nowSec();
    perLine = duration / juce::jmax (1, lines.size());
    running = true;
}

void AiConsole::animate (double t)
{
    now = t;
    if (running)
    {
        const double el = t - t0;
        lineIdx = juce::jmin (lines.size() - 1, (int) (el / perLine));
        const double inLine = (el - lineIdx * perLine) / (perLine * 0.7);
        typed = (int) (juce::jlimit (0.0, 1.0, inLine) * lines[lineIdx].length());
        if (el >= perLine * lines.size()) { running = false; lineIdx = lines.size() - 1; typed = lines[lineIdx].length(); }
    }
    repaint();
}

void AiConsole::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r);
    auto in = r.reduced (12, 8);
    g.setFont (monoFont (13.5f));
    const int maxLines = juce::jmax (1, (int) (in.getHeight() / 19.0f));
    const int first = juce::jmax (0, lineIdx - maxLines + 1);
    for (int i = first; i <= lineIdx && i < lines.size(); ++i)
    {
        const bool cur = i == lineIdx;
        const auto text = cur ? lines[i].substring (0, typed) : lines[i];
        g.setColour (cur ? col::phosphor : col::phosphor.withAlpha (0.45f));
        auto row = in.removeFromTop (19);
        g.drawText ("> " + text, row, juce::Justification::centredLeft);
        if (cur && std::fmod (now, 0.8) < 0.45)
        {
            juce::GlyphArrangement ga; ga.addLineOfText (monoFont (13.5f), "> " + text, 0, 0); const float w = ga.getBoundingBox (0, -1, true).getWidth();
            g.fillRect (row.getX() + w + 2, row.getY() + 3, 8.0f, row.getHeight() - 6);
        }
    }
}

// ================================================================ ResolveGrid
void ResolveGrid::setTarget (const Preset& p)
{
    auto v = [&] (int i) { return p.params[(size_t) i]; };
    auto pct = [] (float x) { return juce::String ((int) std::lround (x * 100)) + " %"; };
    int layers = 0;
    for (int l = 0; l < kNumLayers; ++l) layers += v (P::layer (l, P::lOn)) > 0.5f;
    cells = { { { L ("TONALIDAD", "KEY"), keyNames()[(int) v (P::key)] },
                { L ("ESCALA", "SCALE"), scaleNames()[(int) v (P::scale)] },
                { L ("ARMONÍA", "HARMONY"), pct (v (P::harmony)) },
                { L ("CAPAS", "LAYERS"), juce::String (layers) + " / 6" },
                { L ("EVOLUCIÓN", "EVOLUTION"), pct (v (P::evolution)) },
                { L ("DENSIDAD", "DENSITY"), pct (v (P::density)) },
                { L ("LUZ", "LIGHT"), pct (v (P::light)) },
                { L ("ESPACIO", "SPACE"), pct (v (P::space)) },
                { "SHIMMER", pct (v (P::layer (lyShimmer, P::lLevel))) },
                { L ("TEXTURA", "TEXTURE"), pct (v (P::layer (lyTexture, P::lLevel))) },
                { L ("SEMILLA", "SEED"), juce::String ((int) v (P::seed)) },
                { L ("AFINACIÓN", "TUNING"), (int) v (P::tuning) == 1 ? juce::String ("432 Hz") : juce::String ("440 Hz") } } };
    repaint();
}

void ResolveGrid::animate (float s, float a, float d) { scramble = s; assemble = a; disassemble = d; repaint(); }

void ResolveGrid::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r);
    auto in = r.reduced (8, 6);
    const int cols = 4, rows = 3;
    const float cw = in.getWidth() / cols, ch = in.getHeight() / rows;
    static const char glyphs[] = "0123456789ABCDEF#%&@$*+=<>";
    for (int i = 0; i < 12; ++i)
    {
        auto cell = juce::Rectangle<float> (in.getX() + (i % cols) * cw, in.getY() + (i / cols) * ch, cw, ch).reduced (3);
        // per-cell stagger for the assemble / disassemble choreography
        const float stagger = (float) i / 12.0f;
        const float a = juce::jlimit (0.0f, 1.0f, assemble * 1.6f - stagger * 0.6f);
        const float d = juce::jlimit (0.0f, 1.0f, disassemble * 1.6f - stagger * 0.6f);
        const float offset = (1.0f - a) * 18.0f + d * -18.0f;
        const float alpha = juce::jlimit (0.15f, 1.0f, a * (1.0f - d * 0.85f));
        cell = cell.translated (0, offset);
        g.setColour (col::panelLo.withAlpha (0.8f * alpha));
        g.fillRoundedRectangle (cell, 3);
        g.setColour (col::textDim.withAlpha (alpha));
        g.setFont (monoFont (9.5f));
        g.drawText (cells[(size_t) i].label, cell.reduced (6, 2), juce::Justification::topLeft);
        juce::String val = cells[(size_t) i].value;
        if (scramble > 0.01f)
        {
            juce::String sc;
            for (int k = 0; k < juce::jmax (4, val.length()); ++k) sc << (rng.nextFloat() < scramble ? juce::String::charToString ((juce::juce_wchar) glyphs[rng.nextInt (26)]) : val.substring (k, k + 1));
            val = sc;
        }
        g.setColour ((scramble > 0.01f ? col::accent : col::phosphor).withAlpha (alpha));
        g.setFont (monoFont (13.0f));
        g.drawText (val, cell.reduced (6, 2), juce::Justification::bottomLeft);
    }
}

// ================================================================ GeneratorPage
GeneratorPage::UiState GeneratorPage::uiState() const
{
    UiState s { category, {}, {} };
    for (auto* sl : sliders) s.sliders.push_back (sl->getValue());
    for (auto* t : locks) s.locks.push_back (t->getToggleState());
    return s;
}

void GeneratorPage::setUiState (const UiState& s)
{
    category = s.category;
    for (auto* b : catButtons) b->setToggleState (b->getProperties()["cat"].toString() == category, juce::dontSendNotification);
    for (int i = 0; i < juce::jmin (sliders.size(), (int) s.sliders.size()); ++i) sliders[i]->setValue (s.sliders[(size_t) i], juce::dontSendNotification);
    for (int i = 0; i < juce::jmin (locks.size(), (int) s.locks.size()); ++i) locks[i]->setToggleState (s.locks[(size_t) i], juce::dontSendNotification);
    repaint();
}

GeneratorPage::GeneratorPage (Context& c) : Page (c), macros (c, "Macros", "macros")
{
    history = &ctx.proc.genHistory;
    history->setLimit (ctx.settings.historyLimit());
    for (auto& cat : categories())
    {
        auto* b = catButtons.add (new juce::TextButton (categoryLabel (cat)));
        b->getProperties().set ("cat", cat);
        b->setRadioGroupId (101);
        b->setClickingTogglesState (true);
        b->onClick = [this, cat] { category = cat; };
        b->setToggleState (cat == category, juce::dontSendNotification);
        addAndMakeVisible (b);
    }
    sliderNames = { L ("Luz", "Light"), L ("Movimiento", "Movement"), L ("Densidad", "Density"), L ("Espacio", "Space"), L ("Misterio", "Mystery") };
    const float defaults[] { 0.5f, 0.5f, 0.5f, 0.6f, 0.25f };
    for (int i = 0; i < sliderNames.size(); ++i)
    {
        auto* s = sliders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox));
        s->setRange (0, 1, 0.01);
        s->setValue (defaults[i], juce::dontSendNotification);
        s->setPopupDisplayEnabled (true, true, nullptr);
        s->setTooltip (sliderNames[i] + L (" (doble clic: valor de fábrica)", " (double-click: factory value)"));
        s->setDoubleClickReturnValue (true, defaults[i]);
        s->getProperties().set ("arc", (juce::int64) (i == 4 ? col::accent3 : i == 0 ? col::accent2 : col::accent).getARGB());
        addAndMakeVisible (s);
    }
    resetChar.setButtonText (L ("Restablecer", "Reset"));
    resetChar.setTooltip (L ("Devolver los controles de carácter a sus valores de fábrica", "Return the character controls to their factory values"));
    resetChar.onClick = [this, defaults]
    {
        for (int i = 0; i < sliders.size(); ++i) sliders[i]->setValue (defaults[i], juce::sendNotificationSync);
    };
    addAndMakeVisible (resetChar);
    const juce::String lockNames[] { L ("ARMONÍA", "HARMONY"), L ("CAPAS", "LAYERS"), "FX" };
    const char* lockIds[] { "harmony", "layers", "fx" };
    for (int i = 0; i < 3; ++i)
    {
        auto* t = locks.add (new juce::ToggleButton (lockNames[i]));
        t->getProperties().set ("lock", lockIds[i]);
        t->setTooltip (L ("Bloquear esta sección: GENERAR y MUTAR la conservan", "Lock this section: GENERATE and MUTATE keep it"));
        addAndMakeVisible (t);
    }
    addAndMakeVisible (genButton);
    genButton.onClick = [this] { startJob (Job::Generate); };
    addAndMakeVisible (console);
    addAndMakeVisible (grid);
    for (auto* b : { &mutateBtn, &blendBtn, &saveBtn, &exportBtn, &editorBtn, &backBtn, &fwdBtn }) addAndMakeVisible (b);
    mutateBtn.setTooltip (L ("Variación suave del preset actual", "Gentle variation of the current preset"));
    blendBtn.setTooltip (L ("Mezclar con otro preset", "Blend with another preset"));
    backBtn.setTooltip (L ("Anterior en el historial (Alt+Izquierda)", "Previous in history (Alt+Left)"));
    fwdBtn.setTooltip (L ("Siguiente en el historial (Alt+Derecha)", "Next in history (Alt+Right)"));
    mutateBtn.onClick = [this] { startJob (Job::Mutate); };
    blendBtn.onClick = [this] { showBlendMenu(); };
    saveBtn.onClick = [this] { saveCurrent(); };
    exportBtn.onClick = [this] { if (openExport) openExport(); };
    editorBtn.onClick = [this] { if (openEditor) openEditor(); };
    backBtn.onClick = [this] { historyStep (-1); };
    fwdBtn.onClick = [this] { historyStep (1); };
    histLabel.setJustificationType (juce::Justification::centred);
    histLabel.setFont (monoFont (13.0f));
    histLabel.setColour (juce::Label::textColourId, col::phosphor);
    addAndMakeVisible (histLabel);
    view.showOverlay = false;
    view.setGpu (c.settings.gpu());
    for (auto* comp : std::initializer_list<juce::Component*> { &scope, &spectrum, &view, &vu }) addAndMakeVisible (comp);
    macros.accent = col::accent3;
    std::vector<juce::Component*> mk;
    const juce::String names[] { L ("EVOLUCIÓN", "EVOLUTION"), L ("DENSIDAD", "DENSITY"), L ("LUZ", "LIGHT"), L ("ESPACIO", "SPACE") };
    const int ids[] { P::evolution, P::density, P::light, P::space };
    for (int m = 0; m < 4; ++m)
    {
        auto* k = &macros.add<ParamKnob> (c, ids[m], names[m]);
        k->setAccent (col::accent3);
        mk.push_back (k);
    }
    macros.layout = [mk] (juce::Rectangle<int> r) { gridKnobs (mk, r, 4); };
    addAndMakeVisible (macros);
    grid.setTarget (Preset::capture (ctx.proc));
    console.showIdle (L ("Listo. Elige un estado de ánimo y pulsa GENERAR.", "Ready. Pick a mood and press GENERATE."));
    updateHistoryLabel();
    startTimerHz (30);
}

GeneratorPage::~GeneratorPage()
{
    stopTimer();
    if (worker.joinable()) worker.join();
}

GenRequest GeneratorPage::makeRequest() const
{
    GenRequest r;
    r.category = category;
    r.light = (float) sliders[0]->getValue();
    r.movement = (float) sliders[1]->getValue();
    r.density = (float) sliders[2]->getValue();
    r.space = (float) sliders[3]->getValue();
    r.mystery = (float) sliders[4]->getValue();
    r.seed = forcedSeed() != 0 ? forcedSeed()++ : (uint32_t) juce::Random::getSystemRandom().nextInt() ^ (uint32_t) juce::Time::currentTimeMillis();
    for (auto* l : locks) if (l->getToggleState()) r.locks.insert (l->getProperties()["lock"].toString());
    return r;
}

void GeneratorPage::startJob (Job j, const Preset* other)
{
    if (busy.load()) return;
    if (worker.joinable()) worker.join();
    busy = true;
    jobDone = false;
    auto req = makeRequest();
    auto current = Preset::capture (ctx.proc);
    std::unique_ptr<Preset> otherCopy = other ? std::make_unique<Preset> (*other) : nullptr;
    worker = std::thread ([this, j, req, current, oc = std::shared_ptr<Preset> (std::move (otherCopy))]() mutable
    {
        Preset out;
        if (j == Job::Generate)
        {
            req.base = &current;
            out = Generator::generate (req);
        }
        else
        {
            // live variations go through the same validation; retry with new seeds, never apply a failing sound
            bool ok = false;
            for (int a = 0; a < 8 && ! ok; ++a)
            {
                const uint32_t seed = req.seed + (uint32_t) a * 104729u;
                out = j == Job::Mutate ? Generator::mutate (current, 0.35f - 0.03f * (float) a, seed, req.locks)
                                       : Generator::blend (current, oc ? *oc : current, 0.5f, seed);
                if (out.meta.category.isEmpty()) out.meta.category = current.meta.category;
                ok = Generator::validate (out).ok;
                if (j == Job::Mutate) out.meta.tags = current.meta.tags;
            }
            jobFailed = ! ok;
        }
        result = std::move (out);
        jobDone = true;
    });
    jobStart = nowSec();
    const bool msgs = ctx.settings.aiMessages();
    minDuration = msgs ? 1.6 : 0.5;
    const auto cat = j == Job::Generate ? category : ctx.proc.currentMeta.category;
    if (msgs) console.start (aiMessages (cat, uiEnglish(), msgRng, 4, lastMsg), minDuration);
    else console.showIdle (L ("Generando...", "Generating..."));
    genButton.burst();
}

void GeneratorPage::finishJob()
{
    if (worker.joinable()) worker.join();
    if (jobFailed.exchange (false))
    {
        busy = false;
        console.showIdle (L ("La variación no pasó la validación: se conserva el sonido actual.", "The variation failed validation: keeping the current sound."));
        if (ctx.toast) ctx.toast (L ("Variación descartada (no pasó la validación de calidad)", "Variation discarded (failed the quality check)"));
        return;
    }
    result.applyTo (ctx.proc);
    history->setLimit (ctx.settings.historyLimit());
    history->push (result);
    grid.setTarget (result);
    assembleStart = nowSec();
    busy = false;
    updateHistoryLabel();
    if (! ctx.settings.aiMessages()) console.showIdle (result.meta.name + "  [" + categoryLabel (result.meta.category) + " / " + result.meta.subcategory + "]");
    if (onPresetChanged) onPresetChanged();
    if (! ctx.proc.isHostPlaying()) ctx.proc.setPlaying (true); // hear it right away
}

void GeneratorPage::historyStep (int dir)
{
    if (busy.load()) return;
    const Preset* p = dir < 0 ? history->back() : history->forward();
    if (! p) return;
    p->applyTo (ctx.proc);
    grid.setTarget (*p);
    assembleStart = nowSec();
    updateHistoryLabel();
    console.showIdle (p->meta.name + "  [" + categoryLabel (p->meta.category) + " / " + p->meta.subcategory + "]");
    if (onPresetChanged) onPresetChanged();
}

void GeneratorPage::updateHistoryLabel()
{
    histLabel.setText (juce::String (history->index() + 1) + " / " + juce::String (history->size()), juce::dontSendNotification);
    backBtn.setEnabled (history->canBack());
    fwdBtn.setEnabled (history->canForward());
}

void GeneratorPage::presetLoaded() { grid.setTarget (Preset::capture (ctx.proc)); assembleStart = nowSec(); }

void GeneratorPage::saveCurrent()
{
    auto p = Preset::capture (ctx.proc);
    auto* w = new juce::AlertWindow (L ("Guardar preset generado", "Save generated preset"), "", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", p.meta.name, L ("Nombre", "Name"));
    w->addComboBox ("cat", categories(), L ("Categoría", "Category"));
    w->getComboBoxComponent ("cat")->setText (p.meta.category, juce::dontSendNotification);
    w->addTextEditor ("sub", p.meta.subcategory, L ("Subcategoría", "Subcategory"));
    w->addTextEditor ("tags", p.meta.tags.joinIntoString (", "), L ("Etiquetas", "Tags"));
    w->addTextEditor ("author", L ("Usuario", "User"), L ("Autor", "Author"));
    w->addTextEditor ("notes", p.meta.notes, L ("Notas", "Notes"));
    w->addComboBox ("fav", { L ("No favorito", "Not favourite"), L ("Favorito", "Favourite") }, L ("Favorito", "Favourite"));
    w->addButton (L ("Guardar", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (L ("Cancelar", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, p] (int r) mutable
    {
        if (r != 1) return;
        p.meta.name = w->getTextEditorContents ("name").trim();
        p.meta.category = w->getComboBoxComponent ("cat")->getText();
        p.meta.subcategory = w->getTextEditorContents ("sub").trim();
        p.meta.tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
        p.meta.tags.trim(); p.meta.tags.removeEmptyStrings();
        p.meta.author = w->getTextEditorContents ("author").trim();
        p.meta.notes = w->getTextEditorContents ("notes");
        p.meta.favorite = w->getComboBoxComponent ("fav")->getSelectedItemIndex() == 1;
        p.meta.uuid = {};
        if (ctx.library.saveUser (p))
        {
            ctx.proc.currentMeta = p.meta;
            if (p.meta.favorite) ctx.library.setFavorite (ctx.library.indexOfUuid (p.meta.uuid), true);
            if (ctx.toast) ctx.toast (L ("Guardado en PRESETS: ", "Saved to PRESETS: ") + p.meta.name);
            if (onPresetChanged) onPresetChanged();
        }
    }), true);
}

void GeneratorPage::showBlendMenu()
{
    juce::PopupMenu m;
    m.addItem (1, L ("Con el anterior del historial", "With the previous one in history"), history->previous() != nullptr);
    m.addItem (2, L ("Con un favorito al azar", "With a random favourite"));
    m.addItem (3, L ("Con un preset de la misma categoría", "With a preset from the same category"));
    m.addItem (4, L ("Con un preset cualquiera de la biblioteca", "With any preset from the library"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&blendBtn), [this] (int r)
    {
        if (r == 1) { if (auto* p = history->previous()) startJob (Job::Blend, p); return; }
        if (r < 2) return;
        PresetFilter f;
        if (r == 2) f.favoritesOnly = true;
        if (r == 3) f.category = ctx.proc.currentMeta.category;
        auto idx = ctx.library.query (f);
        if (idx.empty()) { if (ctx.toast) ctx.toast (L ("No hay presets para mezclar con ese criterio", "No presets to blend with for that choice")); return; }
        Preset other;
        if (ctx.library.load (idx[(size_t) juce::Random::getSystemRandom().nextInt ((int) idx.size())], other))
        {
            if (ctx.toast) ctx.toast (L ("Mezclando con: ", "Blending with: ") + other.meta.name);
            startJob (Job::Blend, &other);
        }
    });
}

bool GeneratorPage::handleKey (const juce::KeyPress& k)
{
    if (k.getModifiers().isAltDown() && k.getKeyCode() == juce::KeyPress::leftKey) { historyStep (-1); return true; }
    if (k.getModifiers().isAltDown() && k.getKeyCode() == juce::KeyPress::rightKey) { historyStep (1); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == juce::KeyPress::returnKey) { startJob (Job::Generate); return true; }
    return false;
}

void GeneratorPage::timerCallback()
{
    if (busy.load() && jobDone.load() && nowSec() - jobStart >= minDuration && console.finished()) finishJob();
    else if (busy.load() && jobDone.load() && nowSec() - jobStart >= minDuration + 1.0) finishJob(); // safety
}

void GeneratorPage::tick (const Frame& f)
{
    Page::tick (f);
    const double t = nowSec();
    const float dt = (float) juce::jlimit (0.0, 0.1, t - lastAnim);
    lastAnim = t;
    energy = f.energy;
    genButton.animate (dt, energy, busy.load());
    console.animate (t);
    const bool b = busy.load();
    const float dis = b ? (float) juce::jlimit (0.0, 1.0, (t - jobStart) / 0.35) : 0.0f;
    const float as = (float) juce::jlimit (0.0, 1.0, (t - assembleStart) / 0.6);
    grid.animate (b ? 0.9f : juce::jmax (0.0f, 1.0f - as * 1.4f), b ? 0.0f : as, dis);
    scope.label = b ? L ("ENSAMBLANDO…", "ASSEMBLING…") : L ("OSCILOSCOPIO", "OSCILLOSCOPE");
}

void GeneratorPage::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (290);
    drawPanel (g, left.toFloat());
    g.setColour (col::text);
    g.setFont (font (12.0f, true));
    auto l = left.reduced (12, 8);
    g.drawText (L ("ESTADO DE ÁNIMO", "MOOD"), l.removeFromTop (16), juce::Justification::centredLeft);
    l.removeFromTop ((catButtons.size() + 1) / 2 * 32 + 8);
    g.drawText (L ("CARÁCTER", "CHARACTER"), l.removeFromTop (16), juce::Justification::centredLeft);
    auto sl = l.removeFromTop (230);
    g.setFont (font (10.5f, true));
    g.setColour (col::textDim);
    const int sw = sl.getWidth() / sliderNames.size();
    for (int i = 0; i < sliderNames.size(); ++i)
        g.drawFittedText (juce::StringArray { L ("LUZ", "LIGHT"), L ("MOVIM.", "MOVEM."), L ("DENSID.", "DENSITY"), L ("ESPACIO", "SPACE"), L ("MISTERIO", "MYSTERY") }[i], sl.getX() + i * sw, sl.getBottom() - 16, sw, 16, juce::Justification::centred, 1, 0.6f);
    g.setColour (col::text);
    g.setFont (font (12.0f, true));
    l.removeFromTop (8);
    g.drawText (L ("BLOQUEAR SECCIONES", "LOCK SECTIONS"), l.removeFromTop (16), juce::Justification::centredLeft);
}

void GeneratorPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (290).reduced (12, 8);
    left.removeFromTop (20);
    auto cats = left.removeFromTop ((catButtons.size() + 1) / 2 * 32);
    for (int i = 0; i < catButtons.size(); ++i)
        catButtons[i]->setBounds (juce::Rectangle<int> (cats.getX() + (i % 2) * cats.getWidth() / 2, cats.getY() + (i / 2) * 32, cats.getWidth() / 2, 32).reduced (2));
    {
        auto title = left.removeFromTop (8 + 18).withTrimmedTop (4);   // same row as the CHARACTER label
        resetChar.setBounds (title.removeFromRight (78).withSizeKeepingCentre (78, 20));
    }
    auto sl = left.removeFromTop (230).withTrimmedBottom (18);
    const int sw = sl.getWidth() / sliders.size();
    for (int i = 0; i < sliders.size(); ++i) sliders[i]->setBounds (sl.getX() + i * sw, sl.getY(), sw, sl.getHeight());
    left.removeFromTop (8 + 18);
    for (int i = 0; i < locks.size(); ++i)
        locks[i]->setBounds (juce::Rectangle<int> (left.getX() + i * left.getWidth() / 3, left.getY(), left.getWidth() / 3, 26));

    r.removeFromLeft (8);
    auto right = r.removeFromRight (370);
    r.removeFromRight (8);
    // centre
    auto c = r;
    genButton.setBounds (c.removeFromTop (220));
    auto hist = c.removeFromTop (30);
    backBtn.setBounds (hist.withSizeKeepingCentre (160, 26).removeFromLeft (34));
    fwdBtn.setBounds (hist.withSizeKeepingCentre (160, 26).removeFromRight (34));
    histLabel.setBounds (hist.withSizeKeepingCentre (90, 26));
    c.removeFromTop (6);
    console.setBounds (c.removeFromTop (130));
    c.removeFromTop (8);
    grid.setBounds (c.removeFromTop (140));
    c.removeFromTop (10);
    auto act = c.removeFromTop (36);
    const int aw = act.getWidth() / 5;
    for (auto* b : { &mutateBtn, &blendBtn, &saveBtn, &exportBtn, &editorBtn }) b->setBounds (act.removeFromLeft (aw).reduced (3, 0));
    // right
    auto top = right.removeFromTop (190);
    vu.setBounds (top.removeFromRight (44).reduced (2, 0));
    scope.setBounds (top.reduced (2, 0));
    right.removeFromTop (6);
    spectrum.setBounds (right.removeFromTop (110).reduced (2, 0));
    right.removeFromTop (6);
    view.setBounds (right.removeFromTop (150).reduced (2, 0));
    right.removeFromTop (6);
    macros.setBounds (right.reduced (2, 0));
}

}
