#include "Song.h"
#include "../Params.h"

namespace gx
{
ComposeCfg composeCfgFromPreset (const Preset& pr)
{
    ComposeCfg c;
    auto g = [&] (int i) { return pr.params[(size_t) i]; };
    c.mode = juce::jmax (1, (int) g (P::hands));
    c.key = (int) g (P::handsKey); c.scale = (int) g (P::handsScale); c.style = (int) g (P::handsStyle);
    c.bars = handsBarsFromIndex ((int) g (P::handsBars));
    c.density = g (P::handsDensity); c.expression = g (P::handsExpress); c.pedal = g (P::handsPedal) > 0.5f;
    c.bpm = g (P::handsTempo);
    c.seed = (uint32_t) (((int) g (P::handsSeed) - 1) % 99999) + 1u;
    return c;
}

bool writeMidi (const Score& s, double bpm, const juce::File& file)
{
    constexpr int ppq = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    juce::MidiMessageSequence conductor, track;
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (s.beatsPerBar, 4), 0);
    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, "Memoria by DSK"), 0);
    for (auto& sec : s.sections)
        conductor.addEvent (juce::MidiMessage::textMetaEvent (6, juce::String (sec.type == 'I' ? "Intro" : sec.type == 'A' ? "Verse" : sec.type == 'P' ? "Pre-chorus" : sec.type == 'B' ? "Chorus" : sec.type == 'C' ? "Bridge" : "Outro")),
                            (double) sec.startBar * s.beatsPerBar * ppq);
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Piano"), 0);
    track.addEvent (juce::MidiMessage::programChange (1, 0), 0);
    for (auto& n : s.notes)
    {
        track.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.vel), n.start * ppq);
        track.addEvent (juce::MidiMessage::noteOff (1, n.note), (n.start + n.len) * ppq);
    }
    for (auto& p : s.pedal) track.addEvent (juce::MidiMessage::controllerEvent (1, 64, p.val), p.beat * ppq);
    track.updateMatchedPairs();
    conductor.sort(); track.sort();
    conductor.addEvent (juce::MidiMessage::endOfTrack(), s.totalBeats * ppq);
    track.addEvent (juce::MidiMessage::endOfTrack(), s.totalBeats * ppq);
    mf.addTrack (conductor); mf.addTrack (track);
    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}
}
