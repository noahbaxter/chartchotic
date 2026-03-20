# Backlog

Work from the top.

---

## Up Next

- **Bemani: pre-release fixes** — (1) Per-highway padding for viewWindows in multi-highway layout. (2) Highway length should scale with slot height — taller slot = more visible notes, not same notes stretched over more space. (3) Multi-highway layout must always be a single horizontal row in Bemani mode (no 2x2 grid, no vertical column).
- **Resolve-per-track caching for multi-highway** — `resolveAllDifficulties` computes all 4 difficulties but currently each slot resolves independently. In multi-difficulty grid mode (4 highways showing E/M/H/X of same track), this means 4× redundant resolve. Cache one PartWindow per unique track per frame, let each slot extract its DifficultyWindow. Skip unused difficulties in single-highway mode.
- **All-difficulty overlay mode** — Show all 4 difficulties squeezed into the same highway. Each lane gets up to 4 notes stacked when all difficulties hit them. Color-code by difficulty instead of track color. All note data except specific pitches is shared across difficulties in the MIDI spec, so the data model supports this naturally.
- **Save/load presets + default state** — Persist user settings to disk. "Save as default" makes all new plugin instances start with custom settings. Presets for game-specific profiles (GH, RB, etc.). Foundation for everything downstream.
- **Section borders + event markers** — EVENTS track parsing, blue measure lines, section name overlay. Backend partially wired (`TimeBasedEventMarker`, `populateEventMarkers`). Tempo/timesig marker infrastructure landed (`d23505b`), rendering disabled. Enable for tempo/timesig, then extend for sections, lyrics.
- **Multi-highway: missing track types** — Discovery currently handles GUITAR, BASS, KEYS, DRUMS, GHL_GUITAR, GHL_BASS. Missing 5-fret tracks (priority): `PART GUITAR COOP`, `PART RHYTHM`. Missing 6-fret: `PART RHYTHM GHL`, `PART GUITAR COOP GHL`, `PART KEYS GHL`. Need new Part enum values for GUITAR_COOP and RHYTHM, plus "Guitar 1"/"Guitar 2" badge on the highway label when both are present (instead of a separate icon).
- **Multi-highway: track re-discovery** — `pollForChanges()` only detects note edits on existing tracks. If user adds/removes a REAPER track, session is stale until plugin reopen or view mode toggle. Should periodically re-run discovery or check track count.
- **Multi-highway: ReaperMidiPipeline slimming** — `fetchAllNoteEvents()`, `fetchAllTextEvents()`, `processCachedNotesIntoState()` are duplicated between pipeline and InstrumentSession. Can remove from pipeline once session fully replaces it for note fetching.
- **Ratio-agnostic window resizing** — Viewport-based resize landed (`c030e7d`), but "too much space below strikeline" tuning and REAPER fullscreen restore white gap / ~98% size bug still open.
- **SysEx marker support** — Read Phase Shift SysEx events for open/tap note markers. The open note marker turns ALL notes into opens, not just greens. See [chart format spec](https://thenathannator.github.io/GuitarGame_ChartFormats/Chart-File-Formats/mid-format/Format-Overview/#phase-shift-sysex-event-specification).
- **Audio playback** — Metronome click, guitar note sounds, drum hit sounds for listening back to charts. Assets mostly ready. Needs audio output from plugin + sync.
- **Standalone chart loading** — Open a chart file/folder and play it back without REAPER. Debug standalone already has playback; needs file open UI, chart format parsing, tempo map from file. Pairs with audio playback.
- **Update banner: show changelog** — `releaseNotes` already fetched from GitHub release body (release channel). Need: display in overlay card (taller/scrollable), strip markdown to plaintext, also fetch for dev channel.
- **Solo sections** — Blue highway background during solo passages.
- **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
- **Better mouse scrolling** — shift=faster, ctrl=precise.
- **Info display** — BPM, time sig, measure, beat position.
- **Disco ball animation upgrade** — Replace static disco ball JPG with rotating disco ball (quarter-turn loop), "DISCO FLIP" text underneath in disco/rainbow colors. Fun polish.

### Done (1.2)

**Bemani mode:**
- ~~Flat scrolling viewport~~ — Sidebar rails, per-instrument tuning, data-driven debug panel, BemaniConfig refactor.
- ~~Texture scroll snap~~ — Removed fmod wrapping from scroll offset (both REAPER and standalone paths).
- ~~Disco flip force field markers~~ — Flat horizontal markers in Bemani mode.
- ~~Sidebar edge lines~~ — Sidebar rails via DrawCallMap at TRACK_SIDEBARS draw order.
- ~~Unified note speed~~ — Both modes share 2-20 range, Bemani applies internal 1.7x ratio.

**Multi-highway:**
- ~~Pool HighwayComponents + shared track image cache~~ — 4 pooled slots (never destroyed), TrackImageCache bakes guitar+drums once at full resolution. Difficulty/instrument toggles swap overlay pointers (~0ms).
- ~~Deferred gem type calculation (v2)~~ — Zero-stutter: NoteData stores velocity only, TrackResolver computes gems at render time. Toggle changes instant.
- ~~Batched data build~~ — Extract+resolve once per lock group instead of per-slot.

**Performance:**
- ~~Async track image rebake~~ — Background thread bake with generation counter. Stale cache stretches during drag/resize, snaps on completion.
- ~~Profiling infrastructure~~ — Per-frame TSV logger, per-highway phase timing, pipeline benchmark target. Profiler lifecycle encapsulated in DebugEditorController.

**Other:**
- ~~Disco flip (Pro Drums)~~ — Text event infrastructure + DiscoFlipState + red↔yellow swap. Force field markers at start/end positions.
- ~~Update overlay improvements~~ — ESC dismisses, resizes with window.
- ~~Lane/sustain draw order~~ — SUSTAIN before BAR (bars render on top).
- ~~Source tree reorg~~ — Extract Editor/ module from PluginEditor.

---

## Tech Debt

Do between features or when touching related code.

- **Stretch + Bemani mode interaction** — Stretch-to-fill doesn't work when Bemani mode is active. `onBemaniModeChanged` doesn't account for stretch state, and `resized()` computes `sceneHeight` differently in Bemani mode regardless. Preexisting.
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
- **Community feedback source**: Discord #chartchotic channel
