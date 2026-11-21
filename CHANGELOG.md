# Changelog

## [Unreleased]
### Added
- Modal piano engine, no samples: inharmonic string partials, 2-3 detuned strings per note, double decay, hammer spectrum by velocity,
  strike-position comb, dampers, continuous sustain pedal (half pedal), sympathetic resonance, soundboard modes, 32-voice polyphony with click-free voice stealing.
- Six instruments: concert grand, upright, felt piano, honky-tonk, toy piano, celesta.
- Memory macro plus Warmth, Intimacy, Time and Space.
- Layers of life: wood (bench, key action, pedal, dampers), tape (wow / flutter, saturation, dull highs, hiss, dropouts), vinyl (crackle, rumble, needle pop),
  world (rain, wind, fireplace, dawn birds) and five rooms.
- Hands v2: a song composer (intro / verse / chorus / bridge / outro) with functional harmony, developing motifs, hooks, expressive performance and pedalling; 6 styles, 8 scales, any key, 16-128 bars,
  with their own tempo and PLAY button; they never follow the DAW transport; New variation, Random song with locks; export as MIDI or WAV (drag into the DAW or save).
- Classical style: Pachelbel's Canon in D (public domain) with growing variations. Three more styles: Jazz Ballad, Lo-Fi Loop and a Chopin-style Nocturne.
- Sympathetic resonance is subtle and short now (it could sound like an organ with the pedal down); computer-keyboard notes play at mezzo-forte instead of full velocity; candle shadows without banding; the scene no longer shows a static Memory label.
- Random sound generator with section locks.
- One-page interface: room scene, Memory slider, Hands, sound generator and every control.
- GPU shader room: a real 3D ray-marched scene (soft shadows, AO, reflections, volumetric light with dust, rain on glass, candle, film ageing), piano keys that go down with the notes, three quality levels, CPU fallback.
- Wind ambience (replaces the cafe): gusts on three time scales, breathy body, soft whistling, rumble, rustling leaves; the room curtains sway in the scene.
- The distant-audience ambience is gone (voiced murmur sounded organ-like); the cafe is gone too.
- Factory bank curated: 210 presets, each checked for organ-like sustained drones, level-matched and not near-duplicates of each other.
- Browsing presets quickly no longer piles up test chords (the previous chord is released before a new one is struck).
- GPU scene: starts without waiting for a window, a lighter shader tier if the first one fails, safe smoothstep for Intel drivers, status line in Settings and log in Documents/DSK Music/Memoria/gpu-log.txt.
- Film flaws are round, irregular specks instead of squares.
- Shader fix for Intel graphics (a function name clashed with a GLSL built-in: noise3).
- The Hands never follow the DAW transport any more: they play only on the instrument's own PLAY (a loaded plugin is silent until notes arrive). Export the MIDI to use the music in a DAW.
- The test chord when selecting a preset is off by default (Settings); the maximum level of every ambience (rain, wind, fireplace, birds) is half of before.
- Output limiter (ceiling -1 dB) and an overall level about 3 dB lower (default Master -9 dB, factory presets re-levelled).
- Strings view: 88 live strings, hammers, dampers and pedal in the Piano panel.
- Click on the song map to jump to that bar while playing (in a DAW the song is aligned to the host position).
- Composer v3: pre-chorus, hook repeats, bass approach notes, pushed chords, key lift in the last chorus, best-of-six takes by melodic quality.
- Presets: each category has its own signature sound (instrument, brightness, room, character); Tone / Hammer / Body have a much wider effect.
- Video captions: UTF-8 fixed (Spanish accents), English first then Spanish.
- Rain, wind (gusts, whistling, rustling leaves), fireplace and birds ambiences rebuilt and level-capped.
- Room scene (window light, dust, rain, candle) that ages with Memory.
- Rule-based preset generator with offline validation and level matching; 210 factory presets in 10 categories; Surprise me button.
- Long audio export (WAV, FLAC, OGG) with the hands playing; preset import / export.
- MemoriaTool: tests, factory bank, listening material, screenshots, demo video and trailer.
- Bilingual interface (English / Spanish), resizable, A/B, undo / redo, MIDI learn, click-free preset changes.

## Unreleased - sound and songs
- Fix: saturated sound at ff velocities (velocity range narrowed, tape saturation measured at output level, look-ahead limiter instead of a fast clipper).
- Hands: melodies are now hand-written songbook phrases (question/answer, verse/chorus) over chosen progressions; music lint and gain-staging tests added.
- Hands: songbook grown to 20 themes (8 per style, so the same style gives many different songs); repeats are never copies (second verse with anticipations and passing notes), choruses add a counter-melody and a low bass octave, the last chorus is doubled and lifts a tone (now from 32 bars).
- New instruments: Electric Piano (Rhodes-style) and Vibraphone, with 12 new presets (222 in total).
