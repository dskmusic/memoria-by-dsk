#include "RoomScene.h"

namespace gx::ui
{
namespace
{
inline float sm (float a, float b, float x) { const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a)); return t * t * (3.0f - 2.0f * t); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float hash2 (float x, float y) { float h = std::sin (x * 127.1f + y * 311.7f) * 43758.5453f; return h - std::floor (h); }
constexpr float kWx0 = 0.10f, kWx1 = 0.34f, kWy0 = 0.10f, kWy1 = 0.60f;     // window
constexpr float kDx = 0.62f, kDy = 0.50f;                                    // light direction (towards the floor, right)
}

RoomScene::RoomScene()
{
    setInterceptsMouseClicks (false, false);
    for (auto& m : motes) m = { rng.nextFloat() * 0.8f + 0.1f, rng.nextFloat() * 0.7f + 0.15f, 0, 0, rng.nextFloat() * 6.28f, 0.6f + rng.nextFloat() };
    for (auto& d : drops) d = { kWx0 + rng.nextFloat() * (kWx1 - kWx0), kWy0 + rng.nextFloat() * (kWy1 - kWy0), 0.03f + rng.nextFloat() * 0.07f, 0.02f + rng.nextFloat() * 0.05f };
}

// static picture: walls, window and what is outside it, piano, the shape of the light beam
void RoomScene::rebuild (int world)
{
    built = world;
    const size_t n = (size_t) (W * H);
    baseR.assign (n, 0); baseG = baseB = beam = candle = glow = vig = baseR;
    // outside: sky colours by world
    struct Sky { float tr, tg, tb, br, bg, bb, amb, warm, beamI; };
    static const Sky sky[] {
        { 0.75f, 0.45f, 0.30f, 1.00f, 0.82f, 0.50f, 1.00f, 0.3f, 1.00f },   // golden afternoon
        { 0.40f, 0.46f, 0.56f, 0.60f, 0.64f, 0.70f, 0.55f, 0.0f, 0.30f },   // rain: overcast
        { 0.18f, 0.22f, 0.38f, 0.95f, 0.55f, 0.28f, 0.75f, 0.5f, 0.55f },   // cafe: dusk and street lamps
        { 0.02f, 0.04f, 0.10f, 0.07f, 0.09f, 0.18f, 0.6f, 0.8f, 0.18f },   // fireplace: night
        { 0.50f, 0.42f, 0.62f, 1.00f, 0.70f, 0.48f, 0.85f, 0.3f, 0.85f },   // dawn
        { 0.20f, 0.14f, 0.30f, 0.85f, 0.45f, 0.38f, 0.6f, 0.4f, 0.45f } };  // distant audience: violet dusk
    const auto& sk = sky[juce::jlimit (0, 5, world)];
    skyR = lerpf (sk.tr, sk.br, 0.6f); skyG = lerpf (sk.tg, sk.bg, 0.6f); skyB = lerpf (sk.tb, sk.bb, 0.6f);
    ambient = sk.amb; warm = sk.warm;
    const float beamI = sk.beamI;
    const float aspect = (float) W / H;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const size_t i = (size_t) (y * W + x);
            const float u = (x + 0.5f) / W, v = (y + 0.5f) / H;
            // wall with faint wallpaper stripes, darker towards the top corners
            float r = 0.17f, g = 0.115f, b = 0.09f;
            const float stripe = 1.0f + 0.05f * std::sin (u * 150.0f);
            const float wallLight = 0.75f + 0.35f * sm (0.0f, 0.55f, v) * (1.0f - 0.4f * u);
            r *= stripe * wallLight; g *= stripe * wallLight; b *= stripe * wallLight;
            // floor with planks
            if (v > 0.80f)
            {
                const float t = (v - 0.80f) / 0.20f, plank = 0.85f + 0.15f * std::sin ((u + t * 0.3f) * 60.0f + hash2 (std::floor ((u + t * 0.3f) * 9.0f), 3.0f) * 6.0f);
                r = 0.13f * plank * (0.8f + 0.5f * t); g = 0.08f * plank * (0.8f + 0.5f * t); b = 0.05f * plank * (0.8f + 0.5f * t);
            }
            else if (v > 0.77f) { r = 0.2f; g = 0.13f; b = 0.09f; }             // baseboard
            // curtains
            const bool curtL = u > 0.035f && u < kWx0 && v > 0.05f && v < 0.68f, curtR = u > kWx1 && u < 0.405f && v > 0.05f && v < 0.68f;
            if (curtL || curtR)
            {
                const float fold = 0.55f + 0.45f * std::sin (u * 260.0f + (curtR ? 1.0f : 0.0f));
                r = 0.30f * fold; g = 0.09f * fold; b = 0.08f * fold;
            }
            // window
            const bool inWin = u > kWx0 && u < kWx1 && v > kWy0 && v < kWy1;
            if (inWin)
            {
                const float wu = (u - kWx0) / (kWx1 - kWx0), wv = (v - kWy0) / (kWy1 - kWy0);
                const bool bar = wu < 0.03f || wu > 0.97f || wv < 0.025f || wv > 0.975f || std::abs (wu - 0.5f) < 0.018f || std::abs (wv - 0.34f) < 0.012f || std::abs (wv - 0.67f) < 0.012f;
                if (bar) { r = 0.11f; g = 0.075f; b = 0.05f; }
                else
                {
                    float sr = lerpf (sk.tr, sk.br, wv), sg = lerpf (sk.tg, sk.bg, wv), sb = lerpf (sk.tb, sk.bb, wv);
                    // sun / moon glow, hills and a tree
                    const float dx = (wu - 0.68f) * 0.9f, dy = (wv - 0.72f) * 1.4f, d = std::sqrt (dx * dx + dy * dy);
                    const float sun = std::exp (-d * 5.0f) * 0.6f + (d < 0.05f ? 0.8f : 0.0f);
                    sr += sun * sk.br; sg += sun * sk.bg * 0.9f; sb += sun * sk.bb * 0.7f;
                    if (world == 3 && hash2 (std::floor (wu * 40), std::floor (wv * 40)) > 0.985f && wv < 0.6f) { sr += 0.5f; sg += 0.5f; sb += 0.6f; }
                    const float hill = 0.80f + 0.05f * std::sin (wu * 8.0f) + 0.03f * std::sin (wu * 21.0f + 1.0f);
                    const float tx = wu - 0.25f, ty = wv - 0.66f;
                    const bool tree = (tx * tx * 6.0f + ty * ty * 2.2f < 0.03f && wv < hill) || (std::abs (tx) < 0.012f && wv > 0.66f && wv < hill + 0.03f);
                    if (wv > hill || tree) { sr *= 0.12f; sg *= 0.12f; sb *= 0.14f; }
                    if (world == 2 && wv > hill - 0.06f && hash2 (std::floor (wu * 26), 1.0f) > 0.8f) { sr += 0.9f; sg += 0.6f; sb += 0.2f; }   // street lamps
                    r = sr * 1.5f; g = sg * 1.5f; b = sb * 1.5f;
                }
            }
            // piano (upright-grand silhouette), keyboard, bench, music stand
            const float pu = (u - 0.42f) / 0.54f, pv = (v - 0.52f) / 0.32f;
            if (pu > 0 && pu < 1 && pv > 0 && pv < 1)
            {
                const float cx = std::min (pu, 1.0f - pu) * 0.54f * aspect * 2.0f, cy = pv * 0.32f * 2.0f;
                const bool corner = cx < 0.04f && cy < 0.04f && (0.04f - cx) * (0.04f - cx) + (0.04f - cy) * (0.04f - cy) > 0.0016f;
                if (! corner)
                {
                    r = 0.025f; g = 0.02f; b = 0.026f;
                    if (pv < 0.05f) { const float h = 0.25f + 0.5f * sm (0.0f, 0.05f, 0.05f - pv) * 0.0f; r += 0.22f * (1.0f - pv * 20.0f); g += 0.18f * (1.0f - pv * 20.0f); b += 0.15f * (1.0f - pv * 20.0f); (void) h; }
                    if (pv > 0.60f && pv < 0.72f && pu > 0.03f && pu < 0.97f)     // keys
                    {
                        const float k = std::fmod (pu * 52.0f, 1.0f);
                        const bool blk = pv < 0.665f && (int) (pu * 52.0f) % 7 != 2 && (int) (pu * 52.0f) % 7 != 6 && k > 0.72f;
                        r = g = b = blk ? 0.02f : (k < 0.06f ? 0.25f : 0.72f); b *= blk ? 1.0f : 0.92f;
                    }
                }
            }
            if (u > 0.55f && u < 0.72f && v > 0.40f && v < 0.52f) { const bool sheet = u > 0.575f && u < 0.695f && v > 0.415f && v < 0.505f; r = sheet ? 0.55f : 0.03f; g = sheet ? 0.5f : 0.025f; b = sheet ? 0.4f : 0.025f; }
            if ((u > 0.47f && u < 0.495f && v >= 0.84f && v < 0.97f) || (u > 0.89f && u < 0.915f && v >= 0.84f && v < 0.97f)) { r = 0.03f; g = 0.022f; b = 0.02f; }
            if (u > 0.60f && u < 0.78f && v > 0.865f && v < 0.93f) { r = 0.10f; g = 0.06f; b = 0.04f; }
            if (u > 0.845f && u < 0.862f && v > 0.44f && v < 0.52f) { r = 0.9f; g = 0.82f; b = 0.62f; }        // candle wax
            baseR[i] = r; baseG[i] = g; baseB[i] = b;

            // light shaft from the window: all points p with p - s*D inside the window for some s >= 0
            const float lox = juce::jmax (0.0f, juce::jmax ((u - kWx1) / kDx, (v - kWy1) / kDy)), hix = juce::jmin ((u - kWx0) / kDx, (v - kWy0) / kDy);
            if (hix > lox && ! inWin)
            {
                const float len = hix - lox;
                const float s = 0.5f * (lox + hix), wx = (u - s * kDx - kWx0) / (kWx1 - kWx0), wy = (v - s * kDy - kWy0) / (kWy1 - kWy0);
                const bool mull = std::abs (wx - 0.5f) < 0.03f || std::abs (wy - 0.34f) < 0.022f || std::abs (wy - 0.67f) < 0.022f;
                float m = sm (0.0f, 0.12f, len) * std::exp (-lox * 1.1f) * (mull ? 0.25f : 1.0f);
                beam[i] = m * beamI;
            }
            // soft glow around the window, candle halo
            glow[i] = std::exp (-(std::pow ((u - 0.22f) * aspect, 2.0f) * 14.0f + std::pow (v - 0.35f, 2.0f) * 10.0f)) * 0.5f;
            const float cdx = (u - 0.853f) * aspect, cdy = v - 0.43f;
            candle[i] = std::exp (-(cdx * cdx + cdy * cdy) * 55.0f);
            const float vx = (u - 0.5f) * 1.6f, vy = (v - 0.5f) * 1.5f;
            vig[i] = juce::jlimit (0.0f, 1.0f, 1.0f - (vx * vx + vy * vy) * 0.5f);
        }
}

RoomScene::~RoomScene() { setGpu (false); }

void RoomScene::parentHierarchyChanged() { if (gpuWanted && gl == nullptr && getPeer() != nullptr) setGpu (true); }

void RoomScene::setGpu (bool on)
{
    if (on && gl == nullptr && gpuStatus().load() >= 0 && getPeer() != nullptr)
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

void RoomScene::newOpenGLContextCreated()
{
    const bool ok = gpuRoom.init (*gl);
    glOk = ok;
    if (! ok) gpuStatus() = -1; else if (gpuStatus().load() == 0) gpuStatus() = 1;
}

void RoomScene::openGLContextClosing() { gpuRoom.release(); glOk = false; }

void RoomScene::renderOpenGL()
{
    if (! glOk.load()) return;
    RoomUniforms u;
    { const juce::SpinLock::ScopedLockType sl (ulock); u = uni; }
    const double sc = gl->getRenderingScale();
    gpuRoom.render (u, juce::roundToInt (sc * getWidth()), juce::roundToInt (sc * getHeight()));
}

RoomUniforms RoomScene::uniforms() const
{
    RoomUniforms u;
    u.time = (float) f.time; u.memory = f.memory; u.age = f.timeAge; u.world = (float) f.worldType; u.energy = f.energy;
    u.scale = quality == 0 ? 0.5f : quality == 1 ? 0.75f : 1.0f;
    for (int i = 0; i < 88; ++i) u.keys[i] = juce::jlimit (0.0f, 1.0f, f.keyLevel[(size_t) (21 + i)] * 3.0f);
    u.hit = hitEnv; u.pedal = f.pedal; u.flame = flame; u.intimacy = f.intimacy; u.warmth = f.warmth; u.space = f.space;
    return u;
}

void RoomScene::tick (const Frame& fr)
{
    f = fr;
    const double now = fr.time;
    const float dt = (float) juce::jlimit (0.0, 0.2, now - last);
    last = now;
    // motes drift in the light and are pushed by the notes
    const bool hit = fr.hitCounter != lastHits;
    lastHits = fr.hitCounter;
    hitEnv = hit ? 1.0f : hitEnv * (float) std::exp (-dt * 3.0);
    for (auto& m : motes)
    {
        if (hit) { m.vx += (rng.nextFloat() - 0.5f) * 0.05f; m.vy -= 0.02f * rng.nextFloat(); }
        m.ph += dt * (0.6f + m.size);
        m.vx += (std::sin (m.ph * 1.3f) * 0.004f - m.vx * 0.5f * dt) ;
        m.vy += (std::cos (m.ph) * 0.003f - 0.0004f - m.vy * 0.8f * dt);
        m.x += m.vx * dt * 4.0f; m.y += m.vy * dt * 4.0f;
        if (m.x < 0.05f) m.x += 0.9f; if (m.x > 0.95f) m.x -= 0.9f;
        if (m.y < 0.1f) m.y += 0.75f; if (m.y > 0.88f) m.y -= 0.75f;
    }
    for (auto& d : drops)
    {
        d.y += d.v * dt;
        if (d.y > kWy1) { d.y = kWy0; d.x = kWx0 + rng.nextFloat() * (kWx1 - kWx0); d.v = 0.03f + rng.nextFloat() * 0.07f; }
    }
    flameT += dt * 9.0f;
    flame = 0.85f + 0.12f * std::sin (flameT) * std::sin (flameT * 0.37f + 1.0f) + 0.06f * std::sin (flameT * 2.9f) + juce::jmin (0.4f, fr.energy * 0.8f);
    scratchT -= dt;
    if (scratchT <= 0) { scratchT = 0.08f + rng.nextFloat() * 0.2f; for (auto& s : scratchX) s = rng.nextFloat(); }
    const double interval = fr.reduced ? 1.0 / 12.0 : 1.0 / 24.0;
    if (glOk.load())
    {
        { const juce::SpinLock::ScopedLockType sl (ulock); uni = uniforms(); }
        if (now - lastRender >= (fr.reduced ? 1.0 / 20.0 : 1.0 / 60.0)) { lastRender = now; if (gl != nullptr) gl->triggerRepaint(); }
    }
    else if (external.isNull() && now - lastRender >= interval) { renderNow (fr); lastRender = now; repaint(); }
    else if (! external.isNull()) repaint();
}

void RoomScene::renderNow (const Frame& fr)
{
    f = fr;
    if (built != fr.worldType || baseR.empty()) rebuild (fr.worldType);
    const float mem = juce::jlimit (0.0f, 1.0f, fr.memory), age = juce::jlimit (0.0f, 1.0f, fr.timeAge);
    const float aging = juce::jlimit (0.0f, 1.0f, mem * (0.55f + 0.45f * age));
    const float beamI = 0.9f * (0.8f + 0.4f * fr.energy);
    const float gate = 1.0f + (aging > 0.05f ? (hash2 ((float) std::floor (fr.time * 18.0), 1.0f) - 0.5f) * 0.09f * aging : 0.0f);
    const int jitter = aging > 0.3f ? (int) std::floor (hash2 ((float) std::floor (fr.time * 14.0), 2.0f) * 2.99f) - 1 : 0;
    const bool rain = fr.worldType == 1;
    const float cdim = fr.worldType == 3 ? 1.6f : fr.worldType == 2 ? 1.0f : 0.6f;
    const float candleI = flame * cdim;
    std::vector<float> accR (baseR.size()), accG (baseR.size()), accB (baseR.size());
    const float bR = juce::jmin (1.0f, skyR * 1.1f) * 1.0f, bG = skyG * 1.0f, bB = skyB * 0.9f;
    const float cs = 1.0f - 0.0f;
    (void) cs;
    for (size_t i = 0; i < baseR.size(); ++i)
    {
        const float am = ambient * gate;
        float r = baseR[i] * am, g = baseG[i] * am, b = baseB[i] * am;
        const float bm = beam[i] * beamI;
        r += bm * bR * 0.75f; g += bm * bG * 0.75f; b += bm * bB * 0.75f;
        // beam lights surfaces it falls on (adds to the surface colour)
        r += baseR[i] * bm * 2.0f * bR; g += baseG[i] * bm * 2.0f * bG; b += baseB[i] * bm * 2.0f * bB;
        r += glow[i] * bR * 0.35f * ambient; g += glow[i] * bG * 0.35f * ambient; b += glow[i] * bB * 0.35f * ambient;
        const float cm = candle[i] * candleI;
        r += cm * 0.95f; g += cm * 0.55f; b += cm * 0.22f;
        r += baseR[i] * cm * 3.0f; g += baseG[i] * cm * 2.2f; b += baseB[i] * cm * 1.2f;
        accR[i] = r; accG[i] = g; accB[i] = b;
    }
    auto addDot = [&] (float px, float py, float rad, float rr, float gg, float bb, float amt)
    {
        const int cx = (int) (px * W), cy = (int) (py * H), R = (int) std::ceil (rad * 2.0f);
        for (int yy = cy - R; yy <= cy + R; ++yy)
            for (int xx = cx - R; xx <= cx + R; ++xx)
            {
                if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                const float d2 = ((float) (xx - cx) * (xx - cx) + (float) (yy - cy) * (yy - cy)) / (rad * rad), w = std::exp (-d2 * 1.2f) * amt;
                const size_t i = (size_t) (yy * W + xx);
                accR[i] += rr * w; accG[i] += gg * w; accB[i] += bb * w;
            }
    };
    // dust in the light
    for (auto& m : motes)
    {
        const int ix = juce::jlimit (0, W - 1, (int) (m.x * W)), iy = juce::jlimit (0, H - 1, (int) (m.y * H));
        const float inBeam = beam[(size_t) (iy * W + ix)], tw = 0.55f + 0.45f * std::sin (m.ph * 3.0f);
        const float a = (inBeam * 1.2f + 0.04f) * tw * beamI;
        if (a > 0.02f) addDot (m.x, m.y, 0.9f * m.size, bR, bG, bB, a * 1.2f);
    }
    // candle flame
    {
        const float fx = 0.8535f + 0.002f * std::sin (flameT * 1.7f), fy = 0.425f;
        addDot (fx, fy, 2.2f, 1.0f, 0.78f, 0.35f, 1.4f * flame);
        addDot (fx, fy + 0.006f, 1.2f, 1.0f, 0.95f, 0.8f, 1.2f);
    }
    // rain on the window
    if (rain)
        for (auto& d : drops)
        {
            const int steps = (int) (d.len * H);
            for (int k = 0; k < steps; ++k)
            {
                const float t = 1.0f - (float) k / (float) steps;
                addDot (d.x, d.y - k / (float) H, 0.8f, 0.7f, 0.82f, 1.0f, 0.22f * t);
            }
            addDot (d.x, d.y, 1.4f, 0.85f, 0.92f, 1.0f, 0.5f);
        }
    // tone mapping, ageing, grain, write
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const float sepiaAmt = 0.8f * aging, lift = 0.05f * aging, vigAmt = 0.25f + 0.6f * aging, grain = 0.014f + 0.06f * aging;
    const int seedT = (int) std::floor (fr.time * 24.0);
    for (int y = 0; y < H; ++y)
    {
        const int sy = juce::jlimit (0, H - 1, y + jitter);
        auto* line = bd.getLinePointer (y);
        for (int x = 0; x < W; ++x)
        {
            const size_t i = (size_t) (sy * W + x);
            float r = accR[i], g = accG[i], b = accB[i];
            const float v = lerpf (1.0f, vig[i], vigAmt);
            r *= v; g *= v; b *= v;
            r = r / (1.0f + r * 0.9f) * 1.6f; g = g / (1.0f + g * 0.9f) * 1.6f; b = b / (1.0f + b * 0.9f) * 1.6f;     // soft shoulder
            if (aging > 0.0f)
            {
                const float l = 0.3f * r + 0.59f * g + 0.11f * b;
                r = lerpf (r, l * 1.12f, sepiaAmt); g = lerpf (g, l * 0.92f, sepiaAmt); b = lerpf (b, l * 0.66f, sepiaAmt);
                r += lift; g += lift * 0.9f; b += lift * 0.7f;
                for (float sx : scratchX) if (aging > 0.25f && std::abs ((float) x / W - sx) < 0.0007f && hash2 (sx, (float) seedT) > 0.3f) { r += 0.10f * aging; g += 0.09f * aging; b += 0.07f * aging; }
                const float lx = (float) x / W;
                const float leak = aging > 0.45f ? sm (0.85f, 1.0f, lx) * (0.1f + 0.08f * std::sin ((float) fr.time * 0.7f)) * aging : 0.0f;
                r += leak; g += leak * 0.45f;
            }
            const float n = (hash2 ((float) x + (float) seedT * 17.0f, (float) y * 1.3f + (float) seedT) - 0.5f) * grain;
            r += n; g += n; b += n;
            const auto cr = (juce::uint8) juce::jlimit (0, 255, (int) (r * 255.0f)), cg = (juce::uint8) juce::jlimit (0, 255, (int) (g * 255.0f)), cb = (juce::uint8) juce::jlimit (0, 255, (int) (b * 255.0f));
            const auto px = juce::PixelARGB (255, cr, cg, cb);
            *reinterpret_cast<juce::PixelARGB*> (line + x * bd.pixelStride) = px;
        }
    }
}

void RoomScene::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::Path clip; clip.addRoundedRectangle (r, 8.0f);
    g.saveState();
    if (! glOk.load()) g.reduceClipRegion (clip);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (glOk.load()) { /* the shader draws underneath */ }
    else g.drawImage (external.isNull() ? img : external, r, juce::RectanglePlacement::fillDestination);
    g.restoreState();
    g.setColour (col::accent.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (col::text.withAlpha (0.8f));
    g.setFont (monoFont (11.0f));
    g.drawText (L ("MEMORIA ", "MEMORY ") + juce::String ((int) std::round (f.memory * 100.0f)) + " %", r.reduced (10, 8), juce::Justification::bottomLeft);
}
}
