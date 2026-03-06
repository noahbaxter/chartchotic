# Backlog

Organized by version milestone. Work from the top.

---

## 0.10.0 — Rendering Engine Rewrite (done, releasing from dev)

- Bezier perspective system (replaced exponential positioning)
- Sub-renderer architecture (NoteRenderer, SustainRenderer, GridlineRenderer, AnimationRenderer, TrackRenderer)
- High-res asset pipeline + private submodule (Ghostscript PDF-to-PNG)
- Highway textures (scrolling overlay) + highway length control
- Composited track layers (sidebars, lane lines, strikeline, connectors)
- VBlank-synced rendering (replaced Timer)
- Toolbar extraction + render toggles + gem scale
- DrawCallMap flat array optimization (eliminated per-frame heap churn)
- Profiler fix (real FPS, lock wait, frame delta)
- Frame throttle grid alignment
- Note/lane X1/X2 position tuning (perspective-correct offsets)
- Hit animation improvements (time-based, curvature-aware, dynamic scales)
- Debug standalone with MIDI playback + tuning panel
- CMake build system + CI/CD pipelines
- Integer note speed (2-20, default 7)
- Toolbar moved to top (fixed taskbar clipping)

---

## 1.0.0 — Polish & Ship

Small scope. UI cleanup and verification before tagging stable.

1. **Toolbar visual polish** — Cleaner layout, better spacing, responsive scaling.
2. **Settings persistence audit** — Verify all options save/restore correctly.
3. **Window sizing persistence** — Save/restore on REAPER restart.
4. **Final user testing pass** — Verify note speed feel, latency offset UX.

---

## CI/CD & Infrastructure

5. **R2 CDN upload** for distribution

---

## Up Next (post-1.0)

Features that make the plugin better. Pull into active work after 1.0.

6. **Section borders** — EVENTS track parsing, blue measure lines, section name overlay. Unlocks autodetection and section-aware features downstream.
7. **Solo sections** — Blue highway background during solo passages.
8. **Time sig changes display** — Symbols on left side, scroll with highway.
9. **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
10. **Better mouse scrolling** — shift=faster, ctrl=precise.
11. **Latency offset UI cleanup**

---

## Tech Debt

Do between features or when touching related code.

12. **NoteStateStore wrapper** — `noteStateMapArray` + `noteStateMapLock` passed as separate params to ~15 functions. Wrap into single class that enforces locking via API. Eliminates race condition footgun.
13. **Audio-thread hygiene** — Remove std::function, preallocated vectors. DrawCallMap done, remaining allocations TBD.
14. **Minimize REAPER API calls** — Profiler/DrawCallMap work done, REAPER call frequency still unaudited.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Info display: BPM, time sig, measure, beat position
- Better menu system + advanced settings
- Instrument autodetection (by track name — depends on EVENTS parsing from #6)
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

- **#17 — Linux REAPER scan failure** — May be resolved by cross-platform build fixes. Needs verification. [GitHub](https://github.com/noahbaxter/chart-preview/issues/17)
- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chart-preview/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic.* [GitHub](https://github.com/noahbaxter/chart-preview/issues/3)
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

- **Moonscraper overlap**: Community consensus is Chart Preview is for preview, not charting.
- **Key dependency chain**: EVENTS parsing (#6) -> section detection -> autodetection -> Real Drums MIDI refactor -> generic gem system
