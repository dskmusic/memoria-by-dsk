#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace gx::ui
{
// The look of the scene as numbers: sky, sun, outdoor colours, season, clock. One source for the GPU shader and the CPU fallback.
// The scene eases from one look to the next (hour or season changes are never a hard cut).
enum LookIndex
{
    kTopR, kTopG, kTopB, kBotR, kBotG, kBotB, kSunR, kSunG, kSunB,      // sky top / horizon, light colour (with its intensity)
    kSunI, kSunPx, kSunPy, kDirX, kDirY,                                // disc glow, disc position in the window, light direction (x, elevation)
    kAmb, kNight, kCloud, kWind, kRain, kOutL,                          // fill from the window, night 0..1, cloud cover, wind, rain, brightness of the outdoor objects
    kGrR, kGrG, kGrB, kFoR, kFoG, kFoB,                                 // ground and foliage colours
    kFoAmt, kSnow, kBlossom, kLeaf, kSeason,                            // crown fullness, snow, blossom, falling leaves, 0 = "auto" (dark silhouettes as before)
    kHours,                                                             // time shown by the wall clock (hours, 0..12)
    kLookCount
};
struct SceneLook { float v[kLookCount] {}; };


// world: 0 clear (golden afternoon), 1 rain, 2 wind, 3 fireplace (night), 4 dawn birds, 5 audience (dusk)
inline SceneLook lookFor (int world, int tod, int season)
{
    SceneLook L; auto& v = L.v;
    auto set3 = [&v] (int i, float a, float b, float c) { v[i] = a; v[i + 1] = b; v[i + 2] = c; };
    // 1 dawn, 2 morning, 3 afternoon, 4 sunset, 5 night; 0 = by world (the original look)
    int t = tod;
    if (t == 0) t = world == 3 ? 5 : world == 4 ? 1 : world == 5 ? 4 : 3;
    struct Pal { float top[3], bot[3], sun[3], sunI, px, py, dx, dy, amb, night, hours; };
    static const Pal pal[] {
        { { 0.62f, 0.46f, 0.34f }, { 1.00f, 0.80f, 0.50f }, { 1.00f, 0.78f, 0.50f }, 1.0f, 0.68f, 0.30f, -0.33f, 0.40f, 1.0f, 0.0f, 4.35f },   // [0] afternoon (original)
        { { 0.45f, 0.38f, 0.62f }, { 1.00f, 0.68f, 0.46f }, { 1.00f, 0.70f, 0.55f }, 0.9f, 0.55f, 0.22f, -0.22f, 0.20f, 1.0f, 0.0f, 6.40f },   // [1] dawn (original)
        { { 0.24f, 0.42f, 0.70f }, { 0.66f, 0.72f, 0.80f }, { 1.00f, 0.93f, 0.80f }, 0.7f, 0.32f, 0.36f, -0.30f, 0.34f, 1.0f, 0.0f, 9.20f },   // [2] morning
        { { 0.62f, 0.46f, 0.34f }, { 1.00f, 0.80f, 0.50f }, { 1.00f, 0.78f, 0.50f }, 1.0f, 0.68f, 0.30f, -0.33f, 0.40f, 1.0f, 0.0f, 4.35f },   // [3] afternoon
        { { 0.30f, 0.26f, 0.44f }, { 0.96f, 0.56f, 0.38f }, { 1.00f, 0.55f, 0.32f }, 0.9f, 0.27f, 0.14f, -0.52f, 0.12f, 0.75f, 0.0f, 7.10f },  // [4] sunset
        { { 0.01f, 0.02f, 0.06f }, { 0.05f, 0.07f, 0.15f }, { 0.45f, 0.55f, 0.95f }, 0.0f, 0.62f, 0.30f, -0.25f, 0.50f, 0.25f, 1.0f, 10.40f } };// [5] night
    const Pal& p = pal[juce::jlimit (0, 5, t)];
    float sunMul = (t == 5) ? 0.22f : (t == 4) ? 0.95f : (t == 1) ? 1.0f : (t == 2) ? 1.05f : 1.25f;
    set3 (kTopR, p.top[0], p.top[1], p.top[2]); set3 (kBotR, p.bot[0], p.bot[1], p.bot[2]);
    set3 (kSunR, p.sun[0] * sunMul, p.sun[1] * sunMul, p.sun[2] * sunMul);
    v[kSunI] = p.sunI; v[kSunPx] = p.px; v[kSunPy] = p.py; v[kDirX] = p.dx; v[kDirY] = p.dy; v[kAmb] = p.amb; v[kNight] = p.night; v[kHours] = p.hours;
    v[kCloud] = 0.35f; v[kOutL] = t == 5 ? 0.10f : t == 4 ? 0.55f : t == 1 ? 0.60f : 1.0f;
    // weather: rain is overcast and dim, wind is a clear bright sky with moving cloud (as before)
    if (world == 1 && tod == 0) { set3 (kTopR, 0.36f, 0.42f, 0.50f); set3 (kBotR, 0.58f, 0.62f, 0.68f); set3 (kSunR, 0.55f * 0.35f, 0.65f * 0.35f, 0.85f * 0.35f); v[kSunI] = 0; v[kAmb] = 0.6f; v[kNight] = 0; v[kCloud] = 0.8f; v[kRain] = 1; v[kOutL] = 0.6f; v[kDirY] = 0.40f; }
    else if (world == 1)
    {   // rain at another hour: the chosen light, greyed and darker
        for (int i : { kTopR, kTopG, kTopB, kBotR, kBotG, kBotB }) v[i] = v[i] * 0.55f + 0.20f * (i % 3 == 0 ? 0.7f : i % 3 == 1 ? 0.8f : 0.9f) * (v[kNight] > 0.5f ? 0.2f : 1.0f);
        for (int i : { kSunR, kSunG, kSunB }) v[i] *= 0.35f;
        v[kSunI] = 0; v[kAmb] *= 0.7f; v[kCloud] = 0.8f; v[kRain] = 1; v[kOutL] *= 0.7f;
    }
    else if (world == 5 && tod == 0) { set3 (kTopR, 0.18f, 0.12f, 0.30f); set3 (kBotR, 0.85f, 0.42f, 0.38f); set3 (kSunR, 0.85f * 0.55f, 0.50f * 0.55f, 0.70f * 0.55f); v[kSunI] = 0.3f; v[kSunPx] = 0.25f; v[kSunPy] = 0.2f; v[kDirX] = -0.33f; v[kDirY] = 0.40f; v[kAmb] = 1.0f; v[kOutL] = 0.7f; }
    else if (world == 2)
    {
        if (tod == 0) { set3 (kTopR, 0.38f, 0.52f, 0.72f); set3 (kBotR, 0.88f, 0.86f, 0.80f); set3 (kSunR, 0.95f * 0.85f, 0.90f * 0.85f, 0.80f * 0.85f); v[kSunI] = 0.55f; v[kSunPx] = 0.35f; v[kSunPy] = 0.30f; v[kAmb] = 1.0f; v[kOutL] = 0.9f; }
        v[kWind] = 1; v[kCloud] = 0.35f;
    }
    // season (0 = auto: the dark silhouettes of the original scene)
    if (season > 0)
    {
        v[kSeason] = 1;
        switch (season)
        {
            case 1: set3 (kGrR, 0.24f, 0.36f, 0.15f); set3 (kFoR, 0.30f, 0.44f, 0.20f); v[kFoAmt] = 0.85f; v[kBlossom] = 1.0f; break;                     // spring
            case 2: set3 (kGrR, 0.20f, 0.32f, 0.12f); set3 (kFoR, 0.13f, 0.26f, 0.09f); v[kFoAmt] = 1.0f; break;                                          // summer
            case 3: set3 (kGrR, 0.34f, 0.25f, 0.12f); set3 (kFoR, 0.66f, 0.28f, 0.08f); v[kFoAmt] = 0.8f; v[kLeaf] = 1.0f; break;                          // autumn
            default: set3 (kGrR, 0.36f, 0.40f, 0.48f); set3 (kFoR, 0.30f, 0.24f, 0.20f); v[kFoAmt] = 0.0f; v[kSnow] = 1.0f; break;                         // winter
        }
        if (season == 4 && world == 1) v[kSnow] = 1.0f;
    }
    return L;
}

inline void easeLook (SceneLook& cur, const SceneLook& tgt, float k)
{
    for (int i = 0; i < kLookCount; ++i) cur.v[i] += (tgt.v[i] - cur.v[i]) * k;
}
}
