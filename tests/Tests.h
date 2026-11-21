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
juce::AudioBuffer<float> render (const gx::Preset& p, double seconds, double sr = 48000.0, int blockSize = 512);
std::vector<std::function<void()>>& extraTests();
int runAll();
}
