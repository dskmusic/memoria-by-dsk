#pragma once
#include "Widgets.h"
#include <juce_dsp/juce_dsp.h>

namespace gx::ui
{
// Data gathered once per UI frame by the editor and handed to every animated component.
struct Frame
{
    static constexpr int kScope = 2048, kBands = 96;
    std::array<float, kScope> scope {};      // most recent mono output samples
    std::array<float, kBands> spectrum {};   // 0..1 log-frequency bands
    float peakL = 0, peakR = 0, rms = 0;
    double time = 0;                         // seconds since editor open
    float energy = 0;                        // smoothed rms, drives "aliveness"
    // engine state
    std::array<float, 128> keyLevel {}, keyHit {};
    float pedal = 0;
    int voices = 0, hitCounter = 0;
    float memory = 0, timeAge = 0.5f;
    int worldType = 0;
    bool reduced = false;                    // reduced animation mode
};

struct Animated
{
    virtual ~Animated() = default;
    virtual void tick (const Frame&) = 0;
};

class FrameSource // owned by the editor
{
public:
    explicit FrameSource (MemoriaProcessor& p) : proc (p) {}
    void update (Frame& f, double dt);
private:
    MemoriaProcessor& proc;
    std::vector<float> ring = std::vector<float> (8192, 0.0f);
    int ringPos = 0;
    juce::dsp::FFT fft { 11 };
    std::vector<float> fftBuf = std::vector<float> (4096, 0.0f);
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
};

class Scope : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
    void resized() override { trail = juce::Image(); }
    juce::String label = L ("OSCILOSCOPIO", "OSCILLOSCOPE");
private:
    juce::Image trail;
    std::array<float, 512> trace {};
    float energy = 0; double t = 0;
    float dispPeak = 0.0f, levelDb = -90.0f;
};

class Spectrum : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
private:
    std::array<float, Frame::kBands> bands {}, peaks {};
};

class VuMeter : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
private:
    float l = 0, r = 0, hl = 0, hr = 0;
};
}
