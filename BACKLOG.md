# Backlog

All bugs, feature ideas, and roadmap items in one place.

---

## Inbox

Raw items needing triage.

- [ ] **Speed slider is inverted** — Slider controls `displayWindowTimeSeconds`, so higher value = slower scroll, but labeled "Speed". Past commits have gone back and forth on this — needs investigation, not a quick flip. *(Discord, HopH₂O, 2026-02-21)*
- [ ] **Bottom controls hidden behind taskbar** — UI controls at bottom of window get clipped by OS taskbar/dock. *(Discord, HopH₂O, 2026-02-21)*
- [ ] **Highway too small / controls should move to top** — If controls moved to top, highway gets more vertical space. *(Discord, HopH₂O, 2026-02-21)*
- [ ] **UI needs better responsive scaling** — Plugin window doesn't adapt well to different sizes. *(Discord, HopH₂O, 2026-02-21)*

---

## Up Next

Prioritized, grouped by theme.

### Bugs

- [ ] **#17 — Linux REAPER scan failure** — Plugin fails to scan in REAPER on Linux. [GitHub](https://github.com/sanjay900/chart-preview/issues/17)

### Quick Wins (was v0.9.5)

- [ ] Default highway speed (1.15-1.20 range, confirmed by HopH₂O)
- [ ] Speed slider labeling ("Slower / Faster" instead of raw number)
- [ ] Window sizing persistence (save/restore on REAPER restart)
- [ ] Latency offset UI cleanup

### CI/CD & Infrastructure

- [ ] macOS code signing & notarization
- [ ] Windows installer (Inno Setup)
- [ ] pluginval CI testing
- [ ] VERSION file + auto-release workflow
- [ ] R2 CDN upload for distribution
- [ ] build.sh script (standardized local build)

---

## Roadmap

Versioned feature plan. Order matters — later versions depend on earlier ones.

### v1.0.0 — Core Features + UI Overhaul

**Chart Features:**
- [ ] Time sig changes display (symbols on left, scroll with highway)
- [ ] Section borders (EVENTS track parsing, blue measure lines, section name overlay)
- [ ] Drum fills/BRE (full lanes for kicks/open, activation gem logic)
- [ ] Solo sections (blue highway background)
- [ ] Better mouse scrolling (shift=faster, ctrl=precise)

**UI Overhaul:**
- [ ] Info display: BPM, time sig, measure, beat position
- [ ] Better menu system + advanced settings
- [ ] Settings persistence audit (all options saved/restored)
- [ ] Visual polish

### v1.1.0 — Real Drums

Start with top hat mode (minimal custom assets):
- [ ] Flam note types (split gems)
- [ ] HH open/closed/foot gem types
- [ ] Generic gem system (color any part of note)
- [ ] Reorderable lanes + custom colors
- [ ] Refactored MIDI parsing (supports reductions, benefits all instruments)

Then iterate:
- [ ] 5-lane drums (RB drums + extra HH lane)
- [ ] 8-lane drums (every hit gets own lane)

### v1.2.0+ — Polish & Extras

- [ ] Note color customization (CH color profile templates)
- [ ] GH style gems toggle
- [ ] Instrument autodetection (by PART DRUMS/BASS/GUITAR/etc. track name)
- [ ] GUI auto-scaling refinements
- [ ] Highway length control (configurable visible beats)
- [ ] Performance optimization (minimize expensive REAPER API calls)

### v2.0.0 — Authoring & Vocals

**Authoring:**
- [ ] Moonscraper-style note placement (mouse + grid snapping)
- [ ] Snapping divisions (4th-64th notes, tuple support)
- [ ] Note eraser tool
- [ ] INI generation/export

**Vocals:**
- [ ] 2D pitch-based karaoke display
- [ ] Lyrics with rhythm timing

**Key Dependencies:**
- EVENTS track parsing (v1.0.0) → section detection + autodetection
- Real Drums MIDI refactor (v1.1.0) → generic gem system + custom instruments
- Instrument autodetection (v1.2.0) → depends on EVENTS track

---

## Icebox

Low priority, blocked, or speculative.

### DAW Compatibility

- [ ] **#16 — FL Studio: no notes appear** — +1 from another user. [GitHub](https://github.com/sanjay900/chart-preview/issues/16). *Blocked: don't own FL Studio.*
- [ ] **#3 — Logic: AU loads as Audio FX, no MIDI** — [GitHub](https://github.com/sanjay900/chart-preview/issues/3). *Blocked: don't own Logic.*

### Architecture

- [ ] CMake migration (from Projucer, standard for modern JUCE but big effort)
- [ ] AudioProcessorValueTreeState migration (parameter management)
- [ ] Double-buffered snapshots for renderer (thread safety improvement)
- [ ] Audio-thread hygiene (remove std::function, preallocated vectors)
- [ ] Extract perspective math to shared function (6 functions currently duplicated)

### Features

- [ ] Multi-instrument view (multiple MIDI streams in one plugin)
- [ ] Elite Drums (beyond Real Drums)
- [ ] Extended memory for standard pipeline (keep notes visible when transport stops)
- [ ] REAPER auto track detection (detect which track plugin is on)
- [ ] REAPER preset integration (save to .ini files)

### User Requests (Low Priority)

- [ ] .ini file generation (community suggests integrating with CAT instead)
- [ ] Note length validation (possible REAPER ReaScript utility)
- [ ] Motion sickness mitigation (scroll smoothing, unclear solution)

---

## Notes

- **Moonscraper overlap**: Community consensus is Chart Preview is for preview/visualization, not charting. Focus on being a great preview tool.
- **Star Power rendering**: Partially working but architectural approach unclear. Three TODOs in code about SP state passing.
