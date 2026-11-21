#pragma once
#include <juce_opengl/juce_opengl.h>
#include "../Params.h"

namespace gx::ui
{
// Everything a landscape shader needs for one frame.
struct SceneUniforms
{
    int scene = 0;
    float time = 0, mus = 0, evoT = 0, light = 0.5f, evo = 0.5f, density = 0.5f, space = 0.5f, energy = 0, run = 1, frozen = 0, seed = 1, fade = 1;
    float pres[kNumLayers] {}, glow[kNumLayers] {};
};

// The six landscapes as GLSL fragment shaders (volumetric clouds, fractal terrain, real water, glowing gas...).
// All methods must be called on the OpenGL thread with an active context.
class GpuScenes
{
public:
    bool init (juce::OpenGLContext&);   // compiles the shaders; false = not supported (use the CPU landscapes)
    void release();
    void render (const SceneUniforms&, int width, int height);
    bool has (int scene) const { return scene >= 0 && scene < numScenes && ok[scene].load(); }   // false: that landscape uses the CPU painter
    juce::String error;
private:
    juce::OpenGLContext* ctx = nullptr;
    std::unique_ptr<juce::OpenGLShaderProgram> prog[numScenes];
    std::atomic<bool> ok[numScenes] {};
    juce::uint32 vbo = 0;
    std::unique_ptr<juce::OpenGLShaderProgram> bloom;   // second pass: lights glow into the air (optional)
    juce::OpenGLFrameBuffer fb;
    int fbW = 0, fbH = 0;
    void drawQuad (juce::OpenGLShaderProgram&);
};

// A hidden OpenGL context used to render landscapes into images (video export, screenshots, demo video).
class OffscreenGpu
{
public:
    static OffscreenGpu& get();
    bool available();                                        // tries once
    juce::Image render (const SceneUniforms&, int width, int height); // empty image on failure
    void shutdown();
private:
    struct Host;
    std::unique_ptr<Host> host;
    bool tried = false, ok = false;
};

// Remembers whether the GPU landscapes work on this computer (set by the first context that tries).
inline std::atomic<int>& gpuStatus() { static std::atomic<int> s { 0 }; return s; }   // 0 unknown, 1 ok, -1 failed
}
