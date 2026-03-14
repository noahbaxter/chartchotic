# Backlog

Work from the top.

---

## Up Next

- **Multi-highway split view** — WIP on `refactor/multi-highway`. Renderer decoupling done (`activePart` members, `InstrumentSlot`, parameterized `MidiInterpreter`). Remaining: wire PluginEditor slots to UI trigger, debug controller multi-instrument loading, split view layout/resize, non-standalone mode support.
- **Ratio-agnostic window resizing** — Proper resize support. Fixes "too much space below strikeline" tuning and REAPER fullscreen restore white gap / ~98% size bug.
- **Update banner: show changelog** — `releaseNotes` already fetched from GitHub release body (release channel). Need: display in overlay card (taller/scrollable), strip markdown to plaintext, also fetch for dev channel.
- ~~**Disco flip (Pro Drums)**~~ — ✅ Done. Text event infrastructure + DiscoFlipState + red↔yellow swap in MidiInterpreter. REAPER + standalone pipelines wired.
- ~~**Disco flip highway markers**~~ — ✅ Done. Force field markers at disco start/end positions.
- **Disco ball animation upgrade** — Replace static disco ball JPG with rotating disco ball (quarter-turn loop), "DISCO FLIP" text underneath in disco/rainbow colors. Fun polish.
- **Section borders** — EVENTS track parsing, blue measure lines, section name overlay. Unlocks autodetection and section-aware features downstream.
- **Solo sections** — Blue highway background during solo passages.
- **Event marker rendering** — Backend wired (`TimeBasedEventMarker`, `populateEventMarkers`), rendering commented out in SceneRenderer. Enable for tempo/timesig changes, then extend for sections, lyrics, etc.
- **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
- **Better mouse scrolling** — shift=faster, ctrl=precise.
- **Save as default settings** — Persist user-preferred defaults to disk so new plugin instances start with custom settings instead of hardcoded defaults.
- **Info display** — BPM, time sig, measure, beat position.

---

## Tech Debt

Do between features or when touching related code.

- **NoteStateStore wrapper** — `InstrumentSlot` bundles array+lock but doesn't enforce locking via API yet. Wrap into class with scoped-lock accessors. Eliminates race condition footgun.
- **Audio-thread hygiene** — Remove std::function, preallocated vectors. DrawCallMap done, remaining allocations TBD.
- **Minimize REAPER API calls** — Profiler/DrawCallMap work done, REAPER call frequency still unaudited.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Instrument autodetection (by track name — depends on EVENTS parsing)
- Note color customization (CH color profile templates)
- GH style gems toggle
- Star Power rendering (partially working, architectural approach unclear — finish or remove)

**Real Drums:**
- Flam note types, HH open/closed/foot gems
- Generic gem system (color any part of note)
- Reorderable lanes + custom colors
- 5-lane drums, 8-lane drums
- Refactored MIDI parsing (supports reductions, benefits all instruments)

**Authoring & Vocals (way out):**
- Moonscraper-style note placement, grid snapping, note eraser
- INI generation/export
- 2D pitch-based karaoke display, lyrics with rhythm timing

**Standalone:**
- Standalone chart player — open a chart file/folder and play it back like a DAW session (no REAPER needed). Debug standalone already has playback; needs file open UI, chart format parsing, and tempo map from file.

**DAW Integration:**
- Max4Live wrapper for Ableton — use Ableton LOM to expose REAPER-equivalent features (text events, timeline access) via Max4Live device. Would enable disco flip, sections, etc. in Ableton.

**Architecture (do when it hurts):**
- AudioProcessorValueTreeState migration
- Double-buffered snapshots for renderer

---

## Icebox

Blocked or no clear path forward.

- **#17 — Linux REAPER scan failure** — May be resolved by cross-platform build fixes. Needs verification. [GitHub](https://github.com/noahbaxter/chartchotic/issues/17)
- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/3)
- Elite Drums (beyond Real Drums)
- Extended memory for standard pipeline (notes visible when transport stops)
- REAPER auto track detection
- REAPER preset integration (.ini files)
- .ini file generation (community suggests CAT instead)
- Note length validation (possible ReaScript utility)
- Motion sickness mitigation (unclear solution)

---

## Notes

- **Moonscraper overlap**: Community consensus is Chartchotic is for preview, not charting.
- **Key dependency chain**: EVENTS parsing -> section detection -> autodetection -> Real Drums MIDI refactor -> generic gem system
