#pragma once
#include <juce_opengl/juce_opengl.h>
#include "SceneLook.h"

namespace gx::ui
{
// Everything the room shader needs for one frame.
struct RoomUniforms
{
    float time = 0, memory = 0, age = 0.5f, world = 0, energy = 0, hit = 0, pedal = 0, flame = 1, intimacy = 0.5f, warmth = 0, space = 0.5f, scale = 1.0f;
    SceneLook look;         // sky, light, season, clock (eased by the scene)
    float seed = 1, section = 0;
    float keys[88] {};      // how much each key (A0 .. C8) is down / ringing
};

// The room scene as a GLSL fragment shader: window light with volumetric dust, glass with rain, a glossy grand piano,
// a candle, film ageing. Must be used on the OpenGL thread with an active context.
class GpuRoom
{
public:
    bool init (juce::OpenGLContext&);   // false = not supported (the CPU scene is used)
    void release();
    void render (const RoomUniforms&, int width, int height);
    juce::String error;
private:
    juce::OpenGLContext* ctx = nullptr;
    std::unique_ptr<juce::OpenGLShaderProgram> prog, bloom;
    juce::uint32 vbo = 0;
    juce::OpenGLFrameBuffer fb;
    int fbW = 0, fbH = 0;
    void drawQuad (juce::OpenGLShaderProgram&);
};

// A hidden OpenGL context used to render the scene into images (screenshots, demo video).
class OffscreenRoom
{
public:
    static OffscreenRoom& get();
    bool available();
    juce::Image render (const RoomUniforms&, int width, int height);   // empty image on failure
    void shutdown();
private:
    struct Host;
    std::unique_ptr<Host> host;
    bool tried = false, ok = false;
};

// What happened when the shader scene was set up (shown in Settings, also written to Documents/DSK Music/Memoria/gpu-log.txt)
inline int& gpuForceTier() { static int t = -1; return t; }   // tools: start at this shader tier (0 full, 1 light, 2 minimal)
juce::String getGpuInfo();
void setGpuInfo (const juce::String&);
inline std::atomic<int>& gpuStatus() { static std::atomic<int> s { 0 }; return s; }   // 0 unknown, 1 ok, -1 failed
}
