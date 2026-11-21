#pragma once
#include "Visuals.h"
#include "GpuRoom.h"

namespace gx::ui
{
// A room with a piano, a window, a candle: light from the window with floating dust, rain on the glass, a flickering flame.
// The "World" layer decides the weather / hour outside; the Memory macro ages the picture (sepia, grain, vignette,
// film scratches, light leaks). Rendered at low resolution on the CPU (soft, filmic) and scaled up.
class RoomScene : public juce::Component, public Animated, private juce::OpenGLRenderer
{
public:
    RoomScene();
    ~RoomScene() override;
    // GPU shader scene when the system supports it; the CPU scene below is the fallback
    void setGpu (bool on);
    bool gpuWanted = true;
    int quality = 1;        // 0 low, 1 medium, 2 high (render scale of the shader scene)
    bool gpuActive() const { return glOk.load(); }
    RoomUniforms uniforms() const;
    void setExternalBackground (const juce::Image& i) { external = i; }   // screenshots / video: the GPU picture rendered offscreen
    void parentHierarchyChanged() override;
    void tick (const Frame&) override;
    void paint (juce::Graphics&) override;
    void renderNow (const Frame&);     // used by the screenshot / video tools
    juce::Image& image() { return img; }

    static constexpr int W = 416, H = 260;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    std::unique_ptr<juce::OpenGLContext> gl;
    GpuRoom gpuRoom;
    std::atomic<bool> glOk { false };
    juce::SpinLock ulock;
    RoomUniforms uni;
    juce::Image external;
    float hitEnv = 0, sectionEnv = 0;
    SceneLook look; bool lookInit = false;
    void rebuild (int world, int tod, int season);
    struct Mote { float x, y, vx, vy, ph, size; };
    struct Drop { float x, y, v, len; };
    int built = -1;
    std::vector<float> baseR, baseG, baseB, beam, candle, glow, vig;
    float skyR = 1, skyG = 0.8f, skyB = 0.5f, ambient = 1, warm = 0;
    std::array<Mote, 110> motes;
    std::array<Drop, 40> drops;
    juce::Image img { juce::Image::ARGB, W, H, false };
    Frame f;
    double last = 0, lastRender = -1, scratchT = 0;
    int lastHits = 0;
    float flame = 1, flameT = 0, scratchX[3] {};
    juce::Random rng { 20 };
};
}
