#pragma once
#include "Preset.h"
#include "../dsp/Piano.h"

namespace gx
{
// Renders a preset with a private engine from a fresh start, playing the given MIDI (sample positions relative to the start of 'out').
// Deterministic for any block size. Safe from any thread. 'cancel' (optional) aborts and returns false.
bool renderMidi (const Preset& preset, const juce::MidiBuffer& midi, juce::AudioBuffer<float>& out, double sampleRate, int blockSize = 512,
                 const std::atomic<bool>* cancel = nullptr, bool playHands = false);

// Long audio export (WAV 24-bit / FLAC 24-bit / OGG): the hands play for the whole length, then the room tail fades.
// Returns an empty string on success, otherwise the error (or "cancelled").
enum ExportFormat { efWav, efFlac, efOgg };
juce::String exportFormatExtension (int format);
juce::String exportAudio (const Preset& preset, const juce::File& file, int format, double seconds, double sampleRate,
                          std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);
}
