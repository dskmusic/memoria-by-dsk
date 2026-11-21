#include "Visuals.h"
#include "Pages.h"

namespace gx::ui
{
// ---------------------------------------------------------------- FrameSource
void FrameSource::update (Frame& f, double dt)
{
    float tmp[4096];
    int n;
    while ((n = proc.readScope (tmp, 4096)) > 0)
        for (int i = 0; i < n; ++i) { ring[(size_t) ringPos] = tmp[i]; ringPos = (ringPos + 1) & 8191; }
    for (int i = 0; i < Frame::kScope; ++i) f.scope[(size_t) i] = ring[(size_t) ((ringPos - Frame::kScope + i) & 8191)];

    std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
    std::copy (f.scope.begin(), f.scope.end(), fftBuf.begin());
    window.multiplyWithWindowingTable (fftBuf.data(), 2048);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());
    const double sr = 48000.0;
    for (int b = 0; b < Frame::kBands; ++b)
    {
        const double f0 = 30.0 * std::pow (600.0, (double) b / Frame::kBands), f1 = 30.0 * std::pow (600.0, (double) (b + 1) / Frame::kBands);
        const int k0 = juce::jlimit (1, 1023, (int) (f0 / sr * 2048)), k1 = juce::jlimit (k0 + 1, 1024, (int) (f1 / sr * 2048) + 1);
        float m = 0;
        for (int k = k0; k < k1; ++k) m = juce::jmax (m, fftBuf[(size_t) k]);
        const float db = juce::Decibels::gainToDecibels (m / 512.0f, -90.0f);
        f.spectrum[(size_t) b] = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 72.0f);
    }
    f.peakL = proc.peakL.load(); f.peakR = proc.peakR.load(); f.rms = proc.rms.load();
    f.energy += (juce::jmin (1.0f, f.rms * 6.0f) - f.energy) * (float) juce::jmin (1.0, dt * 4.0);
    f.time += dt;
    auto& t = proc.telemetry();
    for (int l = 0; l < kNumLayers; ++l)
    {
        f.presence[(size_t) l] = t.presence[(size_t) l].load();
        const float e = juce::jmin (1.0f, t.energy[(size_t) l].load() * 8.0f);
        f.layerEnergy[(size_t) l] += (e - f.layerEnergy[(size_t) l]) * (float) juce::jmin (1.0, dt * 3.0);
    }
    for (int i = 0; i < kChordVoices; ++i) f.chord[(size_t) i] = t.chordNotes[(size_t) i].load();
    f.rootPc = t.rootPc.load(); f.degree = t.chordDegree.load(); f.bellCount = t.bellCount.load();
    f.lastBellNote = t.lastBellNote.load(); f.lastBellPan = t.lastBellPan.load();
    f.runGain = t.runGain.load(); f.evoTime = t.evoTime.load(); f.musTime = t.musTime.load(); f.elapsed = t.sessionElapsed.load();
    f.running = t.running.load();
    f.playButton = proc.isPlayingInternal();
    f.hostPlaying = proc.isHostPlaying();
    f.light = proc.getParam (P::light);
    f.space = proc.getParam (P::space);
    f.harmony = proc.getParam (P::harmony);
    f.scale = (int) proc.getParam (P::scale);
    f.timerMin = (int) proc.getParam (P::timer);
    f.seed = (int) proc.getParam (P::seed);
    f.frozen = proc.getParam (P::freeze) > 0.5f;
    f.scene = (int) proc.getParam (P::scene);
    f.evolution = proc.getParam (P::evolution);
    f.density = proc.getParam (P::density);
}

juce::String chordName (int rootPc, int scale, int degree, float harmony)
{
    const auto& s = scaleSteps (scale);
    const int m = (int) s.size();
    auto semis = [&] (int d) { return s[(size_t) (d % m)] + 12 * (d / m); };
    const int r = semis (degree);
    const juce::String root = keyNames()[(rootPc + r) % 12];
    if (harmony < 0.12f) return keyNames()[rootPc] + "5";
    if (harmony < 0.25f) return root + "5";
    const int third = semis (degree + 2) - r, fifth = semis (degree + 4) - r, seventh = semis (degree + 6) - r;
    juce::String q;
    if (third == 3 && fifth == 6) q = "dim";
    else if (third == 4 && fifth == 8) q = "aug";
    else if (third == 3) q = "m";
    else if (third != 4) q = "sus";
    if (harmony > 0.45f && m >= 7)
    {
        const bool nine = harmony > 0.8f;
        if (q.isEmpty()) q = seventh == 11 ? (nine ? "maj9" : "maj7") : (nine ? "9" : "7");
        else if (q == "m") q = seventh == 11 ? "m(maj7)" : (nine ? "m9" : "m7");
        else if (q == "dim") q = seventh == 10 ? "m7b5" : "dim7";
        else q += nine ? "9" : "7";
    }
    return root + q;
}

// ---------------------------------------------------------------- Landscape
namespace
{
inline float hashF (uint32_t a, uint32_t b)
{
    uint32_t x = a * 0x9E3779B1u ^ (b + 0x85ebca6bu);
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (float) (x >> 8) * (1.0f / 16777216.0f);
}
juce::Colour mix3 (float lt, juce::Colour night, juce::Colour dusk, juce::Colour day)
{
    return lt < 0.5f ? night.interpolatedWith (dusk, lt * 2.0f) : dusk.interpolatedWith (day, (lt - 0.5f) * 2.0f);
}
void softBlob (juce::Graphics& g, float x, float y, float rx, float ry, juce::Colour c)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (1.0f, ry / juce::jmax (1.0f, rx), x, y));
    g.setGradientFill (juce::ColourGradient (c, x, y, c.withAlpha (0.0f), x + rx, y, true));
    g.fillEllipse (x - rx, y - rx, rx * 2, rx * 2);
}
} // namespace

Landscape::Landscape()
{
    setInterceptsMouseClicks (true, true);   // children: the scene buttons
    // small camera button (bottom-left): saves the landscape as a clean, branded picture
    // small glass icon buttons over the landscape: picture (bottom-left) and new landscape variation (under the chord)
    struct GlassButton : juce::Button
    {
        int kind;
        GlassButton (int k, const juce::String& tip) : juce::Button ("glass"), kind (k) { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip (tip); setWantsKeyboardFocus (false); }
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colours::white.withAlpha (over ? 0.14f : 0.07f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (over ? 0.35f : 0.18f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            g.setColour (over ? col::accent : juce::Colours::white.withAlpha (0.7f));
            const auto c = r.getCentre().translated (0, down ? 0.5f : 0.0f);
            if (kind == 0)
            {
                auto body = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.27f).translated (0, 1.0f + (down ? 0.5f : 0.0f));
                juce::Path p;
                p.addRoundedRectangle (body, 2.0f);
                g.strokePath (p, juce::PathStrokeType (1.2f));
                g.drawLine (body.getX() + body.getWidth() * 0.3f, body.getY() - 1.6f, body.getX() + body.getWidth() * 0.55f, body.getY() - 1.6f, 1.2f);
                g.drawEllipse (juce::Rectangle<float> (body.getHeight() * 0.62f, body.getHeight() * 0.62f).withCentre (body.getCentre()), 1.2f);
            }
            else
            {
                const float rad = r.getHeight() * 0.28f;
                juce::Path p;
                p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.5f, 5.6f, true);
                g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const auto tip = c.translated (rad * std::sin (5.6f), -rad * std::cos (5.6f));
                juce::Path head;
                head.addTriangle (tip.translated (-3.2f, -1.6f), tip.translated (1.2f, -3.4f), tip.translated (0.6f, 1.6f));
                g.fillPath (head);
            }
        }
    };
    shotButton = std::make_unique<GlassButton> (0, L ("Captura del paisaje (vista previa)", "Landscape picture (preview)"));
    shotButton->onClick = [this] { takeScreenshot(); };
    addChildComponent (*shotButton);
    variantButton = std::make_unique<GlassButton> (1, L ("Nueva variación de este paisaje (el sonido no cambia)", "New variation of this landscape (the sound does not change)"));
    variantButton->onClick = [] { ++landscapeVariant(); };
    addChildComponent (*variantButton);
    for (size_t i = 0; i < stars.size(); ++i)
    {
        stars[i] = { rng.nextFloat(), rng.nextFloat() };
        twinkle[i] = 0.4f + rng.nextFloat() * 2.5f;
        starSize[i] = rng.nextFloat() < 0.08f ? 2.3f : (rng.nextFloat() < 0.3f ? 1.6f : 1.1f);
    }
}

Landscape::~Landscape() { setGpu (false); }

void Landscape::setGpu (bool on)
{
    if (on && gl == nullptr && gpuStatus().load() >= 0)
    {
        gl = std::make_unique<juce::OpenGLContext>();
        gl->setRenderer (this);
        gl->setComponentPaintingEnabled (true);
        gl->setContinuousRepainting (false);
        gl->attachTo (*this);
    }
    else if (! on && gl != nullptr)
    {
        gl->detach();
        gl.reset();
        glOk = false;
        repaint();
    }
}

void Landscape::newOpenGLContextCreated()
{
    const bool ok = scenes.init (*gl);
    glOk = ok;
    if (! ok) gpuStatus() = -1;
    else if (gpuStatus().load() == 0) gpuStatus() = 1;
}

void Landscape::openGLContextClosing() { scenes.release(); glOk = false; }

void Landscape::renderOpenGL()
{
    if (! glOk.load()) return;
    SceneUniforms u;
    { const juce::SpinLock::ScopedLockType sl (ulock); u = uni; }
    ++framesDrawn;
    const double sc = gl->getRenderingScale();
    scenes.render (u, juce::roundToInt (sc * getWidth()), juce::roundToInt (sc * getHeight()));
}

SceneUniforms Landscape::uniforms() const
{
    SceneUniforms u;
    u.scene = f.scene; u.time = (float) f.time; u.mus = f.musTime; u.evoT = f.evoTime; u.light = f.light; u.evo = f.evolution;
    u.density = f.density; u.space = f.space; u.energy = f.energy; u.run = f.runGain; u.frozen = f.frozen ? 1.0f : 0.0f;
    u.seed = (float) f.seed;
    u.fade = 0.35f + 0.65f * (float) juce::jlimit (0.0, 1.0, (f.time - sceneChangedAt) / 0.6);   // short, gentle dip on a scene change
    for (int l = 0; l < kNumLayers; ++l) { u.pres[l] = pres[(size_t) l]; u.glow[l] = glow[(size_t) l]; }
    return u;
}

void Landscape::rebuildCache()
{
    const int w = getWidth(), h = getHeight();
    cacheKey = juce::String (f.seed) + "/" + juce::String (w) + "x" + juce::String (h);
    juce::Random r ((juce::int64) f.seed * 7919 + 17);
    hasLake = r.nextFloat() < 0.5f;
    hasForest = r.nextFloat() < 0.55f;
    hasBirds = r.nextFloat() < 0.5f;
    hasRings = r.nextFloat() < 0.55f;
    numClouds = 2 + r.nextInt (5);
    planetR = (float) h * (0.35f + 0.35f * r.nextFloat());
    planetPos = { (float) w * (r.nextBool() ? 0.12f + 0.15f * r.nextFloat() : 0.72f + 0.18f * r.nextFloat()), (float) h * (0.75f + 0.35f * r.nextFloat()) };
    const juce::Colour pals[][2] { { juce::Colour (0xff3a6ea5), juce::Colour (0xff0b1a33) }, { juce::Colour (0xffc0704a), juce::Colour (0xff2a0f0a) },
                                   { juce::Colour (0xff7a5ac8), juce::Colour (0xff160b2e) }, { juce::Colour (0xff4fb3a0), juce::Colour (0xff06211d) } };
    const int pi = r.nextInt (4);
    planetCol = pals[pi][0]; planetCol2 = pals[pi][1];

    // ---- Neon Rain skyline: three layers of buildings with lit windows, cached as images
    signs.clear();
    arcology = { (float) w * (0.2f + 0.6f * r.nextFloat()), (float) h * 0.62f };
    if (w <= 0 || h <= 0) return;
    const float base = (float) h * 0.86f;
    for (int layer = 0; layer < 3; ++layer)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        const juce::Colour body[] { juce::Colour (0xff1a2240), juce::Colour (0xff0e1226), juce::Colour (0xff04050b) };
        const float maxH[] { 0.55f, 0.45f, 0.32f }, minW[] { 18, 26, 40 }, maxW[] { 46, 70, 110 };
        float x = -10.0f;
        while (x < (float) w)
        {
            const float bw = minW[layer] + r.nextFloat() * (maxW[layer] - minW[layer]);
            const float bh = (float) h * (0.08f + maxH[layer] * std::pow (r.nextFloat(), 0.8f));
            const juce::Rectangle<float> rb (x, base - bh + (float) layer * h * 0.04f, bw, bh + (float) h);
            g.setColour (body[layer]);
            g.fillRect (rb);
            if (r.nextFloat() < 0.3f) g.fillRect (rb.getCentreX() - 1.0f, rb.getY() - bh * 0.15f, 2.0f, bh * 0.15f); // antenna
            // windows
            const float wsz = layer == 0 ? 2.0f : layer == 1 ? 2.5f : 3.0f;
            for (float wy = rb.getY() + 6; wy < base + (float) layer * h * 0.04f - 4; wy += wsz * 3.0f)
                for (float wx = rb.getX() + 4; wx < rb.getRight() - 4; wx += wsz * 2.6f)
                    if (r.nextFloat() < (layer == 2 ? 0.18f : 0.3f))
                    {
                        const auto wc = r.nextFloat() < 0.7f ? juce::Colour (0xffffd59a) : (r.nextBool() ? juce::Colour (0xff7fe8ff) : juce::Colour (0xffff7ac8));
                        g.setColour (wc.withAlpha (0.35f + 0.55f * r.nextFloat()));
                        g.fillRect (wx, wy, wsz, wsz * 1.3f);
                    }
            // neon signs on the two nearer layers
            if (layer > 0 && r.nextFloat() < 0.45f)
            {
                const juce::Colour nc[] { juce::Colour (0xffff3fa4), juce::Colour (0xff3ff0ff), juce::Colour (0xffffb23f), juce::Colour (0xffa16bff), juce::Colour (0xffff4d4d) };
                const bool vertical = r.nextBool();
                const float sw = vertical ? 6.0f + 4.0f * r.nextFloat() : bw * (0.4f + 0.4f * r.nextFloat());
                const float sh = vertical ? bh * (0.2f + 0.25f * r.nextFloat()) : 7.0f + 5.0f * r.nextFloat();
                signs.push_back ({ juce::Rectangle<float> (rb.getX() + (bw - sw) * r.nextFloat(), rb.getY() + 10 + r.nextFloat() * bh * 0.4f, sw, sh),
                                   nc[r.nextInt (5)], r.nextFloat() * 6.28f, layer });
            }
            x += bw + (layer == 2 ? 2.0f : 0.0f) + r.nextFloat() * 6.0f;
        }
        skyline[(size_t) layer] = img;
    }
}

void Landscape::tick (const Frame& fr)
{
    const int visSeed = fr.seed + landscapeVariant() * 7919;   // the variation button changes the scenery, not the sound
    if (getWidth() > 0 && lastT > 0 && (fr.scene != f.scene || visSeed != f.seed) && ! f.reduced && ! glOk.load() && ! external.isValid())
    {
        const bool ov = showOverlay;
        showOverlay = false;
        fadeFrom = createComponentSnapshot (getLocalBounds(), true);
        showOverlay = ov;
        fadeStart = fr.time;
    }
    const double dt = juce::jlimit (0.0, 0.2, fr.time - lastT);
    lastT = fr.time;
    f = fr;
    shownSeed = fr.seed;
    f.seed = visSeed;
    for (int l = 0; l < kNumLayers; ++l)
    {
        pres[(size_t) l] += (f.presence[(size_t) l] - pres[(size_t) l]) * (float) juce::jmin (1.0, dt * 1.5);
        glow[(size_t) l] += (f.layerEnergy[(size_t) l] - glow[(size_t) l]) * (float) juce::jmin (1.0, dt * 4.0);
    }
    const auto b = getLocalBounds().toFloat();
    if (lastBells >= 0 && f.bellCount != lastBells && getWidth() > 0)
    {
        const int count = juce::jmin (4, f.bellCount - lastBells);
        for (int k = 0; k < count; ++k)
        {
            const float x = b.getWidth() * (0.5f + f.lastBellPan * 0.45f) + rng.nextFloat() * 24.0f - 12.0f;
            const float y = b.getHeight() * (0.55f - juce::jlimit (0.0f, 1.0f, (float) (f.lastBellNote - 60) / 40.0f) * 0.4f);
            Spark s { x, y, 0.0f, -8.0f - rng.nextFloat() * 10.0f, 1.0f, 2.0f + rng.nextFloat() * 2.5f, rng.nextBool() ? col::accent2 : juce::Colours::white, 0 };
            if (f.scene == scStarfield) { s.kind = 1; s.vx = (rng.nextBool() ? 1.0f : -1.0f) * (60.0f + 60.0f * rng.nextFloat()); s.vy = 25.0f + 20.0f * rng.nextFloat(); s.y *= 0.6f; }
            if (f.scene == scUnderwater) { s.kind = 3; s.vy = -35.0f - 25.0f * rng.nextFloat(); s.c = juce::Colour (0xffd8fbff); }
            if (f.scene == scNeon) { s.kind = 2; s.vx = (rng.nextBool() ? 1.0f : -1.0f) * (35.0f + 50.0f * rng.nextFloat()); s.vy = 0; s.x = s.vx > 0 ? -10.0f : b.getWidth() + 10.0f;
                                     s.y = b.getHeight() * (0.2f + 0.35f * rng.nextFloat()); s.c = rng.nextBool() ? juce::Colour (0xffff5a5a) : juce::Colour (0xffbfe9ff); }
            sparks.push_back (s);
        }
    }
    lastBells = f.bellCount;
    if (f.scene != lastScene || f.seed != lastSeedSeen) { if (lastScene >= 0) sceneChangedAt = f.time; lastScene = f.scene; lastSeedSeen = f.seed; }
    { const juce::SpinLock::ScopedLockType sl (ulock); uni = uniforms(); }
    if (gl != nullptr) gl->triggerRepaint();
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (fpsT0 <= 0) fpsT0 = now;
        else if (now - fpsT0 >= 1.0) { fps = (float) (framesDrawn.exchange (0) / (now - fpsT0)); fpsT0 = now; }
    }
    for (auto& s : sparks) { s.x += s.vx * (float) dt; s.y += s.vy * (float) dt; s.life -= (float) dt / (s.kind == 2 ? 14.0f : 4.0f); }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [&] (const Spark& s) { return s.life <= 0 || s.x < -40 || s.x > b.getWidth() + 40; }), sparks.end());
    if (sparks.size() > 80) sparks.erase (sparks.begin(), sparks.begin() + (long) (sparks.size() - 80));
    repaint();
}

void Landscape::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto key = juce::String (f.seed) + "/" + juce::String (getWidth()) + "x" + juce::String (getHeight());
    if (key != cacheKey) rebuildCache();
    juce::Path clip;
    clip.addRoundedRectangle (b, fullBleed ? 0.0f : 10.0f);
    if ((glOk.load() && scenes.has (f.scene)) || external.isValid())
    {
        // the scene comes from the GPU: only the note stars, the rounded corners and the texts are painted here
        if (external.isValid()) { g.saveState(); g.reduceClipRegion (clip); g.drawImage (external, b); g.restoreState(); }
        g.saveState();
        g.reduceClipRegion (clip);
        paintSparks (g);
        g.restoreState();
        if (! fullBleed && ! external.isValid())
        {
            juce::Path corners;
            corners.addRectangle (b);
            corners.addRoundedRectangle (b, 10.0f);
            corners.setUsingNonZeroWinding (false);
            g.setColour (col::bg);
            g.fillPath (corners);
        }
        if (showOverlay) paintOverlay (g, b);
        paintFps (g, b);
        if (paintExtra) paintExtra (g);
        return;
    }
    g.saveState();
    g.reduceClipRegion (clip);
    flareAmt = 0;
    {   // slow cinematic camera: a gentle push-in and drift (scene drawn 3-5 % larger, edges never show)
        const float tt = (float) f.time;
        const float z = 1.035f + 0.015f * std::sin (tt * 0.021f);
        const float dx = std::sin (tt * 0.013f) * b.getWidth() * 0.008f, dy = std::sin (tt * 0.017f + 1.0f) * b.getHeight() * 0.006f;
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (z, z, b.getCentreX(), b.getCentreY()).translated (dx, dy));
    }
    switch (f.scene)
    {
        case scStarfield: paintStarfield (g, b); break;
        case scUnderwater: paintUnderwater (g, b); break;
        case scNeon: paintNeon (g, b); break;
        case scOcean: paintOcean (g, b); break;
        case scAurora: paintAurora (g, b); break;
        default: paintValley (g, b); break;
    }
    paintSparks (g);
    g.restoreState();   // end of camera
    paintPost (g, b);
    if (fadeFrom.isValid())
    {
        const float a = 1.0f - (float) ((f.time - fadeStart) / 1.2);
        if (a <= 0.0f) fadeFrom = juce::Image();
        else { g.setOpacity (a * a * (3.0f - 2.0f * a)); g.drawImage (fadeFrom, b); g.setOpacity (1.0f); }
    }
    // cinematic vignette on every scene
    {
        juce::ColourGradient vg (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(), juce::Colours::black.withAlpha (0.45f), b.getX(), b.getY(), true);
        vg.addColour (0.6, juce::Colours::transparentBlack);
        g.setGradientFill (vg);
        g.fillRect (b);
    }
    if (f.frozen) { g.setColour (juce::Colour (0xff9fd8ff).withAlpha (0.10f)); g.fillRect (b); }
    const float dim = 0.45f * (1.0f - f.runGain);
    if (dim > 0.01f) { g.setColour (juce::Colours::black.withAlpha (dim)); g.fillRect (b); }
    g.restoreState();
    if (showOverlay) paintOverlay (g, b);
    ++framesDrawn;
    paintFps (g, b);
    if (paintExtra) paintExtra (g);
}

void Landscape::resized()
{
    cacheKey = {};
}

namespace
{
// preview of the picture with Save... (choose where) / Cancel
struct ShotPreview : juce::Component
{
    juce::Image img;
    juce::TextButton save { L ("Guardar...", "Save...") }, cancel { L ("Cancelar", "Cancel") };
    std::unique_ptr<juce::FileChooser> chooser;
    explicit ShotPreview (const juce::Image& i) : img (i)
    {
        addAndMakeVisible (save);
        addAndMakeVisible (cancel);
        cancel.onClick = [this] { close(); };
        save.onClick = [this]
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userPicturesDirectory);
            const auto name = "Memoria " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
            chooser = std::make_unique<juce::FileChooser> (L ("Guardar captura", "Save picture"), dir.getNonexistentChildFile (name, ".png"), "*.png");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("png");
                f.deleteFile();
                bool ok = false;
                if (auto out = f.createOutputStream()) { juce::PNGImageFormat png; ok = png.writeImageToStream (img, *out); }
                if (ok) close();
                else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Memoria", L ("No se pudo guardar la captura en:\n", "The picture could not be saved to:\n") + f.getFullPathName());
            });
        };
        const int w = 960;
        setSize (w, juce::roundToInt ((float) w * (float) img.getHeight() / (float) img.getWidth()) + 70);
    }
    void close() { if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        g.drawImage (img, getLocalBounds().withTrimmedBottom (60).reduced (12).toFloat(), juce::RectanglePlacement::centred);
    }
    void resized() override
    {
        auto r = getLocalBounds().removeFromBottom (60).reduced (12, 14);
        cancel.setBounds (r.removeFromRight (120));
        r.removeFromRight (10);
        save.setBounds (r.removeFromRight (150));
    }
};

void showShotPreview (const juce::Image& img, juce::Component* around)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ShotPreview (img));
    o.dialogTitle = L ("Captura del paisaje", "Landscape picture");
    o.dialogBackgroundColour = col::bg;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.escapeKeyTriggersCloseButton = true;
    o.componentToCentreAround = around;
    if (auto* dw = o.launchAsync()) dw->setAlwaysOnTop (true);   // stays above a DAW or the full-screen view
}
}

void Landscape::takeScreenshot()
{
    const float k = 2.0f;   // twice the on-screen size
    const int W = juce::roundToInt ((float) getWidth() * k), H = juce::roundToInt ((float) getHeight() * k);
    if (W < 10 || H < 10) return;
    juce::Image bg;
    if (glOk.load())
    {
        bg = OffscreenGpu::get().render (uniforms(), W, H);
        if (! bg.isValid()) { shotMsg = L ("No se pudo hacer la captura", "The picture could not be taken"); shotMsgAt = juce::Time::getMillisecondCounterHiRes() * 0.001; return; }
    }
    // the bare landscape: no texts, no buttons, square corners
    const bool ov = showOverlay, fb = fullBleed;
    const auto oldExt = external;
    showOverlay = false; fullBleed = true;
    if (bg.isValid()) external = bg;
    std::vector<juce::Component*> hidden;
    for (auto* c : getChildren()) if (c->isVisible()) { c->setVisible (false); hidden.push_back (c); }
    juce::Image img = createComponentSnapshot (getLocalBounds(), true, k).convertedToFormat (juce::Image::RGB);
    for (auto* c : hidden) c->setVisible (true);
    showOverlay = ov; fullBleed = fb; external = oldExt;

    {   // branding: logo + name top-right (as in the header), credit at the foot
        juce::Graphics g (img);
        const float s = (float) W / 1280.0f;
        juce::GlyphArrangement ga;
        ga.addLineOfText (techFont (21.0f * s), "MEMORIA", 0, 0);
        juce::Path text;
        ga.createPath (text);
        auto tb = text.getBounds();
        g.setFont (font (12.0f * s, true));
        const float byW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "by DSK");
        const float margin = 22.0f * s, markW = 30.0f * s * 1.6f;
        const float textX = (float) W - margin - byW - 7.0f * s - tb.getWidth();
        const float baseY = margin + 22.0f * s;
        text.applyTransform (juce::AffineTransform::translation (textX - tb.getX(), baseY));
        tb = text.getBounds();
        Logo::drawMark (g, juce::Rectangle<float> (tb.getX() - 8.0f * s - markW, tb.getCentreY() - 15.0f * s, markW, 30.0f * s).reduced (4.0f * s, 5.0f * s), 0.6f, 0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (text, juce::AffineTransform::translation (0, 1.5f * s));
        g.setColour (col::accent.withAlpha (0.3f));
        g.strokePath (text, juce::PathStrokeType (3.0f * s));
        juce::ColourGradient grad (juce::Colours::white, tb.getX(), tb.getY(), col::accent, tb.getRight(), tb.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (text);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText ("by DSK", juce::Rectangle<float> (tb.getRight() + 7.0f * s, tb.getBottom() - 14.0f * s, byW + 4.0f, 16.0f * s), juce::Justification::bottomLeft);
        // credit
        g.setFont (techFont (10.5f * s));
        const juce::String credit = "Memoria by DSK  -  www.dskmusic.com/memoria";
        auto foot = juce::Rectangle<float> (0, (float) H - 34.0f * s, (float) W, 20.0f * s);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawText (credit, foot.translated (0, 1.2f * s), juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (credit, foot, juce::Justification::centred);
    }
    showShotPreview (img, this);
}

void Landscape::paintFps (juce::Graphics& g, juce::Rectangle<float> b)
{
    if (shotButton != nullptr)   // placed here, not in resized(): full screen / page mode can change without a resize
    {
        shotButton->setBounds (12, getHeight() - 58, 26, 20);
        shotButton->setVisible (showOverlay && ! external.isValid());
        variantButton->setBounds (16, 70, 26, 20);
        variantButton->setVisible (showOverlay && ! external.isValid());
    }
    const double age = juce::Time::getMillisecondCounterHiRes() * 0.001 - shotMsgAt;
    if (age < 3.0 && ! external.isValid())
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f * (float) juce::jmin (1.0, (3.0 - age) * 2.0)));
        g.setFont (font (11.5f, true));
        g.drawText (shotMsg, juce::Rectangle<float> (b.getX() + 44.0f, b.getBottom() - 58.0f, 400.0f, 20.0f), juce::Justification::centredLeft);
        return;
    }
    if (! showFps() || ! showOverlay || external.isValid()) return;   // only on the landscape views (not in the generator preview, videos or pictures)   // never in exported videos or screenshots
    const auto r = juce::Rectangle<float> (b.getX() + 44.0f, b.getBottom() - 56.0f, 80.0f, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (juce::roundToInt (fps)) + " FPS" + (glOk.load() ? " GPU" : ""), r, juce::Justification::centredLeft);
}

// ---- Valley: sky, sun/moon, clouds, shooting stars, aurora, mountain ranges, forest, lake, birds, mist, dust
void Landscape::paintValley (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), lt = f.light, t = (float) f.time, mt = f.musTime;
    const float horizon = h * (hasLake ? 0.62f : 0.70f);
    const auto skyTop = mix3 (lt, juce::Colour (0xff03040c), juce::Colour (0xff1b1544), juce::Colour (0xff2c64ad));
    const auto skyLow = mix3 (lt, juce::Colour (0xff151b42), juce::Colour (0xffd9765f), juce::Colour (0xfff0d7a8));
    g.setGradientFill (juce::ColourGradient (skyTop, 0, 0, skyLow, 0, horizon, false));
    g.fillRect (b);
    const float starA = juce::jlimit (0.0f, 1.0f, (0.62f - lt) * 2.2f);
    if (starA > 0.01f)
        for (size_t i = 0; i < stars.size(); ++i)
        {
            const float tw = 0.55f + 0.45f * std::sin (t * twinkle[i] + (float) i);
            g.setColour (juce::Colours::white.withAlpha (starA * tw * (i % 7 == 0 ? 0.95f : 0.55f)));
            g.fillEllipse (stars[i].x * w, stars[i].y * horizon * 0.95f, starSize[i], starSize[i]);
        }
    // shooting stars at night
    if (starA > 0.3f)
    {
        const int slot = (int) (t / 6.0f);
        const float ph = (t - (float) slot * 6.0f) / 0.9f;
        if (hashF ((uint32_t) f.seed, (uint32_t) slot) < 0.55f && ph < 1.0f)
        {
            const float sx = w * (0.1f + 0.8f * hashF ((uint32_t) slot, 3)), sy = horizon * (0.1f + 0.35f * hashF ((uint32_t) slot, 5));
            const float x = sx + ph * w * 0.18f, y = sy + ph * h * 0.08f;
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (starA * (1.0f - ph)), x, y, juce::Colours::white.withAlpha (0.0f), x - w * 0.08f, y - h * 0.035f, false));
            g.drawLine (x - w * 0.08f, y - h * 0.035f, x, y, 1.6f);
        }
    }
    // sun / moon on the evolution clock
    {
        const float ph = wrap01 (f.evoTime / 420.0f + (float) (f.seed % 97) * 0.0137f);
        const float x = w * (0.08f + 0.84f * ph), y = horizon + 10.0f - std::sin (ph * kPi) * h * 0.5f;
        const bool sun = lt > 0.5f;
        const auto c = sun ? juce::Colour (0xffffd27a) : juce::Colour (0xffdfe6ff);
        const float rad = 15.0f + 8.0f * lt, gl = 90.0f + 110.0f * f.energy;
        flarePos = { x, y }; flareCol = c; flareAmt = 0.6f + 0.4f * f.energy;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.35f), x, y, c.withAlpha (0.0f), x + gl, y, true));
        g.fillEllipse (x - gl, y - gl, gl * 2, gl * 2);
        g.setColour (c);
        g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
        if (! sun) { g.setColour (skyTop.withAlpha (0.55f)); g.fillEllipse (x - rad * 0.45f, y - rad * 1.05f, rad * 1.7f, rad * 1.7f); }
    }
    // aurora (Shimmer)
    {
        const float a = juce::jlimit (0.0f, 1.0f, pres[lyShimmer] * (0.35f + glow[lyShimmer] * 1.5f));
        if (a > 0.01f)
            for (int k = 0; k < 3; ++k)
            {
                juce::Path rib;
                const float base = h * (0.14f + 0.09f * (float) k), thick = h * (0.10f + 0.04f * (float) k);
                for (int i = 0; i <= 64; ++i)
                {
                    const float x = w * (float) i / 64.0f;
                    const float y = base + std::sin (x * 0.006f + t * 0.13f + (float) k * 1.7f) * h * 0.05f + std::sin (x * 0.017f - t * 0.09f + (float) k) * h * 0.025f;
                    if (i == 0) rib.startNewSubPath (x, y); else rib.lineTo (x, y);
                }
                for (int i = 64; i >= 0; --i)
                {
                    const float x = w * (float) i / 64.0f;
                    rib.lineTo (x, base + thick + std::sin (x * 0.006f + t * 0.13f + (float) k * 1.7f + 0.6f) * h * 0.05f);
                }
                rib.closeSubPath();
                const auto c = (k == 1 ? col::accent3 : col::accent);
                juce::ColourGradient gr (c.withAlpha (0.0f), 0, base - h * 0.04f, c.withAlpha (0.0f), 0, base + thick + h * 0.05f, false);
                gr.addColour (0.45, c.withAlpha (a * (0.30f - 0.06f * (float) k)));
                g.setGradientFill (gr);
                g.fillPath (rib);
            }
    }
    // clouds drifting (more visible by day and dusk)
    {
        const auto cc = mix3 (lt, juce::Colour (0xff3a4268), juce::Colour (0xffe8a48c), juce::Colours::white);
        for (int i = 0; i < numClouds; ++i)
        {
            const float sp = 3.0f + 2.0f * hashF ((uint32_t) f.seed, (uint32_t) i + 40);
            const float cx = std::fmod (w * hashF ((uint32_t) f.seed, (uint32_t) i + 50) + mt * sp, w * 1.4f) - w * 0.2f;
            const float cy = horizon * (0.15f + 0.45f * hashF ((uint32_t) f.seed, (uint32_t) i + 60));
            const float cw = w * (0.08f + 0.08f * hashF ((uint32_t) f.seed, (uint32_t) i + 70));
            for (int k = 0; k < 4; ++k)
                softBlob (g, cx + (float) (k - 1.5f) * cw * 0.45f, cy + (k % 2) * cw * 0.08f, cw * (0.5f + 0.15f * (float) (k % 3)), cw * 0.22f, cc.withAlpha (0.16f + 0.18f * lt));
        }
    }
    // mountain ranges
    auto ridge = [&] (int k, float baseY, float amp, float scroll, float f1, float f2, float f3)
    {
        juce::Path p;
        p.startNewSubPath (0, h);
        const float s0 = (float) (f.seed % 1000) * 0.37f + (float) k * 2.1f;
        for (int i = 0; i <= 160; ++i)
        {
            const float x = w * (float) i / 160.0f, xs = x + scroll;
            const float n = 0.55f * std::sin (xs * f1 + s0) + 0.3f * std::sin (xs * f2 + s0 * 1.7f) + 0.15f * std::sin (xs * f3 + s0 * 2.3f);
            p.lineTo (x, baseY - amp * (0.5f + 0.5f * n));
        }
        p.lineTo (w, h);
        p.closeSubPath();
        return p;
    };
    auto ridgeY = [&] (int k, float baseY, float amp, float scroll, float f1, float f2, float f3, float x)
    {
        const float s0 = (float) (f.seed % 1000) * 0.37f + (float) k * 2.1f, xs = x + scroll;
        const float n = 0.55f * std::sin (xs * f1 + s0) + 0.3f * std::sin (xs * f2 + s0 * 1.7f) + 0.15f * std::sin (xs * f3 + s0 * 2.3f);
        return baseY - amp * (0.5f + 0.5f * n);
    };
    struct R { int layer; float base, amp, speed, f1, f2, f3, shade; };
    const R ranges[3] { { lyDrone, horizon - h * 0.02f, h * 0.30f, 0.6f, 0.0042f, 0.011f, 0.027f, 0.35f },
                        { lyPad, horizon + h * 0.07f, h * 0.22f, 1.5f, 0.0061f, 0.016f, 0.041f, 0.55f },
                        { lyTexture, horizon + h * 0.16f, h * 0.14f, 3.2f, 0.0089f, 0.023f, 0.058f, 0.75f } };
    for (int ri = 0; ri < (hasLake ? 2 : 3); ++ri)
    {
        const auto& r = ranges[ri];
        const float pr = pres[(size_t) r.layer];
        const float amp = r.amp * (0.35f + 0.65f * pr) * (1.0f + 0.12f * glow[(size_t) r.layer]);
        auto p = ridge (r.layer, r.base, amp, mt * r.speed, r.f1, r.f2, r.f3);
        const auto lc = col::layer[r.layer];
        const auto topC = lc.interpolatedWith (skyLow, 0.55f - 0.35f * r.shade).darker (0.6f + r.shade * 1.4f).withMultipliedSaturation (0.6f + 0.4f * pr);
        g.setGradientFill (juce::ColourGradient (topC.brighter (0.25f * glow[(size_t) r.layer]), 0, r.base - r.amp, juce::Colour (0xff05060c).interpolatedWith (topC, 0.25f), 0, h, false));
        g.fillPath (p);
        g.setColour (lc.withAlpha (0.18f + 0.5f * pr * glow[(size_t) r.layer]));
        g.strokePath (p, juce::PathStrokeType (1.2f));
        // pine forest on the middle range
        if (ri == 1 && hasForest)
        {
            g.setColour (juce::Colour (0xff05070f).interpolatedWith (topC, 0.2f));
            for (int i = 0; i < 70; ++i)
            {
                const float x = w * hashF ((uint32_t) f.seed, (uint32_t) i + 100);
                const float y = ridgeY (r.layer, r.base, amp, mt * r.speed, r.f1, r.f2, r.f3, x);
                const float th = h * (0.025f + 0.03f * hashF ((uint32_t) f.seed, (uint32_t) i + 200));
                juce::Path tree;
                tree.addTriangle (x - th * 0.28f, y + 2, x + th * 0.28f, y + 2, x, y - th);
                g.fillPath (tree);
            }
        }
        // mist (Wind) between the ranges
        const float mist = pres[lyWind] * (0.25f + 0.6f * glow[lyWind]);
        if (mist > 0.01f)
            for (int i = 0; i < 5; ++i)
            {
                const float mx = std::fmod ((float) i * w * 0.31f + mt * (12.0f + 6.0f * (float) i) * (0.6f + r.speed * 0.3f), w * 1.6f) - w * 0.3f;
                softBlob (g, mx, r.base - r.amp * 0.15f + (float) i * 3.0f, w * (0.18f + 0.05f * (float) (i % 3)), h * 0.03f, juce::Colours::white.withAlpha (0.10f * mist));
            }
    }
    // lake with reflections
    if (hasLake)
    {
        const float wy = horizon + h * 0.10f;
        auto water = b.withTop (wy);
        g.setGradientFill (juce::ColourGradient (skyLow.darker (0.5f).withAlpha (0.95f), 0, wy, skyTop.darker (0.8f), 0, h, false));
        g.fillRect (water);
        for (int i = 0; i < 26; ++i)
        {
            const float y = wy + 3.0f + (float) i * (h - wy) / 26.0f;
            const float len = w * (0.05f + 0.12f * hashF ((uint32_t) i, 9)) * (1.0f + 0.3f * std::sin (t * 0.8f + (float) i));
            const float x = w * hashF ((uint32_t) i, (uint32_t) (t * 0.3f) + 11);
            g.setColour (skyLow.brighter (0.3f).withAlpha (0.10f + 0.12f * lt));
            g.fillRect (x, y, len, 1.0f);
        }
        // grain dust over the water (Texture) as glints
        const float a = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.5f);
        for (int i = 0; i < 40 && a > 0.01f; ++i)
        {
            const float x = w * hashF ((uint32_t) i, 77), y = wy + (h - wy) * hashF ((uint32_t) i, 78);
            g.setColour (col::layer[lyTexture].withAlpha (juce::jlimit (0.0f, 0.8f, a * (0.5f + 0.5f * std::sin (t * 2.0f + (float) i)))));
            g.fillRect (x, y, 3.0f, 1.0f);
        }
    }
    // grain dust (Texture)
    {
        const float a = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.6f);
        if (a > 0.01f)
            for (int i = 0; i < 70; ++i)
            {
                const float x = std::fmod ((float) (i * 7919 % 1000) / 1000.0f * w + mt * (4.0f + (float) (i % 5)), w);
                const float y = horizon + h * 0.05f - (float) (i * 104729 % 1000) / 1000.0f * h * 0.32f + std::sin (t * 0.7f + (float) i) * 4.0f;
                const float tw = 0.5f + 0.5f * std::sin (t * (1.5f + (float) (i % 4)) + (float) i * 1.3f);
                g.setColour (col::layer[lyTexture].withAlpha (juce::jlimit (0.0f, 0.9f, a * tw * 0.7f)));
                g.fillEllipse (x, y, 1.8f, 1.8f);
            }
    }
    // fireflies at dusk and night (Bells)
    if (lt < 0.5f)
        for (int i = 0; i < 36; ++i)
        {
            const float x = std::fmod (w * hashF ((uint32_t) f.seed, (uint32_t) i + 3000) + std::sin (t * 0.3f + (float) i) * 30.0f + w, w);
            const float y = h * (0.72f + 0.2f * hashF ((uint32_t) i, 3001)) + std::cos (t * 0.4f + (float) i * 1.3f) * 12.0f;
            const float blink = juce::jmax (0.0f, std::sin (t * (0.8f + 0.5f * hashF ((uint32_t) i, 3002)) + (float) i * 2.1f));
            const float a = blink * blink * (0.25f + 0.75f * pres[lyBells]) * (0.5f - lt) * 2.0f;
            if (a > 0.02f) softBlob (g, x, y, 7.0f, 7.0f, juce::Colour (0xffd9ff7a).withAlpha (a));
        }
    // birds by day
    if (hasBirds && lt > 0.5f)
    {
        g.setColour (juce::Colour (0xff1a1a24).withAlpha (0.7f * juce::jmin (1.0f, (lt - 0.5f) * 4.0f)));
        const float bx = std::fmod (mt * 14.0f + w * 0.3f, w * 1.5f) - w * 0.25f, by = horizon * 0.42f;
        for (int i = 0; i < 7; ++i)
        {
            const float x = bx - (float) std::abs (i - 3) * 14.0f, y = by + (float) i * 3.0f + (float) std::abs (i - 3) * 6.0f;
            const float flap = 3.0f * std::sin (t * 6.0f + (float) i);
            juce::Path bird;
            bird.startNewSubPath (x - 6, y - flap);
            bird.quadraticTo (x - 3, y - 3, x, y);
            bird.quadraticTo (x + 3, y - 3, x + 6, y - flap);
            g.strokePath (bird, juce::PathStrokeType (1.3f));
        }
    }
    // ground
    {
        auto p = ridge (9, h * 0.97f, h * 0.10f, mt * 6.0f, 0.012f, 0.031f, 0.07f);
        g.setColour (juce::Colour (0xff04050a));
        g.fillPath (p);
    }
}

// ---- Starfield: parallax stars, galactic band (Shimmer), planet (Drone) + moon (Pad), asteroid dust, comets (Bells)
void Landscape::paintStarfield (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), t = (float) f.time, mt = f.musTime;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff02030a), 0, 0, juce::Colour (0xff0a0f24).interpolatedWith (planetCol2, 0.3f), 0, h, false));
    g.fillRect (b);
    // galactic band
    {
        const float a = 0.15f + 0.6f * pres[lyShimmer] * (0.4f + glow[lyShimmer]);
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (-0.35f, w * 0.5f, h * 0.5f));
        juce::ColourGradient band (col::accent3.withAlpha (0.0f), 0, h * 0.3f, col::accent3.withAlpha (0.0f), 0, h * 0.7f, false);
        band.addColour (0.5, col::accent3.interpolatedWith (col::accent, 0.4f).withAlpha (0.22f * a));
        g.setGradientFill (band);
        g.fillRect (-w * 0.3f, h * 0.3f, w * 1.6f, h * 0.4f);
        for (int i = 0; i < 160; ++i)
        {
            const float x = -w * 0.3f + w * 1.6f * hashF ((uint32_t) i, 301), y = h * 0.5f + h * 0.13f * (hashF ((uint32_t) i, 302) + hashF ((uint32_t) i, 303) - 1.0f);
            g.setColour (juce::Colours::white.withAlpha (a * 0.6f * hashF ((uint32_t) i, 304)));
            g.fillRect (x, y, 1.0f, 1.0f);
        }
    }
    {   // shooting stars
        const int slot = (int) (t / 5.0f);
        const float ph = (t - (float) slot * 5.0f) / 0.7f;
        if (hashF ((uint32_t) f.seed + 7, (uint32_t) slot) < 0.6f && ph < 1.0f)
        {
            const float sx0 = w * hashF ((uint32_t) slot, 13), sy0 = h * 0.4f * hashF ((uint32_t) slot, 15);
            const float x = sx0 + ph * w * 0.2f, y = sy0 + ph * h * 0.1f;
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (1.0f - ph), x, y, juce::Colours::white.withAlpha (0.0f), x - w * 0.09f, y - h * 0.045f, false));
            g.drawLine (x - w * 0.09f, y - h * 0.045f, x, y, 1.6f);
        }
    }
    // parallax stars (three depths), speed follows Evolution
    const float speed = 2.0f + 30.0f * f.evolution;
    for (size_t i = 0; i < stars.size(); ++i)
    {
        const float depth = 0.3f + 0.7f * (float) (i % 3) / 2.0f;
        const float x = std::fmod (stars[i].x * w - mt * speed * depth + w * 100.0f, w), y = stars[i].y * h;
        const float tw = 0.6f + 0.4f * std::sin (t * twinkle[i] + (float) i);
        g.setColour (juce::Colours::white.withAlpha (tw * (0.3f + 0.6f * depth)));
        const float s = starSize[i] * depth * 1.2f;
        g.fillEllipse (x, y, s, s);
        if (f.density > 0.75f && depth > 0.9f) // light speed streaks when dense
        {
            g.setColour (juce::Colours::white.withAlpha (0.15f * (f.density - 0.75f) * 4.0f));
            g.drawLine (x, y + s * 0.5f, x + s * 10.0f, y + s * 0.5f, 0.6f);
        }
    }
    // solar wind (Wind)
    if (pres[lyWind] > 0.02f)
        for (int i = 0; i < 30; ++i)
        {
            const float x = std::fmod (w * hashF ((uint32_t) i, 401) - mt * 40.0f * (1.0f + hashF ((uint32_t) i, 402)) + w * 100.0f, w);
            const float y = h * hashF ((uint32_t) i, 403);
            g.setColour (col::layer[lyWind].withAlpha (0.12f * pres[lyWind] * (0.5f + glow[lyWind])));
            g.drawLine (x, y, x + 40.0f, y, 0.8f);
        }
    // asteroid dust (Texture)
    {
        const float a = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.5f);
        for (int i = 0; i < 50 && a > 0.01f; ++i)
        {
            const float x = std::fmod (w * hashF ((uint32_t) i, 501) + mt * (8.0f + 6.0f * hashF ((uint32_t) i, 502)), w);
            const float y = h * (0.55f + 0.4f * hashF ((uint32_t) i, 503)) + std::sin (t * 0.4f + (float) i) * 3.0f;
            g.setColour (juce::Colour (0xffb7a99a).withAlpha (juce::jlimit (0.0f, 0.8f, a * 0.8f)));
            const float s = 1.5f + 2.5f * hashF ((uint32_t) i, 504);
            g.fillEllipse (x, y, s, s * 0.8f);
        }
    }
    // planet (Drone) with atmosphere and optional rings
    {
        const float r = planetR, gl = r * (1.15f + 0.1f * glow[lyDrone] + 0.05f * pres[lyDrone]);
        const auto p = planetPos;
        g.setGradientFill (juce::ColourGradient (planetCol.withAlpha (0.35f * (0.5f + glow[lyDrone])), p.x, p.y, planetCol.withAlpha (0.0f), p.x + gl * 1.2f, p.y, true));
        g.fillEllipse (p.x - gl * 1.2f, p.y - gl * 1.2f, gl * 2.4f, gl * 2.4f);
        auto ringPath = [&] (bool back)
        {
            juce::Path rp;
            rp.addCentredArc (p.x, p.y, r * 1.9f, r * 0.35f, -0.25f, back ? -kPi * 0.5f : kPi * 0.5f, back ? kPi * 0.5f : kPi * 1.5f, true);
            return rp;
        };
        if (hasRings) { g.setColour (planetCol.brighter (0.5f).withAlpha (0.35f)); g.strokePath (ringPath (true), juce::PathStrokeType (r * 0.06f)); }
        g.setGradientFill (juce::ColourGradient (planetCol.brighter (0.3f), p.x - r * 0.6f, p.y - r * 0.6f, planetCol2, p.x + r * 0.5f, p.y + r * 0.6f, true));
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path disc; disc.addEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.reduceClipRegion (disc);
        for (int k = 0; k < 7; ++k) // cloud bands drifting
        {
            const float by = p.y - r + (float) k * r * 0.3f + std::sin (t * 0.05f + (float) k) * 4.0f;
            g.setColour (planetCol.interpolatedWith (juce::Colours::white, 0.2f).withAlpha (0.10f));
            g.fillRect (p.x - r, by, r * 2, r * 0.08f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, p.x - r * 0.3f, p.y - r * 0.3f, juce::Colours::black.withAlpha (0.75f), p.x + r, p.y + r, true));
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
    }
    if (hasRings)
    {
        juce::Path rp;
        rp.addCentredArc (planetPos.x, planetPos.y, planetR * 1.9f, planetR * 0.35f, -0.25f, kPi * 0.5f, kPi * 1.5f, true);
        g.setColour (planetCol.brighter (0.6f).withAlpha (0.5f));
        g.strokePath (rp, juce::PathStrokeType (planetR * 0.06f));
    }
    // moon (Pad) orbiting
    {
        const float a = f.evoTime * 0.02f + (float) (f.seed % 31);
        const float mx = planetPos.x + std::cos (a) * planetR * 2.4f;
        float my = planetPos.y - planetR * 0.9f + std::sin (a) * planetR * 0.5f;
        const float mr = planetR * 0.12f * (0.7f + 0.3f * pres[lyPad]);
        my = juce::jmax (my, mr + 14.0f);
        const auto mc = col::layer[lyPad].interpolatedWith (juce::Colours::white, 0.5f);
        g.setGradientFill (juce::ColourGradient (mc.brighter (0.3f), mx - mr * 0.5f, my - mr * 0.5f, mc.darker (1.6f), mx + mr, my + mr, true));
        g.fillEllipse (mx - mr, my - mr, mr * 2, mr * 2);
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (mx - mr * 0.3f, my - mr * 0.1f, mr * 0.35f, mr * 0.3f);
        g.fillEllipse (mx + mr * 0.2f, my + mr * 0.3f, mr * 0.25f, mr * 0.2f);
    }
}

// ---- Underwater: light rays from the surface, caustics, kelp and coral, jellyfish pulsing with the drone,
//      a school of fish, plankton (Texture), bubbles (Bells) and, now and then, a whale far away
void Landscape::paintUnderwater (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), t = (float) f.time, lt = f.light, mt = f.musTime;
    const auto top = juce::Colour (0xff0d5c7a).interpolatedWith (juce::Colour (0xff3fa9c9), lt * 0.6f);
    const auto mid = juce::Colour (0xff06304a).interpolatedWith (juce::Colour (0xff12607e), lt * 0.5f);
    const auto deep = juce::Colour (0xff010812);
    juce::ColourGradient water (top, 0, 0, deep, 0, h, false);
    water.addColour (0.45, mid);
    g.setGradientFill (water);
    g.fillRect (b);
    // the surface, seen from below
    {
        juce::Path surf;
        surf.startNewSubPath (0, 0);
        for (int i = 0; i <= 80; ++i)
        {
            const float x = w * (float) i / 80.0f;
            surf.lineTo (x, h * 0.035f + std::sin (x * 0.02f + t * 1.1f) * 4.0f + std::sin (x * 0.047f - t * 0.7f) * 3.0f);
        }
        surf.lineTo (w, 0); surf.closeSubPath();
        g.setColour (juce::Colour (0xffbff4ff).withAlpha (0.25f + 0.25f * lt));
        g.fillPath (surf);
    }
    // god rays (Shimmer), swaying
    const float rayA = 0.06f + 0.16f * pres[lyShimmer] * (0.6f + glow[lyShimmer]) + 0.06f * lt;
    for (int k = 0; k < 9; ++k)
    {
        const float x0 = w * (0.05f + 0.11f * (float) k) + std::sin (t * 0.13f + (float) k) * 30.0f;
        const float spread = w * (0.04f + 0.03f * hashF ((uint32_t) k, 2001)), lean = w * 0.18f;
        juce::Path ray;
        ray.addQuadrilateral (x0 - spread * 0.3f, 0, x0 + spread * 0.3f, 0, x0 + lean + spread, h * 0.95f, x0 + lean - spread, h * 0.95f);
        const float a = rayA * (0.5f + 0.5f * std::sin (t * (0.3f + 0.05f * (float) k) + (float) k * 1.7f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8fbff).withAlpha (a), x0, 0, juce::Colour (0xffd8fbff).withAlpha (0.0f), x0 + lean, h * 0.95f, false));
        g.fillPath (ray);
    }
    // caustics near the top
    for (int i = 0; i < 90; ++i)
    {
        const float x = std::fmod (w * hashF ((uint32_t) i, 2101) + std::sin (t * 0.6f + (float) i) * 18.0f, w);
        const float y = h * (0.05f + 0.25f * hashF ((uint32_t) i, 2102)) + std::cos (t * 0.5f + (float) i * 0.7f) * 6.0f;
        const float a = (0.05f + 0.12f * lt) * (0.5f + 0.5f * std::sin (t * 2.0f + (float) i));
        g.setColour (juce::Colour (0xffcffaff).withAlpha (a));
        g.drawLine (x, y, x + 10.0f + 8.0f * hashF ((uint32_t) i, 2103), y + 2.0f * std::sin ((float) i), 1.0f);
    }
    // a whale crossing far away, once every ~90 s (humpback silhouette, slow tail beat, lost in the blue haze)
    {
        const float period = 90.0f, slot = std::floor (t / period), ph = (t - slot * period) / 45.0f;
        if (ph < 1.0f && hashF ((uint32_t) f.seed, (uint32_t) slot) < 0.8f)
        {
            const bool ltr = hashF ((uint32_t) slot, 7) < 0.5f;
            const float x = ltr ? -w * 0.35f + ph * w * 1.7f : w * 1.35f - ph * w * 1.7f, y = h * (0.32f + 0.12f * hashF ((uint32_t) slot, 9));
            const float fade = juce::jmin (1.0f, juce::jmin (ph, 1.0f - ph) * 6.0f);
            paintWhale (g, x, y, w * 0.34f, ltr ? 1.0f : -1.0f, t, mid.interpolatedWith (deep, 0.55f).withAlpha (0.55f * fade));
        }
    }
    // school of fish (Wind)
    {
        const float cx = std::fmod (mt * 25.0f + w * 0.2f, w * 1.4f) - w * 0.2f, cy = h * 0.45f + std::sin (t * 0.3f) * h * 0.08f;
        g.setColour (juce::Colour (0xffd8f6ff).withAlpha (0.35f + 0.5f * pres[lyWind]));
        for (int i = 0; i < 26; ++i)
        {
            const float fx = cx + (hashF ((uint32_t) i, 2201) - 0.5f) * 140.0f + std::sin (t * 1.5f + (float) i) * 6.0f;
            const float fy = cy + (hashF ((uint32_t) i, 2202) - 0.5f) * 60.0f + std::cos (t * 1.2f + (float) i) * 4.0f;
            juce::Path fish;
            fish.addTriangle (fx - 6, fy - 2, fx - 6, fy + 2, fx - 2, fy);
            fish.addEllipse (fx - 3, fy - 2, 8, 4);
            g.fillPath (fish);
        }
    }
    // jellyfish (Drone pulses them, Pad tints them): the bell contracts, the tentacles trail behind
    for (int k = 0; k < 4; ++k)
    {
        const float sp = 6.0f + 4.0f * hashF ((uint32_t) f.seed, (uint32_t) k + 2300);
        const float x = w * (0.12f + 0.25f * (float) k) + std::sin (t * 0.12f + (float) k * 2.0f) * 30.0f;
        const float y = h * 1.1f - std::fmod (mt * sp + h * hashF ((uint32_t) f.seed, (uint32_t) k + 2310), h * 1.3f);
        const float beat = std::sin (t * (1.1f + 0.25f * (float) k) + (float) k * 1.7f);
        const float pulse = 0.5f + 0.5f * beat;
        const float r = (22.0f + 16.0f * hashF ((uint32_t) k, 2320)) * (h / 400.0f) * (0.8f + 0.4f * pres[lyDrone]);
        const float bw = r * (1.0f - 0.12f * pulse), bh = r * (0.72f + 0.12f * pulse);   // contracting bell
        const auto jc = (k % 3 == 0 ? juce::Colour (0xffff7ad9) : k % 3 == 1 ? juce::Colour (0xff7ff0e0) : juce::Colour (0xffb9a2ff))
                            .withMultipliedBrightness (0.85f + 0.35f * glow[lyDrone]);
        softBlob (g, x, y - bh * 0.3f, r * 2.6f, r * 2.0f, jc.withAlpha (0.10f + 0.12f * pres[lyDrone]));
        // tentacles: long, thin, tapered, swaying with a delay along their length
        for (int tn = 0; tn < 9; ++tn)
        {
            const float u0 = ((float) tn / 8.0f - 0.5f) * 1.7f;
            const float len = r * (2.4f + 1.4f * hashF ((uint32_t) (k * 31 + tn), 2330));
            juce::Path ten;
            ten.startNewSubPath (x + u0 * bw, y);
            for (int sgm = 1; sgm <= 12; ++sgm)
            {
                const float v = (float) sgm / 12.0f;
                ten.lineTo (x + u0 * bw * (1.0f - 0.3f * v) + std::sin (t * 1.4f - v * 3.5f + (float) tn) * r * 0.18f * v, y + v * len);
            }
            g.setGradientFill (juce::ColourGradient (jc.withAlpha (0.45f), x, y, jc.withAlpha (0.0f), x, y + len, false));
            g.strokePath (ten, juce::PathStrokeType (juce::jmax (0.7f, r * 0.03f)));
        }
        // oral arms: a few wider, frilled ribbons in the centre
        for (int arm = 0; arm < 3; ++arm)
        {
            juce::Path ra;
            const float ax = x + ((float) arm - 1.0f) * bw * 0.22f;
            ra.startNewSubPath (ax, y);
            for (int sgm = 1; sgm <= 10; ++sgm)
            {
                const float v = (float) sgm / 10.0f;
                ra.lineTo (ax + std::sin (t * 1.1f - v * 4.0f + (float) arm * 2.0f) * r * 0.16f, y + v * r * 1.5f);
            }
            g.setColour (jc.withAlpha (0.28f));
            g.strokePath (ra, juce::PathStrokeType (r * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // bell: dome with a scalloped rim
        juce::Path bell;
        bell.startNewSubPath (x - bw, y);
        bell.cubicTo (x - bw, y - bh * 1.15f, x + bw, y - bh * 1.15f, x + bw, y);
        for (int sc = 8; sc > 0; --sc)
        {
            const float xa = x - bw + 2.0f * bw * (float) sc / 8.0f, xb = x - bw + 2.0f * bw * (float) (sc - 1) / 8.0f;
            bell.quadraticTo ((xa + xb) * 0.5f, y + r * 0.08f, xb, y);
        }
        bell.closeSubPath();
        g.setGradientFill (juce::ColourGradient (jc.brighter (0.4f).withAlpha (0.75f), x, y - bh, jc.withAlpha (0.25f), x, y + r * 0.1f, false));
        g.fillPath (bell);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (bell, juce::PathStrokeType (0.8f));
        // inner glow: four gonads, brighter on the beat
        for (int gn = 0; gn < 4; ++gn)
        {
            const float gx = x + ((float) gn - 1.5f) * bw * 0.28f, gy = y - bh * 0.45f + std::abs ((float) gn - 1.5f) * bh * 0.08f;
            softBlob (g, gx, gy, r * 0.18f, r * 0.13f, juce::Colours::white.withAlpha (0.25f + 0.35f * pulse * (0.5f + glow[lyDrone])));
        }
        // bioluminescent dots on the rim
        for (int d = 0; d <= 8; ++d)
        {
            const float dx = x - bw + 2.0f * bw * (float) d / 8.0f;
            const float blink = 0.4f + 0.6f * std::sin (t * 3.0f + (float) d * 0.8f + (float) k);
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.5f * blink * pres[lyDrone]));
            g.fillEllipse (dx - 1.2f, y - 1.2f, 2.4f, 2.4f);
        }
    }
    // plankton (Texture)
    const float pa = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.5f) + 0.1f;
    for (int i = 0; i < 110; ++i)
    {
        const float x = std::fmod (w * hashF ((uint32_t) i, 2401) + mt * 3.0f + std::sin (t * 0.3f + (float) i) * 10.0f, w);
        const float y = std::fmod (h * hashF ((uint32_t) i, 2402) - mt * 2.0f + h * 10.0f, h);
        g.setColour (juce::Colour (0xff9ff5e0).withAlpha (juce::jlimit (0.0f, 0.9f, pa * (0.4f + 0.6f * std::sin (t * 1.8f + (float) i)))));
        g.fillEllipse (x, y, 1.6f, 1.6f);
    }
    // seabed with dunes, coral and swaying kelp (Pad = kelp height)
    {
        juce::Path bed;
        bed.startNewSubPath (0, h);
        for (int i = 0; i <= 80; ++i)
        {
            const float x = w * (float) i / 80.0f;
            bed.lineTo (x, h * 0.88f - std::sin (x * 0.008f + (float) (f.seed % 40)) * h * 0.04f - std::sin (x * 0.023f) * h * 0.02f);
        }
        bed.lineTo (w, h); bed.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c3a4a), 0, h * 0.84f, juce::Colour (0xff02101a), 0, h, false));
        g.fillPath (bed);
        softBlob (g, w * 0.5f, h * 0.92f, w * 0.6f, h * 0.08f, juce::Colour (0xff4fe3c1).withAlpha (0.08f + 0.12f * glow[lyTexture]));
        for (int k = 0; k < 16; ++k)
        {
            const float x = w * hashF ((uint32_t) f.seed, (uint32_t) k + 2500);
            const float hh = h * (0.2f + 0.3f * hashF ((uint32_t) f.seed, (uint32_t) k + 2510)) * (0.6f + 0.6f * pres[lyPad]);
            juce::Path kelp;
            kelp.startNewSubPath (x, h * 0.9f);
            for (int s = 1; s <= 10; ++s)
            {
                const float u = (float) s / 10.0f;
                kelp.lineTo (x + std::sin (t * 0.6f + (float) k + u * 3.0f) * 14.0f * u, h * 0.9f - hh * u);
            }
            g.setColour (juce::Colour (0xff1f7a55).interpolatedWith (col::layer[lyPad], 0.2f).withAlpha (0.9f));
            g.strokePath (kelp, juce::PathStrokeType ((4.0f + 3.0f * hashF ((uint32_t) k, 2520)) * (h / 400.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xff6fe0a8).withAlpha (0.25f));
            g.strokePath (kelp, juce::PathStrokeType (1.2f));
        }
        for (int k = 0; k < 7; ++k) // coral fans glowing softly with the drone
        {
            const float x = w * hashF ((uint32_t) f.seed, (uint32_t) k + 2600), y = h * 0.9f;
            const auto cc = (k % 2 ? juce::Colour (0xffff7a9a) : juce::Colour (0xffffb26b)).withAlpha (0.35f + 0.3f * glow[lyDrone]);
            g.setColour (cc);
            for (int br = 0; br < 6; ++br)
            {
                const float a = -kPi * 0.5f + ((float) br - 2.5f) * 0.28f;
                g.drawLine (x, y, x + std::cos (a) * 26.0f, y + std::sin (a) * 26.0f, 2.0f);
            }
        }
    }
    // ambient bubble streams
    for (int i = 0; i < 24; ++i)
    {
        const float x = w * hashF ((uint32_t) f.seed, (uint32_t) i + 2700) + std::sin (t * 2.0f + (float) i) * 3.0f;
        const float y = h - std::fmod (t * (30.0f + 20.0f * hashF ((uint32_t) i, 2701)) + h * hashF ((uint32_t) i, 2702), h);
        const float s = 2.0f + 3.0f * hashF ((uint32_t) i, 2703);
        g.setColour (juce::Colour (0xffd8fbff).withAlpha (0.35f));
        g.drawEllipse (x, y, s, s, 0.8f);
    }
}

// ---- Neon Rain: smoggy sky glow, arcology, three skyline layers, neon signs, searchlights, flying cars, rain, fog
void Landscape::paintNeon (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), t = (float) f.time, lt = f.light;
    // smog: dark navy above, orange / magenta glow of the city below
    const auto glowC = juce::Colour (0xffc2552f).interpolatedWith (juce::Colour (0xff8a2b7a), 0.4f + 0.3f * std::sin (t * 0.05f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff04060f), 0, 0, glowC.withAlpha (1.0f).darker (1.0f - 0.6f * lt), 0, h * 0.8f, false));
    g.fillRect (b);
    // searchlights (Shimmer)
    const float sa = pres[lyShimmer] * (0.25f + glow[lyShimmer] * 1.2f);
    for (int k = 0; k < 3 && sa > 0.01f; ++k)
    {
        const float bx = w * (0.2f + 0.3f * (float) k), ang = std::sin (t * (0.11f + 0.04f * (float) k) + (float) k * 2.0f) * 0.5f;
        juce::Path beam;
        beam.addTriangle (bx, h * 0.85f, bx + std::sin (ang - 0.05f) * h * 1.4f, h * 0.85f - std::cos (ang - 0.05f) * h * 1.4f,
                          bx + std::sin (ang + 0.05f) * h * 1.4f, h * 0.85f - std::cos (ang + 0.05f) * h * 1.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8f4ff).withAlpha (0.16f * sa), bx, h * 0.85f, juce::Colour (0xffd8f4ff).withAlpha (0.0f), bx, 0, false));
        g.fillPath (beam);
    }
    // arcology pyramid in the distance (Drone), with tiny lights and a burning flare
    {
        const float ax = arcology.x, base = h * 0.8f, ah = h * 0.42f * (0.85f + 0.15f * pres[lyDrone]);
        juce::Path pyr;
        pyr.addTriangle (ax - ah * 0.9f, base, ax + ah * 0.9f, base, ax, base - ah);
        g.setColour (juce::Colour (0xff0d1020).interpolatedWith (glowC, 0.12f));
        g.fillPath (pyr);
        for (int i = 0; i < 90; ++i)
        {
            const float u = hashF ((uint32_t) i, 901), v = hashF ((uint32_t) i, 902);
            const float y = base - ah * u, half = ah * 0.9f * (1.0f - u);
            g.setColour (juce::Colour (0xffffd59a).withAlpha (0.3f + 0.4f * glow[lyDrone] * (0.5f + 0.5f * std::sin (t + (float) i))));
            g.fillRect (ax - half + 2.0f * half * v, y, 1.5f, 1.2f);
        }
        const float fl = 0.5f + 0.5f * std::sin (t * 3.1f) * std::sin (t * 1.7f);
        softBlob (g, ax - ah * 0.55f, base - ah * 0.35f, 14.0f + 10.0f * fl, 10.0f + 6.0f * fl, juce::Colour (0xffffa040).withAlpha (0.5f * fl * (0.4f + pres[lyDrone])));
    }
    // skyline layers, neon signs between them
    for (int layer = 0; layer < 3; ++layer)
    {
        if (skyline[(size_t) layer].isValid())
        {
            const float drift = std::fmod (f.musTime * (1.0f + 2.0f * (float) layer), (float) getWidth());
            g.setOpacity (layer == 0 ? 0.85f : 1.0f);
            g.drawImageAt (skyline[(size_t) layer], (int) -drift, 0);
            g.drawImageAt (skyline[(size_t) layer], (int) (w - drift), 0);
            g.setOpacity (1.0f);
        }
        for (auto& s : signs)
        {
            if (s.layer != layer) continue;
            const float drift = std::fmod (f.musTime * (1.0f + 2.0f * (float) layer), (float) getWidth());
            const float flick = std::sin (t * 9.0f + s.phase) > 0.97f ? 0.3f : 1.0f;
            const float lvl = flick * (0.55f + 0.45f * pres[lyPad]) * (0.7f + 0.3f * glow[lyPad]);
            for (float off : { -drift, w - drift })
            {
                auto r = s.r.translated (off, 0);
                softBlob (g, r.getCentreX(), r.getCentreY(), r.getWidth() * 1.6f + 10.0f, r.getHeight() * 1.6f + 10.0f, s.c.withAlpha (0.18f * lvl));
                g.setColour (s.c.withAlpha (0.9f * lvl));
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.5f * lvl));
                g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.3f, r.getHeight() * 0.3f), 1.0f);
            }
        }
    }
    // a few flying cars always cruise (Density), plus the ones launched by bells (sparks kind 2)
    for (int i = 0; i < (int) (2 + 4 * f.density); ++i)
    {
        const float sp = 30.0f + 40.0f * hashF ((uint32_t) i, 1001), dir = i % 2 ? 1.0f : -1.0f;
        const float x = dir > 0 ? std::fmod (t * sp + w * hashF ((uint32_t) i, 1002), w + 80.0f) - 40.0f : w - std::fmod (t * sp + w * hashF ((uint32_t) i, 1002), w + 80.0f) + 40.0f;
        const float y = h * (0.2f + 0.35f * hashF ((uint32_t) i, 1003)) + std::sin (t * 0.5f + (float) i) * 4.0f;
        g.setColour (juce::Colour (0xffff5a5a).withAlpha (0.8f));
        g.fillEllipse (x, y, 2.5f, 2.5f);
        g.setColour (juce::Colour (0xffd8f4ff).withAlpha (0.8f));
        g.fillEllipse (x + 5.0f * dir, y, 2.0f, 2.0f);
    }
    // fog (Wind)
    if (pres[lyWind] > 0.02f)
        for (int i = 0; i < 6; ++i)
        {
            const float mx = std::fmod ((float) i * w * 0.27f + f.musTime * (8.0f + 4.0f * (float) i), w * 1.6f) - w * 0.3f;
            softBlob (g, mx, h * (0.78f + 0.03f * (float) (i % 3)), w * 0.25f, h * 0.06f, glowC.brighter (0.4f).withAlpha (0.10f * pres[lyWind] * (0.6f + glow[lyWind])));
        }
    // rain
    {
        const float amount = juce::jlimit (0.0f, 1.0f, 0.3f + 0.6f * pres[lyWind]);
        const int n = (int) (amount * 380.0f);
        g.setColour (juce::Colour (0xffbcd6ff).withAlpha (0.18f + 0.15f * amount));
        for (int i = 0; i < n; ++i)
        {
            const float sp = 0.9f + 0.5f * hashF ((uint32_t) i, 1101);
            const float x = std::fmod (w * hashF ((uint32_t) i, 1102) + t * 60.0f * sp, w + 40.0f) - 20.0f;
            const float y = std::fmod (h * hashF ((uint32_t) i, 1103) + t * 520.0f * sp, h + 30.0f) - 30.0f;
            g.drawLine (x, y, x - 3.5f, y + 14.0f * sp, 0.8f);
        }
    }
    // wet street: reflections of the signs, stretched and rippling
    {
        const float sy = h * 0.88f;
        g.setColour (juce::Colour (0xff05060c));
        g.fillRect (b.withTop (sy));
        for (auto& sgn : signs)
        {
            if (sgn.layer == 0) continue;
            const float drift = std::fmod (f.musTime * (1.0f + 2.0f * (float) sgn.layer), (float) getWidth());
            for (float off : { -drift, w - drift })
            {
                const float x = sgn.r.getX() + off;
                if (x < -40 || x > w + 40) continue;
                for (int k = 0; k < 6; ++k)
                {
                    const float y = sy + 4.0f + (float) k * 4.5f, wob = std::sin (t * 2.0f + (float) k + x * 0.05f) * 3.0f;
                    g.setColour (sgn.c.withAlpha (0.20f - 0.03f * (float) k));
                    g.fillRect (x + wob, y, sgn.r.getWidth(), 2.0f);
                }
            }
        }
    }
    // street-level glow
    g.setGradientFill (juce::ColourGradient (glowC.withAlpha (0.0f), 0, h * 0.75f, glowC.withAlpha (0.3f), 0, h, false));
    g.fillRect (b.withTop (h * 0.75f));
}

void Landscape::paintSparks (juce::Graphics& g)
{
    for (auto& s : sparks)
    {
        if (s.kind == 3) // bubble
        {
            g.setColour (s.c.withAlpha (0.7f * juce::jlimit (0.0f, 1.0f, s.life * 1.5f)));
            g.drawEllipse (s.x - s.size * 1.5f + std::sin (s.y * 0.05f) * 3.0f, s.y - s.size * 1.5f, s.size * 3.0f, s.size * 3.0f, 1.2f);
            continue;
        }
        if (s.kind == 2) // flying light with a trail
        {
            const float dir = s.vx > 0 ? -1.0f : 1.0f;
            g.setGradientFill (juce::ColourGradient (s.c.withAlpha (0.8f), s.x, s.y, s.c.withAlpha (0.0f), s.x + dir * 50.0f, s.y, false));
            g.drawLine (s.x, s.y, s.x + dir * 50.0f, s.y, 1.5f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (s.x - 2, s.y - 2, 4, 4);
            continue;
        }
        // in the Starfield, notes pass behind the planet
        if (f.scene == scStarfield && s.kind == 1 && planetPos.getDistanceFrom ({ s.x, s.y }) < planetR * 1.02f) continue;
        const float life = juce::jlimit (0.0f, 1.0f, s.life);
        const float k = noteStarIntensity();
        const float a = juce::jlimit (0.0f, 1.0f, (life * 1.4f) * (0.35f + 0.65f * pres[lyBells])) * k;
        if (a < 0.01f) continue;
        const float tw = 0.75f + 0.25f * std::sin ((float) f.time * 6.0f + s.size * 13.0f);   // gentle twinkle
        const float R = s.size * (1.0f + 0.6f * life) * (0.5f + 0.9f * k);
        if (s.kind == 1) // comet: tapered, fading tail
        {
            const float len = 70.0f + 30.0f * life;
            const float sp = std::sqrt (s.vx * s.vx + s.vy * s.vy) + 1.0e-3f;
            const float nx = -s.vx / sp, ny = -s.vy / sp, px = -ny, py = nx;
            juce::Path tail;
            tail.startNewSubPath (s.x + px * s.size, s.y + py * s.size);
            tail.lineTo (s.x + nx * len, s.y + ny * len);
            tail.lineTo (s.x - px * s.size, s.y - py * s.size);
            tail.closeSubPath();
            g.setGradientFill (juce::ColourGradient (s.c.withAlpha (0.55f * a), s.x, s.y, s.c.withAlpha (0.0f), s.x + nx * len, s.y + ny * len, false));
            g.fillPath (tail);
        }
        // soft halo
        g.setGradientFill (juce::ColourGradient (s.c.withAlpha (0.35f * a), s.x, s.y, s.c.withAlpha (0.0f), s.x + R * 4.0f, s.y, true));
        g.fillEllipse (s.x - R * 4.0f, s.y - R * 4.0f, R * 8.0f, R * 8.0f);
        // four tapered rays (long) + four short diagonal ones, slowly turning
        auto rays = [&] (float len, float width, float rot, float alpha)
        {
            juce::Path p;
            for (int k = 0; k < 4; ++k)
            {
                const float ang = rot + (float) k * kPi * 0.5f, cx = std::cos (ang), cy = std::sin (ang);
                p.startNewSubPath (s.x - cy * width, s.y + cx * width);
                p.lineTo (s.x + cx * len, s.y + cy * len);
                p.lineTo (s.x + cy * width, s.y - cx * width);
                p.closeSubPath();
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (alpha), s.x, s.y, s.c.withAlpha (0.0f), s.x + len, s.y, true));
            g.fillPath (p);
        };
        const float rot = s.size * 0.7f + (float) f.time * 0.15f;
        rays (R * 3.2f * tw, R * 0.22f, rot, 0.9f * a);
        rays (R * 1.5f * tw, R * 0.15f, rot + kPi * 0.25f, 0.55f * a);
        // bright core
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (a), s.x, s.y, s.c.withAlpha (0.0f), s.x + R * 0.9f, s.y, true));
        g.fillEllipse (s.x - R * 0.9f, s.y - R * 0.9f, R * 1.8f, R * 1.8f);
    }
}

void Landscape::paintOverlay (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float t = (float) f.time;
    auto r = b.reduced (16, 12);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (techFont (24.0f));
    g.drawText (chordName (f.rootPc, f.scale, f.degree, f.harmony), r.removeFromTop (30), juce::Justification::topLeft);
    g.setFont (font (12.5f, true));
    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.drawText (keyNames()[(int) f.rootPc % 12] + " " + scaleNames()[f.scale] + juce::String::fromUTF8 ("   ·   ") + L ("semilla ", "seed ") + juce::String (shownSeed),
                r.removeFromTop (18), juce::Justification::topLeft);
    auto mmss = [] (float s) { const int v = (int) s; return juce::String (v / 60).paddedLeft ('0', 2) + ":" + juce::String (v % 60).paddedLeft ('0', 2); };
    auto top = b.reduced (16, 12);
    g.setFont (techFont (18.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    juce::String clock = mmss (f.hostPlaying ? f.musTime : f.elapsed);
    if (f.timerMin > 0 && ! f.hostPlaying) clock = L ("quedan ", "") + mmss (juce::jmax (0.0f, (float) f.timerMin * 60.0f - f.elapsed)) + L ("", " left");
    g.drawText (clock, top.removeFromTop (26), juce::Justification::topRight);
    g.setFont (font (11.5f, true));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText (f.hostPlaying ? L ("SIGUE AL DAW", "FOLLOWING THE DAW") : (f.running ? L ("SONANDO", "PLAYING") : L ("PARADO", "STOPPED")), top.removeFromTop (16), juce::Justification::topRight);
    if (f.frozen)
    {
        g.setFont (techFont (16.0f));
        g.setColour (juce::Colour (0xffbfe6ff));
        g.drawText (L ("CONGELADO", "FROZEN"), b.reduced (12).removeFromTop (26), juce::Justification::centredTop);
    }
    if (! f.running && f.runGain <= 0.01f)
    {
        g.setFont (techFont (22.0f));
        g.setColour (juce::Colours::white.withAlpha (0.8f + 0.2f * std::sin (t * 2.0f)));
        g.drawText (L ("▶  PULSA PLAY", "▶  PRESS PLAY"), b, juce::Justification::centred);
    }
    auto leg = b.reduced (16, 12).removeFromBottom (16);
    const float lw = juce::jmin (66.0f, leg.getWidth() / kNumLayers);
    for (int l = 0; l < kNumLayers; ++l)
    {
        auto cell = leg.removeFromLeft (lw).reduced (3, 0);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (col::layer[l].withAlpha (0.85f));
        g.fillRoundedRectangle (cell.withWidth (cell.getWidth() * pres[(size_t) l]), 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (font (10.0f, true));
        g.drawText (layerName (l), cell, juce::Justification::centred);
    }
}

// ---- Ocean: sky, moon/sun with its glittering path on the water, perspective waves, distant islands, lighthouse
void Landscape::paintOcean (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), lt = f.light, t = (float) f.time, mt = f.musTime;
    const float horizon = h * 0.52f;
    const auto skyTop = mix3 (lt, juce::Colour (0xff02040d), juce::Colour (0xff20194a), juce::Colour (0xff3a7cc4));
    const auto skyLow = mix3 (lt, juce::Colour (0xff14204a), juce::Colour (0xffe58a63), juce::Colour (0xfff5e1b8));
    g.setGradientFill (juce::ColourGradient (skyTop, 0, 0, skyLow, 0, horizon, false));
    g.fillRect (b.withHeight (horizon + 1));
    const float starA = juce::jlimit (0.0f, 1.0f, (0.62f - lt) * 2.2f);
    for (size_t i = 0; i < stars.size() && starA > 0.01f; ++i)
    {
        const float tw = 0.55f + 0.45f * std::sin (t * twinkle[i] + (float) i);
        g.setColour (juce::Colours::white.withAlpha (starA * tw * 0.7f));
        g.fillEllipse (stars[i].x * w, stars[i].y * horizon * 0.9f, starSize[i], starSize[i]);
    }
    // sun / moon
    const float ph = wrap01 (f.evoTime / 420.0f + (float) (f.seed % 97) * 0.0137f);
    const float sx = w * (0.15f + 0.7f * ph), sy = horizon - std::sin (ph * kPi) * h * 0.38f - 10.0f;
    const bool sun = lt > 0.5f;
    const auto sc = sun ? juce::Colour (0xffffd27a) : juce::Colour (0xffe6ecff);
    {
        const float rad = 16.0f + 8.0f * lt, gl = 110.0f + 120.0f * f.energy;
        flarePos = { sx, sy }; flareCol = sc; flareAmt = 0.7f + 0.4f * f.energy;
        softBlob (g, sx, sy, gl, gl, sc.withAlpha (0.35f));
        g.setColour (sc);
        g.fillEllipse (sx - rad, sy - rad, rad * 2, rad * 2);
    }
    // volumetric clouds: lit tops, shaded bellies; rays of light between them when the sun is low
    {
        const auto lit = mix3 (lt, juce::Colour (0xff3a4270), juce::Colour (0xffffb08a), juce::Colours::white);
        const auto shade = mix3 (lt, juce::Colour (0xff141a34), juce::Colour (0xff7a4a6a), juce::Colour (0xffa9b8cc));
        for (int i = 0; i < numClouds + 2; ++i)
        {
            const float cx = std::fmod (w * hashF ((uint32_t) f.seed, (uint32_t) i + 50) + mt * (3.0f + (float) (i % 3)), w * 1.4f) - w * 0.2f;
            const float cy = horizon * (0.18f + 0.5f * hashF ((uint32_t) f.seed, (uint32_t) i + 60));
            const float cw = w * (0.07f + 0.07f * hashF ((uint32_t) f.seed, (uint32_t) i + 70));
            for (int k = 0; k < 6; ++k)
            {
                const float bx = cx + ((float) k - 2.5f) * cw * 0.32f, by = cy - std::sin ((float) k / 5.0f * kPi) * cw * 0.18f;
                softBlob (g, bx, by + cw * 0.05f, cw * 0.38f, cw * 0.22f, shade.withAlpha (0.35f));
                softBlob (g, bx, by - cw * 0.04f, cw * 0.32f, cw * 0.18f, lit.withAlpha (0.28f + 0.2f * lt));
            }
        }
        if (lt > 0.35f)
            for (int k = 0; k < 6; ++k)
            {
                const float ang = -0.6f + 0.24f * (float) k + std::sin (t * 0.05f + (float) k) * 0.03f;
                juce::Path ray;
                ray.addTriangle (sx, sy, sx + std::sin (ang - 0.04f) * h * 1.2f, sy + std::cos (ang - 0.04f) * h * 1.2f, sx + std::sin (ang + 0.04f) * h * 1.2f, sy + std::cos (ang + 0.04f) * h * 1.2f);
                g.setGradientFill (juce::ColourGradient (sc.withAlpha (0.10f * (lt - 0.35f)), sx, sy, sc.withAlpha (0.0f), sx, horizon, false));
                g.fillPath (ray);
            }
    }
    // aurora-ish shimmer glow low on the horizon
    softBlob (g, w * 0.5f, horizon, w * 0.6f, h * 0.12f * (0.4f + pres[lyShimmer]), col::accent.withAlpha (0.12f * pres[lyShimmer] * (0.5f + glow[lyShimmer])));
    // distant islands (Drone)
    {
        juce::Path isl;
        isl.startNewSubPath (0, horizon);
        for (int i = 0; i <= 120; ++i)
        {
            const float x = w * (float) i / 120.0f;
            const float n = std::sin (x * 0.011f + (float) (f.seed % 50)) * std::sin (x * 0.0047f + 1.3f);
            isl.lineTo (x, horizon - juce::jmax (0.0f, n) * h * 0.08f * (0.5f + 0.5f * pres[lyDrone]));
        }
        isl.lineTo (w, horizon); isl.closeSubPath();
        g.setColour (skyTop.interpolatedWith (juce::Colours::black, 0.4f).withAlpha (0.9f));
        g.fillPath (isl);
    }
    // sea
    const auto seaTop = skyLow.interpolatedWith (juce::Colour (0xff0a1a33), 0.55f), seaBot = juce::Colour (0xff020610);
    g.setGradientFill (juce::ColourGradient (seaTop, 0, horizon, seaBot, 0, h, false));
    g.fillRect (b.withTop (horizon));
    // glittering path of the sun / moon
    for (int i = 0; i < 70; ++i)
    {
        const float u = (float) i / 70.0f, y = horizon + 2.0f + (h - horizon) * u * u;
        const float spread = 8.0f + 140.0f * u;
        const float x = sx + (hashF ((uint32_t) i, (uint32_t) (t * 4.0f)) - 0.5f) * spread * 2.0f;
        g.setColour (sc.withAlpha ((0.15f + 0.5f * (1.0f - u)) * (0.5f + 0.5f * lt)));
        g.fillRect (x, y, 4.0f + 30.0f * u * hashF ((uint32_t) i, 3), 1.0f + u * 1.5f);
    }
    // waves in perspective (Pad = swell height, Wind = choppiness)
    const float swell = 0.4f + 0.6f * pres[lyPad] + 0.4f * glow[lyWind];
    for (int r = 0; r < 18; ++r)
    {
        const float u = (float) (r + 1) / 18.0f, y0 = horizon + (h - horizon) * u * u;
        const float amp = (1.0f + 9.0f * u * u) * swell, k = 0.02f / (0.3f + u);
        juce::Path wave;
        for (int i = 0; i <= 80; ++i)
        {
            const float x = w * (float) i / 80.0f;
            const float y = y0 + amp * std::sin (x * k + t * (0.6f + 0.5f * u) + (float) r * 1.7f) + amp * 0.4f * std::sin (x * k * 2.3f - t * 0.9f);
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (skyLow.brighter (0.4f).withAlpha (0.05f + 0.16f * u * (0.5f + lt)));
        g.strokePath (wave, juce::PathStrokeType (0.8f + 1.4f * u));
    }
    // foam sparkles (Texture)
    const float fa = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.5f);
    for (int i = 0; i < 60 && fa > 0.01f; ++i)
    {
        const float u = hashF ((uint32_t) i, 1201), y = horizon + (h - horizon) * u * u;
        const float x = std::fmod (w * hashF ((uint32_t) i, 1202) + mt * 6.0f, w);
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 0.8f, fa * (0.5f + 0.5f * std::sin (t * 3.0f + (float) i)))));
        g.fillRect (x, y, 2.0f + 4.0f * u, 1.0f);
    }
    // the shore: wet sand and a foam line that comes and goes with the swell
    {
        const float sy = h * 0.9f + std::sin (t * 0.45f) * h * 0.025f * swell;
        juce::Path sand;
        sand.startNewSubPath (0, h);
        for (int i = 0; i <= 60; ++i) { const float x = w * (float) i / 60.0f; sand.lineTo (x, sy + std::sin (x * 0.013f + (float) (f.seed % 30)) * 6.0f); }
        sand.lineTo (w, h); sand.closeSubPath();
        g.setGradientFill (juce::ColourGradient (skyLow.interpolatedWith (juce::Colour (0xff2a2016), 0.7f), 0, sy, juce::Colour (0xff0a0806), 0, h, false));
        g.fillPath (sand);
        juce::Path foam;
        for (int i = 0; i <= 60; ++i) { const float x = w * (float) i / 60.0f, y = sy + std::sin (x * 0.013f + (float) (f.seed % 30)) * 6.0f - 1.0f + std::sin (x * 0.07f + t * 1.3f) * 2.0f;
                                         if (i == 0) foam.startNewSubPath (x, y); else foam.lineTo (x, y); }
        g.setColour (juce::Colours::white.withAlpha (0.35f + 0.25f * lt));
        g.strokePath (foam, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ---- Aurora: arctic night, curtains of light with vertical rays, snowy peaks, frozen lake reflection, snowfall
void Landscape::paintAurora (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), t = (float) f.time, lt = f.light, mt = f.musTime;
    const float horizon = h * 0.66f;
    const auto skyTop = juce::Colour (0xff01030a).interpolatedWith (juce::Colour (0xff0a1a3a), lt * 0.6f);
    const auto skyLow = juce::Colour (0xff0b1838).interpolatedWith (juce::Colour (0xff3a5a8a), lt * 0.6f);
    g.setGradientFill (juce::ColourGradient (skyTop, 0, 0, skyLow, 0, horizon, false));
    g.fillRect (b);
    for (size_t i = 0; i < stars.size(); ++i)
    {
        const float tw = 0.55f + 0.45f * std::sin (t * twinkle[i] + (float) i);
        g.setColour (juce::Colours::white.withAlpha ((0.75f - 0.4f * lt) * tw));
        g.fillEllipse (stars[i].x * w, stars[i].y * horizon, starSize[i], starSize[i]);
    }
    // crescent moon
    {
        const float mx = w * (0.15f + 0.7f * hashF ((uint32_t) f.seed, 3100)), my = h * 0.16f, mr = 16.0f;
        softBlob (g, mx, my, 70.0f, 70.0f, juce::Colour (0xffdfe8ff).withAlpha (0.15f));
        g.setColour (juce::Colour (0xffeef3ff));
        g.fillEllipse (mx - mr, my - mr, mr * 2, mr * 2);
        g.setColour (skyTop);
        g.fillEllipse (mx - mr * 0.5f, my - mr * 1.15f, mr * 1.9f, mr * 1.9f);
    }
    // curtains (Shimmer + Pad): a glowing band and many vertical rays rising from a waving base line
    const float power = juce::jlimit (0.3f, 1.3f, 0.4f + 0.6f * pres[lyShimmer] + 0.3f * pres[lyPad] + 0.5f * glow[lyShimmer]);
    for (int k = 0; k < 3; ++k)
    {
        const float baseY = h * (0.36f + 0.08f * (float) k), height = h * (0.22f + 0.08f * (float) (2 - k));
        const auto c1 = k == 1 ? juce::Colour (0xff6dffb0) : (k == 0 ? juce::Colour (0xff4fe3c1) : juce::Colour (0xff9be15d));
        const auto c2 = k == 2 ? juce::Colour (0xffff5fb4) : col::accent3;
        auto baseAt = [&] (float x) { return baseY + std::sin (x * 0.004f + t * 0.11f + (float) k * 2.0f) * h * 0.07f + std::sin (x * 0.011f - t * 0.07f + (float) k) * h * 0.03f; };
        juce::Path band;
        for (int i = 0; i <= 80; ++i) { const float x = w * (float) i / 80.0f; if (i == 0) band.startNewSubPath (x, baseAt (x)); else band.lineTo (x, baseAt (x)); }
        for (int i = 80; i >= 0; --i) { const float x = w * (float) i / 80.0f; band.lineTo (x, baseAt (x) - height); }
        band.closeSubPath();
        juce::ColourGradient gr (c1.withAlpha (0.0f), 0, baseY + h * 0.03f, c2.withAlpha (0.0f), 0, baseY - height, false);
        gr.addColour (0.12, c1.withAlpha (0.28f * power));
        gr.addColour (0.55, c1.interpolatedWith (c2, 0.5f).withAlpha (0.12f * power));
        g.setGradientFill (gr);
        g.fillPath (band);
        for (int i = 0; i < 140; ++i)
        {
            const float x = w * hashF ((uint32_t) (k * 1000 + i), 1301);
            const float flick = 0.5f + 0.5f * std::sin (t * (0.6f + hashF ((uint32_t) i, 1302) * 1.5f) + x * 0.02f);
            const float y0 = baseAt (x), len = height * (0.4f + 0.6f * hashF ((uint32_t) i, 1303)) * flick;
            g.setGradientFill (juce::ColourGradient (c1.withAlpha (0.35f * power * flick), x, y0, c2.withAlpha (0.0f), x, y0 - len, false));
            g.fillRect (x, y0 - len, 1.6f, len);
        }
    }
    // snowy peaks (Drone far, Pad near)
    auto peaks = [&] (int k, float base, float amp, float sp)
    {
        juce::Path p;
        p.startNewSubPath (0, h);
        const float s0 = (float) (f.seed % 1000) * 0.21f + (float) k;
        for (int i = 0; i <= 60; ++i)
        {
            const float x = w * (float) i / 60.0f, xs = x + mt * sp;
            const float n = std::abs (std::sin (xs * 0.006f + s0)) * 0.7f + 0.3f * std::abs (std::sin (xs * 0.017f + s0 * 2.0f));
            p.lineTo (x, base - amp * n);
        }
        p.lineTo (w, h); p.closeSubPath();
        return p;
    };
    const float farAmp = h * 0.22f * (0.5f + 0.5f * pres[lyDrone]), nearAmp = h * 0.14f * (0.5f + 0.5f * pres[lyPad]);
    auto far = peaks (0, horizon, farAmp, 0.4f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdfe9ff).withAlpha (0.85f), 0, horizon - farAmp, juce::Colour (0xff1a2a4a), 0, horizon, false));
    g.fillPath (far);
    auto nearP = peaks (1, horizon + h * 0.08f, nearAmp, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb8c8e8), 0, horizon - nearAmp, juce::Colour (0xff0b1426), 0, horizon + h * 0.1f, false));
    g.fillPath (nearP);
    // frozen lake with a faint reflection of the aurora
    const float ly = horizon + h * 0.12f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10284a).interpolatedWith (juce::Colour (0xff4fe3c1), 0.18f * power), 0, ly, juce::Colour (0xff050a14), 0, h, false));
    g.fillRect (b.withTop (ly));
    for (int i = 0; i < 18; ++i)
    {
        const float y = ly + 3.0f + (float) i * (h - ly) / 18.0f;
        g.setColour (juce::Colour (0xff6dffb0).withAlpha (0.05f * power * (1.0f - (float) i / 18.0f)));
        g.fillRect (w * hashF ((uint32_t) i, 1401), y, w * 0.3f, 1.0f);
    }
    // snowfall (Wind) and floating ice crystals (Texture)
    const int flakes = (int) (40 + 160 * pres[lyWind]);
    for (int i = 0; i < flakes; ++i)
    {
        const float sp = 0.5f + hashF ((uint32_t) i, 1501);
        const float x = std::fmod (w * hashF ((uint32_t) i, 1502) + std::sin (t * 0.5f + (float) i) * 12.0f + t * 8.0f * sp, w);
        const float y = std::fmod (h * hashF ((uint32_t) i, 1503) + t * 22.0f * sp, h);
        g.setColour (juce::Colours::white.withAlpha (0.35f + 0.4f * sp * 0.5f));
        const float s = 1.2f + 1.6f * sp;
        g.fillEllipse (x, y, s, s);
    }
    const float ia = pres[lyTexture] * (0.3f + glow[lyTexture] * 1.5f);
    for (int i = 0; i < 40 && ia > 0.01f; ++i)
    {
        const float x = w * hashF ((uint32_t) i, 1601), y = horizon * (0.3f + 0.7f * hashF ((uint32_t) i, 1602));
        g.setColour (juce::Colour (0xffcff8ff).withAlpha (juce::jlimit (0.0f, 0.9f, ia * (0.5f + 0.5f * std::sin (t * 2.5f + (float) i)))));
        g.fillRect (x, y, 1.5f, 1.5f);
    }
}

// ---- humpback whale silhouette, facing 'dir' (+1 right), length L
void Landscape::paintWhale (juce::Graphics& g, float x, float y, float L, float dir, float t, juce::Colour body)
{
    const float sway = std::sin (t * 0.55f) * 0.035f;
    auto P = [&] (float u, float v) { return juce::Point<float> (x + dir * u * L, y + v * L); };
    juce::Path p;
    p.startNewSubPath (P (0.50f, 0.01f));
    p.cubicTo (P (0.48f, -0.07f), P (0.33f, -0.115f), P (0.12f, -0.12f));       // head and back
    p.cubicTo (P (-0.10f, -0.125f), P (-0.28f, -0.075f), P (-0.40f, -0.025f + sway * 0.3f)); // back to the tail stock
    p.cubicTo (P (-0.45f, -0.06f + sway), P (-0.52f, -0.11f + sway), P (-0.56f, -0.09f + sway)); // upper fluke
    p.quadraticTo (P (-0.50f, -0.035f + sway), P (-0.47f, -0.005f + sway));
    p.quadraticTo (P (-0.50f, 0.03f + sway), P (-0.56f, 0.07f + sway));             // lower fluke
    p.cubicTo (P (-0.51f, 0.075f + sway), P (-0.45f, 0.035f + sway), P (-0.40f, 0.012f + sway * 0.3f));
    p.cubicTo (P (-0.25f, 0.05f), P (-0.02f, 0.11f), P (0.20f, 0.10f));            // belly
    p.cubicTo (P (0.36f, 0.095f), P (0.47f, 0.06f), P (0.50f, 0.01f));
    p.closeSubPath();
    // long pectoral fin (humpback signature), slowly flapping
    const float flap = std::sin (t * 0.4f) * 0.04f;
    juce::Path fin;
    fin.startNewSubPath (P (0.24f, 0.075f));
    fin.cubicTo (P (0.18f, 0.16f + flap), P (0.06f, 0.24f + flap), P (-0.02f, 0.26f + flap));
    fin.cubicTo (P (0.06f, 0.20f + flap), P (0.14f, 0.13f), P (0.15f, 0.085f));
    fin.closeSubPath();
    const auto top = body.darker (0.4f), belly = body.brighter (0.25f);
    g.setGradientFill (juce::ColourGradient (top, x, y - L * 0.12f, belly, x, y + L * 0.12f, false));
    g.fillPath (p);
    g.fillPath (fin);
    // ventral grooves: a few faint lines under the jaw
    g.setColour (belly.brighter (0.4f).withAlpha (body.getFloatAlpha() * 0.35f));
    for (int k = 0; k < 4; ++k)
    {
        juce::Path gr;
        gr.startNewSubPath (P (0.45f, 0.035f + 0.012f * (float) k));
        gr.quadraticTo (P (0.33f, 0.07f + 0.012f * (float) k), P (0.18f, 0.08f + 0.01f * (float) k));
        g.strokePath (gr, juce::PathStrokeType (0.8f));
    }
    // sunlight on the back
    g.setColour (juce::Colour (0xffbff4ff).withAlpha (body.getFloatAlpha() * 0.25f));
    juce::Path rim;
    rim.startNewSubPath (P (0.46f, -0.05f));
    rim.cubicTo (P (0.33f, -0.11f), P (0.12f, -0.12f), P (-0.10f, -0.115f));
    g.strokePath (rim, juce::PathStrokeType (1.2f));
}

// ---- cinematic finish: anamorphic flare, colour grade, foreground bokeh, film grain
void Landscape::paintPost (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float w = b.getWidth(), h = b.getHeight(), t = (float) f.time;
    // anamorphic flare: a long thin horizontal streak through the sun / moon + two soft ghosts
    if (flareAmt > 0.01f)
    {
        const auto fp = flarePos;
        const float a = 0.18f * flareAmt;
        juce::ColourGradient st (flareCol.withAlpha (0.0f), fp.x - w * 0.45f, fp.y, flareCol.withAlpha (0.0f), fp.x + w * 0.45f, fp.y, false);
        st.addColour (0.5, flareCol.brighter (0.6f).withAlpha (a));
        g.setGradientFill (st);
        g.fillRect (fp.x - w * 0.45f, fp.y - 1.2f, w * 0.9f, 2.4f);
        st.addColour (0.5, flareCol.withAlpha (a * 0.4f));
        g.setGradientFill (st);
        g.fillRect (fp.x - w * 0.3f, fp.y - 6.0f, w * 0.6f, 12.0f);
        const auto c = b.getCentre();
        for (int k = 1; k <= 2; ++k)   // ghosts mirrored through the centre
        {
            const float u = 0.6f * (float) k;
            const float gx = fp.x + (c.x - fp.x) * (1.0f + u), gy = fp.y + (c.y - fp.y) * (1.0f + u);
            softBlob (g, gx, gy, 18.0f * (float) k, 18.0f * (float) k, flareCol.withAlpha (0.05f * flareAmt));
        }
    }
    // colour grade: cool shadows below, warm air above (very light, unifies every scene)
    {
        juce::ColourGradient cg (juce::Colour (0xffffb27a).withAlpha (0.045f), 0, 0, juce::Colour (0xff1c6b8a).withAlpha (0.07f), 0, h, false);
        cg.addColour (0.5, juce::Colours::transparentBlack);
        g.setGradientFill (cg);
        g.fillRect (b);
    }
    // foreground bokeh: big out-of-focus specks drifting slowly in front of the scene
    if (! f.reduced)
        for (int i = 0; i < 9; ++i)
        {
            const float sp = 4.0f + 6.0f * hashF ((uint32_t) i, 4001);
            const float x = std::fmod (w * hashF ((uint32_t) f.seed, (uint32_t) i + 4002) + t * sp, w * 1.3f) - w * 0.15f;
            const float y = h * hashF ((uint32_t) f.seed, (uint32_t) i + 4003) + std::sin (t * 0.2f + (float) i) * 12.0f;
            const float r = h * (0.04f + 0.06f * hashF ((uint32_t) i, 4004));
            const float a = (0.025f + 0.035f * hashF ((uint32_t) i, 4005)) * (0.6f + 0.6f * f.energy);
            const auto c = (i % 3 == 0 ? col::accent2 : i % 3 == 1 ? juce::Colours::white : col::accent);
            softBlob (g, x, y, r, r, c.withAlpha (a));
        }
    // film grain (static tile, jittered every frame)
    {
        if (! grain.isValid())
        {
            grain = juce::Image (juce::Image::ARGB, 192, 192, true);
            juce::Random r (99);
            for (int y = 0; y < 192; ++y)
                for (int x = 0; x < 192; ++x)
                {
                    const auto v = (juce::uint8) r.nextInt (256);
                    grain.setPixelAt (x, y, juce::Colour (v, v, v, (juce::uint8) 5));
                }
        }
        const int ox = (int) (hashF ((uint32_t) (t * 24.0f), 1) * 192.0f), oy = (int) (hashF ((uint32_t) (t * 24.0f), 2) * 192.0f);
        for (int y = -oy; y < (int) h; y += 192)
            for (int x = -ox; x < (int) w; x += 192)
                g.drawImageAt (grain, x, y);
    }
}
}
