#include "Genome.h"

namespace gx
{
static float gauss (float x, float c, float w) { const float d = (x - c) / w; return std::exp (-d * d); }

const std::array<std::array<float, kGenomeHarm>, kNumArchetypes>& genomeArchetypes()
{
    static const auto a = []
    {
        std::array<std::array<float, kGenomeHarm>, kNumArchetypes> g {};
        for (int k = 1; k < kGenomeHarm; ++k)
        {
            const float fk = (float) k;
            g[0][(size_t) k] = 1.0f / fk;                                               // saw
            g[1][(size_t) k] = (k % 2) ? 1.0f / fk : 0.0f;                              // square
            g[2][(size_t) k] = (k % 2) ? 1.0f / (fk * fk) : 0.0f;                       // triangle-ish
            g[3][(size_t) k] = (gauss (fk * 120, 730, 120) + 0.6f * gauss (fk * 120, 1090, 160) + 0.05f) / std::sqrt (fk); // "ah"
            g[4][(size_t) k] = (gauss (fk * 120, 570, 100) + 0.5f * gauss (fk * 120, 840, 130) + 0.03f) / std::sqrt (fk);  // "oh"
            g[5][(size_t) k] = (k == 1 || k == 2 || k == 3 || k == 4 || k == 8) ? 0.8f / std::sqrt (fk) : 0.0f;           // organ
            g[6][(size_t) k] = (k % 2 ? 0.25f : 1.0f) / fk;                             // hollow (even heavy)
            g[7][(size_t) k] = std::pow (fk, -0.45f) * (0.4f + 0.6f * std::abs (std::cos (fk * 0.37f))); // bright comb
        }
        return g;
    }();
    return a;
}
} // namespace gx
