#include "PluginProcessor.h"
#include "PluginEditor.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define GX_HAS_BINARYDATA 1
#endif

MemoriaProcessor::MemoriaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MEMORIA", gx::createLayout())
{
    for (auto& d : gx::paramDefs())
    {
        raw.push_back (apvts.getRawParameterValue (d.id));
        paramPtrs.push_back (apvts.getParameter (d.id));
    }
    clearMidiLearn();
    snapshot.assign (raw.size(), 0.0f);
#if GX_HAS_BINARYDATA
    // every new instance starts with a random factory preset (and its landscape); a saved project / state replaces it
    int size = 0;
    if (auto* data = BinaryData::getNamedResource ("factory_memoriabank", size))
    {
        const juce::var bank = juce::JSON::parse (juce::String::fromUTF8 (data, size));   // keep it alive while we read it
        if (auto* arr = bank.getProperty ("presets", {}).getArray(); arr != nullptr && ! arr->isEmpty())
        {
            auto p = gx::Preset::fromVar ((*arr)[juce::Random::getSystemRandom().nextInt (arr->size())]);
            for (int i = 0; i < gx::P::numParams; ++i)
                if (auto* prm = paramPtrs[(size_t) i]) prm->setValueNotifyingHost (prm->convertTo0to1 (p.params[(size_t) i]));
            currentMeta = p.meta;
        }
    }
#endif
}

MemoriaProcessor::~MemoriaProcessor() = default;

void MemoriaProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    engine.prepare (sampleRate, juce::jmax (maxBlock, 16));
    keyboardState.reset();
}

bool MemoriaProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MemoriaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isController()) continue;
        const int cc = m.getControllerNumber();
        if (cc == 64 || cc >= 120) continue; // reserved controllers
        if (const int lp = learnParam.exchange (-1); lp >= 0)
        {
            for (auto& c : ccMap) if (c.load() == lp) c.store (-1);
            ccMap[(size_t) cc].store (lp);
        }
        if (const int pi = ccMap[(size_t) cc].load(); pi >= 0)
            paramPtrs[(size_t) pi]->setValueNotifyingHost ((float) m.getControllerValue() / 127.0f);
    }
    if (switchRequested.exchange (false))
        duck = lastPeak > 1.0e-4f ? Duck::Out : Duck::In;   // silent? switch at once
    if (previewRequested.load() && duck != Duck::Out)
    {
        previewRequested.store (false);
        for (int n : { 57, 64, 69, 72 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 72), 0);
        previewOffLeft = (int) (1.4 * engine.getSampleRate());
    }
    else if (previewOffLeft > 0 && (previewOffLeft -= buffer.getNumSamples()) <= 0)
        for (int n : { 57, 64, 69, 72 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
    if (duck != Duck::Out)                                  // while dipping, keep playing the old sound
        for (size_t i = 0; i < raw.size(); ++i) snapshot[i] = raw[i]->load (std::memory_order_relaxed);

    gx::HostInfo hi;
    hi.internalPlay = true;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hi.hostPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSamples()) { hi.hasPosition = true; hi.hostSample = *t; }
            if (auto q = pos->getPpqPosition()) { hi.hasPpq = true; hi.ppq = *q; }
            if (auto b = pos->getBpm()) hi.bpm = *b;
        }
    hostPlaying.store (hi.hostPlaying);

    const int n = buffer.getNumSamples();
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, midi, snapshot.data(), hi);
    if (duck != Duck::None)
    {
        const float sr = (float) engine.getSampleRate(); // the engine always knows it (getSampleRate() can be 0 outside a host)
        const float step = duck == Duck::Out ? 1.0f / (0.06f * sr) : 1.0f / (0.2f * sr);
        for (int i = 0; i < n; ++i)
        {
            duckGain = duck == Duck::Out ? juce::jmax (0.0f, duckGain - step) : juce::jmin (1.0f, duckGain + step);
            const float g = duckGain * duckGain * (3.0f - 2.0f * duckGain);
            buffer.getWritePointer (0)[i] *= g;
            buffer.getWritePointer (1)[i] *= g;
        }
        if (duck == Duck::Out && duckGain <= 0.0f) duck = Duck::In;      // next block takes the new parameters
        else if (duck == Duck::In && duckGain >= 1.0f) duck = Duck::None;
    }

    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getReadPointer (1);
    float pl = 0, pr = 0, sq = 0;
    for (int i = 0; i < n; ++i) { pl = juce::jmax (pl, std::abs (L[i])); pr = juce::jmax (pr, std::abs (R[i])); sq += L[i] * L[i] + R[i] * R[i]; }
    peakL.store (juce::jmax (pl, peakL.load() * 0.9f));
    peakR.store (juce::jmax (pr, peakR.load() * 0.9f));
    rms.store (std::sqrt (sq / (float) juce::jmax (1, n * 2)));
    lastPeak = juce::jmax (pl, pr);
    pushScope (L, R, n);
}

void MemoriaProcessor::pushScope (const float* L, const float* R, int n)
{
    const auto scope = scopeFifo.write (juce::jmin (n, scopeFifo.getFreeSpace()));
    for (int i = 0; i < scope.blockSize1; ++i) scopeBuf[(size_t) (scope.startIndex1 + i)] = 0.5f * (L[i] + R[i]);
    for (int i = 0; i < scope.blockSize2; ++i) scopeBuf[(size_t) (scope.startIndex2 + i)] = 0.5f * (L[scope.blockSize1 + i] + R[scope.blockSize1 + i]);
}

int MemoriaProcessor::readScope (float* dest, int maxSamples)
{
    const auto scope = scopeFifo.read (juce::jmin (maxSamples, scopeFifo.getNumReady()));
    for (int i = 0; i < scope.blockSize1; ++i) dest[i] = scopeBuf[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[scope.blockSize1 + i] = scopeBuf[(size_t) (scope.startIndex2 + i)];
    return scope.blockSize1 + scope.blockSize2;
}

void MemoriaProcessor::setParamPlain (int index, float plain)
{
    if (auto* prm = paramPtrs[(size_t) index])
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

juce::AudioProcessorEditor* MemoriaProcessor::createEditor() { return new MemoriaEditor (*this); }

void MemoriaProcessor::getStateInformation (juce::MemoryBlock& mb)
{
    auto v = gx::Preset::capture (*this).toVar();
    juce::Array<juce::var> map;
    for (int cc = 0; cc < 128; ++cc)
        if (const int pi = ccMap[(size_t) cc].load(); pi >= 0) map.add (juce::Array<juce::var> { cc, gx::paramDefs()[(size_t) pi].id });
    v.getDynamicObject()->setProperty ("midiLearn", map);
    juce::Array<juce::var> mem;
    for (auto& m : memories) mem.add (m);
    v.getDynamicObject()->setProperty ("memories", mem);
    const auto json = juce::JSON::toString (v, true);
    mb.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void MemoriaProcessor::setStateInformation (const void* data, int size)
{
    // the Standalone opens with a fresh random preset each time instead of the last session
    if (wrapperType == juce::AudioProcessor::wrapperType_Standalone) return;   // presets, A/B and undo never go through here
    gx::Preset p;
    const auto text = juce::String::fromUTF8 (static_cast<const char*> (data), size);
    if (gx::Preset::fromJson (text, p))
    {
        clearMidiLearn();
        if (auto* map = juce::JSON::parse (text).getProperty ("midiLearn", {}).getArray())
            for (auto& e : *map)
                if (auto* pair = e.getArray(); pair != nullptr && pair->size() == 2)
                    if (const int pi = gx::paramIndex ((*pair)[1].toString()); pi >= 0) ccMap[(size_t) juce::jlimit (0, 127, (int) (*pair)[0])].store (pi);
        if (auto* mem = juce::JSON::parse (text).getProperty ("memories", {}).getArray())
            for (int i = 0; i < juce::jmin (8, mem->size()); ++i) memories[(size_t) i] = (*mem)[i];
        p.applyTo (*this);
        presetGeneration.fetch_add (1);
    }
}
