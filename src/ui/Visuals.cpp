#include "Visuals.h"

namespace gx::ui
{
// ---------------------------------------------------------------- Scope
void Scope::tick (const Frame& f)
{
    t = f.time;
    energy = f.energy;
    // trigger on a rising zero crossing for a stable picture
    int start = Frame::kScope - 1024;
    for (int i = Frame::kScope - 1024; i > 600; --i)
        if (f.scope[(size_t) i - 1] < 0 && f.scope[(size_t) i] >= 0) { start = i - 400; break; }
    // responsive auto-gain: instant on louder signals, slow release (~1.5 s) so dynamics and decays stay visible
    float peak = 0.0f;
    for (int i = 0; i < 512; ++i) peak = juce::jmax (peak, std::abs (f.scope[(size_t) (start + i)]));
    if (peak > dispPeak) dispPeak = peak;
    else dispPeak += (peak - dispPeak) * 0.012f;
    const float g = juce::jlimit (1.0f, 4.0f, 0.85f / juce::jmax (dispPeak, 0.02f));
    levelDb += (juce::Decibels::gainToDecibels (f.rms, -90.0f) - levelDb) * 0.25f;
    for (int i = 0; i < 512; ++i)
    {
        float v = juce::jlimit (-1.1f, 1.1f, f.scope[(size_t) (start + i)] * g);
        if (f.rms < 1.0e-4f) // idle: slow organic drift
            v = 0.08f * std::sin ((float) i * 0.045f + (float) t * 1.7f) * std::sin ((float) t * 0.37f + (float) i * 0.011f);
        trace[(size_t) i] = v;
    }
    repaint();
}

void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r);
    if (trail.isNull() || trail.getWidth() != getWidth())
        trail = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    {
        juce::Graphics tg (trail);
        trail.multiplyAllAlphas (0.62f); // phosphor decay
        juce::Path p;
        const float w = r.getWidth() - 8, h = r.getHeight() * 0.42f, cy = r.getCentreY();
        for (int i = 0; i < 512; ++i)
        {
            const float x = 4 + w * (float) i / 511.0f, y = cy - trace[(size_t) i] * h;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        tg.setColour (col::phosphor.withAlpha (0.12f));
        tg.strokePath (p, juce::PathStrokeType (4.0f));
        tg.setColour (col::phosphor.withAlpha (0.9f));
        tg.strokePath (p, juce::PathStrokeType (1.6f));
    }
    g.drawImageAt (trail, 0, 0);
    g.setColour (col::phosphor.withAlpha (0.55f));
    g.setFont (monoFont (10.0f));
    g.drawText (label, r.reduced (8, 4), juce::Justification::topLeft);
    g.drawText (levelDb <= -89.0f ? juce::String ("-inf dB") : juce::String (levelDb, 1) + " dB", r.reduced (8, 4), juce::Justification::topRight);
}

// ---------------------------------------------------------------- Spectrum
void Spectrum::tick (const Frame& f)
{
    for (int i = 0; i < Frame::kBands; ++i)
    {
        const float v = f.spectrum[(size_t) i];
        bands[(size_t) i] = v > bands[(size_t) i] ? v : bands[(size_t) i] * 0.86f + v * 0.14f;
        peaks[(size_t) i] = juce::jmax (bands[(size_t) i], peaks[(size_t) i] - 0.006f);
    }
    repaint();
}

void Spectrum::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r);
    auto in = r.reduced (6, 8);
    const float bw = in.getWidth() / Frame::kBands;
    for (int i = 0; i < Frame::kBands; ++i)
    {
        const float h = bands[(size_t) i] * in.getHeight();
        const auto c = col::accent2.interpolatedWith (col::accent, (float) i / Frame::kBands);
        g.setColour (c.withAlpha (0.85f));
        g.fillRect (in.getX() + i * bw + 0.5f, in.getBottom() - h, juce::jmax (1.0f, bw - 1.2f), h);
        g.setColour (c.brighter (0.6f));
        g.fillRect (in.getX() + i * bw + 0.5f, in.getBottom() - peaks[(size_t) i] * in.getHeight() - 1.5f, juce::jmax (1.0f, bw - 1.2f), 1.5f);
    }
    g.setColour (col::phosphor.withAlpha (0.55f));
    g.setFont (monoFont (10.0f));
    g.drawText (L ("ESPECTRO", "SPECTRUM"), r.reduced (8, 4), juce::Justification::topLeft);
}

// ---------------------------------------------------------------- VuMeter
void VuMeter::tick (const Frame& f)
{
    l = juce::jmax (f.peakL, l * 0.85f); r = juce::jmax (f.peakR, r * 0.85f);
    hl = juce::jmax (l, hl - 0.01f); hr = juce::jmax (r, hr - 0.01f);
    repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawScreen (g, b);
    auto in = b.reduced (5, 8);
    auto bar = [&] (juce::Rectangle<float> rr, float v, float hold)
    {
        const float db = juce::Decibels::gainToDecibels (v, -60.0f), hdb = juce::Decibels::gainToDecibels (hold, -60.0f);
        const int segs = 24;
        for (int s = 0; s < segs; ++s)
        {
            const float th = -60.0f + 63.0f * (float) s / segs;
            const auto c = th > -3 ? col::danger : th > -12 ? col::accent : col::accent2;
            const float y = rr.getBottom() - (s + 1) * rr.getHeight() / segs;
            g.setColour (db > th ? c : c.withAlpha (0.12f));
            g.fillRect (rr.getX(), y + 1, rr.getWidth(), rr.getHeight() / segs - 2);
        }
        const float hy = rr.getBottom() - juce::jlimit (0.0f, 1.0f, (hdb + 60.0f) / 63.0f) * rr.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (rr.getX(), hy, rr.getWidth(), 1.5f);
    };
    auto lr = in.removeFromLeft (in.getWidth() * 0.5f).reduced (2, 0);
    bar (lr, l, hl);
    bar (in.reduced (2, 0), r, hr);
}
}
