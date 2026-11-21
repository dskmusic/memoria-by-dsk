#include "Render.h"

namespace gx
{
bool renderMidi (const Preset& preset, const juce::MidiBuffer& midi, juce::AudioBuffer<float>& out, double sr, int bs, const std::atomic<bool>* cancel)
{
    auto eng = std::make_unique<Engine>();
    eng->prepare (sr, bs);
    eng->reset ((uint32_t) preset.get (P::seed));
    HostInfo hi;
    hi.internalPlay = true;
    const int total = out.getNumSamples();
    juce::MidiBuffer block;
    for (int pos = 0; pos < total; pos += bs)
    {
        if (cancel != nullptr && cancel->load()) return false;
        const int n = juce::jmin (bs, total - pos);
        block.clear();
        block.addEvents (midi, pos, n, -pos);
        eng->process (out.getWritePointer (0, pos), out.getWritePointer (1, pos), n, block, preset.params.data(), hi);
    }
    return true;
}
}
