#pragma once
#include "Common.h"

namespace gx
{
// Harmonic spectra used by the Spectral Mutation drone (archetype "genomes", from Genesix).
constexpr int kGenomeHarm = 96, kNumArchetypes = 8;
const std::array<std::array<float, kGenomeHarm>, kNumArchetypes>& genomeArchetypes();
}
