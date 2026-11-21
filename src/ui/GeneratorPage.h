#pragma once
#include "Pages.h"
#include "../gen/Generator.h"
#include "../gen/History.h"
#include <thread>

namespace gx::ui
{
// Big round GENERAR button with a burst animation.
class GenerateButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    GenerateButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip (L ("Generar un preset nuevo (Ctrl+Enter)", "Generate a new preset (Ctrl+Enter)")); }
    void paint (juce::Graphics&) override;
    // the component spans the whole centre column (for the backdrop) but only the disc is clickable
    bool hitTest (int x, int y) override
    {
        const auto b = getLocalBounds().toFloat();
        return b.getCentre().getDistanceFrom ({ (float) x, (float) y }) <= juce::jmin (b.getWidth(), b.getHeight()) * 0.36f * 1.12f;
    }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { pressed = false; repaint(); if (hitTest (e.x, e.y) && onClick) onClick(); }
    void burst();
    void animate (float dt, float energy, bool busy);
    std::function<void()> onClick;
private:
    struct Particle { juce::Point<float> p, v; float life = 0; juce::Colour c; };
    std::vector<Particle> particles;
    float spin = 0, pulse = 0, glow = 0, level = 0;
    bool pressed = false, working = false;
    juce::Random rng;
};

// Console with typewriter lines and a blinking cursor.
class AiConsole : public juce::Component
{
public:
    void start (const juce::StringArray& lines, double duration);
    void showIdle (const juce::String& text) { lines = { text }; typed = text.length(); lineIdx = 0; running = false; repaint(); }
    void animate (double now);
    void paint (juce::Graphics&) override;
    bool finished() const { return ! running; }
private:
    juce::StringArray lines, shown;
    int lineIdx = 0, typed = 0;
    double t0 = 0, perLine = 0.3, now = 0;
    bool running = false;
};

// Parameter readouts that scramble while "the AI thinks" and settle on the final values.
class ResolveGrid : public juce::Component
{
public:
    void setTarget (const Preset& p);
    void animate (float scramble, float assemble, float disassemble);
    void paint (juce::Graphics&) override;
private:
    struct Cell { juce::String label, value; };
    std::array<Cell, 12> cells;
    float scramble = 0, assemble = 1, disassemble = 0;
    juce::Random rng;
};

class GeneratorPage : public Page, private juce::Timer
{
public:
    explicit GeneratorPage (Context& c);
    ~GeneratorPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick (const Frame& f) override;
    bool handleKey (const juce::KeyPress&);
    void generate() { startJob (Job::Generate); }
    bool isBusy() const { return busy.load(); }
    void presetLoaded();
    std::function<void()> onPresetChanged, openEditor, openExport, openPresets;
    struct UiState { juce::String category; std::vector<double> sliders; std::vector<bool> locks; };
    UiState uiState() const;              // kept across a UI rebuild (language switch)
    void setUiState (const UiState&);

private:
    enum class Job { Generate, Mutate, Blend };
    void timerCallback() override;
    void startJob (Job j, const Preset* other = nullptr);
    void finishJob();
    void historyStep (int dir);
    void updateHistoryLabel();
    void saveCurrent();
    void showBlendMenu();
    GenRequest makeRequest() const;

    // left
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::OwnedArray<juce::Slider> sliders;
    juce::StringArray sliderNames;
    juce::TextButton resetChar;          // "Reset" next to the CHARACTER title
    juce::OwnedArray<juce::ToggleButton> locks;
    juce::String category = "Ambient";
    // centre
    GenerateButton genButton;
    AiConsole console;
    ResolveGrid grid;
    juce::TextButton mutateBtn { L ("Mutar", "Mutate") }, blendBtn { L ("Mezclar", "Blend") }, saveBtn { L ("Guardar", "Save") }, exportBtn { L ("Exportar", "Export") }, editorBtn { L ("Ir a Sonido", "Go to Sound") };
    juce::TextButton backBtn { "<" }, fwdBtn { ">" };
    juce::Label histLabel;
    // right
    Scope scope;
    Spectrum spectrum;
    Landscape view;
    VuMeter vu;
    Box macros;

    GenHistory* history = nullptr; // owned by the processor (survives closing the editor, not the session)
    std::thread worker;
    std::atomic<bool> jobDone { false }, busy { false }, jobFailed { false };
    Preset result;
    double jobStart = 0, minDuration = 1.5, lastAnim = 0, assembleStart = -10;
    juce::String lastMsg;
    Rng msgRng { (uint32_t) juce::Time::currentTimeMillis() };
    float energy = 0;
};

}
