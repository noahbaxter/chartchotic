# Rendering Refactor — April 2026

**Status:** Shipped on `refactor/frame-rendering` branch. Awaiting commit-history cleanup before merge.

**Authors:** Noah Baxter, with Claude Code (April 22 – April 25, 2026)

**TL;DR:** Rewrote how chart elements get from MIDI to pixels. Replaced `isDrums ? guitar : drum` branching at every read site with a `RenderTypeConfig` indirection. Replaced per-sprite perspective math (which had a depth-drift bug) with a `Frame` composite that shares one anchor + one scale per row. Added bemani-specific Z knobs (`cymNudge`), wired hits to track gem nudges, baked tuned defaults, removed ~70 LOC of duplication. Net `−27 LOC` across 16 files in the final cleanup pass.

The commit history that produced this is messy on purpose — the work happened in tight feedback loops with a real chart in REAPER, and many small "tune X by Y" commits coexist with the structural ones. This doc is the readable version. See "Commits, by theme" at the end for the mapping.

---

## Why this happened

Two long-standing problems converged:

1. **Gem-on-bar depth drift.** Chord rows (kick bar + 4 toms) rendered with each sprite using its own perspective projection. The gap between the bar and a tom-on-top stretched/compressed nonlinearly with depth — see `docs/Z_POSITIONING.md` for the postmortem on the two earlier attempts to fix this by tuning. Both reverted; the math doesn't allow a position-space Z lift to project to a constant pixel ratio in a fake-3D pipeline.
2. **Tuning state was scattered and brittle.** Per-instrument values (offsets, lane coords, scales, perspective params, bemani nudges) lived in paired arrays (`guitar*`/`drum*`) read via `isDrums ? a : b` ternaries at dozens of sites in `NoteRenderer`, `SustainRenderer`, `GridlineRenderer`, `AnimationRenderer`. Adding a new render style would mean updating every site. The debug panel duplicated state across `DebugTuningPanel` and `SceneRenderer`, with sync churn every paint.

The Frame system was conceived as the architectural fix to (1). `RenderTypeConfig` was the data-side fix for (2). Everything else followed from there.

---

## What changed, by theme

### 1. Data identity: `RenderTypeConfig`

`Chartchotic/Source/Visual/Utils/RenderTypeConfig.{h,cpp}`

Replaced pairs of `guitar*` / `drum*` lookup tables and `isDrums ? a : b` branches in renderers with a single const-pointer indirection: `config->X`. The struct is a POD bundle with static-lifetime singleton instances per `RenderType`:

- `laneCount`
- `fretboardCoords`, `bezierLaneCoords` (pointers into the existing constant tables)
- `instOffsets`, `colAdjust`, `animOffsets`
- `getPerspectiveParams()` — function pointer (returns by value so DEBUG live-tuning and RELEASE constexpr defaults share one path)
- Bemani accessors (`bemaniGemNudge`, `bemaniLaneEndPx`, `bemaniHwyScale`, `bemaniHitBarNudge`) — function pointers so live debug tuning still works

Renderers ask `getRenderTypeConfig(getRenderType(activePart))` once per `populate()` and read `config->X` from then on. Result: zero `isDrums?guitar:drum` ternaries in the hot path for data lookups; behavior branches that genuinely differ (cymbal-only logic, HOPO rendering) still use `isDrumLike()` / `isGuitarLike()` because they're different *behavior*, not different *data*.

### 2. Frame composite: shared anchor + scale per row

`Chartchotic/Source/Visual/Utils/Frame.h`, `FrameRenderer.{h,cpp}`

A `Frame` is a list of `FrameSprite`s sharing one anchor (projected screen position) and one scale (per-axis perspective factor). `drawFrame(frame, anchor, scale, drawCalls)` enqueues one draw call per sprite, computing each sprite's pixel center and size as:

```
pixel_center = anchor + (offsetX * scale.x, offsetY * scale.y)
pixel_size   = (width * scale.x, height * scale.y)
```

Both `offsetY` and `height` multiply by the same `scale.y`, so within a frame the offset-to-size ratio between any two sprites is invariant with depth. **Gem-on-bar drift is gone by construction**, not by tuning.

Migration order (one commit per phase, all revertable):
- `6fde8f4` — types + helper + 7 Catch2 tests, no callers yet
- `5b03097` — `NoteRenderer::drawGem` → `appendGemSprites` building into a per-row composite (perspective path only)
- `95f027d` — composite chord rendering goes live; isotropic scale ratio; defaults retuned
- `8bf031a` — `GridlineRenderer` migrated to single-sprite Frame in the perspective path
- `fc1b85a` — bemani path extracted from `appendGemSprites` into its own `drawGemBemani` (also Frame-based)

The bemani path uses Frame too, with anchor at the gem's screen position and scale `(1, 1)` (no perspective). It doesn't need composite drift protection (chord stacks don't drift in flat mode) but going through the same `drawFrame` keeps the rendering pipeline uniform.

### 3. Z separation: cymbals tuned independently from toms

`485cd68`, `bf8a6f4`

Drums had one `gemZ` value used for both toms and cymbals. Cymbal art is taller than tom art, so the same Z offset doesn't produce the same visual overhang. Split into:

- `InstrumentOffsets::cymZ` (perspective)
- `BemaniConfig::cymNudge` (bemani — added in the WIP commit)

Both render paths route cymbals to the cym-specific value when `gem ∈ {CYM, CYM_GHOST, CYM_ACCENT}`. Cymbal hit animations also follow the cym nudge in bemani — see "Bemani tuning" below.

### 4. Debug panel cleanup

The debug panel (`Chartchotic/Source/DebugTools/DebugTuningPanel.{h,cpp}`) accumulated cruft over time. Cleanup commits in order:

- `0289018` — surface useful sections at top, add Cym row
- `a5ab0c3` — narrow + mono font + bigger spacing; **hide "noise" sections** (perspective, track layers, hit scale, strike, lane shape, etc. — the bemani section was hidden as collateral damage here)
- `5bce9cc` — double-click any tunable to type an exact value
- `b0fb2b2` — scrollable + click-to-nudge; bake cym overlay offsets
- `e9980c3` — unified Adjust table with glyph icons; bake cym + bar tunings

The Adjust table (`Note / Cymbal / HOPO / Bar / Gtr Tap / Drm Ghost / Drm Accent / Cym Ghost / Cym Accent / SP Gem / SP Bar` × `X / Y / W / H / S`) is the only visible tuning surface. Everything else (perspective params, lane shape, etc.) is wired but invisible.

The bemani section was inadvertently hidden by `a5ab0c3` — re-exposed in the WIP cleanup commit so users can tune `gemNudge*`, `cymNudge`, `barNudge`, `gemW/H`, `barW/H`, lane Z, sustain Z, gridline Z, hit-bar nudge, etc. The Adjust table now also routes its Note/Bar W/H rows to per-instrument fields based on `panelIsDrums` (set when the user toggles part in the toolbar) — see "Per-instrument debug state" below.

### 5. Bemani tuning

WIP cleanup commit `00740f4`:

- **`bemaniConfig.cymNudge`** added (parity with perspective `cymZ`). `drawGemBemani` picks `cymNudge` when the gem is cymbal-typed, else `gemNudge(isDrums)`.
- **Hit animations now track gem nudges in bemani.** `AnimationRenderer::renderFretAnimation`'s bemani re-center used to translate the hit rect to `padY = h * strikelinePos`, ignoring the gem's Y nudge. So if you set `gemNudge=−0.010` the gem moved up but the hit landed at padY — visible mismatch. Now the hit re-center honors `padY + pixelsPerUnit * nudge` with `nudge ∈ {cymNudge, gemNudge(isDrums)}` per gem type.
- **Defaults baked from your tuned values:**
  - `gemNudgeGuitar`: `0.010 → −0.010` (sign flip — previous default was wrong direction)
  - `gemNudgeDrums`: `0.010 → −0.010`
  - `cymNudge`: `0.000 → −0.010` (matches gem nudges)
  - `barNudge`: `0.000 → 0.015`
  - `gemW`: `0.95 → 0.75` (gems were too wide)
  - `barSustainWidth`: `0.90 → 1.00`

### 6. Hot-path perf (cached config + arrays-as-pointers)

The big win: per-gem config lookups + 16-float-per-frame array copies are gone.

**Cached at populate-time** (set once in `NoteRenderer::populate` / `SustainRenderer::populate`, read per gem):

- `currentConfig` — `getRenderTypeConfig()` was being called once per gem (`appendGemSprites:113`, `drawGemBemani:344`, `drawSustainBody:153`). Now once per populate.
- `currentVpDepth` — `config->getPerspectiveParams().vanishingPointDepth` was a 40-byte struct return per gem to read one float.
- `currentNoteCurvature` — was an `isDrums ? a : b` branch per gem.

**Const-pointer arrays** (no per-frame copy):

- `NoteRenderer::overlayAdjusts`, `guitarColAdjust`, `drumColAdjust` — were arrays receiving `std::copy_n` / for-loop writes from `SceneRenderer::paint` every frame. Now `const T*` pointers into scene-side arrays. SceneRenderer just sets the pointer.
- `AnimationRenderer::drumColAdjust` — same treatment; the per-frame `for (int i=0; i<5; i++) drumColZAdjust[i] = drumColAdjust[i].z * resScale;` is gone.

The trade-off: `ColumnAdjust::z` values are tuned at `REFERENCE_HEIGHT` and need to be multiplied by `resScale` at the read site (NoteRenderer.cpp:274, AnimationRenderer.cpp:315) instead of pre-baked. One extra multiply per cymbal/tom Z apply — negligible.

**Curve cache** (`getCurvedImage`): cache key now includes the curvature value (quantized to 1e-4) so dragging the curvature slider through old/new values doesn't flush the cache on every step. Capped at 100 entries to bound RAM during slider drags (~1.6 MB peak vs ~16 KB before; still trivial).

### 7. Per-instrument debug state

`SceneRenderer::paint` used to have an epsilon check:

```cpp
bool gemScaleAtGuitarDefault = std::abs(gemScale.width - GUITAR_GEM_SCALE.width) < 0.001f && ...;
if (isDrums) noteRenderer.gemScale = gemScaleAtGuitarDefault ? DRUM_GEM_SCALE : gemScale;
```

This conflated "user has tuned" with "happens to equal guitar default". If the user happened to tune the drum scale to exactly the guitar default value, it'd be silently overridden.

Replaced with explicit per-instrument fields on both `DebugTuningPanel` and `SceneRenderer`:

- `guitarGemScale` / `drumGemScale`
- `guitarBarScale` / `drumBarScale`

`DebugTuningPanel::setDrums(bool)` (already called from `PluginEditor` on toolbar part-change) now also flips `panelIsDrums` and refreshes the Adjust labels. `getAdjustPtr(r=0|3, c=2|3)` routes to the active-instrument field. `SceneRenderer::paint` just picks `isDrums ? drum* : guitar*`. The epsilon hack is gone.

### 8. Code consolidation (helpers + dead code)

WIP cleanup commit:

- **Three helpers extracted from NoteRenderer**, removing ~70 LOC of duplication between perspective and bemani paths:
  - `gemTypeScale(gem, isDrums, scales)` — the per-gem-type scale switch (was duplicated identically in `appendGemSprites` and `drawGemBemani`)
  - `getOverlayAdjustForGem(gem, isDrums)` — overlay-adjust selector lambda (was duplicated)
  - `applyCurvedImageSwap(frame, gemIdx, ovlIdx, args)` — curvature-swap block that replaces gem (and overlay) sprite images with cached curved variants and adjusts heights/offsets
- **Shared helper:** `PositionMath::columnDistFromCenter(fbCoords, colCoords)` — same formula was inline in both `NoteRenderer::getColumnDistFromCenter` and `AnimationRenderer::renderFretAnimation`.
- **Dead code removed:**
  - `NoteRenderer::getOverlayGlyphRect` — orphaned static, no callers since the Frame migration
  - `SceneRenderer::getColumnEdge` — private member, never called
  - `Frame::position` / `column` / `isBar` — set by callers, never read by `drawFrame`. Pure metadata noise.
  - `LANE_SIDE_CURVE = 0.0f` constant + 4 always-false `if (std::abs(LANE_SIDE_CURVE) > 0.001f)` guards in `SustainRenderer.cpp`
- **Renamed:** `NoteRenderer::drawFrame` → `drawNoteRow` (member function was the orchestrator; `PositionConstants::drawFrame` is the actual draw helper — same name in the call graph caused confusion)
- **Stale doc comments:** `RenderTypeConfig.h` had "Phase 1 / Phase 4 will…" planning artifacts. Replaced with current-state docs.

---

## What's better now

| Concern | Before | After |
|---|---|---|
| Gem-on-bar drift | Position-space Z lift drifted with depth (two attempts reverted in `Z_POSITIONING.md`) | Eliminated by construction — Frame shares scale across sprites |
| Tunable separation | Cymbal Z piggy-backed on tom Z in both modes | `cymZ` (perspective) + `cymNudge` (bemani) both present |
| Bemani hit alignment | Hits pinned to strikeline pad regardless of gem nudge | Hits track the gem's bemani Y nudge per gem type |
| Per-instrument scale | Epsilon check guessing whether user tuned | Explicit `guitar*Scale` / `drum*Scale` fields, panel routes by active part |
| Hot-path lookups | `getRenderTypeConfig()` per gem, `getPerspectiveParams()` per gem | Cached once per `populate()` |
| Per-frame array churn | 16 float writes per frame from SceneRenderer into NoteRenderer arrays | `const T*` pointers, zero copies |
| Curve cache invalidation | Full flush on every curvature change | Keyed by curvature, cache hit on revisits, 100-entry cap |
| Debug panel | Bemani section hidden, untunable | Bemani Position/Sustains/Visual sections all visible |
| Code volume | — | ~70 LOC of duplication gone, dead code purged, net `−27 LOC` in final pass |

---

## What's still open

Skipped during the cleanup pass; flagged as follow-ups:

- **`drumColumnIndex` consolidation.** Inline ternaries in `NoteRenderer::getColumnDistFromCenter:492` and `AnimationRenderer::renderFretAnimation:295` have an extra defensive clamp (`(col < DRUM_LANE_COUNT) ? col : 1`) that the existing helper doesn't. Replacing would lose defensive coverage; leaving inline preserves it. Low priority.
- **Stringly-typed state property keys** (`"gemScale"`, `"hitIndicators"`, `"starPower"`). JUCE `ValueTree` convention; constants would be cleaner but typo-bomb risk is real. Low priority.
- **Bemani function-pointer indirection** (`config->bemaniGemNudge()` etc.). One indirect call per gem; CPU branch-predicts these. Marginal perf, would break the live-tuning design. Skipping.
- **`LANE_WIDTH` / `LANE_OPEN_WIDTH` mutable inline globals in `PositionConstants.h`.** Sit alongside `constexpr` constants in the same namespace, which is confusing. Move to a dedicated runtime-tuning struct (or fold into bemaniConfig-style global). Touches every read site.
- **Further `NoteRenderer` field reduction.** Twelve fields still receive per-frame writes from `SceneRenderer::paint` (`gemScale`, `barScale`, `gemTypeScales`, `noteCurvatureGuitar/Drums`, `gemZOffset`, `cymZOffset`, `barZOffset`, `strikePosGem`, `strikePosBar`, `showGems`, `showBars`). Total ~12 floats/frame — not a perf issue, but the architectural smell of "scene writes to sub-renderer's public fields every frame" persists. Bigger refactor: hand a `const RenderConfig&` ref via `populate()`.
- **AnimationRenderer perspective-params caching.** Currently calls `config->getPerspectiveParams()` per active animation (~7 max). Negligible, but mirrors the NoteRenderer fix that already shipped.
- **Gem+overlay sprite-build duplication.** `appendGemSprites` (177 LOC, perspective) and `drawGemBemani` (138 LOC, bemani) share helpers (`gemTypeScale`, `applyCurvedImageSwap`, `getOverlayAdjustForGem`) but the *callers* still inline the same triad: build gem sprite → build overlay sprite (if present) → curvature swap. Extracting `buildGemSprite(frame, args)` + `buildOverlaySprite(frame, args)` would shed another ~60-80 LOC and bring the two paths to the same shape. Deferred from the cleanup pass — defensible to leave because the perspective path uses strike-reference offsets (non-zero) and the bemani path uses screen-space offsets (zero, with anchor at the gem's center), so the two callers don't quite share a signature. Easy follow-up; would shrink `NoteRenderer.cpp` from ~608 LOC to under 550.
- **`drawNoteRow` vs `appendGemSprites` naming.** Mixed conventions — one named for its call-graph role (orchestrator), the other for its side effect (appends to a Frame). Cosmetic, but worth standardizing to either side-effect-style or role-style across both. Trivial.

---

## Commits, by theme

The chronological order is messy; the logical groupings are:

**Data layer (`RenderTypeConfig`)**
- `ba7a4f7` add RenderTypeConfig to bundle per-render-style data
- `55777d9` migrate renderers to RenderTypeConfig
- `44069d4` hoist isDrums decl in NoteRenderer::drawGem

**Z separation**
- `485cd68` add cymZ field — cymbals tunable independently from toms
- `bf8a6f4` tune: bake DRUM_OFFSETS.cymZ = 12 (visible cymbal overhang above bar)
- `5d27af4` docs: Z-positioning postmortem + backlog parking-lot
- `fa1405c` tune: bump NOTE_DEPTH_FORESHORTEN 0.55→0.80, BAR_FRETBOARD_FIT 1.15→1.0

**Frame system (the architectural fix)**
- `6eb2601` spec: frame-based rendering refactor
- `40e9ecd` plan: frame-rendering refactor, 5 phases
- `6fde8f4` frame: scaffold Frame/FrameSprite types and drawFrame helper
- `5b03097` wip: Phase 2 — gem+bar drawGem migration to Frame, two-axis scale
- `95f027d` frame: composite chord rendering, isotropic scale, retune defaults
- `8bf031a` frame: migrate GridlineRenderer to Frame / drawFrame
- `fc1b85a` refactor: extract Bemani path from appendGemSprites
- `ce0ec4a` tune: drum Z defaults + bake 15% size bump into GEM_SIZE

**Debug panel evolution**
- `0289018` debug panel: add Cym row + surface useful sections at top
- `a5ab0c3` debug panel: hide noise sections, mono font, bigger spacing
- `6302e97` debug panel: re-expose Overlay Adjust section for accent/ghost tuning
- `5bce9cc` debug panel: double-click any tunable to type a precise value
- `b0fb2b2` debug panel: narrow + scrollable + click-to-nudge; bake cym overlay offsets
- `e9980c3` debug panel: unified Adjust table with glyph icons; bake cym + bar tunings

**Cleanup pass (single WIP commit, will be split)**
- `00740f4` WIP: rendering refactor + bemani tuning (will split)

The four logical buckets inside `00740f4`, ready to split:

1. **refactor**: helpers (gemTypeScale, getOverlayAdjustForGem, applyCurvedImageSwap), dead-code removal (getOverlayGlyphRect, SceneRenderer::getColumnEdge, Frame metadata, LANE_SIDE_CURVE), shared columnDistFromCenter, drawFrame→drawNoteRow rename, stale RenderTypeConfig doc comments
2. **perf**: cached config/vpDepth/curvature per populate; arrays as const-pointers + resScale; AnimationRenderer drumColZ pointer; curve cache key + cap
3. **refactor**: per-instrument gemScale/barScale, drop gemScaleAtGuitarDefault epsilon hack
4. **bemani**: cymNudge field/descriptor + use site, re-expose bemani panel section, hit-anim gem-nudge wiring, baked tuned defaults
