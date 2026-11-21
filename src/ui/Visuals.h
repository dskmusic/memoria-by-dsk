#pragma once
#include "Widgets.h"
#include "GpuScenes.h"
#include <juce_dsp/juce_dsp.h>

namespace gx::ui
{
// Data gathered once per UI frame by the editor and handed to every animated component.
struct Frame
{
    static constexpr int kScope = 2048, kBands = 96;
    std::array<float, kScope> scope {};      // most recent mono output samples
    std::array<float, kBands> spectrum {};   // 0..1 log-frequency bands
    float peakL = 0, peakR = 0, rms = 0;
    double time = 0;                         // seconds since editor open
    float energy = 0;                        // smoothed rms, drives "aliveness"
    // engine state
    std::array<float, kNumLayers> presence {}, layerEnergy {};
    std::array<int, kChordVoices> chord {};
    int rootPc = 9, degree = 0, bellCount = 0, lastBellNote = 72, seed = 1;
    float runGain = 0, evoTime = 0, musTime = 0, elapsed = 0, lastBellPan = 0, light = 0.5f, space = 0.5f, harmony = 0.5f;
    int scale = 1, timerMin = 0, scene = 0;
    float evolution = 0.5f, density = 0.5f;
    bool running = false, frozen = false, playButton = false, hostPlaying = false;
    bool reduced = false;                    // reduced animation mode
};

struct Animated
{
    virtual ~Animated() = default;
    virtual void tick (const Frame&) = 0;
};

class FrameSource // owned by the editor
{
public:
    explicit FrameSource (MemoriaProcessor& p) : proc (p) {}
    void update (Frame& f, double dt);
private:
    MemoriaProcessor& proc;
    std::vector<float> ring = std::vector<float> (8192, 0.0f);
    int ringPos = 0;
    juce::dsp::FFT fft { 11 };
    std::vector<float> fftBuf = std::vector<float> (4096, 0.0f);
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
};

class Scope : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
    void resized() override { trail = juce::Image(); }
    juce::String label = L ("OSCILOSCOPIO", "OSCILLOSCOPE");
private:
    juce::Image trail;
    std::array<float, 512> trace {};
    float energy = 0; double t = 0;
    float dispPeak = 0.0f, levelDb = -90.0f;
};

class Spectrum : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
private:
    std::array<float, Frame::kBands> bands {}, peaks {};
};

class VuMeter : public juce::Component, public Animated
{
public:
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
private:
    float l = 0, r = 0, hl = 0, hr = 0;
};

// Generative landscape with four scenes (Valley, Starfield, Nebula, Neon Rain). Every layer has a visual role:
// drone and pad shape the big masses, texture is dust/grain, shimmer is aurora/beams, wind is mist/fog,
// bells are sparks/comets/flying lights; the seed decides the scenery (lake, forest, planet, skyline...).
class Landscape : public juce::Component, public Animated, private juce::OpenGLRenderer
{
public:
    Landscape();
    ~Landscape() override;
    // GPU landscapes (shaders) when the system supports them; the painted ones are the fallback
    void setGpu (bool on);
    bool gpuActive() const { return glOk.load(); }
    // background supplied from outside (offscreen GPU render for videos / screenshots)
    void setExternalBackground (const juce::Image& img) { external = img; }
    SceneUniforms uniforms() const;
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void takeScreenshot();
    bool showOverlay = true;                 // chord / key / clock texts
    bool fullBleed = false;                  // no rounded corners (full screen, video)
    std::function<void()> onClick;
    std::function<void (juce::Graphics&)> paintExtra;   // drawn last, on top (works over the GPU landscape too)
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }
private:
    struct Spark { float x, y, vx, vy, life, size; juce::Colour c; int kind; };
    void paintValley (juce::Graphics&, juce::Rectangle<float>);
    void paintStarfield (juce::Graphics&, juce::Rectangle<float>);
    void paintUnderwater (juce::Graphics&, juce::Rectangle<float>);
    void paintNeon (juce::Graphics&, juce::Rectangle<float>);
    void paintOcean (juce::Graphics&, juce::Rectangle<float>);
    void paintAurora (juce::Graphics&, juce::Rectangle<float>);
    void paintSparks (juce::Graphics&);
    void paintOverlay (juce::Graphics&, juce::Rectangle<float>);
    void rebuildCache();                     // seed/size dependent scenery
    Frame f;
    std::vector<Spark> sparks;
    std::array<juce::Point<float>, 260> stars {};
    std::array<float, 260> twinkle {}, starSize {};
    std::array<float, kNumLayers> pres {}, glow {};
    int lastBells = -1;
    double lastT = 0;
    juce::Random rng { 1234 };
    // seed-dependent scenery
    juce::String cacheKey;
    bool hasLake = false, hasForest = false, hasBirds = false, hasRings = false;
    int numClouds = 3;
    juce::Point<float> planetPos; float planetR = 100; juce::Colour planetCol, planetCol2;
    std::array<juce::Image, 3> skyline;      // Neon Rain building layers (with lit windows)
    struct Sign { juce::Rectangle<float> r; juce::Colour c; float phase; int layer; };
    std::vector<Sign> signs;
    juce::Point<float> arcology;
    juce::Image fadeFrom;   // previous picture during a scene / seed transition
    juce::Image grain;      // film grain tile
    juce::Point<float> flarePos; juce::Colour flareCol; float flareAmt = 0;   // anamorphic flare of the sun / moon
    void paintPost (juce::Graphics&, juce::Rectangle<float>);
    void paintWhale (juce::Graphics&, float x, float y, float L, float dir, float t, juce::Colour body);
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    std::unique_ptr<juce::OpenGLContext> gl;
    GpuScenes scenes;
    std::atomic<bool> glOk { false };
    std::atomic<int> framesDrawn { 0 };
    double fpsT0 = 0; float fps = 0;
    void paintFps (juce::Graphics&, juce::Rectangle<float>);
    std::unique_ptr<juce::Button> shotButton, variantButton;
    int shownSeed = 1;
    juce::String shotMsg; double shotMsgAt = -100;
    juce::File lastShot;
    juce::SpinLock ulock;
    SceneUniforms uni;
    juce::Image external;
    int lastScene = -1, lastSeedSeen = -1;
    double sceneChangedAt = -10.0;
    double fadeStart = 0;
};

juce::String chordName (int rootPc, int scale, int degree, float harmony); // "Am7", "Fmaj9"...
// Visual intensity of the stars that appear with the notes (Settings, 0..1)
inline float& noteStarIntensity() { static float v = 0.3f; return v; }
inline bool& showFps() { static bool v = false; return v; }
inline int& landscapeVariant() { static int v = 0; return v; }   // scenery variation (button under the chord)
}
