#pragma once
#include "PluginProcessor.h"
#include "preset/Render.h"
#include <functional>

namespace gxtest
{
struct Stats { float peak = 0, rms = 0, dc = 0; bool finite = true; };
Stats analyse (const juce::AudioBuffer<float>& b, int start = 0, int len = -1);
void check (bool ok, const juce::String& what);
void setParam (MemoriaProcessor& p, const juce::String& id, float plain);
void setParam (gx::Preset& p, const juce::String& id, float plain);

// tiny MIDI score: times in seconds
struct Song
{
    juce::MidiBuffer buf;
    double sr = 48000.0;
    std::vector<double> onsets;
    void note (double t, double dur, int n, int vel)
    {
        buf.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) vel), (int) (t * sr));
        buf.addEvent (juce::MidiMessage::noteOff (1, n), (int) ((t + dur) * sr));
        onsets.push_back (t);
    }
    void pedal (double t, int v) { buf.addEvent (juce::MidiMessage::controllerEvent (1, 64, v), (int) (t * sr)); }
};
juce::AudioBuffer<float> render (const gx::Preset& p, const Song& s, double seconds, double sr = 48000.0, int blockSize = 512);
float rmsDb (const juce::AudioBuffer<float>& b, double t0, double t1, double sr = 48000.0);
float centroidHz (const juce::AudioBuffer<float>& b, double t0, double sr = 48000.0);
int countClicks (const juce::AudioBuffer<float>& b, const std::vector<double>& onsets, double sr = 48000.0);
void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f, double sr = 48000.0);
std::vector<std::function<void()>>& extraTests();
int runAll();
}
