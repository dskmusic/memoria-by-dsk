#pragma once
#include "Preset.h"
#include "../dsp/Piano.h"

namespace gx
{
// Renders a preset with a private engine from a fresh start, playing the given MIDI (sample positions relative to the start of 'out').
// Deterministic for any block size. Safe from any thread. 'cancel' (optional) aborts and returns false.
bool renderMidi (const Preset& preset, const juce::MidiBuffer& midi, juce::AudioBuffer<float>& out, double sampleRate, int blockSize = 512,
                 const std::atomic<bool>* cancel = nullptr);
}
