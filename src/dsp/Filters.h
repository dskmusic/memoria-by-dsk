#pragma once
#include "Common.h"

namespace gx
{
// Zero-delay-feedback state variable filter (Simper/TPT).
struct Svf
{
    float ic1 = 0, ic2 = 0, k = 2, a1 = 1, a2 = 0, a3 = 0;
    void set (float g, float kk) { k = kk; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    void reset() { ic1 = ic2 = 0; }
    // returns lp; bp/hp via refs
    inline float tick (float v0, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        bp = v1;
        hp = v0 - k * v1 - v2;
        return v2;
    }
};

// RBJ biquad (transposed direct form II) for EQ / tone shaping.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0); }
    void peak (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void shelf (double sr, double f, double db, bool high)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        if (high) setNorm (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq), (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
        else setNorm (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq), (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void lowpass (double sr, double f, double q = 0.7071)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (f, sr * 0.45) / sr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNorm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double sr, double f, double q = 0.7071)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (f, sr * 0.45) / sr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNorm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};
}
