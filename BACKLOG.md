# Backlog

Prioritized top-to-bottom. Work from the top.

---

## CI/CD & Infrastructure

1. **R2 CDN upload** for distribution

---

## User-Reported Fixes

Real users waiting on these. *(Discord, HopH₂O, 2026-02-21 unless noted)*

2. **Speed slider polish** — Reworked to integer note speed (2–20, default 7). Remaining: verify feel with users, consider whether range/default needs adjustment.
3. **UI layout overhaul** — Controls hidden behind taskbar, highway too small, poor responsive scaling. Moving controls to top likely fixes clipping AND gives highway more space.
4. **Window sizing persistence** — Save/restore on REAPER restart.
5. **#17 — Linux REAPER scan failure** — [GitHub](https://github.com/noahbaxter/chart-preview/issues/17) *(may be resolved by cross-platform build fixes in 0.9.5-dev — needs verification)*
6. **Latency offset UI cleanup**

---

## Up Next

Fun stuff first. These are the features that make the plugin better.

7. **Section borders** — EVENTS track parsing, blue measure lines, section name overlay. Unlocks autodetection and section-aware features downstream.
8. **Solo sections** — Blue highway background during solo passages.
9. **Time sig changes display** — Symbols on left side, scroll with highway.
10. **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
11. **Better mouse scrolling** — shift=faster, ctrl=precise.

---

## Tech Debt

Do between features or when touching related code.

12. ~~**Deduplicate perspective math**~~ — Done. GlyphRenderer deleted, absorbed into NoteRenderer. All perspective math flows through PositionMath.
13. **NoteStateStore wrapper** — Bigger refactor. `noteStateMapArray` + `noteStateMapLock` passed as separate params to ~15 functions. Wrap into single class that enforces locking via API. Eliminates race condition footgun.
14. **Settings persistence audit** — Verify all options save/restore correctly.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Info display: BPM, time sig, measure, beat position
- Better menu system + advanced settings
- Instrument autodetection (by track name — depends on EVENTS parsing from #7)
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
- Audio-thread hygiene (remove std::function, preallocated vectors)
- Performance optimization (minimize REAPER API calls)

---

## Icebox

Blocked or no clear path forward.

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
- **Key dependency chain**: EVENTS parsing (#7) → section detection → autodetection → Real Drums MIDI refactor → generic gem system
