#include "Space.h"

namespace gx
{
void Fdn::prepare (double s)
{
    sr = s;
    for (auto& l : line) l.init ((int) (sr * 0.45));
    for (auto& a : ap) a.init ((int) (sr * 0.05));
    pre.init ((int) (sr * 0.12));
    clear();
    set (0.5f, 0.3f);
}

void Fdn::clear()
{
    for (auto& l : line) l.clear();
    for (auto& a : ap) a.clear();
    pre.clear();
    lp.fill (0);
    modPh = 0;
    primed = false;
}

void Fdn::set (float size, float dampAmt, float rt60Scale)
{
    static const float base[8] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
    static const float apb[6] { 4.7f, 3.6f, 12.7f, 9.3f, 17.1f, 23.3f };
    size = juce::jlimit (0.0f, 1.2f, size);
    const float scale = 0.45f + size * 1.75f;
    const float rt60 = (0.35f + size * size * 9.0f) * rt60Scale;
    for (int i = 0; i < 8; ++i)
    {
        len[(size_t) i] = base[i] * scale * 0.001f * (float) sr;
        g[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (rt60 * (float) sr));
    }
    for (int i = 0; i < 6; ++i) apLen[(size_t) i] = apb[i] * 0.001f * (float) sr * (0.8f + 0.4f * size);
    preLen = (8.0f + 60.0f * juce::jlimit (0.0f, 1.0f, size)) * 0.001f * (float) sr; // pre-delay grows with the space
    damp = 0.05f + juce::jlimit (0.0f, 1.0f, dampAmt) * 0.85f;
}

void Fdn::process (float inL, float inR, float& outL, float& outR)
{
    if (! primed) { lenNow = len; apNow = apLen; preNow = preLen; primed = true; }
    constexpr float glide = 0.0004f; // sizes glide (~50 ms): turning Space never clicks
    for (int i = 0; i < 8; ++i) lenNow[(size_t) i] += glide * (len[(size_t) i] - lenNow[(size_t) i]);
    for (int i = 0; i < 6; ++i) apNow[(size_t) i] += glide * (apLen[(size_t) i] - apNow[(size_t) i]);
    preNow += glide * (preLen - preNow);
    pre.push ((inL + inR) * 0.5f);
    float x = pre.read (preNow); // pre-delay, then input diffusion
    for (int i = 0; i < 6; ++i)
    {
        const float d = ap[(size_t) i].read (apNow[(size_t) i]);
        const float v = x + 0.6f * d;
        ap[(size_t) i].push (v);
        x = d - 0.6f * v;
    }
    modPh += 0.7f / (float) sr;
    if (modPh > 1) modPh -= 1;
    const float m = fastSin (modPh) * 6.0f;
    float y[8];
    for (int i = 0; i < 8; ++i)
    {
        const float rd = line[(size_t) i].read (lenNow[(size_t) i] + (i == 1 ? m : i == 5 ? -m : i == 3 ? m * 0.6f : i == 6 ? -m * 0.6f : 0.0f));
        lp[(size_t) i] = rd + damp * (lp[(size_t) i] - rd);
        y[i] = lp[(size_t) i] * g[(size_t) i];
    }
    for (int h = 1; h < 8; h <<= 1) // fast Walsh-Hadamard 8x8
        for (int i = 0; i < 8; i += h * 2)
            for (int j = i; j < i + h; ++j) { const float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
    const float nrm = 0.35355339f;
    for (int i = 0; i < 8; ++i) line[(size_t) i].push (y[i] * nrm + x * ((i & 2) ? -0.35f : 0.35f)); // sign pattern decorrelates L/R
    outL = (lp[0] - lp[2] + lp[4] - lp[6]) * 0.5f;
    outR = (lp[1] - lp[3] + lp[5] - lp[7]) * 0.5f;
}

float PitchShifter::process (float x, float ratio)
{
    d.push (x);
    phase += (1.0f - ratio) / win;
    phase -= std::floor (phase);
    const float p2 = wrap01 (phase + 0.5f);
    const float g1 = 0.5f - 0.5f * std::cos (kTwoPi * phase), g2 = 0.5f - 0.5f * std::cos (kTwoPi * p2);
    return d.read (phase * win + 2.0f) * g1 + d.read (p2 * win + 2.0f) * g2;
}
}
