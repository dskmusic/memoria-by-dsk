#pragma once
#include "Preset.h"
#include "../dsp/Engine.h"

namespace gx
{
// Renders a preset with a private engine from a fresh start (internal Play on, like pressing Play).
// Deterministic for any block size. Safe from any thread. 'cancel' (optional) aborts and returns false.
bool renderPreset (const Preset& preset, juce::AudioBuffer<float>& out, double sampleRate, int blockSize = 512,
                   const std::atomic<bool>* cancel = nullptr);

// Long-form export (YouTube / meditation): streams straight to disk, block by block.
// format: 0 = WAV 24-bit, 1 = FLAC 24-bit, 2 = OGG Vorbis (high quality). Fades out at the end.
enum ExportFormat { efWav, efFlac, efOgg };
juce::String exportFormatExtension (int format);
// Returns an empty string on success, otherwise the error (or "cancelled").
juce::String exportAudio (const Preset& preset, const juce::File& file, int format, double seconds, double sampleRate,
                          std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);
}
