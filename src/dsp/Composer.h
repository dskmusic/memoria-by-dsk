#pragma once
#include "Common.h"
#include <vector>

// The "Hands" composer: writes a whole song - intro, verses, choruses, bridge, outro - as a score (notes + pedal),
// deterministically from a seed. Playback just follows the score, so MIDI export, WAV export and live playback are the same music.
//   * harmony: functional progressions per section, 7ths / 9ths / sus colours, secondary dominants, half and full cadences
//   * melody: motifs with development (repetition, sequence, variation), hooks in the chorus, cadences, ornaments
//   * accompaniment: style patterns (arpeggios, waltz, stabs, block chords) with voice-leading
//   * performance: dynamics by section and phrase, accents, humanised timing, rolled chords, rubato, pedal changes
namespace gx
{
struct NoteEv { double start = 0, len = 0; int note = 60, vel = 64, voice = 0; };   // voice 1 = melody
struct PedEv { double beat = 0; int val = 0; };
struct SectionInfo { char type = 'A'; int startBar = 0, bars = 0; };   // I intro, A verse, B chorus, C bridge, O outro

struct Score
{
    std::vector<NoteEv> notes;        // sorted by start
    std::vector<PedEv> pedal;         // sorted by beat
    std::vector<SectionInfo> sections;
    double totalBeats = 0;
    int beatsPerBar = 4, bars = 0;
};

enum HandsMode { hmOff, hmSong, hmAccomp, hmMelody };
enum SongStyle { ssLofi, ssCinematic, ssRomantic, ssMinimal, ssLullaby, ssBallad, numSongStyles };

struct ComposeCfg
{
    int mode = 1;                     // 1 full song, 2 accompaniment only, 3 melody only
    int key = 9, scale = 1, style = 0, bars = 32;
    uint32_t seed = 1;
    float density = 0.6f, expression = 0.6f;
    bool pedal = true;
    double bpm = 80;
};

void compose (const ComposeCfg& cfg, Score& out);
}
