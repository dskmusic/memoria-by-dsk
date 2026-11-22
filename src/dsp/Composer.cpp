#include "Composer.h"
#include <algorithm>
#include <sstream>
#include <cstdio>

namespace gx
{
namespace
{
const int kScales[8][7] { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 6, 7, 9, 11 },
                          { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 } };
const int kMinorPentMask[7] { 1, 0, 1, 1, 1, 0, 1 }, kMajorPentMask[7] { 1, 1, 1, 0, 1, 1, 0 };   // degrees allowed in the melody

inline int fdiv (int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
inline int imod (int a, int b) { return ((a % b) + b) % b; }

struct Chord { int deg = 0, ext = 0; };   // ext: 0 triad, 1 seventh, 2 ninth, 3 sus2, 4 sus4
struct Mot { double beat, dur; int step; };

const int kBankA[8][4] { { 0, 5, 3, 4 }, { 0, 3, 4, 0 }, { 5, 3, 0, 4 }, { 0, 4, 5, 3 }, { 0, 5, 2, 6 }, { 0, 2, 5, 4 }, { 5, 4, 3, 4 }, { 0, 3, 5, 4 } };
const int kBankB[6][4] { { 3, 4, 0, 5 }, { 5, 4, 3, 0 }, { 3, 0, 4, 5 }, { 0, 4, 5, 3 }, { 5, 3, 4, 0 }, { 0, 4, 5, 3 } };
const int kBankP[4][4] { { 1, 3, 4, 4 }, { 3, 4, 5, 4 }, { 5, 3, 4, 4 }, { 1, 4, 3, 4 } };
const int kBankC[4][4] { { 1, 4, 0, 5 }, { 5, 2, 3, 4 }, { 3, 5, 1, 4 }, { 2, 5, 3, 4 } };


// ---------------------------------------------------------------------------------------------------------------------------
// The songbook: melodies written by hand, the way a songwriter would, as 4-bar phrases over a 4-chord loop. Each note is
// "beat:duration:degree" (degree 0 = tonic of the melody register, 7 = the octave above, -1 = the leading tone below).
// Strong beats (1 and 3) always land on chord tones; weak beats move by step; every loop has a question phrase (ends open),
// an answer phrase (ends settled), a quieter verse version and a higher, more rhythmic chorus version.
// The generator chooses a loop for the style, transposes, adds the accompaniment texture, the arrangement and the performance.
// ---------------------------------------------------------------------------------------------------------------------------
struct BookNote { float b, d; int deg; };
using BookBar = std::vector<BookNote>;
struct Phrase { BookBar bar[4]; };
struct Loop { int chords[4]; Phrase v, va, c, ca; bool waltz = false; int moods = 0; };

BookBar parseBar (const char* s)
{
    BookBar out; std::istringstream is (s); std::string tok;
    while (is >> tok) { float b = 0, d = 0; int deg = 0; if (std::sscanf (tok.c_str(), "%f:%f:%d", &b, &d, &deg) == 3) out.push_back ({ b, d, deg }); }
    return out;
}
Phrase mk (const char* a, const char* b, const char* c, const char* d) { Phrase p; p.bar[0] = parseBar (a); p.bar[1] = parseBar (b); p.bar[2] = parseBar (c); p.bar[3] = parseBar (d); return p; }
Phrase withLast (Phrase p, const char* last) { p.bar[3] = parseBar (last); return p; }
Loop mkLoop (std::array<int, 4> ch, Phrase v, const char* vaLast, Phrase c, const char* caLast, bool waltz = false)
{
    Loop l; for (int i = 0; i < 4; ++i) l.chords[i] = ch[(size_t) i];
    l.v = v; l.va = withLast (v, vaLast); l.c = c; l.ca = withLast (c, caLast); l.waltz = waltz;
    return l;
}
#include "SongbookData.inc"

const std::vector<Loop>& book()
{
    static const std::vector<Loop> b = []
    {
        std::vector<Loop> l;
        // 0: bright pop  I - V - vi - IV
        l.push_back (mkLoop ({ 0, 4, 5, 3 },
            mk ("0:1.5:2 1.5:.5:4 2:2:4", "0:1.5:4 1.5:.5:2 2:2:1", "0:1.5:2 1.5:.5:0 2:2:2", "0:1.5:3 1.5:.5:2 2:1:0 3:1:-1"), "0:1.5:3 1.5:.5:2 2:2:0",
            mk ("0:.5:4 .5:.5:4 1:1:7 2:1.5:7 3.5:.5:6", "0:1:6 1:1:4 2:2:8", "0:.5:7 .5:.5:7 1:1:9 2:1.5:9 3.5:.5:7", "0:1:7 1:1:5 2:2:5"), "0:1:7 1:1:5 2:2:7"));
        // 1: emotional  vi - IV - I - V
        l.push_back (mkLoop ({ 5, 3, 0, 4 },
            mk ("0:1.5:2 1.5:.5:2 2:1:0 3:1:2", "0:1.5:3 1.5:.5:3 2:1:5 3:1:3", "0:1.5:4 1.5:.5:4 2:1:2 3:1:4", "0:2:6 2:1:4 3:1:1"), "0:1.5:6 1.5:.5:4 2:2:4",
            mk ("0:1:5 1:1:7 2:2:9", "0:1:10 1:1:9 2:2:7", "0:1:7 1:1:9 2:2:11", "0:1:13 1:1:11 2:2:8"), "0:1:13 1:1:11 2:2:11"));
        // 2: gentle  I - vi - IV - V
        l.push_back (mkLoop ({ 0, 5, 3, 4 },
            mk ("0:1:4 1:.5:2 1.5:.5:4 2:2:7", "0:1.5:5 1.5:.5:4 2:2:2", "0:1:3 1:.5:5 1.5:.5:3 2:2:0", "0:1.5:1 1.5:.5:-1 2:2:1"), "0:1.5:-1 1.5:.5:1 2:2:4",
            mk ("0:1:7 1:1:9 2:1:11 3:1:9", "0:1:9 1:1:7 2:2:5", "0:1:10 1:1:9 2:1:7 3:1:5", "0:2:8 2:1:6 3:1:4"), "0:1:8 1:1:6 2:2:4"));
        // 3: jazzy  ii - V - I - vi
        l.push_back (mkLoop ({ 1, 4, 0, 5 },
            mk ("0.5:1:5 1.5:.5:3 2:2:1", "0:1:6 1:1:4 2:1.5:6 3.5:.5:4", "0.5:1:2 2:2:4", "0:1.5:5 1.5:.5:7 2:2:9"), "0:1.5:5 1.5:.5:4 2:2:2",
            mk ("0:1:8 1:1:5 2:1:3 3:1:5", "0:1.5:6 1.5:.5:4 2:2:8", "0:1:9 1:1:7 2:2:4", "0:2:5 2:2:2"), "0:2:5 2:1:2 3:1:0"));
        // 4: lullaby waltz (3/4)  I - IV - I - V
        l.push_back (mkLoop ({ 0, 3, 0, 4 },
            mk ("0:2:4 2:1:2", "0:2:5 2:1:3", "0:1:2 1:1:4 2:1:2", "0:2:1 2:1:-1"), "0:2:1 2:1:0",
            mk ("0:1:7 1:1:9 2:1:7", "0:1:10 1:1:9 2:1:7", "0:1:7 1:1:11 2:1:9", "0:2:8 2:1:6"), "0:2:8 2:1:7", true));
        // 5: cinematic  vi - iii - IV - I
        l.push_back (mkLoop ({ 5, 2, 3, 0 },
            mk ("0:2:2 2:2:0", "0:2:4 2:2:2", "0:2:5 2:2:3", "0:4:4"), "0:4:2",
            mk ("0:2:5 2:2:7", "0:2:6 2:2:9", "0:2:10 2:2:12", "0:2:11 2:2:7"), "0:2:11 2:2:9"));
        // anthem I-iii-IV-V
        l.push_back (mkLoop ({ 0, 2, 3, 4 },
            mk ("0:1:4 1:1:2 2:2:4", "0:1.5:6 1.5:.5:4 2:2:2", "0:1.5:5 1.5:.5:4 2:2:3", "0:1:4 1:1:6 2:2:4"), "0:1:6 1:1:4 2:2:8",
            mk ("0:.5:7 .5:.5:7 1:1:9 2:2:7", "0:1:9 1:1:11 2:2:9", "0:1:10 1:1:9 2:2:7", "0:1:11 1:1:9 2:2:11"), "0:1:13 1:1:11 2:2:8"));
        // emotional vi-iii-IV-V
        l.push_back (mkLoop ({ 5, 2, 3, 4 },
            mk ("0:1.5:2 1.5:.5:0 2:2:2", "0:1.5:4 1.5:.5:2 2:2:4", "0:2:5 2:2:3", "0:1.5:6 1.5:.5:4 2:2:6"), "0:1.5:6 1.5:.5:4 2:2:1",
            mk ("0:1:5 1:1:7 2:2:9", "0:1:11 1:1:9 2:2:11", "0:1.5:12 1.5:.5:10 2:2:10", "0:1:13 1:1:11 2:2:13"), "0:1:11 1:1:13 2:2:8"));
        // turnaround I-vi-ii-V
        l.push_back (mkLoop ({ 0, 5, 1, 4 },
            mk ("0:1:2 1:1:4 2:2:7", "0:1.5:5 1.5:.5:4 2:2:2", "0:1:3 1:1:5 2:2:3", "0:1.5:4 1.5:.5:6 2:2:4"), "0:1.5:6 1.5:.5:4 2:2:1",
            mk ("0:1:7 1:1:9 2:2:11", "0:1:12 1:1:11 2:2:9", "0:1.5:12 1.5:.5:10 2:2:8", "0:1:11 1:1:13 2:2:11"), "0:1:13 1:1:11 2:2:8"));
        // doo-wop I-IV-vi-V
        l.push_back (mkLoop ({ 0, 3, 5, 4 },
            mk ("0:1.5:4 1.5:.5:4 2:1:2 3:1:4", "0:1.5:5 1.5:.5:5 2:2:3", "0:1:7 1:1:6 2:2:5", "0:1.5:6 1.5:.5:4 2:2:6"), "0:1.5:4 1.5:.5:6 2:2:1",
            mk ("0:1:11 1:1:9 2:1:7 3:1:9", "0:2:10 2:2:12", "0:1.5:14 1.5:.5:12 2:2:9", "0:1:13 1:1:11 2:2:13"), "0:1:11 1:1:13 2:2:8"));
        // rock ballad vi-V-IV-V
        l.push_back (mkLoop ({ 5, 4, 3, 4 },
            mk ("0:2:2 2:2:0", "0:2:1 2:2:6", "0:2:0 2:1:5 3:1:3", "0:2:4 2:2:6"), "0:2:6 2:2:8",
            mk ("0:2:9 2:2:7", "0:2:8 2:2:11", "0:2:10 2:1:12 3:1:10", "0:2:11 2:2:13"), "0:2:13 2:2:11"));
        // cinematic vi-IV-I-iii
        l.push_back (mkLoop ({ 5, 3, 0, 2 },
            mk ("0:2:2 2:2:0", "0:2:3 2:2:5", "0:2:4 2:2:2", "0:2:4 2:2:6"), "0:2:2 2:2:4",
            mk ("0:2:7 2:2:9", "0:2:10 2:2:12", "0:2:11 2:2:9", "0:2:11 2:2:13"), "0:2:13 2:2:9"));
        // waltz I-IV-V-I
        l.push_back (mkLoop ({ 0, 3, 4, 0 },
            mk ("0:1:2 1:1:4 2:1:2", "0:2:3 2:1:5", "0:1:4 1:1:6 2:1:4", "0:2:2 2:1:1"), "0:3:0",
            mk ("0:1:7 1:1:9 2:1:11", "0:2:10 2:1:12", "0:1:11 1:1:13 2:1:11", "0:2:9 2:1:8"), "0:3:7", true));
        // waltz vi-IV-I-V
        l.push_back (mkLoop ({ 5, 3, 0, 4 },
            mk ("0:2:2 2:1:0", "0:1:3 1:1:5 2:1:7", "0:2:4 2:1:2", "0:2:6 2:1:4"), "0:3:1",
            mk ("0:2:9 2:1:7", "0:1:10 1:1:12 2:1:14", "0:2:11 2:1:9", "0:2:13 2:1:11"), "0:3:8", true));
        // axis IV-I-V-vi
        l.push_back (mkLoop ({ 3, 0, 4, 5 },
            mk ("0:1.5:3 1.5:.5:5 2:2:7", "0:1.5:4 1.5:.5:2 2:2:0", "0:1:6 1:1:4 2:2:6", "0:1.5:7 1.5:.5:5 2:2:2"), "0:1.5:7 1.5:.5:5 2:2:7",
            mk ("0:1:12 1:1:10 2:2:12", "0:1:11 1:1:9 2:2:7", "0:1:13 1:1:11 2:2:13", "0:1.5:14 1.5:.5:12 2:2:9"), "0:1.5:14 1.5:.5:12 2:2:14"));
        // sweet I-iii-vi-IV
        l.push_back (mkLoop ({ 0, 2, 5, 3 },
            mk ("0:1.5:2 1.5:.5:4 2:2:2", "0:1:4 1:1:6 2:2:4", "0:1.5:7 1.5:.5:5 2:2:2", "0:1:5 1:1:3 2:2:5"), "0:1.5:5 1.5:.5:3 2:2:7",
            mk ("0:1:7 1:1:9 2:2:11", "0:1:13 1:1:11 2:2:13", "0:1:12 1:1:14 2:2:16", "0:1:14 1:1:12 2:2:10"), "0:1:14 1:1:12 2:2:14"));
        // resolved ii-V-I-I
        l.push_back (mkLoop ({ 1, 4, 0, 0 },
            mk ("0:1:5 1:1:3 2:2:5", "0:1.5:4 1.5:.5:6 2:2:8", "0:2:7 2:2:4", "0:2:4 2:2:2"), "0:4:7",
            mk ("0:1:12 1:1:10 2:2:8", "0:1:11 1:1:13 2:2:11", "0:2:14 2:2:11", "0:1:9 1:1:11 2:2:9"), "0:4:14"));
        // walk I-V-vi-iii
        l.push_back (mkLoop ({ 0, 4, 5, 2 },
            mk ("0:1:4 1:1:3 2:1:2 3:1:1", "0:1:1 1:1:2 2:2:4", "0:1:2 1:1:1 2:2:0", "0:1:2 1:1:3 2:2:4"), "0:1:2 1:1:3 2:2:2",
            mk ("0:1:11 1:1:10 2:1:9 3:1:8", "0:1:8 1:1:9 2:2:11", "0:1:9 1:1:8 2:2:7", "0:1:9 1:1:10 2:2:11"), "0:1:9 1:1:10 2:2:9"));
        // bittersweet iii-IV-I-V
        l.push_back (mkLoop ({ 2, 3, 0, 4 },
            mk ("0:2:4 2:1:2 3:1:4", "0:2:5 2:2:3", "0:2:4 2:2:2", "0:1.5:6 1.5:.5:4 2:2:6"), "0:2:4 2:2:1",
            mk ("0:1:9 1:1:11 2:2:13", "0:1:12 1:1:10 2:2:7", "0:1:11 1:1:9 2:2:11", "0:1:13 1:1:11 2:2:13"), "0:2:13 2:2:8"));
        // longing vi-I-V-IV
        l.push_back (mkLoop ({ 5, 0, 4, 3 },
            mk ("0:1.5:2 1.5:.5:0 2:2:2", "0:1:4 1:1:2 2:2:0", "0:1.5:6 1.5:.5:4 2:2:1", "0:1:5 1:1:3 2:2:5"), "0:1:5 1:1:3 2:2:0",
            mk ("0:1:9 1:1:7 2:2:9", "0:1:11 1:1:9 2:2:7", "0:1.5:13 1.5:.5:11 2:2:8", "0:1:12 1:1:10 2:2:12"), "0:1:12 1:1:10 2:2:7"));
        // moods of the hand-written loops: 1 bright, 2 sad, 4 gentle, 8 jazz, 16 lofi, 32 cinematic
        static const int hm[20] { 1, 2, 4, 8 | 16, 0, 32, 1, 2, 8 | 16, 1, 2 | 1, 32 | 2, 0, 0, 1, 4 | 16, 8, 4 | 32, 2, 2 | 16 };
        for (size_t i = 0; i < l.size() && i < 20; ++i) l[i].moods = hm[i];
        for (const auto& g : kGen)
        {
            Loop x = mkLoop ({ g.ch[0], g.ch[1], g.ch[2], g.ch[3] }, mk (g.s[0], g.s[1], g.s[2], g.s[3]), g.s[4], mk (g.s[5], g.s[6], g.s[7], g.s[8]), g.s[9], g.waltz);
            x.moods = g.moods; l.push_back (x);
        }
        return l;
    }();
    return b;
}
const Phrase& preChorus()
{
    static const Phrase p = mk ("0:2:3 2:2:5", "0:2:5 2:2:7", "0:2:6 2:2:8", "0:1:8 1:1:8 2:1:6 3:1:4");   // over ii - IV - V - V
    return p;
}

struct Composer
{
    struct Sec { char type; int bars, start; };
    ComposeCfg c;
    Score& out;
    Rng rng;
    int bpb = 4, key = 0, scaleIdx = 1, modShift = 0;
    const int* sc = kScales[1];
    const int* mask = nullptr;
    float expr = 0.6f, dens = 0.6f;
    double msBeat = 0.0;                 // milliseconds -> beats
    bool useBook = true;
    const Loop* loop = nullptr; const Loop* bridgeLoop = nullptr;

    Composer (const ComposeCfg& cfg, Score& o) : c (cfg), out (o), rng (cfg.seed * 2654435761u + 977u)
    {
        for (int i = 0; i < 5; ++i) rng.next();
        bpb = c.style == ssLullaby ? 3 : 4;
        key = imod (c.key, 12);
        scaleIdx = juce::jlimit (0, 7, c.scale);
        sc = kScales[scaleIdx];
        mask = scaleIdx == 4 ? kMinorPentMask : scaleIdx == 5 ? kMajorPentMask : nullptr;
        expr = juce::jlimit (0.0f, 1.0f, c.expression); dens = juce::jlimit (0.0f, 1.0f, c.density);
        msBeat = c.bpm / 60000.0;
        // the loops that suit each style (indices into the songbook); the seed picks one, the next one is the bridge
        // the loops that suit each style are chosen by mood; the seed picks the theme and (differently) the bridge theme
        static const int styleMask[14] { 4 | 16 | 8, 32 | 2, 2 | 4 | 1, 32 | 4, 0, 1 | 2, 63, 8, 16 | 8, 2 | 32, 64, 128, 256, 512 };
        const auto& bk = book();
        std::vector<int> pool;
        for (size_t i = 0; i < bk.size(); ++i)
            if (c.style == ssLullaby ? bk[i].waltz : (! bk[i].waltz && (bk[i].moods & styleMask[juce::jlimit (0, 13, c.style)]) != 0)) pool.push_back ((int) i);
        if (pool.empty()) pool.push_back (0);
        const size_t n = pool.size();
        const int li = pool[(size_t) (c.seed % (uint32_t) n)];
        loop = &bk[(size_t) li]; out.theme = li;
        bridgeLoop = &bk[(size_t) pool[(size_t) ((c.seed * 2654435761u >> 7) % (uint32_t) n)]];
        if (bridgeLoop == loop) bridgeLoop = &bk[(size_t) pool[(size_t) ((c.seed % (uint32_t) n + 1) % n)]];
        if (loop->waltz) bpb = 3; else if (bpb == 3) bpb = 4;
    }

    int bookChord (const Sec& sec, int b) const
    {
        static const int pre[4] { 1, 3, 4, 4 };
        if (sec.type == 'P') return pre[b % 4];
        if (sec.type == 'C') return bridgeLoop->chords[b % 4];
        return loop->chords[b % 4];
    }

    // the hand-written melody of the section: question and answer phrases, the chorus higher and more rhythmic.
    // Repeats are never copies: the second verse gets anticipations and passing notes, choruses get a counter-melody and a
    // low bass octave, and the last chorus (after the key change) is doubled an octave down.
    int verseCount = 0, chorusCount = 0;
    BookBar vary (BookBar bar, int rep) const
    {
        if (rep <= 0) return bar;
        BookBar o;
        for (size_t k = 0; k < bar.size(); ++k)
        {
            BookNote n = bar[k];
            const bool prevFree = k == 0 || bar[k - 1].b + bar[k - 1].d <= 1.55f;
            if (rep % 2 == 1 && bpb == 4 && std::abs (n.b - 2.0f) < 0.01f && n.d >= 1.5f && prevFree) { n.b -= 0.5f; n.d += 0.5f; }      // anticipation
            if (n.d >= 2.0f && (rep >= 1) && k + 1 == bar.size() && ((int) (n.b * 2) + rep) % 2 == 0)
            {   // split the last long note: it holds, then steps up as a neighbour before the next bar
                o.push_back ({ n.b, n.d - 0.5f, n.deg }); o.push_back ({ n.b + n.d - 0.5f, 0.5f, n.deg + 1 }); continue;
            }
            o.push_back (n);
        }
        return o;
    }
    void bookMelody (const Sec& sec, float energy)
    {
        if (sec.type == 'I') return;
        if (sec.start == 0) { verseCount = 0; chorusCount = 0; }
        const Loop* L = sec.type == 'C' ? bridgeLoop : loop;
        const double sw = (c.style == ssLofi || c.style == ssJazz || c.style == ssLofiLoop) ? 0.17 : 0.0;
        const int rep = sec.type == 'B' ? chorusCount : sec.type == 'A' ? verseCount : 0;
        const bool lastChorus = sec.type == 'B' && modShift != 0;
        const int dbl = (sec.type == 'B' && (lastChorus || c.style == ssBallad || c.style == ssCinematic || c.style == ssRomantic)) ? -7 : 0;
        const bool counter = (sec.type == 'B' || (sec.type == 'A' && rep >= 1)) && bpb == 4 && c.style != ssMinimal;
        for (int b = 0; b < sec.bars; ++b)
        {
            const int g = b / 4, bi = b % 4;
            const bool answer = (g % 2 == 1);
            BookBar bar;
            if (sec.type == 'P') bar = preChorus().bar[bi];
            else if (sec.type == 'O') bar = (b == sec.bars - 1) ? parseBar (bpb == 3 ? "0:3:7" : "0:4:7") : L->va.bar[bi];
            else if (sec.type == 'B') bar = (answer ? L->ca : L->c).bar[bi];
            else bar = (answer ? L->va : L->v).bar[bi];
            if (sec.type == 'A' || sec.type == 'B') bar = vary (bar, rep);
            const int gb = sec.start + b;
            const bool secEnd = b == sec.bars - 1;
            const int chDeg = bookChord (sec, b);
            if (sec.type == 'B')       // the bass drops an octave on the downbeat in the chorus
                add ((double) gb * bpb, bpb - 0.3, bassOf (chDeg) - 12, velOf (energy, -14), 3.0f, rubato (gb, 0.0, false, false));
            for (size_t k = 0; k < bar.size(); ++k)
            {
                const auto& n = bar[k];
                const bool strong = n.b < 0.01f || (bpb == 4 && std::abs (n.b - 2.0f) < 0.01f);
                const double frac = n.b - std::floor (n.b);
                const double off = (sw > 0 && std::abs (frac - 0.5) < 0.01) ? sw : (sw > 0 ? 0.02 : 0.0);
                const double start = (double) gb * bpb + n.b + off;
                double dur = n.d;
                if (sec.type == 'O') dur *= 1.4;
                const int vel = velOf (energy, 8 + (strong ? 3 : 0) + (int) (2.0f * bi) - (sec.type == 'O' ? 6 : 0));
                const int note = midi (n.deg + 7);
                const double rb = rubato (gb, n.b, secEnd, sec.type == 'O');
                add (start, dur * 0.97, note, vel, 8.0f, rb, true);
                if (dbl != 0) add (start, dur * 0.97, midi (n.deg + 7 + dbl), vel - 12, 8.0f, rb, true);
                if (expr > 0.45f && n.d >= 2.0f && chance (0.3f) && (c.style == ssRomantic || c.style == ssNocturne || c.style == ssCinematic || c.style == ssTango))
                    add (start - 0.14, 0.12, midi (n.deg + 8), vel - 14, 2.0f, 0.0, true);          // grace note before a long note
                if (counter && n.d >= 2.0f && n.b + 2.0f <= (float) bpb + 0.01f)
                {   // counter-melody: a chord tone a third or more below, entering one beat after the melody note
                    for (int off2 : { 2, 4 })
                    {
                        const int cn = midi (chDeg + off2 - (chDeg + off2 >= 7 ? 7 : 0));
                        const int d = note - cn;
                        if (d >= 3 && d != 11 && d != 13 && (cn % 12) != (note % 12)) { add (start + 1.0, 0.9, cn, vel - 18, 6.0f, rb, false); break; }
                    }
                }
            }
        }
        if (sec.type == 'A') ++verseCount; else if (sec.type == 'B') ++chorusCount;
    }

    int midi (int deg) const { return 60 + (key > 6 ? key - 12 : key) + modShift + sc[imod (deg, 7)] + 12 * fdiv (deg, 7); }
    float uni() { return rng.uni(); }
    bool chance (float p) { return rng.uni() < p; }
    int pick (int n) { return (int) (rng.next() % (uint32_t) n); }

    // lowest pitch with the pitch class of 'deg' that is above 'floorNote'
    int above (int floorNote, int deg) const { int m = midi (deg); while (m <= floorNote) m += 12; while (m - 12 > floorNote) m -= 12; return m; }
    int bassOf (int deg) const { int m = midi (deg); while (m > 47) m -= 12; while (m < 36) m += 12; return m; }

    bool isChordTone (int deg, const Chord& ch) const
    {
        const int r = imod (deg - ch.deg, 7);
        if (r == 0 || r == 4) return true;
        if (ch.ext == 3) return r == 1; if (ch.ext == 4) return r == 3;
        if (r == 2) return true;
        return (ch.ext == 1 && r == 6) || (ch.ext == 2 && (r == 6 || r == 1));
    }
    bool allowed (int deg) const { return mask == nullptr || mask[imod (deg - 0, 7)] != 0; }

    // ------------------------------------------------------------------ writing notes
    double humanBeats (float amountMs) { return (double) ((uni() - 0.5f) * 2.0f * amountMs * expr) * msBeat; }
    double rubato (int globalBar, double beatInBar, bool sectionEnd, bool outro) const
    {
        if (c.form == 1 || c.form == 2) return 0.0;      // loops stay exactly on the grid
        const double pos = beatInBar / bpb;
        double d = 0;
        if (globalBar % 4 == 3) d += 0.05 * expr * pos;
        if (sectionEnd) d += 0.10 * expr * pos;
        if (outro) d += 0.04 * expr * (double) (globalBar % 8) * pos;
        return d;
    }
    void add (double start, double len, int note, int vel, float jitterMs = 6.0f, double extra = 0.0, bool mel = false)
    {
        if (note < 21 || note > 108) return;
        NoteEv n;
        n.start = std::max (0.0, start + extra + humanBeats (jitterMs));
        n.len = std::max (0.06, len);
        n.note = note; n.voice = mel ? 1 : 0;
        n.vel = juce::jlimit (1, 127, vel + (int) ((uni() - 0.5f) * 8.0f * expr));
        out.notes.push_back (n);
    }

    // ------------------------------------------------------------------ plan
    std::vector<Sec> plan()
    {
        std::vector<Sec> s;
        const int total = std::max (8, c.bars);
        auto push = [&] (char t, int b) { s.push_back ({ t, b, 0 }); };
        if (c.form == 1) push ('A', 8);
        else if (c.form == 2) push ('B', 8);
        else if (c.form == 3) { push ('I', 2); push ('A', 8); push ('B', 8); push ('O', 4); }
        else if (total <= 16) { push ('I', 2); push ('A', 4); push ('B', 4); push ('A', 4); push ('O', 2); }
        else
        {
            push ('I', 4);
            int remain = total - 8;
            static const char seqT[] { 'A', 'P', 'B', 'A', 'P', 'B', 'C', 'B', 'A', 'P', 'B', 'B' };
            static const int seqL[] { 8, 4, 8, 8, 4, 8, 8, 8, 8, 4, 8, 8 };
            for (int i = 0; i < 12 && remain >= seqL[i]; ++i) { push (seqT[i], seqL[i]); remain -= seqL[i]; }
            if (remain > 0) { if (! s.empty()) s.back().bars += remain; }
            push ('O', 4);
        }
        int at = 0;
        for (auto& x : s) { x.start = at; at += x.bars; }
        return s;
    }

    // ------------------------------------------------------------------ harmony
    std::vector<std::array<Chord, 2>> harmony (const Sec& sec, bool& finalSection, std::vector<bool>& split)
    {
        std::vector<std::array<Chord, 2>> h ((size_t) sec.bars);
        split.assign ((size_t) sec.bars, false);
        int p = 0; const int (*bank)[4] = kBankA; int nb = 8;
        if (sec.type == 'B') { bank = kBankB; nb = 6; }
        else if (sec.type == 'P') { bank = kBankP; nb = 4; }
        else if (sec.type == 'C') { bank = kBankC; nb = 4; }
        p = (int) ((c.seed * 7u + (uint32_t) sec.start * 13u + (sec.type == 'A' ? 0u : sec.type == 'B' ? 5u : sec.type == 'P' ? 3u : 9u)) % (uint32_t) nb);
        if (sec.type == 'A' || sec.type == 'I' || sec.type == 'O') p = (int) (c.seed % (uint32_t) nb);       // verses share one progression
        for (int b = 0; b < sec.bars; ++b)
        {
            Chord ch; ch.deg = bank[p][b % 4];
            if (c.style == ssLofiLoop) { static const int L[4][4] { { 0, 5, 1, 4 }, { 1, 4, 0, 5 }, { 0, 2, 5, 4 }, { 3, 2, 1, 4 } }; ch.deg = L[c.seed % 4u][b % 4]; }
            else if (c.style == ssJazz) { static const int J[4][4] { { 1, 4, 0, 5 }, { 2, 5, 1, 4 }, { 0, 5, 1, 4 }, { 3, 4, 2, 5 } }; ch.deg = J[(c.seed + (sec.type == 'B' ? 1u : 0u)) % 4u][b % 4]; }
            if (useBook) ch.deg = bookChord (sec, b);
            const float colour = c.style == ssBossa ? 0.9f : c.style == ssFolk ? 0.15f : c.style == ssTango ? 0.2f : c.style == ssVillage ? 0.05f : c.style == ssLofi ? 0.8f : c.style == ssRomantic ? 0.45f : c.style == ssMinimal ? 0.5f : c.style == ssCinematic ? 0.3f : c.style == ssBallad ? 0.3f : c.style == ssJazz ? 0.95f : c.style == ssLofiLoop ? 0.9f : c.style == ssNocturne ? 0.12f : 0.05f;
            if (chance (colour * (0.4f + 0.6f * dens)))
                ch.ext = c.style == ssMinimal || c.style == ssCinematic ? (chance (0.5f) ? 3 : 2) : ((c.style == ssLofi || c.style == ssJazz || c.style == ssLofiLoop || c.style == ssBossa) ? 1 + pick (2) : 1);
            else if (c.style == ssCinematic && chance (0.15f)) ch.ext = 4;
            if (useBook && ch.ext >= 3) ch.ext = 2;
            h[(size_t) b][0] = ch; h[(size_t) b][1] = ch;
        }
        // cadences and leading chords
        if (sec.bars >= 4 && ! useBook)
        {
            const int last = sec.bars - 1;
            if (sec.type == 'A') { h[(size_t) last][0] = { 4, 0 }; if (sec.bars >= 8) h[(size_t) (last - 1)][0] = { chance (0.5f) ? 3 : 1, 0 }; }
            else if (sec.type == 'B') { h[(size_t) last][0] = { finalSection ? 0 : 0, chance (0.4f) ? 2 : 0 }; if (sec.bars >= 8) h[(size_t) (last - 1)][0] = { 4, 0 }; }
            else if (sec.type == 'P') { h[(size_t) last][0] = { 4, chance (0.4f) ? 4 : 0 }; }
            else if (sec.type == 'C') { h[(size_t) last][0] = { 4, 0 }; if (sec.bars >= 8) h[(size_t) (last - 1)][0] = { 3, 0 }; }
            else if (sec.type == 'O') { h[(size_t) last][0] = { 0, chance (0.6f) ? 2 : 0 }; if (sec.bars >= 2) h[(size_t) (last - 1)][0] = { chance (0.5f) ? 3 : 4, 0 }; }
            else if (sec.type == 'I') { h[(size_t) last][0] = { 4, 0 }; }
            for (int b = 0; b < sec.bars; ++b) h[(size_t) b][1] = h[(size_t) b][0];
        }
        // two chords in some bars: a dominant leading into the next chord
        if (c.style != ssLullaby && c.style != ssMinimal && ! useBook)
            for (int b = 0; b + 1 < sec.bars; ++b)
                if ((b % 4 == 3 || b % 4 == 1) && chance (0.18f + 0.35f * dens))
                {
                    split[(size_t) b] = true;
                    h[(size_t) b][1] = { imod (h[(size_t) b + 1][0].deg + 4, 7), 0 };
                }
        if (useBook && sec.type == 'O' && ! h.empty()) { h.back()[0] = Chord { 0, 2 }; h.back()[1] = h.back()[0]; }      // the song ends at home
        return h;
    }

    // ------------------------------------------------------------------ accompaniment
    struct Arp { int n[8]; };
    Arp arpOf (const Chord& ch) const
    {
        Arp a;
        a.n[0] = bassOf (ch.deg);
        const int second = ch.ext == 3 ? ch.deg + 1 : ch.ext == 4 ? ch.deg + 3 : ch.deg + 2;
        a.n[1] = above (a.n[0], ch.deg + 4);
        a.n[2] = a.n[0] + 12;
        a.n[3] = above (a.n[2], second);
        a.n[4] = above (a.n[3], ch.deg + 4);
        a.n[5] = a.n[2] + 12;
        a.n[6] = above (a.n[5], ch.ext == 1 ? ch.deg + 6 : second);
        a.n[7] = above (a.n[6], ch.ext == 2 ? ch.deg + 8 : ch.deg + 4);
        for (int& x : a.n) while (x > 70) x -= 12;      // the melody (from B4 up) is always the top voice
        return a;
    }
    int velOf (float energy, int bonus = 0) const
    {
        const float contrast = 0.5f + 0.5f * expr;
        return (int) (60.0f + (energy - 0.55f) * 95.0f * contrast + (float) bonus);
    }

    void accompany (int globalBar, const Sec& sec, int barInSec, const Chord& ch, double t0, double t1, float energy, bool secEnd, int nextDeg = -1)
    {
        // t0..t1 = part of the bar (in beats) this chord rules
        const Arp a = arpOf (ch);
        const double barStart = (double) globalBar * bpb;
        const bool outro = sec.type == 'O';
        auto rb = [&] (double bt) { return rubato (globalBar, bt, secEnd, outro); };
        auto note = [&] (double bt, double len, int n, int vel, float jit = 6.0f)
        {
            if (bt < t0 - 1.0e-6 || bt >= t1 - 1.0e-6) return;
            if (bt > 0.01 && ! (bt == (double) (int) bt) && chance ((1.0f - dens) * 0.35f)) return;      // thinner when density is low
            add (barStart + bt, len, n, vel, jit, rb (bt));
        };
        const bool sparse = sec.type == 'I' || sec.type == 'O';
        const float e = energy;
        const int v = velOf (e, -4);
        switch (c.style)
        {
            case ssMinimal:
            {
                static const int idx[8] { 0, 2, 3, 2, 4, 2, 3, 2 };
                static const int idx2[8] { 0, 3, 2, 4, 3, 2, 4, 3 };
                const int* pat = (barInSec % 2 == 0) ? idx : idx2;
                for (int i = 0; i < 8; ++i)
                {
                    if (sparse && i % 2 == 1 && sec.type == 'I' && barInSec < 2) continue;
                    note (i * 0.5, 0.95, a.n[pat[i]], v + (i == 0 ? 7 : (i == 3 || i == 6) ? 4 : 0) - (i % 2) * 3);
                }
                note (0.0, 3.6, a.n[0], v + 4, 3.0f);
                break;
            }
            case ssRomantic:
            {
                static const int idx[8] { 0, 1, 2, 3, 4, 3, 2, 1 }, idx2[8] { 0, 1, 2, 4, 6, 4, 2, 1 };
                const int* pat = (e > 0.7f || barInSec % 4 == 3) ? idx2 : idx;
                const double step = e > 0.75f && chance (0.5f) ? 0.5 : 0.5;
                for (int i = 0; i < 8; ++i) note (i * step, 1.4, a.n[pat[i]], v + (i == 0 ? 8 : i == 4 ? 3 : 0) + (int) (6.0f * std::sin (i * 0.8f)), 8.0f);
                if (e > 0.6f) note (0.0, 3.5, a.n[0] - 12 >= 28 ? a.n[0] - 12 : a.n[0], v - 2);
                break;
            }
            case ssCinematic:
            {
                if (e < 0.6f)
                {
                    note (0.0, bpb - 0.2, a.n[0], v + 4, 3.0f);
                    note (0.0, bpb - 0.2, a.n[3], v - 4, 3.0f); note (0.0, bpb - 0.2, a.n[4], v - 6, 3.0f);
                    static const int idx[6] { 5, 4, 3, 4, 5, 6 };
                    for (int i = 0; i < 6 && ! sparse; ++i) note (1.0 + i * 0.5, 0.9, a.n[idx[i]], v - 8);
                }
                else
                {
                    static const int idx[16] { 0, 2, 4, 3, 5, 3, 4, 2, 1, 2, 4, 3, 6, 3, 4, 2 };
                    for (int i = 0; i < 16; ++i) note (i * 0.25, 0.7, a.n[idx[i]], v + (i % 4 == 0 ? 7 : -2));
                    note (0.0, 1.8, a.n[0] - 12 >= 28 ? a.n[0] - 12 : a.n[0] + 0, v + 6); note (2.0, 1.8, a.n[0], v + 3);
                }
                break;
            }
            case ssLofi:
            {
                const double sw = 0.16 * (0.6 + 0.4 * expr);
                note (0.0, 1.4, a.n[0], v + 4, 4.0f);
                for (int i : { 3, 4, 6 }) note (0.0 + i * 0.0, 1.1, a.n[i], v - 3 - (i == 6 ? 4 : 0), 5.0f);
                note (1.5 + sw, 0.6, a.n[3], v - 8); note (1.5 + sw, 0.6, a.n[5], v - 10);
                note (2.0, 1.0, a.n[1], v);
                if (chance (0.55f)) { note (3.0 + sw, 0.5, a.n[4], v - 9); note (3.0 + sw, 0.5, a.n[6], v - 11); }
                if (chance (0.35f * dens)) note (3.5 + sw, 0.4, a.n[2], v - 6);
                break;
            }
            case ssLullaby:
            {
                note (0.0, 0.95, a.n[0], v + 4, 3.0f);
                for (double b : { 1.0, 2.0 }) { note (b, 0.9, a.n[3] + 0, v - 6); note (b, 0.9, a.n[4], v - 8); }
                if (e > 0.6f) note (0.0, 0.9, a.n[2], v - 4);
                break;
            }
            case ssJazz:   // walking bass, rootless comping on the "Charleston" rhythm
            {
                const double sw = 0.17;
                note (0.0, 0.95, a.n[0], v + 3, 3.0f);
                note (1.0, 0.95, bassOf (ch.deg + 2), v - 2, 3.0f);
                note (2.0, 0.95, bassOf (ch.deg + 4), v, 3.0f);
                note (3.0, 0.95, nextDeg >= 0 ? bassOf (nextDeg + (chance (0.5f) ? -1 : 1)) : a.n[0], v - 6, 3.0f);
                for (int i : { 3, 4, 6, 7 }) note (0.0, 0.7, a.n[i], v - 10, 8.0f);
                for (int i : { 3, 6, 7 }) note (1.5 + sw, 0.5, a.n[i], v - 14, 8.0f);
                if (chance (0.5f)) for (int i : { 4, 6 }) note (3.5 + sw, 0.4, a.n[i], v - 15, 8.0f);
                break;
            }
            case ssLofiLoop:   // laid-back loop: rolled chord, a bass fifth, lazy stabs
            {
                const double sw = 0.18;
                note (0.0, 1.8, a.n[0], v + 2, 4.0f);
                for (int i : { 3, 4, 6 }) note (0.0, 1.5, a.n[i], v - 6, 14.0f);
                note (2.0, 0.9, a.n[1], v - 4, 4.0f);
                for (int i : { 4, 6 }) note (2.5 + sw, 0.6, a.n[i], v - 12, 10.0f);
                if (chance (0.6f)) for (int i : { 3, 6 }) note (3.5 + sw, 0.4, a.n[i], v - 13, 10.0f);
                break;
            }
            case ssNocturne:   // flowing left-hand arpeggios in triplets under a singing melody
            {
                static const int g1[3] { 0, 3, 4 }, g2[3] { 1, 4, 6 };
                for (int bt = 0; bt < 4; ++bt)
                {
                    const int* gg = (bt % 2 == 0) ? g1 : g2;
                    for (int j = 0; j < 3; ++j) note (bt + j / 3.0, 0.9, a.n[gg[j]], v - 6 + (j == 0 ? 6 : 0) + (bt == 0 && j == 0 ? 4 : 0), 5.0f);
                }
                break;
            }
            case ssBossa:   // bossa nova: bass on 1 and the "and" of 2, syncopated comping
            {
                note (0.0, 0.9, a.n[0], v + 3, 4.0f); note (1.5, 0.9, a.n[1], v - 2, 4.0f); note (2.0, 0.9, a.n[0], v, 4.0f); note (3.5, 0.45, a.n[1], v - 4, 4.0f);
                for (double bt : { 0.0, 1.5, 2.5 }) for (int i : { 3, 4, 6, 7 }) note (bt, 0.55, a.n[i], v - 12 - (bt == 0.0 ? 2 : 0), 8.0f);
                if (chance (0.5f)) for (int i : { 4, 6 }) note (3.5, 0.4, a.n[i], v - 16, 8.0f);
                break;
            }
            case ssFolk:    // fingerpicked: a bass note, then a rolling pattern on the higher strings
            {
                static const int pat[8] { 0, 3, 4, 5, 1, 3, 4, 5 };
                for (int i = 0; i < 8; ++i) note (i * 0.5, (i == 0 || i == 4) ? 1.8 : 0.9, a.n[pat[i]], v + (i == 0 ? 6 : i == 4 ? 2 : -4 - (i % 2) * 2), 7.0f);
                break;
            }
            case ssTango:   // habanera: marked bass and chords, with the syncopated "dotted" push
            {
                note (0.0, 0.7, a.n[0], v + 8, 3.0f); note (0.75, 0.25, a.n[1], v - 2, 3.0f);
                for (int i : { 3, 4, 6 }) note (1.0, 0.45, a.n[i], v - 4, 5.0f);
                note (2.0, 0.7, a.n[1], v + 5, 3.0f); note (2.75, 0.25, a.n[0], v - 2, 3.0f);
                for (int i : { 3, 4, 6 }) note (3.0, 0.45, a.n[i], v - 4, 5.0f);
                break;
            }
            case ssVillage: // oom-pah: light, bouncy, a little sparkle
            {
                note (0.0, 0.9, a.n[0], v + 4, 3.0f); for (int i : { 3, 4 }) note (1.0, 0.5, a.n[i], v - 6, 5.0f);
                note (2.0, 0.9, a.n[1], v, 3.0f); for (int i : { 3, 5 }) note (3.0, 0.5, a.n[i], v - 6, 5.0f);
                if (chance (0.4f)) note (3.5, 0.4, a.n[2], v - 9, 5.0f);
                break;
            }
            default: // ballad: block chords with an octave bass
            {
                note (0.0, 2.0, a.n[0], v + 5, 3.0f); note (0.0, 2.0, a.n[2], v - 1, 3.0f);
                for (int i : { 3, 4, 6 }) note (0.0, 1.9, a.n[i], v - 5, 12.0f);
                for (int i : { 3, 4, 6 }) note (2.0, 1.8, a.n[i], v - 9, 8.0f);
                note (2.0, 1.8, a.n[1], v - 3, 3.0f);
                break;
            }
        }
        // bass movement: an approach note into the next chord, and a pushed (anticipated) chord in the choruses
        if (nextDeg >= 0 && nextDeg != ch.deg && t1 >= bpb - 1.0e-6 && bpb == 4 && ! sparse && c.style != ssMinimal && c.style != ssLullaby && c.style != ssJazz && c.style != ssNocturne && chance (0.30f + 0.45f * dens))
        {
            note (bpb - 0.5, 0.45, bassOf (nextDeg + (chance (0.5f) ? -1 : 1)), v - 8, 4.0f);
            if ((sec.type == 'B' || sec.type == 'P') && (c.style == ssBallad || c.style == ssLofi || c.style == ssCinematic) && chance (0.55f))
            {
                const Arp na = arpOf ({ nextDeg, 0 });
                for (int i : { 3, 4, 6 }) note (bpb - 0.5, 0.7, na.n[i], v - 6, 5.0f);
            }
        }
    }

    // ------------------------------------------------------------------ melody
    std::vector<Mot> genMotif (int kind, int lenBars)
    {
        std::vector<Mot> m;
        const bool waltz = bpb == 3;
        struct R { double b, d; };
        static const std::vector<R> r44[] {
            { { 0, 1.5 }, { 1.5, 0.5 }, { 2, 2 }, { 4, 1 }, { 5, 1 }, { 6, 2 } },
            { { 0, 1 }, { 1, 0.5 }, { 1.5, 0.5 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 6, 1 }, { 7, 1 } },
            { { 0, 0.75 }, { 0.75, 0.75 }, { 1.5, 1.5 }, { 3, 1 }, { 4, 0.75 }, { 4.75, 0.75 }, { 5.5, 1.5 }, { 7, 1 } },
            { { 0, 0.5 }, { 0.5, 0.5 }, { 1, 1 }, { 2, 2 }, { 4, 0.5 }, { 4.5, 0.5 }, { 5, 1 }, { 6, 2 } },
            { { 0, 3 }, { 3, 1 }, { 4, 3 }, { 7, 1 } },
            { { 0, 0.5 }, { 0.5, 0.5 }, { 1, 0.5 }, { 1.5, 0.5 }, { 2, 1 }, { 3, 1 }, { 4, 0.5 }, { 4.5, 0.5 }, { 5, 1 }, { 6, 2 } } };
        static const std::vector<R> r34[] {
            { { 0, 2 }, { 2, 1 }, { 3, 2 }, { 5, 1 } }, { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 2 }, { 5, 1 } }, { { 0, 1.5 }, { 1.5, 0.5 }, { 2, 1 }, { 3, 3 } } };
        const std::vector<R>& rr = waltz ? r34[kind % 3] : r44[kind % 6];
        (void) lenBars;
        int cum = 0;
        for (size_t i = 0; i < rr.size(); ++i)
        {
            int step;
            if (i == 0) step = 0;
            else
            {
                const float u = uni();
                step = u < 0.27f ? 1 : u < 0.54f ? -1 : u < 0.64f ? 2 : u < 0.74f ? -2 : u < 0.82f ? 0 : u < 0.9f ? 3 : u < 0.96f ? -3 : 4;
                if (kind % 3 == 1 && i == 1 && chance (0.6f)) step = 3 + pick (2);          // a leap up, then settle back: the hook
                if (cum > 4) step = -std::abs (step); else if (cum < -3) step = std::abs (step);
                if (i + 1 == rr.size() && std::abs (step) > 2) step = step > 0 ? 1 : -1;   // end by step
            }
            cum += step;
            m.push_back ({ rr[i].b, rr[i].d, step });
        }
        return m;
    }

    int snapChord (int deg, const Chord& ch) const
    {
        for (int d = 0; d <= 3; ++d) { if (isChordTone (deg + d, ch) && allowed (deg + d)) return deg + d; if (isChordTone (deg - d, ch) && allowed (deg - d)) return deg - d; }
        return deg;
    }
    int snapAllowed (int deg) const { for (int d = 0; d <= 2; ++d) { if (allowed (deg + d)) return deg + d; if (allowed (deg - d)) return deg - d; } return deg; }

    // realise a motif in a section: returns the last degree
    int realise (const std::vector<Mot>& m, int startDeg, int shift, int globalBar0, const Sec& sec, int barInSec0,
                 const std::vector<std::array<Chord, 2>>& h, int lo, int hi, float energy, bool variant, bool finalLong, int dbl, double slow)
    {
        int deg = startDeg + shift;
        for (size_t i = 0; i < m.size(); ++i)
        {
            const auto& n = m[i];
            deg += n.step;
            while (deg > hi) deg -= 7; while (deg < lo) deg += 7;
            const int barOff = (int) std::floor (n.beat / bpb);
            const int bis = barInSec0 + barOff;
            if (bis >= sec.bars) break;
            const double inBar = n.beat - barOff * bpb;
            const Chord ch = h[(size_t) bis][inBar >= bpb / 2.0 && h[(size_t) bis][1].deg != h[(size_t) bis][0].deg ? 1 : 0];
            const bool strong = inBar < 0.01 || (bpb == 4 && std::abs (inBar - 2.0) < 0.01);
            if (strong || n.dur >= 2.0) deg = snapChord (deg, ch); else deg = snapAllowed (deg);
            double dur = n.dur * slow;
            if (variant && i + 1 == m.size()) { deg += chance (0.5f) ? 1 : -1; deg = snapAllowed (deg); dur *= 1.3; }
            if (finalLong && i + 1 == m.size()) dur = std::max (dur, (double) bpb * 1.6);
            const int gb = globalBar0 + barOff;
            const bool secEnd = bis >= sec.bars - 1;
            const double start = (double) gb * bpb + inBar * slow;
            const int vel = velOf (energy, 7 + (strong ? 3 : 0) + (int) (5.0f * std::sin (6.28f * (float) i / (float) m.size() * 0.5f)));
            add (start, dur * 0.97, midi (deg), vel + 4, 9.0f, rubato (gb, inBar, secEnd, sec.type == 'O') + 0.01 * expr, true);
            if (dbl != 0) add (start, dur * 0.97, midi (deg + dbl), vel - 12, 9.0f, rubato (gb, inBar, secEnd, sec.type == 'O'), true);
            if (expr > 0.45f && n.dur >= 2.0 && chance (0.3f) && c.style != ssMinimal)              // grace note before a long note
                add (start - 0.14, 0.12, midi (deg + 1), vel - 14, 2.0f, 0.0, true);
        }
        return deg;
    }

    void melody (const Sec& sec, int globalBar0, const std::vector<std::array<Chord, 2>>& h, float energy, int& motifKind, std::vector<Mot>& motA, std::vector<Mot>& hook)
    {
        if (sec.type == 'I')
        {
            if (sec.bars >= 4 && (c.style == ssMinimal || c.style == ssRomantic || c.style == ssCinematic || chance (0.4f)))
                realise (motA, 9, 0, globalBar0 + sec.bars - 2, sec, sec.bars - 2, h, 5, 16, 0.3f, false, false, 0, 1.0);
            return;
        }
        const int lo = sec.type == 'B' ? 7 : 4, hi = sec.type == 'B' ? 19 : 16;
        const bool pre = sec.type == 'P';
        const int centre = (c.style == ssLullaby ? 11 : 9) + (sec.type == 'B' ? 2 : 0);
        const int dbl = (sec.type == 'B' && (c.style == ssCinematic || c.style == ssBallad || c.style == ssRomantic)) ? -7 : 0;
        const double slow = sec.type == 'O' ? 1.5 : 1.0;
        int cur = centre;
        const int units = sec.bars / 2;
        std::vector<Mot> neu = genMotif ((motifKind + 2) % 6, 2);
        for (int u = 0; u < units; ++u)
        {
            const int barIn = u * 2;
            const std::vector<Mot>* m = &motA;
            int shift = 0; bool variant = false, longEnd = false;
            if (sec.type == 'B') { m = &hook; shift = (u % 4 == 2) ? 2 : 0; variant = false; longEnd = (u % 4 == 3); }
            else if (pre) { shift = u; variant = false; longEnd = false; }
            else if (sec.type == 'C') { m = &neu; shift = -(u % 4 == 0 ? 0 : u % 4); longEnd = (u % 4 == 3); }
            else if (sec.type == 'O') { shift = -(u % 2); longEnd = (u == units - 1); }
            else { shift = (u % 4 == 2) ? 1 : 0; variant = (u % 4 == 1) && c.style != ssLofiLoop; longEnd = (u % 4 == 3); }
            if ((u % 4 == 3 && ! pre) || (sec.type == 'O' && u == units - 1))
            {
                // cadence unit: a slower answer ending on a stable note
                static const std::vector<Mot> cad { { 0, 1.5, 0 }, { 1.5, 0.5, -1 }, { 2, 2, -1 } };
                static const std::vector<Mot> cadW { { 0, 1, 0 }, { 1, 1, -1 }, { 2, 3, -1 } };
                const auto& cm = bpb == 3 ? cadW : cad;
                const int endDeg = sec.type == 'B' || sec.type == 'O' ? 7 + (sec.type == 'B' ? 7 : 0) : 8;
                cur = std::max (lo + 1, std::min (hi, cur));
                cur = realise (cm, endDeg + 2, 0, globalBar0 + barIn, sec, barIn, h, lo, hi, energy, false, true, dbl, slow);
            }
            else
                cur = realise (*m, cur, shift, globalBar0 + barIn, sec, barIn, h, lo, hi, energy * (0.9f + 0.1f * (float) u / (float) std::max (1, units)), variant, longEnd, dbl, slow);
            if (cur > hi - 1) cur -= 3;
            if (cur < lo + 1) cur += 3;
            if (sec.type == 'B' && u == 2) cur = std::min (hi, centre + 5);      // the peak of the chorus
            if (pre && u == units - 1 && bpb == 4)                                 // pick-up into the chorus
            {
                const double at = (double) (globalBar0 + sec.bars - 1) * bpb;
                add (at + 2.5, 0.45, midi (centre + 2), energy * 80.0f, 6.0f, 0.0, true); add (at + 3.0, 0.45, midi (centre + 3), energy * 85.0f, 6.0f, 0.0, true); add (at + 3.5, 0.45, midi (centre + 4), energy * 92.0f, 6.0f, 0.0, true);
            }
        }
    }

    // ------------------------------------------------------------------ pedal
    void pedalFor (int globalBar, const Sec& sec, int bis, bool split, double lastTotal)
    {
        (void) lastTotal; (void) sec; (void) bis;
        if (! c.pedal) return;
        const double b0 = (double) globalBar * bpb;
        auto change = [&] (double at) { if (at > 0.05) out.pedal.push_back ({ at - 0.07, 0 }); out.pedal.push_back ({ at + 0.02, 127 }); };
        change (b0);
        if (split) change (b0 + bpb / 2.0);
    }

    void run()
    {
        out.notes.clear(); out.pedal.clear(); out.sections.clear(); out.chordMasks.clear();
        out.beatsPerBar = bpb;
        const auto secs = plan();
        int totalBars = 0; for (auto& s : secs) totalBars += s.bars;
        out.bars = totalBars;
        int motifKind = (int) (c.seed % 6u); if (bpb == 3) motifKind %= 3;
        std::vector<Mot> motA = genMotif (motifKind, 2), hook = genMotif ((motifKind + 3) % (bpb == 3 ? 3 : 6), 2);
        for (size_t si = 0; si < secs.size(); ++si)
        {
            const auto& sec = secs[si];
            out.sections.push_back ({ sec.type, sec.start, sec.bars });
            bool fin = si + 2 >= secs.size();
            modShift = (sec.type == 'B' && si + 2 >= secs.size() && totalBars >= 32 && c.style != ssLullaby) ? 2 : 0;   // the last chorus lifts a tone
            std::vector<bool> split;
            const auto h = harmony (sec, fin, split);
            for (int b = 0; b < sec.bars; ++b)
            {
                const int gb = sec.start + b;
                const float prog = (float) b / (float) std::max (1, sec.bars - 1);
                float e = sec.type == 'I' ? 0.28f + 0.1f * prog : sec.type == 'A' ? 0.50f + 0.05f * prog : sec.type == 'P' ? 0.55f + 0.30f * prog : sec.type == 'B' ? 0.82f + 0.08f * prog
                        : sec.type == 'C' ? 0.52f + 0.35f * prog : 0.36f - 0.2f * prog;
                if (c.style == ssLofi || c.style == ssLofiLoop) e *= 0.85f; if (c.style == ssLullaby) e *= 0.8f;
                const bool secEnd = b == sec.bars - 1;
                {
                    unsigned mask = 0;
                    const Chord& cc0 = h[(size_t) b][0];
                    for (int off : { 0, 2, 4, (cc0.ext == 1 ? 6 : -1), (cc0.ext == 2 ? 8 : -1) }) if (off >= 0) mask |= 1u << (unsigned) imod (midi (cc0.deg + off), 12);
                    out.chordMasks.push_back ({ (double) gb * bpb, mask });
                }
                if (c.mode != 3)
                {
                    const int nd = b + 1 < sec.bars ? h[(size_t) b + 1][0].deg : -1;
                    if (split[(size_t) b]) { accompany (gb, sec, b, h[(size_t) b][0], 0, bpb / 2.0, e, secEnd); accompany (gb, sec, b, h[(size_t) b][1], bpb / 2.0, bpb, e, secEnd, nd); }
                    else accompany (gb, sec, b, h[(size_t) b][0], 0, bpb, e, secEnd, nd);
                    // final chord: a rolled chord held under the pedal
                    if (sec.type == 'O' && secEnd)
                    {
                        const Arp a = arpOf (h[(size_t) b][0]);
                        for (int i = 0; i < 6; ++i) add ((double) gb * bpb + 0.02 + i * 0.09, bpb * 2.2, a.n[i == 0 ? 0 : i + (i > 3 ? 1 : 0)], velOf (0.32f) - 2, 1.0f);
                    }
                }
                pedalFor (gb, sec, b, split[(size_t) b], 0);
            }
            if (c.mode != 2)
            {
                float en = sec.type == 'I' ? 0.3f : sec.type == 'A' ? 0.52f : sec.type == 'P' ? 0.66f : sec.type == 'B' ? 0.86f : sec.type == 'C' ? 0.68f : 0.34f;
                if (c.style == ssLofi || c.style == ssLofiLoop) en *= 0.9f;
                if (useBook) bookMelody (sec, en); else melody (sec, sec.start, h, en, motifKind, motA, hook);
            }
        }
        // the very end: let everything ring
        const bool loopForm = c.form == 1 || c.form == 2;
        out.totalBeats = (double) totalBars * bpb + (loopForm ? 0.0 : 2.0);
        if (loopForm)
        {   // a loop must end clean: nothing sounds past the last beat, the pedal is up
            const double end = out.totalBeats;
            for (auto& n : out.notes) n.len = std::min (n.len, end - 0.03 - n.start);
            out.notes.erase (std::remove_if (out.notes.begin(), out.notes.end(), [end] (const NoteEv& n) { return n.len < 0.05 || n.start >= end - 0.05; }), out.notes.end());
            out.pedal.erase (std::remove_if (out.pedal.begin(), out.pedal.end(), [end] (const PedEv& p) { return p.beat >= end - 0.06; }), out.pedal.end());
            if (c.pedal) out.pedal.push_back ({ end - 0.06, 0 });
        }
        else if (c.pedal) out.pedal.push_back ({ out.totalBeats - 0.5, 0 });
        std::stable_sort (out.notes.begin(), out.notes.end(), [] (const NoteEv& a, const NoteEv& b) { return a.start < b.start; });
        std::stable_sort (out.pedal.begin(), out.pedal.end(), [] (const PedEv& a, const PedEv& b) { return a.beat < b.beat; });
    }
};
}


// ---------------------------------------------------------------------------------------------------------------------------
// Classical: Pachelbel's Canon in D (public domain). The famous ground bass (D A Bm F#m G D G A, two beats each) and the
// descending violin line over it, followed by variations that get busier cycle after cycle: half notes, quarters, eighths,
// arpeggiated sixteenths, a lyrical dotted variation, and the theme again, softly, in the high register.
// ---------------------------------------------------------------------------------------------------------------------------
static void composeCanon (const ComposeCfg& cfg, Score& out)
{
    Rng rng (cfg.seed * 2654435761u + 4242u);
    for (int i = 0; i < 4; ++i) rng.next();
    const int cycles = std::max (3, cfg.bars / 4);
    int tr = ((cfg.key - 2) % 12 + 12) % 12; if (tr > 6) tr -= 12;                  // D major unless another key is chosen
    const float expr = juce::jlimit (0.0f, 1.0f, cfg.expression);
    const double msBeat = cfg.bpm / 60000.0;
    static const int semis[7] { 2, 4, 6, 7, 9, 11, 13 };                             // D E F# G A B C#
    auto dm = [&] (int deg) { return 60 + tr + semis[((deg % 7) + 7) % 7] + 12 * (int) std::floor ((double) deg / 7.0); };
    static const int rootDeg[8] { 0, 4, 5, 2, 3, 0, 3, 4 };                          // D A Bm F#m G D G A
    static const int lineDeg[8] { 9, 8, 7, 6, 5, 4, 5, 6 };                          // F# E D C# B A B C#   (the violin line)
    out.notes.clear(); out.pedal.clear(); out.sections.clear();
    out.beatsPerBar = 4; out.bars = cycles * 4;
    out.totalBeats = (double) out.bars * 4 + 2.0;
    for (int k = 0; k < cycles; ++k)
        out.sections.push_back ({ k == 0 ? 'I' : k == cycles - 1 ? 'O' : k == cycles - 2 ? 'B' : 'A', k * 4, 4 });
    auto add = [&] (double start, double len, int note, int vel, float jit = 5.0f, bool mel = false)
    {
        if (note < 21 || note > 108) return;
        NoteEv n;
        n.start = std::max (0.0, start + (double) ((rng.uni() - 0.5f) * 2.0f * jit * expr) * msBeat);
        n.len = std::max (0.08, len); n.note = note; n.voice = mel ? 1 : 0;
        n.vel = juce::jlimit (1, 127, vel + (int) ((rng.uni() - 0.5f) * 8.0f * expr));
        out.notes.push_back (n);
    };
    static const int baseVel[8] { 44, 50, 56, 64, 72, 62, 52, 40 };
    for (int k = 0; k < cycles; ++k)
    {
        const int v = std::min (7, k);
        const bool last = k == cycles - 1;
        const int variant = (int) ((cfg.seed + (uint32_t) k) % 3u);
        for (int c = 0; c < 8; ++c)
        {
            const double t0 = (double) (k * 16 + c * 2);                              // two beats per chord
            const int rd = rootDeg[c];
            const int bass = dm (rd - 14);
            const int vel = baseVel[last ? 7 : v] + (c % 2 == 0 ? 3 : 0);
            const int m = lineDeg[c], nxt = lineDeg[(c + 1) % 8], dir = nxt > m ? 1 : nxt < m ? -1 : 1;
            // chord tones around the melody register (degrees stacked in thirds)
            const int t0d = rd + 7, t1d = rd + 9, t2d = rd + 11;                      // octave above the root: 1, 3, 5
            if (cfg.mode != 3)
            {
                add (t0, 2.0, bass, vel + 8, 3.0f);
                if (k >= 1 && ! last) for (int e = 0; e < 4; ++e) add (t0 + 0.5 * e, 0.95, dm (rd - 7 + (e == 1 || e == 3 ? 2 : e == 2 ? 4 : 0)), vel - 16, 5.0f);
                if (k >= 3 && k <= 4) add (t0 + 1.0, 0.9, bass + 12, vel - 12, 5.0f);
            }
            if (cfg.mode != 2 && k >= 0)
            {
                if (k == 0 || last)                                                    // the theme in half notes (soft, high at the end)
                    add (t0, 2.0, dm (m) + (last ? 12 : 0), vel + 6, 8.0f, true);
                else if (k == 1)                                                       // quarters: the line and a step towards the next note
                { add (t0, 1.0, dm (m), vel + 8, 6.0f, true); add (t0 + 1.0, 1.0, dm (m + dir), vel + 4, 6.0f, true); }
                else if (k == 2)                                                       // eighths
                {
                    static const int p[3][4] { { 0, 1, 0, -1 }, { 0, 2, 1, 0 }, { 0, -1, -2, -1 } };
                    for (int e = 0; e < 4; ++e) add (t0 + 0.5 * e, 0.55, dm (m + p[variant][e] * (e ? dir : 1)), vel + 6 + (e == 0 ? 6 : 0), 5.0f, true);
                }
                else if (k == 3)                                                       // arpeggiated sixteenths on the chord
                {
                    const int arp[3][8] { { 0, 1, 2, 1, 0, 1, 2, 1 }, { 2, 1, 0, 1, 2, 1, 0, 1 }, { 0, 2, 1, 2, 0, 2, 1, 2 } };
                    const int tones[3] { t0d, t1d, t2d };
                    for (int e = 0; e < 8; ++e) add (t0 + 0.25 * e, 0.3, dm (tones[arp[variant][e]]), vel + 4 + (e % 4 == 0 ? 7 : 0), 4.0f, true);
                }
                else if (k == 4)                                                       // lyrical: dotted rhythm, a long note and a step
                { add (t0, 1.5, dm (m + (c % 2 == 0 ? 1 : 0)), vel + 10, 7.0f, true); add (t0 + 1.5, 0.5, dm (m + dir), vel + 2, 5.0f, true); }
                else                                                                   // later variations alternate between runs and long notes
                {
                    if (c % 2 == 0) for (int e = 0; e < 4; ++e) add (t0 + 0.5 * e, 0.55, dm (m + (e % 2 ? dir : 0)), vel + 4, 5.0f, true);
                    else add (t0, 2.0, dm (m + 1), vel + 6, 6.0f, true);
                }
            }
            if (cfg.pedal) { if (c > 0 || k > 0) out.pedal.push_back ({ t0 - 0.06, 0 }); out.pedal.push_back ({ t0 + 0.02, 127 }); }
        }
    }
    // the ending: a rolled D major chord held
    {
        const double e0 = (double) (cycles * 16);
        for (int i = 0; i < 5; ++i) add (e0 - 0.0 + 0.05 * i, 3.5, dm (i == 0 ? -14 : i == 1 ? -7 : i == 2 ? 0 : i == 3 ? 2 : 4), 40, 1.0f);
        out.pedal.push_back ({ e0 + 2.4, 0 });
    }
    std::stable_sort (out.notes.begin(), out.notes.end(), [] (const NoteEv& a, const NoteEv& b) { return a.start < b.start; });
    std::stable_sort (out.pedal.begin(), out.pedal.end(), [] (const PedEv& a, const PedEv& b) { return a.beat < b.beat; });
}

// how "song-like" the melody is: stepwise motion, repeated rhythmic cells, a singable amount of notes per bar
static float quality (const Score& sc)
{
    std::vector<const NoteEv*> mel;
    for (auto& n : sc.notes) if (n.voice == 1 && n.vel > 20) mel.push_back (&n);
    if (mel.size() < 8) return -100.0f;
    double leap = 0; int big = 0;
    for (size_t i = 1; i < mel.size(); ++i) { const int d = std::abs (mel[i]->note - mel[i - 1]->note); if (d < 12 && mel[i]->start - mel[i - 1]->start > 0.02) { leap += d; if (d > 7) ++big; } }
    const double mean = leap / (double) (mel.size() - 1);
    // rhythmic cells per bar: how many bars repeat the onset pattern of one, two or four bars earlier
    const int bars = sc.bars;
    std::vector<uint32_t> mask ((size_t) bars, 0u);
    std::vector<int> count ((size_t) bars, 0);
    for (auto* n : mel)
    {
        const int b = (int) (n->start / sc.beatsPerBar);
        if (b >= 0 && b < bars) { const int slot = (int) std::floor ((n->start - b * sc.beatsPerBar) * 2.0 + 0.5); mask[(size_t) b] |= 1u << juce::jlimit (0, 15, slot); ++count[(size_t) b]; }
    }
    int rep = 0, withNotes = 0, busy = 0;
    for (int b = 0; b < bars; ++b)
    {
        if (mask[(size_t) b] == 0) continue;
        ++withNotes;
        if ((b >= 1 && mask[(size_t) b] == mask[(size_t) b - 1]) || (b >= 2 && mask[(size_t) b] == mask[(size_t) b - 2]) || (b >= 4 && mask[(size_t) b] == mask[(size_t) b - 4])) ++rep;
        if (count[(size_t) b] > 11) ++busy;
    }
    const double repRatio = withNotes > 0 ? (double) rep / withNotes : 0.0;
    const double presence = (double) withNotes / bars;
    return (float) (3.0 * std::min (repRatio, 0.7) - std::abs (mean - 2.6) * 0.5 - 0.15 * big / std::max (1.0, (double) mel.size() / 8.0) - 1.5 * busy / std::max (1, withNotes) + 1.0 * std::min (presence, 0.7));
}

void compose (const ComposeCfg& cfg, Score& out)
{
    if (cfg.style == ssClassic) { composeCanon (cfg, out); return; }
    // several takes, the most song-like one is kept (deterministic: the takes come from sub-seeds)
    Score best; float bestQ = -1.0e9f;
    for (int i = 0; i < 6; ++i)
    {
        ComposeCfg c2 = cfg;
        c2.seed = cfg.seed * 2246822519u + (uint32_t) i * 3266489917u + 374761393u;
        Score s;
        Composer c (c2, s);
        c.run();
        const float q = cfg.mode == 2 ? 0.0f : quality (s);
        if (q > bestQ) { bestQ = q; best = std::move (s); }
        if (cfg.mode == 2) break;
    }
    out = std::move (best);
}
}
