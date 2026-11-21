#include "Render.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace gx
{
bool renderMidi (const Preset& preset, const juce::MidiBuffer& midi, juce::AudioBuffer<float>& out, double sr, int bs, const std::atomic<bool>* cancel, bool playHands)
{
    auto eng = std::make_unique<Engine>();
    eng->prepare (sr, bs);
    eng->reset ((uint32_t) preset.get (P::seed));
    HostInfo hi;
    hi.internalPlay = playHands;
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

juce::String exportFormatExtension (int f) { return f == efFlac ? ".flac" : f == efOgg ? ".ogg" : ".wav"; }

juce::String exportAudio (const Preset& preset, const juce::File& file, int format, double seconds, double sr, std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    std::unique_ptr<juce::AudioFormat> fmt;
    if (format == efFlac) fmt = std::make_unique<juce::FlacAudioFormat>();
    else if (format == efOgg) fmt = std::make_unique<juce::OggVorbisAudioFormat>();
    else fmt = std::make_unique<juce::WavAudioFormat>();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) return "cannot write " + file.getFullPathName();
    int quality = 0;
    if (format == efOgg) quality = juce::jmax (0, fmt->getQualityOptions().size() - 2);
    auto writer = fmt->createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (format == efOgg ? 16 : 24).withQualityOptionIndex (quality));
    if (writer == nullptr) return "this format is not available at " + juce::String ((int) sr) + " Hz";
    Engine eng;
    constexpr int bs = 1024;
    eng.prepare (sr, bs);
    eng.reset ((uint32_t) preset.get (P::seed));
    HostInfo hi;
    const double stopAt = juce::jmax (0.0, seconds - 8.0);
    juce::MidiBuffer none;
    juce::AudioBuffer<float> buf (2, bs);
    const int64_t total = (int64_t) (seconds * sr);
    for (int64_t pos = 0; pos < total; pos += bs)
    {
        if (cancel != nullptr && cancel->load()) { writer.reset(); file.deleteFile(); return "cancelled"; }
        const int n = (int) juce::jmin ((int64_t) bs, total - pos);
        hi.internalPlay = (double) pos / sr < stopAt;
        eng.process (buf.getWritePointer (0), buf.getWritePointer (1), n, none, preset.params.data(), hi);
        const int64_t endFade = (int64_t) (0.05 * sr);
        for (int i = 0; i < n; ++i)
        {
            const int64_t left = total - (pos + i);
            if (left < endFade) { const float g = (float) left / (float) endFade; buf.getWritePointer (0)[i] *= g; buf.getWritePointer (1)[i] *= g; }
        }
        if (! writer->writeFromAudioSampleBuffer (buf, 0, n)) { writer.reset(); file.deleteFile(); return "disk write failed"; }
        if (progress != nullptr) progress->store ((float) ((double) (pos + n) / (double) total));
    }
    writer.reset();
    return {};
}
}
