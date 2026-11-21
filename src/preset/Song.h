#pragma once
#include "Preset.h"
#include "../dsp/Composer.h"

namespace gx
{
// The song the Hands play for a preset (from its Hands parameters). Identical to what the engine plays at the start of a run.
ComposeCfg composeCfgFromPreset (const Preset& p);
// Standard MIDI file: tempo, time signature, notes and sustain pedal (CC64).
bool writeMidi (const Score& s, double bpm, const juce::File& file);
}
