#include "VideoExport.h"
#include <cstdio>
#include <thread>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace gx::ui
{
namespace
{
constexpr int kFps = 30;
constexpr double kSr = 48000.0;

} // namespace

// ---------------------------------------------------------------- ffmpeg
juce::File findFfmpeg (Settings& settings)
{
    const juce::File custom (settings.file().getValue ("ffmpeg"));
    if (custom.existsAsFile()) return custom;
#if JUCE_WINDOWS
    const juce::String exe = "ffmpeg.exe";
    juce::StringArray dirs { juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {}) + "\\Microsoft\\WinGet\\Links",
                             "C:\\ffmpeg\\bin", juce::SystemStats::getEnvironmentVariable ("ProgramFiles", {}) + "\\ffmpeg\\bin",
                             juce::SystemStats::getEnvironmentVariable ("ProgramFiles(x86)", {}) + "\\ffmpeg",
                             juce::SystemStats::getEnvironmentVariable ("ProgramFiles(x86)", {}) + "\\ffmpeg\\bin",
                             juce::SystemStats::getEnvironmentVariable ("ProgramData", {}) + "\\chocolatey\\bin" };
    // winget "portable" installs also live in a versioned folder under Packages
    for (auto& d : juce::File (juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {}) + "\\Microsoft\\WinGet\\Packages")
                       .findChildFiles (juce::File::findDirectories, false, "Gyan.FFmpeg*"))
        for (auto& f : d.findChildFiles (juce::File::findFiles, true, exe)) dirs.add (f.getParentDirectory().getFullPathName());
    dirs.addTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), ";", "\"");
#else
    const juce::String exe = "ffmpeg";
    juce::StringArray dirs { "/opt/homebrew/bin", "/usr/local/bin", "/usr/bin", "/snap/bin" };
    dirs.addTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), ":", "\"");
#endif
    for (auto& d : dirs)
        if (d.trim().isNotEmpty())
        {
            const juce::File f = juce::File (d.trim().unquoted()).getChildFile (exe);
            if (f.existsAsFile()) return f;
        }
    return {};
}

juce::String ffmpegInstallCommand()
{
#if JUCE_WINDOWS
    return "winget install -e --id Gyan.FFmpeg --accept-source-agreements --accept-package-agreements";
#elif JUCE_MAC
    for (auto b : { "/opt/homebrew/bin/brew", "/usr/local/bin/brew" })
        if (juce::File (b).existsAsFile()) return juce::String (b) + " install ffmpeg";
    return {};
#else
    return {};
#endif
}

juce::String ffmpegInstructions()
{
    return L ("Para exportar vídeo, Memoria usa ffmpeg (gratuito).\n\n"
              "Windows: abre una terminal y escribe:\n   winget install Gyan.FFmpeg\n"
              "macOS (con Homebrew): brew install ffmpeg\n"
              "Linux: sudo apt install ffmpeg   (o el gestor de tu distribución)\n\n"
              "También puedes descargarlo de https://ffmpeg.org/download.html y elegir el ejecutable con el botón «Elegir ffmpeg…».",
              "To export video, Memoria uses ffmpeg (free).\n\n"
              "Windows: open a terminal and type:\n   winget install Gyan.FFmpeg\n"
              "macOS (with Homebrew): brew install ffmpeg\n"
              "Linux: sudo apt install ffmpeg   (or your distribution's package manager)\n\n"
              "You can also download it from https://ffmpeg.org/download.html and pick the executable with \"Choose ffmpeg...\".");
}

void showFfmpegHelp()
{
    struct Commands : juce::Component
    {
        juce::OwnedArray<juce::TextButton> buttons;
        juce::Label status;
        Commands()
        {
            const juce::String sys[] { "Windows", "macOS (Homebrew)", "Linux" };
            const juce::String cmd[] { "winget install Gyan.FFmpeg", "brew install ffmpeg", "sudo apt install ffmpeg" };
            for (int i = 0; i < 3; ++i)
            {
                auto* b = buttons.add (new juce::TextButton (sys[i] + ":   " + cmd[i]));
                b->setTooltip (L ("Clic para copiar el comando", "Click to copy the command"));
                const auto c = cmd[i];
                b->onClick = [this, c]
                {
                    juce::SystemClipboard::copyTextToClipboard (c);
                    status.setText (L ("Copiado al portapapeles: ", "Copied to the clipboard: ") + c, juce::dontSendNotification);
                };
                addAndMakeVisible (b);
            }
            status.setJustificationType (juce::Justification::centred);
            status.setColour (juce::Label::textColourId, col::accent);
            addAndMakeVisible (status);
            setSize (440, 3 * 36 + 26);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            for (auto* b : buttons) b->setBounds (r.removeFromTop (36).reduced (0, 4));
            status.setBounds (r);
        }
    };
    struct Help : juce::AlertWindow
    {
        Commands commands;
        Help() : juce::AlertWindow ("ffmpeg", L ("Para exportar vídeo, Memoria usa ffmpeg (gratuito). Abre una terminal y escribe el comando de tu sistema (clic en él para copiarlo):",
                                                 "To export video, Memoria uses ffmpeg (free). Open a terminal and type the command for your system (click it to copy it):"),
                                     juce::MessageBoxIconType::InfoIcon)
        {
            addCustomComponent (&commands);
            addTextBlock (L ("También puedes descargarlo de https://ffmpeg.org/download.html y elegir el ejecutable con el botón «Elegir ffmpeg…».",
                             "You can also download it from https://ffmpeg.org/download.html and pick the executable with \"Choose ffmpeg...\"."));
            addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        }
    };
    (new Help())->enterModalState (true, nullptr, true);
}

// ---------------------------------------------------------------- pipe (no console window)
bool VideoExporter::Pipe::open (const juce::StringArray& args)
{
#if JUCE_WINDOWS
    juce::String cmd;
    for (auto& a : args) cmd << (cmd.isEmpty() ? "" : " ") << (a.containsChar (' ') ? a.quoted() : a);
    SECURITY_ATTRIBUTES sa { sizeof (sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (! CreatePipe (&rd, &wr, &sa, 1 << 20)) return false;
    SetHandleInformation (wr, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si {};
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = rd;
    si.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle (STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi {};
    std::wstring line (cmd.toWideCharPointer());
    const BOOL ok = CreateProcessW (nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);
    CloseHandle (rd);
    if (! ok) { CloseHandle (wr); return false; }
    CloseHandle (pi.hThread);
    h = pi.hProcess; in = wr;
    return true;
#else
    juce::String cmd;
    for (auto& a : args) cmd << (cmd.isEmpty() ? "" : " ") << a.quoted();
    FILE* f = popen ((cmd + " 2>/dev/null").toRawUTF8(), "w");
    h = f;
    return f != nullptr;
#endif
}

bool VideoExporter::Pipe::write (const void* data, size_t bytes)
{
#if JUCE_WINDOWS
    const char* p = static_cast<const char*> (data);
    while (bytes > 0)
    {
        DWORD done = 0;
        if (! WriteFile ((HANDLE) in, p, (DWORD) juce::jmin<size_t> (bytes, 1 << 20), &done, nullptr) || done == 0) return false;
        p += done; bytes -= done;
    }
    return true;
#else
    return std::fwrite (data, 1, bytes, (FILE*) h) == bytes;
#endif
}

int VideoExporter::Pipe::close()
{
    if (h == nullptr) return 0;
#if JUCE_WINDOWS
    CloseHandle ((HANDLE) in);
    WaitForSingleObject ((HANDLE) h, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess ((HANDLE) h, &code);
    CloseHandle ((HANDLE) h);
    h = in = nullptr;
    return (int) code;
#else
    const int rc = pclose ((FILE*) h);
    h = nullptr;
    return rc;
#endif
}

// ---------------------------------------------------------------- exporter
bool VideoExporter::start (const Preset& p, const juce::File& out, double seconds, int width, int height, const juce::File& ffmpeg, juce::String& error, bool useGpu)
{
    gpu = useGpu && OffscreenGpu::get().available();
    preset = p; target = out; secs = seconds; w = width; h = height; ffmpegExe = ffmpeg;
    frame = 0; total = (int) (seconds * kFps); result = {};
    cancelFlag = false; audioDone = false; audioProgress = 0;
    wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("memoria_video_audio_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".wav");
    if (! ffmpegExe.existsAsFile()) { error = "ffmpeg"; return false; }
    audioPending = true;
    audioThread = std::thread ([this]
    {
        audioError = exportAudio (preset, wav, efWav, secs, kSr, &audioProgress, &cancelFlag);
        audioDone = true;
    });
    return true;
}

void VideoExporter::cancel() { cancelFlag = true; }

void VideoExporter::finish (const juce::String& err)
{
    if (pipe.isOpen()) { const int rc = pipe.close(); if (err.isEmpty() && rc != 0) result = "ffmpeg error " + juce::String (rc); else result = err; }
    else result = err;
    if (audioThread.joinable()) audioThread.join();
    audioPending = false;
    wav.deleteFile();
    view.reset();
    eng.reset();
    if (result.isNotEmpty()) target.deleteFile();
}

bool VideoExporter::step (double budgetMs)
{
    if (audioPending)
    {
        if (! audioDone.load()) return true;           // still rendering the soundtrack
        audioThread.join();
        audioPending = false;
        if (audioError.isNotEmpty()) { finish (audioError); return false; }
        // soundtrack ready: start ffmpeg reading raw frames from stdin
        target.deleteFile();
        // the landscape is painted at half size and upscaled by ffmpeg (soft, painterly images scale beautifully)
        rw = gpu ? w : w / 2; rh = gpu ? h : h / 2;   // GPU frames are rendered at full size
        juce::StringArray args { ffmpegExe.getFullPathName(), "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "bgra",
                                 "-s", juce::String (rw) + "x" + juce::String (rh), "-r", juce::String (kFps), "-i", "-", "-i", wav.getFullPathName(),
                                 "-vf", "scale=" + juce::String (w) + ":" + juce::String (h) + ":flags=lanczos",
                                 "-c:v", "libx264", "-preset", "veryfast", "-tune", "film", "-crf", "20", "-pix_fmt", "yuv420p",
                                 "-c:a", "aac", "-b:a", "256k", "-shortest", "-movflags", "+faststart", target.getFullPathName() };
        if (! pipe.open (args)) { finish ("cannot start ffmpeg"); return false; }
        eng = std::make_unique<Engine>();
        eng->prepare (kSr, 1600);
        eng->reset ((uint32_t) preset.get (P::seed));
        view = std::make_unique<Landscape>();
        view->showOverlay = false;
        view->fullBleed = true;
        view->setSize (rw, rh);
        buf.setSize (2, (int) (kSr / kFps));
        row.resize ((size_t) rw * 4);
        fr = Frame();
    }
    if (cancelFlag.load()) { finish ("cancelled"); return false; }
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const double stopAt = juce::jmax (0.0, secs - juce::jmin ((double) preset.get (P::fadeOut), secs * 0.25) - 3.0);
    juce::MidiBuffer none;
    while (frame < total && juce::Time::getMillisecondCounterHiRes() - t0 < budgetMs)
    {
        // run the engine for one video frame (same settings as the soundtrack) to drive the visuals
        HostInfo hi;
        hi.internalPlay = (double) frame / kFps < stopAt;
        const int n = buf.getNumSamples();
        eng->process (buf.getWritePointer (0), buf.getWritePointer (1), n, none, preset.params.data(), hi);
        float sq = 0;
        for (int i = 0; i < n; ++i) sq += buf.getSample (0, i) * buf.getSample (0, i) + buf.getSample (1, i) * buf.getSample (1, i);
        const float rms = std::sqrt (sq / (2.0f * (float) n)) * dbToGain (preset.get (P::master) * 0.0f);
        const float dt = 1.0f / kFps;
        auto& t = eng->tele;
        fr.time = (double) frame / kFps + 1.0;
        fr.energy += (juce::jmin (1.0f, rms * 6.0f) - fr.energy) * juce::jmin (1.0f, dt * 4.0f);
        for (int l = 0; l < kNumLayers; ++l)
        {
            fr.presence[(size_t) l] = t.presence[(size_t) l].load();
            fr.layerEnergy[(size_t) l] += (juce::jmin (1.0f, t.energy[(size_t) l].load() * 8.0f) - fr.layerEnergy[(size_t) l]) * juce::jmin (1.0f, dt * 3.0f);
        }
        for (int i = 0; i < kChordVoices; ++i) fr.chord[(size_t) i] = t.chordNotes[(size_t) i].load();
        fr.rootPc = t.rootPc.load(); fr.degree = t.chordDegree.load(); fr.bellCount = t.bellCount.load();
        fr.lastBellNote = t.lastBellNote.load(); fr.lastBellPan = t.lastBellPan.load();
        fr.runGain = t.runGain.load(); fr.evoTime = t.evoTime.load(); fr.musTime = t.musTime.load();
        fr.running = t.running.load(); fr.playButton = true;
        fr.light = preset.get (P::light); fr.space = preset.get (P::space); fr.harmony = preset.get (P::harmony);
        fr.scale = (int) preset.get (P::scale); fr.seed = (int) preset.get (P::seed); fr.frozen = preset.get (P::freeze) > 0.5f;
        fr.scene = (int) preset.get (P::scene); fr.evolution = preset.get (P::evolution); fr.density = preset.get (P::density);
        view->tick (fr);
        if (gpu) view->setExternalBackground (OffscreenGpu::get().render (view->uniforms(), rw, rh));
        juce::Image img (juce::Image::ARGB, rw, rh, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            view->paintEntireComponent (g, true);
            g.setColour (juce::Colours::white.withAlpha (0.35f)); // discreet signature
            g.setFont (font ((float) rh * 0.022f, true));
            g.drawText ("Memoria by DSK", juce::Rectangle<int> (0, 0, rw - (int) (rh * 0.025f), rh - (int) (rh * 0.02f)), juce::Justification::bottomRight);
        }
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < rh; ++y)
        {
            const uint8_t* src = bd.getLinePointer (y);
            for (int x = 0; x < rw; ++x) { const uint8_t* px = src + x * bd.pixelStride; std::memcpy (&row[(size_t) x * 4], px, 4); row[(size_t) x * 4 + 3] = 255; }
            if (! pipe.write (row.data(), row.size())) { finish ("ffmpeg stopped (disk full?)"); return false; }
        }
        ++frame;
    }
    if (frame >= total) { finish ({}); return false; }
    return true;
}
}
