#include "Hands.h"
#include <algorithm>

namespace gx
{
namespace
{
const std::vector<std::vector<int>>& scales()
{
    static const std::vector<std::vector<int>> s {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 6, 7, 9, 11 },
        { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 } };
    return s;
}
inline uint32_t mix (uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
const int kProg[8][4] { { 0, 5, 3, 4 }, { 0, 3, 4, 0 }, { 0, 2, 5, 4 }, { 5, 3, 0, 4 }, { 0, 4, 5, 3 }, { 0, 5, 2, 4 }, { 3, 0, 4, 5 }, { 0, 3, 5, 3 } };
// melodic rhythm templates on an eighth-note grid (8 per bar)
const int kRhythm[6][5] { { 0, 3, 4, 6, -1 }, { 0, 2, 4, 7, -1 }, { 0, 4, -1, -1, -1 }, { 0, 3, 6, -1, -1 }, { 0, 2, 3, 4, 6 }, { 0, 1, 4, 6, -1 } };
}

float Hands::hf (uint32_t salt, int64_t s) const { return (float) (mix (cfg.seed * 31u + salt, (uint32_t) s) >> 8) * (1.0f / 16777216.0f); }

void Hands::reset (double startBeat)
{
    last = startBeat; started = false; offs.clear(); melDeg = 4; lastRoot = 0; voicingCentre = 62; voicing.clear();
    pedalState = 0; pedalUpAt = -1;
}

int Hands::degreeNote (int degree, int octaveBase) const
{
    const auto& sc = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, cfg.scale)];
    const int n = (int) sc.size();
    const int oct = (degree >= 0 ? degree / n : -((-degree + n - 1) / n)), idx = ((degree % n) + n) % n;
    return octaveBase + cfg.key + sc[(size_t) idx] + 12 * oct;
}

void Hands::play (std::vector<Ev>& out, int note, int vel, double len)
{
    note = juce::jlimit (21, 108, note);
    out.push_back ({ note, juce::jlimit (1, 127, vel), true });
    offs.push_back ({ last + len, note });
}

void Hands::allOff (std::vector<Ev>& out)
{
    for (auto& o : offs) out.push_back ({ o.note, 0, false });
    offs.clear();
}

void Hands::step (int64_t s, std::vector<Ev>& out)
{
    const int spb = 1 << cfg.rate, stepsPerBar = 4 * spb;
    const int64_t bar = s / stepsPerBar;
    const int sib = (int) (s % stepsPerBar);
    const int phrase = (int) (bar / 4), barInPhrase = (int) (bar % 4);
    const int prog = (int) (mix (cfg.seed, (uint32_t) phrase) % 8);
    const int deg = kProg[prog][barInPhrase];
    const float vscale = 0.55f + 0.5f * (1.0f - cfg.softness);
    auto vel = [&] (float base, uint32_t salt) { return (int) ((base + 12.0f * (hf (salt, s) - 0.5f)) * vscale * 1.5f); };
    const double stepLen = 1.0 / spb;
    const int n7 = (int) scales()[(size_t) juce::jlimit (0, 7, cfg.scale)].size();

    if (sib == 0)
    {
        lastRoot = ((degreeNote (deg, 0) % 12) + 12) % 12;
        // voicing: chord tones nearest to the previous centre (voice leading)
        voicing.clear();
        const int tones = n7 >= 7 && hf (1, bar) > 0.5f ? 4 : 3;
        std::vector<int> pcs;
        for (int k = 0; k < tones; ++k) pcs.push_back (degreeNote (deg + 2 * k, 0));
        for (int pc : pcs)
        {
            int best = 60, bd = 1000;
            for (int o = 36; o <= 84; ++o) if (((o - pc) % 12 + 12) % 12 == 0 && std::abs (o - voicingCentre) < bd && o >= 52 && o <= 76) { bd = std::abs (o - voicingCentre); best = o; }
            voicing.push_back (best);
        }
        std::sort (voicing.begin(), voicing.end());
        voicing.erase (std::unique (voicing.begin(), voicing.end()), voicing.end());
        int sum = 0; for (int v : voicing) sum += v;
        voicingCentre = voicing.empty() ? 62 : sum / (int) voicing.size();
        // bass note and pedal
        if (cfg.mode != hmOff)
        {
            const int bass = degreeNote (deg, 36);
            play (out, bass < 36 ? bass + 12 : bass > 47 ? bass - 12 : bass, vel (58, 2), 3.7);
            if (cfg.pedal && cfg.mode != hmArp) pedalState = 2;       // 2 = lift needed now (re-pedal at the chord change)
        }
        if (cfg.mode == hmChords || cfg.mode == hmPhrases)
        {
            const bool strumLate = hf (3, s) > 0.5f;
            int i = 0;
            for (int v : voicing) play (out, v, vel (50 + 3 * i++, 4 + (uint32_t) i), strumLate ? 3.2 : 3.7);
        }
    }
    if (cfg.mode == hmChords && cfg.density > 0.5f && sib == stepsPerBar / 2 && hf (6, s) < cfg.density)
        for (size_t i = 1; i < voicing.size(); ++i) play (out, voicing[i], vel (42, 7 + (uint32_t) i), 1.6);

    if (cfg.mode == hmArp || cfg.mode == hmFull)
    {
        if (hf (8, s) < 0.55f + 0.45f * cfg.density && ! voicing.empty())
        {
            static const int pat[4][8] { { 0, 1, 2, 1, 0, 1, 2, 3 }, { 0, 2, 1, 2, 0, 2, 1, 3 }, { 0, 1, 2, 3, 2, 1, 2, 1 }, { 0, 2, 3, 2, 1, 2, 3, 2 } };
            const int p = pat[mix (cfg.seed, (uint32_t) phrase + 99u) % 4][(size_t) (s % 8)];
            const int note = voicing[(size_t) (p % (int) voicing.size())] + ((sib % 4 == 3 && cfg.density > 0.4f) ? 12 : 0);
            play (out, note, vel (sib % spb == 0 ? 56.0f : 44.0f, 9), stepLen * 2.2);
        }
        if (cfg.mode == hmArp && cfg.pedal && sib == 0) pedalState = 2;
    }

    if (cfg.mode == hmPhrases || cfg.mode == hmFull)
    {
        // melody: motif in bar 0 and 1, answer in bar 2, cadence on the root in bar 3
        const int eighth = sib * 8 / stepsPerBar;
        if (sib * 8 % stepsPerBar == 0)
        {
            const int tpl = (int) (mix (cfg.seed + 5u, (uint32_t) phrase * 4u + (barInPhrase == 1 ? 0u : (uint32_t) barInPhrase)) % 6);
            bool hitNote = false;
            for (int k = 0; k < 5; ++k) if (kRhythm[tpl][k] == eighth) hitNote = true;
            if (barInPhrase == 3) hitNote = eighth == 0 || eighth == 4 || (eighth == 6 && cfg.density > 0.5f);
            if (hitNote && hf (11, s) < 0.35f + 0.65f * cfg.density)
            {
                const int target = deg + (barInPhrase == 3 ? 0 : 2 * (int) (hf (12, s) * 2.0f));
                const float r = hf (13, s);
                if (barInPhrase == 3 && eighth == 0) melDeg = melDeg + (target - melDeg) / 2;
                else melDeg += r < 0.3f ? -1 : r < 0.6f ? 1 : r < 0.75f ? 2 : r < 0.9f ? -2 : 0;
                if (melDeg > 12) melDeg -= 3; if (melDeg < 2) melDeg += 3;
                if (barInPhrase == 3 && eighth == 4) melDeg = deg + 7;
                const double len = barInPhrase == 3 && eighth >= 4 ? 3.0 : (eighth % 2 ? 0.7 : 1.1);
                const int note = degreeNote (melDeg, 48);
                play (out, note < 66 ? note + 12 : note, vel (barInPhrase == 3 ? 62 : 68, 14), len);
            }
        }
    }
}

void Hands::advance (double beatEnd, std::vector<Ev>& out, bool& pedalDown, bool& pedalChanged)
{
    pedalChanged = false;
    const int spb = 1 << cfg.rate;
    if (! started) { started = true; last = beatEnd - 1.0e-9; }
    // note-offs that are due
    for (size_t i = 0; i < offs.size();)
        if (offs[i].beat <= beatEnd) { out.push_back ({ offs[i].note, 0, false }); offs[i] = offs.back(); offs.pop_back(); }
        else ++i;
    const int64_t s0 = (int64_t) std::ceil (last * spb - 1.0e-9), s1 = (int64_t) std::ceil (beatEnd * spb - 1.0e-9);
    for (int64_t s = s0; s < s1; ++s)
    {
        const double at = (double) s / spb;
        const double keep = last; last = at;
        if (cfg.mode != hmOff) step (s, out);
        last = keep;
        if (pedalState == 2)
        {
            // lift the pedal just before the chord change, press it again right after
            pedalState = 1; pedalDown = false; pedalChanged = true; pedalUpAt = at + 0.06;
        }
    }
    if (pedalUpAt >= 0 && beatEnd >= pedalUpAt) { pedalDown = true; pedalChanged = true; pedalUpAt = -1; }
    if (cfg.mode == hmOff && pedalState != 0) { pedalState = 0; pedalDown = false; pedalChanged = true; }
    last = beatEnd;
}
}
