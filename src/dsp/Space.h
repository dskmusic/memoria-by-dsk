#pragma once
#include "Common.h"
#include <vector>

// Delay line, FDN reverb and granular pitch shifter (ported from Genesix).
namespace gx
{
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init (int minSize) { int s = 1; while (s < minSize) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float d) const // d samples behind the last pushed
    {
        float r = (float) w - 1.0f - d;
        while (r < 0) r += (float) (mask + 1);
        const int i = (int) r;
        const float t = r - (float) i;
        return buf[(size_t) (i & mask)] * (1 - t) + buf[(size_t) ((i + 1) & mask)] * t;
    }
};

struct Fdn // 8-line feedback delay network reverb
{
    std::array<DelayLine, 8> line;
    std::array<float, 8> len {}, lenNow {}, lp {}, g {};
    std::array<DelayLine, 6> ap;
    std::array<float, 6> apLen {};
    DelayLine pre;
    float preLen = 0, preNow = 0;
    std::array<float, 6> apNow {};
    bool primed = false;
    float damp = 0.3f, modPh = 0;
    double sr = 48000;
    void prepare (double s);
    void clear();
    void set (float size, float dampAmt, float rt60Scale = 1.0f);
    void process (float inL, float inR, float& outL, float& outR);
};

struct PitchShifter
{
    DelayLine d;
    float phase = 0, win = 2400;
    void prepare (double sr) { d.init ((int) (sr * 0.2)); win = (float) (sr * 0.06); }
    void clear() { d.clear(); phase = 0; }
    float process (float x, float ratio);
};
}
