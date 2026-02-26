# Backlog

Prioritized top-to-bottom. Work from the top.

---

## CI/CD & Infrastructure

Solid foundation before shipping fixes.

1. ~~**In-plugin update checker**~~ — Done. UpdateChecker.h/.cpp, checks GitHub releases API on editor open, dev/release channel support.
2. ~~macOS code signing & notarization~~ — Done. Signing + notarization in CI, .pkg installer with VST3+AU.
3. pluginval CI testing
4. ~~VERSION file + auto-release workflow~~ — Done. VERSION file, release-dev.yml (dev-latest prerelease on push to dev), release.yml (tagged release on VERSION change to main).
5. ~~Windows installer (Inno Setup)~~ — Done. .iss script in .ci/, built in CI.
6. R2 CDN upload for distribution
7. build.sh script (standardized local build)

---

## User-Reported Fixes

Real users waiting on these. *(Discord, HopH₂O, 2026-02-21 unless noted)*

8. **Speed slider overhaul** — Three related issues: slider is inverted (`displayWindowTimeSeconds` means higher=slower), needs "Slower/Faster" labels instead of raw number, default should be 1.15-1.20 range. Past commits went back and forth — needs investigation.
9. **UI layout overhaul** — Controls hidden behind taskbar, highway too small, poor responsive scaling. Moving controls to top likely fixes clipping AND gives highway more space.
10. **Window sizing persistence** — Save/restore on REAPER restart.
11. **#17 — Linux REAPER scan failure** — [GitHub](https://github.com/noahbaxter/chart-preview/issues/17)
12. **Latency offset UI cleanup**

---

## Up Next

Fun stuff first. These are the features that make the plugin better.

13. **Section borders** — EVENTS track parsing, blue measure lines, section name overlay. Unlocks autodetection and section-aware features downstream.
14. **Solo sections** — Blue highway background during solo passages.
15. **Time sig changes display** — Symbols on left side, scroll with highway.
16. **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
17. **Better mouse scrolling** — shift=faster, ctrl=precise.

---

## Tech Debt

Do between features or when touching related code.

18. **Deduplicate perspective math** — ~30 min. `GlyphRenderer::createPerspectiveGlyphRect()` and `PositionMath::createPerspectiveGlyphRect()` are identical. Delete GlyphRenderer's copy, call PositionMath's. Also fix 4 inlined width-scaling copy-pastes in GlyphRenderer → use `applyWidthScaling()`.
19. **Resolve `pluginIsMidiEffectPlugin` jucer flag** — Uncommitted change in .jucer adds this flag + changes VST3 category Analyzer→Tools. Probably an attempt to fix #3 (Logic). Either test in REAPER and commit, or revert. Don't leave it dangling.
20. **NoteStateStore wrapper** — Bigger refactor. `noteStateMapArray` + `noteStateMapLock` passed as separate params to ~15 functions. Wrap into single class that enforces locking via API. Eliminates race condition footgun.
21. **Settings persistence audit** — Verify all options save/restore correctly.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Info display: BPM, time sig, measure, beat position
- Better menu system + advanced settings
- Instrument autodetection (by track name — depends on EVENTS parsing from #13)
- Highway length control (configurable visible beats)
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
- CMake migration (from Projucer)
- AudioProcessorValueTreeState migration
- Double-buffered snapshots for renderer
- Audio-thread hygiene (remove std::function, preallocated vectors)
- Performance optimization (minimize REAPER API calls)

---

## Icebox

Blocked or no clear path forward.

- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chart-preview/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic. `pluginIsMidiEffectPlugin` flag (#19) may fix.* [GitHub](https://github.com/noahbaxter/chart-preview/issues/3)
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
- **Key dependency chain**: EVENTS parsing (#13) → section detection → autodetection → Real Drums MIDI refactor → generic gem system
