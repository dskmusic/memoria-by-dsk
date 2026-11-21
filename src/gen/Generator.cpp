#include "Generator.h"
#include "../preset/Render.h"

namespace gx
{
namespace
{
struct Range { float lo, hi; };
struct Variant { const char* name; int world; int room; float memBias; };   // sub-category: where / how it lives

struct Arch
{
    const char* cat;
    std::vector<int> pianos;          // weighted by repetition
    Range tone, hammer, sustain, damper, detune, stretch, dynamics, body, resonance, width, room, roomSize;
    Range memory, warmth, intimacy, time, space, tape, vinyl, wood, worldLevel;
    std::array<Variant, 3> sub;
    std::vector<const char*> adj, noun;
};

enum { W_OFF, W_RAIN, W_CAFE, W_FIRE, W_BIRDS, W_AUDIENCE };
enum { R_LIVING, R_CHURCH, R_STUDIO, R_CABIN, R_HALL };

const std::vector<Arch>& archs()
{
    static const std::vector<Arch> a {
        { "Lo-Fi", { 1, 1, 0, 0 }, { 0.30f, 0.55f }, { 0.3f, 0.6f }, { 0.4f, 0.6f }, { 0.4f, 0.7f }, { 0.4f, 0.8f }, { 0.5f, 0.8f }, { 0.3f, 0.5f }, { 0.5f, 0.8f }, { 0.3f, 0.6f }, { 0.7f, 1.1f }, { 0.15f, 0.35f }, { 0.2f, 0.4f },
          { 0.55f, 0.95f }, { 0.4f, 0.9f }, { 0.5f, 0.9f }, { 0.4f, 0.9f }, { 0.2f, 0.5f }, { 0.4f, 0.8f }, { 0.3f, 0.7f }, { 0.3f, 0.8f }, { 0.25f, 0.6f },
          { { { "Tape Dream", W_OFF, R_LIVING, 0.0f }, { "Dusty Vinyl", W_RAIN, R_LIVING, 0.0f }, { "Bedroom", W_CAFE, R_CABIN, -0.1f } } },
          { "Dusty", "Faded", "Late Night", "Sunday", "Bedroom", "Borrowed", "Slow", "Tape", "Washed", "Warm", "Hazy" },
          { "Memories", "Morning", "Notes", "Letter", "Tapes", "Loop", "Afternoon", "Hours", "Drive", "Postcards" } },
        { "Felt", { 2, 2, 2, 1 }, { 0.15f, 0.4f }, { 0.1f, 0.4f }, { 0.3f, 0.55f }, { 0.5f, 0.8f }, { 0.2f, 0.5f }, { 0.3f, 0.6f }, { 0.4f, 0.7f }, { 0.4f, 0.7f }, { 0.3f, 0.6f }, { 0.5f, 0.9f }, { 0.1f, 0.3f }, { 0.15f, 0.35f },
          { 0.15f, 0.5f }, { 0.5f, 0.9f }, { 0.7f, 1.0f }, { 0.3f, 0.7f }, { 0.1f, 0.4f }, { 0.1f, 0.4f }, { 0.0f, 0.3f }, { 0.5f, 0.9f }, { 0.2f, 0.5f },
          { { { "Soft", W_OFF, R_LIVING, 0.0f }, { "Intimate", W_OFF, R_STUDIO, -0.05f }, { "Muted", W_FIRE, R_CABIN, 0.1f } } },
          { "Soft", "Quiet", "Gentle", "Hushed", "Muffled", "Tender", "Close", "Whispered", "Velvet", "Cotton" },
          { "Hands", "Keys", "Lamp", "Blanket", "Thoughts", "Room", "Lullabies", "Candle", "Breath", "Pages" } },
        { "Cinematic", { 0, 0, 0, 2 }, { 0.4f, 0.7f }, { 0.4f, 0.7f }, { 0.55f, 0.85f }, { 0.4f, 0.6f }, { 0.1f, 0.35f }, { 0.5f, 0.8f }, { 0.6f, 0.9f }, { 0.4f, 0.7f }, { 0.5f, 0.8f }, { 0.9f, 1.3f }, { 0.4f, 0.7f }, { 0.5f, 0.85f },
          { 0.05f, 0.4f }, { 0.2f, 0.6f }, { 0.2f, 0.6f }, { 0.2f, 0.6f }, { 0.55f, 0.9f }, { 0.0f, 0.25f }, { 0.0f, 0.15f }, { 0.1f, 0.4f }, { 0.2f, 0.5f },
          { { { "Emotional", W_OFF, R_HALL, 0.05f }, { "Hopeful", W_BIRDS, R_HALL, 0.0f }, { "Tension", W_RAIN, R_CHURCH, 0.1f } } },
          { "Distant", "Last", "Silent", "Falling", "Rising", "Golden", "Broken", "Quiet", "Endless", "Winter" },
          { "Horizon", "Farewell", "Credits", "Return", "Embers", "Dawn", "Theme", "Station", "Promise", "Light" } },
        { "Rainy", { 0, 1, 1, 0 }, { 0.3f, 0.55f }, { 0.35f, 0.6f }, { 0.45f, 0.65f }, { 0.4f, 0.7f }, { 0.3f, 0.6f }, { 0.5f, 0.8f }, { 0.4f, 0.7f }, { 0.5f, 0.8f }, { 0.4f, 0.7f }, { 0.8f, 1.1f }, { 0.2f, 0.45f }, { 0.2f, 0.45f },
          { 0.3f, 0.75f }, { 0.4f, 0.8f }, { 0.4f, 0.8f }, { 0.3f, 0.8f }, { 0.2f, 0.5f }, { 0.2f, 0.6f }, { 0.1f, 0.5f }, { 0.3f, 0.7f }, { 0.4f, 0.8f },
          { { { "Window", W_RAIN, R_LIVING, 0.0f }, { "Cafe", W_CAFE, R_LIVING, 0.0f }, { "Night", W_RAIN, R_CABIN, 0.1f } } },
          { "Rainy", "Grey", "Wet", "Drizzle", "Blue", "Misty", "Umbrella", "Puddle", "Cold", "Evening" },
          { "Window", "Window Seat", "Tea", "Streets", "Afternoon", "Café", "Train", "Reflections", "Thoughts", "Season" } },
        { "Vintage", { 1, 1, 3, 3, 0 }, { 0.35f, 0.65f }, { 0.4f, 0.7f }, { 0.35f, 0.55f }, { 0.4f, 0.7f }, { 0.65f, 1.0f }, { 0.5f, 0.9f }, { 0.4f, 0.7f }, { 0.6f, 0.9f }, { 0.3f, 0.6f }, { 0.7f, 1.0f }, { 0.2f, 0.4f }, { 0.2f, 0.4f },
          { 0.6f, 1.0f }, { 0.5f, 0.9f }, { 0.4f, 0.8f }, { 0.7f, 1.0f }, { 0.2f, 0.5f }, { 0.5f, 0.9f }, { 0.4f, 0.9f }, { 0.4f, 0.9f }, { 0.2f, 0.5f },
          { { { "Old Upright", W_OFF, R_LIVING, 0.0f }, { "Honky-Tonk", W_AUDIENCE, R_LIVING, 0.0f }, { "Attic Tape", W_OFF, R_CABIN, 0.1f } } },
          { "Grandmother's", "Attic", "Saloon", "Old", "Forgotten", "Antique", "Yellowed", "Sepia", "Creaky", "1974" },
          { "Parlor", "Upright", "Reel", "Rag", "Photographs", "Trunk", "Jukebox", "Ballroom", "Music Room", "Recital" } },
        { "Church", { 0, 0, 2 }, { 0.35f, 0.6f }, { 0.3f, 0.55f }, { 0.55f, 0.85f }, { 0.4f, 0.6f }, { 0.1f, 0.4f }, { 0.5f, 0.8f }, { 0.5f, 0.8f }, { 0.4f, 0.7f }, { 0.6f, 0.9f }, { 1.0f, 1.4f }, { 0.6f, 0.9f }, { 0.7f, 1.0f },
          { 0.0f, 0.35f }, { 0.3f, 0.7f }, { 0.1f, 0.4f }, { 0.2f, 0.6f }, { 0.6f, 1.0f }, { 0.0f, 0.2f }, { 0.0f, 0.1f }, { 0.1f, 0.4f }, { 0.2f, 0.4f },
          { { { "Nave", W_OFF, R_CHURCH, 0.0f }, { "Chapel", W_BIRDS, R_CHURCH, 0.0f }, { "Cathedral", W_OFF, R_HALL, 0.0f } } },
          { "Sacred", "Stone", "Vesper", "Candlelit", "Hollow", "Silent", "Echoing", "Holy", "Ancient", "Morning" },
          { "Nave", "Prayer", "Hymn", "Vigil", "Aisle", "Bells", "Chapel", "Light", "Echo", "Offering" } },
        { "Lullaby", { 5, 5, 4, 2, 2 }, { 0.5f, 0.8f }, { 0.2f, 0.5f }, { 0.4f, 0.65f }, { 0.4f, 0.7f }, { 0.2f, 0.5f }, { 0.4f, 0.7f }, { 0.3f, 0.6f }, { 0.3f, 0.6f }, { 0.3f, 0.6f }, { 0.7f, 1.0f }, { 0.2f, 0.45f }, { 0.3f, 0.55f },
          { 0.25f, 0.7f }, { 0.4f, 0.8f }, { 0.5f, 0.9f }, { 0.3f, 0.8f }, { 0.3f, 0.6f }, { 0.2f, 0.6f }, { 0.1f, 0.4f }, { 0.2f, 0.6f }, { 0.2f, 0.5f },
          { { { "Music Box", W_OFF, R_LIVING, 0.0f }, { "Dreaming", W_FIRE, R_CABIN, 0.0f }, { "Goodnight", W_RAIN, R_LIVING, 0.05f } } },
          { "Sleepy", "Little", "Moonlit", "Sweet", "Starlit", "Drowsy", "Tiny", "Silver", "Hush", "Cradle" },
          { "Dreams", "Stars", "Lullaby", "Moon", "Bear", "Night-light", "Cloud", "Song", "Crib", "Goodnight" } },
        { "Study", { 0, 2, 0, 1 }, { 0.3f, 0.55f }, { 0.3f, 0.55f }, { 0.4f, 0.6f }, { 0.5f, 0.7f }, { 0.2f, 0.5f }, { 0.5f, 0.7f }, { 0.3f, 0.55f }, { 0.4f, 0.65f }, { 0.3f, 0.6f }, { 0.7f, 1.0f }, { 0.15f, 0.35f }, { 0.2f, 0.4f },
          { 0.2f, 0.55f }, { 0.4f, 0.8f }, { 0.5f, 0.9f }, { 0.3f, 0.6f }, { 0.2f, 0.5f }, { 0.2f, 0.5f }, { 0.1f, 0.4f }, { 0.2f, 0.5f }, { 0.25f, 0.6f },
          { { { "Focus", W_OFF, R_STUDIO, 0.0f }, { "Calm", W_RAIN, R_LIVING, 0.0f }, { "Morning", W_BIRDS, R_LIVING, -0.05f } } },
          { "Focused", "Library", "Calm", "Steady", "Clear", "Quiet", "Slow", "Patient", "Early", "Deep" },
          { "Study", "Desk", "Pages", "Hours", "Notes", "Exam Week", "Focus", "Flow", "Thesis", "Shelves" } },
        { "Concert", { 0, 0, 0, 0 }, { 0.45f, 0.7f }, { 0.4f, 0.65f }, { 0.55f, 0.85f }, { 0.4f, 0.6f }, { 0.0f, 0.25f }, { 0.5f, 0.8f }, { 0.6f, 0.9f }, { 0.4f, 0.65f }, { 0.4f, 0.7f }, { 0.9f, 1.3f }, { 0.35f, 0.65f }, { 0.5f, 0.85f },
          { 0.0f, 0.12f }, { 0.1f, 0.5f }, { 0.2f, 0.55f }, { 0.1f, 0.4f }, { 0.5f, 0.85f }, { 0.0f, 0.1f }, { 0.0f, 0.05f }, { 0.0f, 0.2f }, { 0.2f, 0.4f },
          { { { "Hall", W_OFF, R_HALL, 0.0f }, { "Studio", W_OFF, R_STUDIO, 0.0f }, { "Close Mic", W_OFF, R_STUDIO, 0.0f } } },
          { "Grand", "Open", "Resonant", "Bright", "Velvet", "Pure", "Radiant", "Solo", "Pristine", "Silver" },
          { "Hall", "Recital", "Sonata", "Nocturne", "Grand", "Stage", "Prelude", "Etude", "Concerto", "Encore" } },
        { "Toy", { 4, 5, 5, 4 }, { 0.5f, 0.85f }, { 0.4f, 0.7f }, { 0.4f, 0.65f }, { 0.4f, 0.7f }, { 0.2f, 0.6f }, { 0.4f, 0.7f }, { 0.4f, 0.7f }, { 0.3f, 0.6f }, { 0.2f, 0.5f }, { 0.7f, 1.1f }, { 0.15f, 0.4f }, { 0.2f, 0.45f },
          { 0.2f, 0.65f }, { 0.3f, 0.7f }, { 0.5f, 0.9f }, { 0.3f, 0.8f }, { 0.3f, 0.6f }, { 0.2f, 0.6f }, { 0.1f, 0.5f }, { 0.3f, 0.7f }, { 0.2f, 0.5f },
          { { { "Toy Piano", W_OFF, R_LIVING, 0.0f }, { "Celesta", W_OFF, R_STUDIO, -0.05f }, { "Bells", W_BIRDS, R_CHURCH, 0.0f } } },
          { "Tiny", "Wind-up", "Glass", "Paper", "Magic", "Plastic", "Little", "Toyshop", "Clockwork", "Sparkling" },
          { "Parade", "Attic", "Music Box", "Circus", "Tin Soldier", "Playroom", "Snow Globe", "Carousel", "Secret", "Gift" } },
    };
    return a;
}

const Arch* findArch (const juce::String& c)
{
    for (auto& a : archs()) if (c == a.cat) return &a;
    return nullptr;
}

float draw (Rng& r, Range g) { return g.lo + (g.hi - g.lo) * r.uni(); }
}

Preset Generator::candidate (const GenRequest& req)
{
    const Arch* a = findArch (req.category);
    if (a == nullptr) a = &archs()[0];
    Rng rng (req.seed * 2654435761u + 12345u);
    for (int i = 0; i < 4; ++i) rng.next();
    Preset p;
    const int sub = req.sub >= 0 ? req.sub % 3 : (int) (rng.next() % 3);
    const auto& v = a->sub[(size_t) sub];
    auto set = [&] (int idx, float val) { p.params[(size_t) idx] = paramDefs()[(size_t) idx].range.snapToLegalValue (val); };
    set (P::seed, (float) (1 + (req.seed * 7919u) % 99999u));
    set (P::piano, (float) a->pianos[rng.next() % a->pianos.size()]);
    set (P::tone, draw (rng, a->tone)); set (P::hammer, draw (rng, a->hammer)); set (P::sustain, draw (rng, a->sustain)); set (P::damper, draw (rng, a->damper));
    set (P::detune, draw (rng, a->detune)); set (P::stretch, draw (rng, a->stretch)); set (P::dynamics, draw (rng, a->dynamics));
    set (P::body, draw (rng, a->body)); set (P::resonance, draw (rng, a->resonance)); set (P::width, draw (rng, a->width));
    set (P::room, draw (rng, a->room)); set (P::roomSize, draw (rng, a->roomSize)); set (P::roomType, (float) v.room);
    set (P::memory, juce::jlimit (0.0f, 1.0f, draw (rng, a->memory) + v.memBias)); set (P::warmth, draw (rng, a->warmth)); set (P::intimacy, draw (rng, a->intimacy));
    set (P::time, draw (rng, a->time)); set (P::spaceMacro, draw (rng, a->space));
    set (P::tape, draw (rng, a->tape) * 0.5f); set (P::vinyl, draw (rng, a->vinyl) * 0.5f); set (P::wood, draw (rng, a->wood));
    set (P::world, (float) v.world); set (P::worldLevel, draw (rng, a->worldLevel));
    // every category has its own signature sound: instrument, brightness, room, character (so presets do not blur into one another)
    {
        auto between = [&] (int idx, float lo, float hi) { set (idx, lo + (hi - lo) * rng.uni()); };
        auto choose = [&] (int idx, std::initializer_list<int> v) { set (idx, (float) *(v.begin() + rng.next() % v.size())); };
        const juce::String cn = a->cat;
        if (cn == "Lo-Fi") { choose (P::piano, { 1, 1, 0 }); between (P::tone, 0.15f, 0.42f); between (P::hammer, 0.2f, 0.5f); between (P::detune, 0.55f, 0.95f); between (P::body, 0.55f, 0.9f); choose (P::roomType, { 0, 3 }); between (P::room, 0.1f, 0.3f); }
        else if (cn == "Felt") { set (P::piano, 2.0f); between (P::tone, 0.05f, 0.3f); between (P::hammer, 0.0f, 0.25f); between (P::sustain, 0.25f, 0.5f); between (P::room, 0.05f, 0.22f); between (P::intimacy, 0.75f, 1.0f); }
        else if (cn == "Cinematic") { set (P::piano, 0.0f); between (P::tone, 0.5f, 0.78f); between (P::hammer, 0.5f, 0.8f); between (P::sustain, 0.65f, 0.95f); choose (P::roomType, { 1, 4, 4 }); between (P::room, 0.5f, 0.85f); between (P::roomSize, 0.6f, 0.95f); }
        else if (cn == "Rainy") { choose (P::piano, { 0, 1 }); between (P::tone, 0.25f, 0.5f); between (P::sustain, 0.5f, 0.75f); choose (P::roomType, { 0, 3 }); set (P::world, 1.0f); between (P::worldLevel, 0.45f, 0.9f); }
        else if (cn == "Vintage") { choose (P::piano, { 3, 3, 1 }); between (P::detune, 0.75f, 1.0f); between (P::hammer, 0.55f, 0.9f); between (P::tone, 0.4f, 0.7f); between (P::sustain, 0.25f, 0.45f); between (P::body, 0.7f, 1.0f); choose (P::roomType, { 0, 3 }); }
        else if (cn == "Church") { set (P::piano, 0.0f); set (P::roomType, 1.0f); between (P::room, 0.7f, 1.0f); between (P::roomSize, 0.75f, 1.0f); between (P::sustain, 0.7f, 0.95f); between (P::tone, 0.35f, 0.62f); between (P::intimacy, 0.0f, 0.3f); }
        else if (cn == "Lullaby") { choose (P::piano, { 5, 5, 4 }); between (P::tone, 0.6f, 0.9f); between (P::hammer, 0.25f, 0.5f); between (P::room, 0.2f, 0.4f); }
        else if (cn == "Study") { choose (P::piano, { 0, 2 }); between (P::tone, 0.3f, 0.5f); between (P::hammer, 0.2f, 0.45f); between (P::room, 0.08f, 0.25f); between (P::memory, 0.15f, 0.45f); }
        else if (cn == "Concert") { set (P::piano, 0.0f); between (P::tone, 0.58f, 0.88f); between (P::hammer, 0.62f, 0.92f); between (P::sustain, 0.6f, 0.9f); set (P::roomType, 4.0f); between (P::room, 0.3f, 0.5f); between (P::memory, 0.0f, 0.06f); between (P::intimacy, 0.2f, 0.5f); }
        else if (cn == "Toy") { choose (P::piano, { 4, 4, 5 }); between (P::tone, 0.6f, 0.92f); between (P::hammer, 0.45f, 0.75f); between (P::detune, 0.2f, 0.5f); }
    }
    p.meta.category = a->cat; p.meta.subcategory = v.name;
    const juce::String name = juce::String (a->adj[rng.next() % a->adj.size()]) + " " + a->noun[rng.next() % a->noun.size()];
    p.meta.name = name;
    p.meta.author = "DSK Music"; p.meta.factory = true;
    p.meta.notes = "A " + juce::String (a->cat).toLowerCase() + " memory (" + juce::String (v.name) + "). Turn Memory up to age it, down to bring it close.";
    p.meta.tags = deriveTags (p);
    return p;
}

juce::StringArray Generator::deriveTags (const Preset& p)
{
    juce::StringArray t;
    auto g = [&] (int i) { return p.params[(size_t) i]; };
    if (g (P::memory) > 0.55f || g (P::tape) > 0.3f) t.add ("Tape");
    if (g (P::memory) > 0.6f) t.add ("Aged");
    if (g (P::vinyl) > 0.2f) t.add ("Vinyl");
    if (g (P::tone) < 0.4f) t.add (g (P::tone) < 0.3f ? "Dark" : "Soft"); else if (g (P::tone) > 0.62f) t.add ("Bright");
    if (g (P::warmth) > 0.6f) t.add ("Warm");
    if (g (P::intimacy) > 0.7f) t.add ("Intimate");
    if (g (P::width) > 1.05f) t.add ("Wide");
    if ((int) g (P::world) == W_RAIN) t.add ("Rain");
    if (g (P::detune) > 0.65f) t.add ("Detuned");
    if ((int) g (P::piano) == 2) t.add ("Felt");
    if ((int) g (P::piano) >= 4) t.add ("Bell");
    if (g (P::memory) > 0.45f && g (P::tone) < 0.5f) t.add ("Nostalgic");
    if (g (P::memory) < 0.15f && g (P::tape) < 0.1f) t.add ("Clean");
    return t;
}

bool Generator::validate (Preset& p, float* rmsOut)
{
    // a short phrase: bass note, arpeggio, a melody line over a held chord, with the pedal
    juce::MidiBuffer m;
    const double sr = 44100.0;
    auto note = [&] (double t, double d, int n, int v) { m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) v), (int) (t * sr)); m.addEvent (juce::MidiMessage::noteOff (1, n), (int) ((t + d) * sr)); };
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    note (0.1, 3.0, 45, 70);
    const int arp[] { 57, 60, 64, 69, 64, 60 };
    for (int i = 0; i < 6; ++i) note (0.2 + i * 0.45, 0.9, arp[i], 62 + (i % 2) * 8);
    note (3.0, 3.0, 53, 70);
    const int mel[] { 72, 71, 69, 67, 69, 72 };
    const double mt[] { 3.2, 3.8, 4.3, 4.9, 5.5, 6.1 };
    for (int i = 0; i < 6; ++i) note (mt[i], 0.7, mel[i], 78);
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), (int) (7.2 * sr));
    const int total = (int) (8.5 * sr);
    float rms = -100;
    for (int pass = 0; pass < 3; ++pass)
    {
        juce::AudioBuffer<float> b (2, total);
        b.clear();
        renderMidi (p, m, b, sr, 512);
        double sq = 0; float peak = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = (int) (0.4 * sr); i < total; ++i)
            {
                const float x = b.getSample (c, i);
                if (! std::isfinite (x)) return false;
                sq += (double) x * x; peak = juce::jmax (peak, std::abs (x));
            }
        rms = juce::Decibels::gainToDecibels ((float) std::sqrt (sq / (2.0 * (total - 0.4 * sr))), -100.0f);
        if (rms < -70.0f) return false;
        const float delta = targetRmsDb() - rms;
        if (std::abs (delta) < 0.8f && peak < 0.93f) break;
        const float master = juce::jlimit (-30.0f, 9.0f, p.params[(size_t) P::master] + delta * (pass == 2 ? 1.0f : 0.95f));
        p.params[(size_t) P::master] = master;
        if (pass == 2 && peak > 0.97f) return false;
    }
    if (rmsOut) *rmsOut = rms;
    return true;
}

Preset Generator::generate (const GenRequest& req, int maxAttempts)
{
    Preset best;
    for (int i = 0; i < maxAttempts; ++i)
    {
        auto r = req; r.seed = req.seed + (uint32_t) i * 104729u;
        auto p = candidate (r);
        if (validate (p)) return p;
        best = p;
    }
    return best;
}
}
