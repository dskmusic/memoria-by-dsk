# Design decisions
| # | Topic | Decision | Why |
|---|-------|----------|-----|
| 1 | Origin | Scaffold forked from Infinitum by DSK (GPLv3, namespace `gx` kept); Infinitum / Genesix never modified | Reuse build, tool, tests, installer, CI, UI widgets. |
| 2 | Plugin codes | Manufacturer `Dskm`, plugin `Mmra`, bundle `com.dskmusic.memoria` | Frozen forever (DAW session compatibility). Checked: no clash with Gnsx, Infm, Apex. |
| 3 | Repo visibility | Private (`dskmusic/memoria-by-dsk`) until first release; GitHub Actions **disabled repo level** (verified by API before first push) | Private-repo Actions minutes are metered. No public binaries while private (GPLv3). |
| 4 | Commit e-mail | Repo-local GitHub noreply address from the first commit | History becomes public when repo flipped. |
| 5 | Installer | New Inno Setup AppId | Must not upgrade/uninstall other plugins. |
| 6 | Logo | "Fading keys": piano keys whose right edge dissolves into particles; amber gradient | Chosen by the author. |
| 7 | Accent colour | Warm honey/amber `#E8B26A`, secondaries dusty rose and dusk blue | Nostalgia; warmer than Genesix / Infinitum. |
| 8 | Piano engine | Modal / partial-bank synthesis (no samples): real inharmonicity, 2-3 detuned strings per note, double decay, hammer spectrum by velocity, strike-position comb, damper model, soundboard body, sympathetic resonance | Realistic piano without a multi-GB library. Arbitrary partial tables (celesta, toy piano are bars). |
| 9 | Determinism | Per-note detune from hash(seed, note); engine decisions on a fixed tick grid | Offline bounce == what was heard; identical for any host block size. |
| 10 | Export formats | WAV, FLAC, OGG (JUCE built-in) | No extra encoder dependency. |
| 11 | Factory bank / demo / trailer | Not generated until the author approves the piano sound by ear | Presets built on a sound that later changes are wasted. |
