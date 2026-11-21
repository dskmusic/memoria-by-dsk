#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Piano.h"
#include "preset/Preset.h"

class MemoriaProcessor : public juce::AudioProcessor
{
public:
    MemoriaProcessor();
    ~MemoriaProcessor() override;

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Memoria by DSK"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API
    juce::AudioProcessorValueTreeState apvts;
    float getParam (int index) const { return raw[(size_t) index]->load (std::memory_order_relaxed); }
    void setParamPlain (int index, float plainValue); // notifies host
    gx::Telemetry& telemetry() { return engine.tele; }
    gx::PresetMeta currentMeta;               // metadata of the loaded preset
    std::atomic<int> presetGeneration { 0 };  // bumped whenever a full state/preset is applied (UI polls)
    std::array<juce::var, 8> memories;         // quick memory slots (preset vars), saved with the project (UI thread)

    // A whole new sound is about to be applied: the audio dips (60 ms), switches, and comes back (200 ms) - no clicks.
    void beginPresetSwitch() { switchRequested.store (true); }
    void setPlaying (bool b) { playRequested.store (b); }
    bool isPlayingInternal() const { return playRequested.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    // plays a short test chord once the (click-free) preset switch has finished
    void previewChord() { previewRequested.store (true); }

    // Scope / meter feed (lock-free SPSC)
    int readScope (float* dest, int maxSamples);
    std::atomic<float> peakL { 0 }, peakR { 0 }, rms { 0 };
    // MIDI learn: CC -> parameter index
    std::atomic<int> learnParam { -1 };
    std::array<std::atomic<int>, 128> ccMap;
    void clearMidiLearn() { for (auto& c : ccMap) c.store (-1); }
    // UI-injected MIDI (on-screen keyboard)
    juce::MidiKeyboardState keyboardState;

private:
    void pushScope (const float* L, const float* R, int n);

    gx::Engine engine;
    std::vector<std::atomic<float>*> raw;
    std::vector<juce::RangedAudioParameter*> paramPtrs;
    std::vector<float> snapshot;
    std::atomic<bool> playRequested { false }, hostPlaying { false }, switchRequested { false }, previewRequested { false };
    int previewOffLeft = 0;
    bool standaloneStateSkipped = false;
    enum class Duck { None, Out, In } duck = Duck::None;
    float duckGain = 1.0f, lastPeak = 0.0f;
    juce::AbstractFifo scopeFifo { 32768 };
    std::vector<float> scopeBuf = std::vector<float> (32768, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoriaProcessor)
};
