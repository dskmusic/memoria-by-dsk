#pragma once
#include "Visuals.h"
#include "../preset/Render.h"

namespace gx::ui
{
// ffmpeg discovery / installation (video export needs it; it is free and not bundled).
juce::File findFfmpeg (Settings& settings);
juce::String ffmpegInstallCommand();          // empty if no package manager is available
juce::String ffmpegInstructions();            // localized, human-readable
void showFfmpegHelp();                        // the same, with one click-to-copy button per system

// Renders the landscape of a preset to an MP4 (H.264 + AAC) by piping raw frames straight into ffmpeg.
// Runs on the message thread in small slices (call step() from a timer). The audio is rendered first.
class VideoExporter
{
public:
    bool start (const Preset& p, const juce::File& out, double seconds, int width, int height, const juce::File& ffmpeg, juce::String& error, bool useGpu = true);
    bool step (double budgetMs);                // false when finished (or failed)
    void cancel();
    float progress() const { return total > 0 ? (float) frame / (float) total : 0.0f; }
    bool running() const { return pipe.isOpen() || audioPending; }
    juce::String result;                        // empty = ok, otherwise error / "cancelled"
    std::atomic<float> audioProgress { 0 };

private:
    void finish (const juce::String& err);
    Preset preset;
    juce::File target, wav, ffmpegExe;
    int w = 1920, h = 1080, frame = 0, total = 0;
    double secs = 60;
    // ffmpeg child process with a writable stdin, started without any console window
    struct Pipe
    {
        bool open (const juce::StringArray& args);
        bool write (const void* data, size_t bytes);
        int close();                       // waits for ffmpeg, returns its exit code
        bool isOpen() const { return h != nullptr; }
        void* h = nullptr;                 // Windows: process handle / POSIX: FILE*
        void* in = nullptr;                // Windows: stdin write handle
    } pipe;
    int rw = 960, rh = 540;
    bool gpu = false;               // internal render size (ffmpeg upscales: 4x faster than painting at 1080p)
    bool audioPending = false;
    std::thread audioThread;
    std::atomic<bool> audioDone { false }, cancelFlag { false };
    juce::String audioError;
    std::unique_ptr<Engine> eng;
    std::unique_ptr<Landscape> view;
    Frame fr;
    juce::AudioBuffer<float> buf;
    std::vector<uint8_t> row;
};
}
