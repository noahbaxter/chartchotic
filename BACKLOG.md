# Backlog

Work from the top.

---

## Up Next

- **Ratio-agnostic window resizing** — Proper resize support. Fixes "too much space below strikeline" tuning and REAPER fullscreen restore white gap / ~98% size bug.
- **Update banner: show changelog** — `releaseNotes` already fetched from GitHub release body (release channel). Need: display in overlay card (taller/scrollable), strip markdown to plaintext, also fetch for dev channel.
- **Section borders** — EVENTS track parsing, blue measure lines, section name overlay. Unlocks autodetection and section-aware features downstream.
- **Solo sections** — Blue highway background during solo passages.
- **Time sig changes display** — Symbols on left side, scroll with highway.
- **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
- **Better mouse scrolling** — shift=faster, ctrl=precise.
- **Save as default settings** — Persist user-preferred defaults to disk so new plugin instances start with custom settings instead of hardcoded defaults.
- **Info display** — BPM, time sig, measure, beat position.

---

## Tech Debt

Do between features or when touching related code.

- **NoteStateStore wrapper** — `noteStateMapArray` + `noteStateMapLock` passed as separate params to ~15 functions. Wrap into single class that enforces locking via API. Eliminates race condition footgun.
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

**Architecture (do when it hurts):**
- AudioProcessorValueTreeState migration
- Double-buffered snapshots for renderer

---

## Icebox

Blocked or no clear path forward.

- **#17 — Linux REAPER scan failure** — May be resolved by cross-platform build fixes. Needs verification. [GitHub](https://github.com/noahbaxter/chartchotic/issues/17)
- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/3)
- Multi-instrument view (multiple MIDI streams in one plugin)
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
