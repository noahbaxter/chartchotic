# Rendering Performance Fixes

Post-0.9.5 rendering pipeline changes (curved notes, offscreen compositing, bezier
positioning, SVG track overlay, highway texture) introduced significant per-frame overhead.

The dominant cost is **JUCE image rendering across multiple offscreen passes**. Note:
JUCE's software renderer treats `mediumResamplingQuality` and `highResamplingQuality`
identically (both bilinear, 4 source samples per pixel). On macOS CoreGraphics,
`highResamplingQuality` maps to `kCGInterpolationHigh` (Lanczos-ish) which IS more
expensive. Only `lowResamplingQuality` uses nearest-neighbor (1 sample). A stress test
frame with ~40 notes, ~20 sustains, ~15 gridlines, and highway texture on produces
**~1000+ scaled drawImage calls per frame**.

## SVG Migration Context

All assets except highway textures are moving to SVG in the near future. This means:

- **Note curvature** will be handled by path transforms, not the 12-strip drawImage hack
- **Gridlines** will be simple filled paths, not scaled 1024×64 PNG blits
- **Sustains/lanes** are already path-based (`fillPath`)

Optimizations targeting the raster note/gridline pipeline are **throwaway work**.
Focus on **durable wins** that survive the SVG migration:
- Highway texture rendering (stays raster forever)
- Perspective math (`pow()` calls, redundant computations)
- Frame-level caching (ValueTree lookups, dirty flags)
- DrawCallMap overhead (structural, independent of asset format)

---

## Benchmark Baseline (2026-03-03, macOS-arm64, 30 iterations)

### Cost Breakdown at 1080p (DRUMS)

| Scenario      | Total   | Texture | Build | Execute Draws | Grid    | Notes   | Bar     |
|---------------|---------|---------|-------|---------------|---------|---------|---------|
| empty         | 6.9ms   | 6.9ms   | 0     | 0             | —       | —       | —       |
| gridlines     | 15.4ms  | 7.0ms   | 0     | 8.3ms         | 8.3ms   | —       | —       |
| light (10n)   | 10.6ms  | 7.1ms   | 0     | 3.5ms         | 2.3ms   | 1.2ms   | —       |
| medium (30n)  | 17.4ms  | 7.2ms   | 0     | 10.2ms        | 6.3ms   | 3.5ms   | 0.3ms   |
| heavy (60n)   | 23.4ms  | 7.0ms   | 0     | 16.4ms        | 8.3ms   | 7.6ms   | 0.4ms   |
| stress (150n) | 38.0ms  | 7.2ms   | 0     | 30.7ms        | 12.2ms  | 17.5ms  | 1.1ms   |

### Key Observations

1. **Texture is constant ~7ms** regardless of note count — it's the highway background fill
2. **Gridlines are disproportionately expensive** — 20 gridlines cost 8.3ms (each gridline
   blits a 1024×64 PNG through software resampling)
3. **Notes scale linearly** with count — each curved note = 12 strip drawImage + 1 composite
4. **Build phases are negligible** (<0.1ms even at stress) — all cost is in drawing
5. **4K is ~4x 1080p** across the board — pixel-fill bound as expected

### Resolution Scaling (heavy scenario, drums)

| Resolution | Total   | Texture | Execute |
|------------|---------|---------|---------|
| 640×480    | 7.9ms   | 2.8ms   | 5.1ms   |
| 1280×720   | 14.4ms  | 4.6ms   | 9.8ms   |
| 1920×1080  | 23.4ms  | 7.0ms   | 16.4ms  |
| 3840×2160  | 70.9ms  | 26.0ms  | 45.0ms  |

---

## Status

Done:
- [x] ~~P0-A: Cache `drawCurved()` offscreen image~~ (curvedOffscreen member)
- [x] ~~P0-B: Cache 4:3 canvas offscreen image~~ (canvasOffscreen member)
- [x] ~~P1-A (partial): Deduplicate widthProgress/progress~~ (single `progress` variable)
- [x] ~~P2: Reduce offscreen supersampling scale~~ (both NOTE_RENDER_SCALE and HIGHWAY_RENDER_SCALE already at 1)

Remaining — **durable (survive SVG migration):**
- [ ] P9-A: Skip texture redraw when paused (dirty flag)
- [ ] P9-B: Render texture offscreen at reduced resolution
- [ ] P9-C: Low resampling quality for texture strip draws
- [ ] P9-D: Cap texture strip count / adaptive strips
- [ ] P9-E: Skip texture offscreen entirely (draw strips direct with clip)
- [ ] P5: pow() LUT for perspective math
- [ ] P5-B: Constexpr the perspective denominator
- [ ] P4: Return fretboard edge from `getColumnPosition`
- [ ] P6: Cache frame-level ValueTree lookups
- [ ] P8: Refactor DrawCallMap away from `std::function`

Remaining — **pre-SVG only (skip if SVG migration is imminent):**
- [ ] P0: Drop resampling quality for intermediate strip renders (notes)
- [ ] P1: Skip curved rendering when arc height is negligible (already has 1.5px threshold)
- [ ] P3: Cap highway texture strip count (superseded by P9-D)
- [ ] P7: Precompute edge LUT in `drawImageWithFade`

---

## Durable Optimizations (survive SVG migration)

### P9: Highway Texture Rendering

The highway texture is inherently raster — these optimizations are permanent.

**File:** `HighwayTextureRenderer.cpp`

The offscreen buffer is allocated at `width × height` pixels and **fully cleared + redrawn
every frame**, even when only the scroll offset changed. At 1920×1080, that's ~8MB cleared
and 40-150 bilinear-scaled strip draws. At 4K, ~32MB.

**Current cost:** ~7ms at 1080p, ~26ms at 4K (constant, independent of note count).

**What happens each frame:**
1. Check geometry cache — rebuild strip edge table if params changed
2. `offscreen.clear()` — zeroes `width × height × 4` bytes
3. Loop over 40-150 strips: compute dest rect (averaged trapezoid), compute source UV
   from scroll offset, per-strip fade opacity, `drawImage` with high resampling
4. Handle texture tile wrap at seam with two-draw split
5. `reduceClipRegion(fretboardPath)` + composite offscreen → canvas

#### P9-A: Skip redraw when paused (dirty flag) ⭐ Easy win

Strip geometry is cached, but strip *contents* are redrawn every frame because
`scrollOffset` shifts UVs. When paused, scrollOffset is static → entire offscreen
is identical to last frame.

**Fix:** Track `lastScrollOffset`. If unchanged AND geometry cache valid, skip the
strip loop and reuse previous offscreen.

**Impact:** ~7ms saved per frame when paused. Zero during playback.
**Risk:** None — purely additive check.

#### P9-B: Render offscreen at reduced resolution

The texture is tiled/scrolling — fine detail isn't critical. Rendering at 75% or 50%
resolution and upscaling on final composite would be visually identical.

**Fix:** Allocate offscreen at `width * 3/4 × height * 3/4`, scale strip coordinates.
Final composite stretches to full size.

**Impact:** ~44% fewer pixels at 3/4, ~75% fewer at 1/2.
**Risk:** Low — may need tuning. Compare visual quality.

#### P9-C: Low resampling quality for strip draws ⭐ Easy win

Each strip drawImage uses `highResamplingQuality`. On macOS CoreGraphics, that's
Lanczos-ish. These are intermediate renders into an offscreen that gets composited
at high quality anyway — the intermediate quality is wasted work.

**Fix:** `og.setImageResamplingQuality(juce::Graphics::lowResamplingQuality)` for
strip-to-offscreen draws. Keep high quality on the final offscreen→canvas composite.

**Impact:** Significant — each of 40-150 strips goes from Lanczos to nearest-neighbor.
**Risk:** None if visual quality holds. Final composite handles smoothing.

#### P9-D: Adaptive strip count ⭐ Good win

Currently `std::clamp(pixelHeight / 4, 40, 150)`. At 1080p the highway is ~600px
tall → 150 strips (the max). Perspective changes slowly near the strikeline (where
strips are large) and fast near the far end (where strips are small).

**Option 1 — Simple cap:** Lower max from 150 to 60-80.

**Option 2 — Variable-height strips:** Bigger strips near strikeline (slow perspective
change), smaller near far end (fast change). Could cut total strip count ~40% with
no visible quality loss. More complex to implement.

**Impact:** ~60% fewer strip draws at high resolutions with simple cap.
**Risk:** Very low — perspective gradient is smooth.

#### P9-E: Skip offscreen entirely

The offscreen exists solely for `reduceClipRegion(fretboardPath)` clipping. But strips
are already positioned within fretboard bounds. The clip only catches sub-pixel bleed.

**Fix:** Draw strips directly to `g` with clip path on main context (save/restore state).
Eliminates offscreen allocation, clear, and composite.

**Impact:** Eliminates largest costs (clear + composite).
**Risk:** Medium — verify clip path doesn't interfere with subsequent draws. Verify
strip edge seams look clean without offscreen masking.

---

### P5: Perspective Math — pow() LUT

**Files:** `PositionMath.cpp:57`, `GlyphRenderer.cpp:137`

```cpp
float progress = (std::pow(10, 0.5f * (1 - depth)) - 1)
               / (std::pow(10, 0.5f) - 1);
```

Called for every note, gridline, sustain, strip edge, lane — ~400+ times per frame.
Each call does 2 `std::pow()` evaluations. The denominator is constant (`sqrt(10) - 1`).

#### P5-A: Constexpr the denominator (trivial)

```cpp
constexpr float kPerspDenomInv = 1.0f / (3.16227766f - 1.0f);
float progress = (std::pow(10.0f, 0.5f * (1.0f - depth)) - 1.0f) * kPerspDenomInv;
```

Saves ~200 pow calls/frame. The numerator pow still dominates.

#### P5-B: Full LUT (256 entries)

`depth` ranges from ~-0.3 to ~1.12. Precompute 256 entries spanning this range.
At draw time: `float progress = perspLUT[(int)((depth - POS_START) * lutScale)]`.

One multiply + one table fetch replaces `pow(10, ...)`.

**Impact:** Small per-call but 400+ calls/frame. Estimated ~0.5-1ms total.
**Risk:** None — identical output within float precision. LUT is ~1KB.

#### P5-C: Fast pow approximation

Replace `std::pow(10, x)` with `exp2f(3.321928f * x)` (since `log2(10) ≈ 3.321928`).
`exp2f` is often a single instruction on ARM (NEON `fexp`).

**Impact:** Similar to LUT, less memory, slightly less precise.
**Risk:** Very low — precision difference is sub-pixel.

---

### P4: Eliminate redundant fretboard edge computation

**Files:** `PositionMath.cpp:125-152`, `HighwayRenderer.h:194-205`

`getColumnPosition()` calls `getFretboardEdge()` internally then discards the result.
Callers needing both column + fretboard edge recompute the fretboard edge separately.

Per-frame waste (stress): ~80 unnecessary `createPerspectiveGlyphRect` calls (each = pow + bezier + rect math).

**Fix:** Add `LaneCorners* outFretboardEdge` param to `getColumnPosition`. When non-null,
write the fretboard edge. Update `getColumnEdge` and callers.

**Impact:** Small-medium — eliminates ~80 redundant perspective computations.
**Risk:** Low — internal signature change, all callers in HighwayRenderer.

---

### P6: Cache frame-level ValueTree lookups

**Files:** `HighwayRenderer.cpp:62,103,159,189,201,215,323,365,398,532,638`

`isPart(state, Part::DRUMS)` called in `drawGem`, `drawSustain`, `drawGridline`,
`getColumnEdge`, etc. Each is a `ValueTree::getProperty()` string hash lookup.
Same for `gemScale`, `starPower`, `hitIndicators`. All constant for the entire frame.

**Fix:** Cache at top of `paint()`:
```cpp
struct FrameState {
    bool isDrums;
    bool starPower;
    bool hitIndicators;
    float gemScale;
    float wNear, wMid, wFar;
    const NormalizedCoordinates& fbCoords;
};
```

**Impact:** Eliminates hundreds of string hash lookups per frame.
**Risk:** None.

---

### P8: Refactor DrawCallMap away from std::function

**Files:** `HighwayRenderer.cpp`, `Utils.h:59`

```cpp
using DrawCallMap = std::map<DrawOrder, std::map<uint, std::vector<std::function<void(Graphics&)>>>>;
```

Every visible element creates a `std::function` lambda. Captures likely exceed the
24-32 byte small-buffer optimization → heap allocation per closure. Nested `std::map`
has poor cache locality.

**Fix:** Flat `std::vector<DrawCall>` with tagged union/struct + draw type enum.
Sort by `{drawOrder, column}` once, iterate linearly.

**Impact:** Better cache locality, fewer allocations. Hard to quantify without profiling.
**Risk:** Medium — largest refactor. Defer unless earlier fixes aren't sufficient.

---

## Pre-SVG Only (skip if SVG migration is imminent)

These target the raster note/gridline pipeline. Once SVGs replace PNGs, the cost
model changes completely (path rasterization vs image blitting).

### P0: Drop resampling quality for intermediate note renders

**Files:** `HighwayRenderer.cpp:278,303`

Each curved note's 12 strip draws use `highResamplingQuality`. Same logic as P9-C —
intermediate quality is wasted since the final composite handles smoothing.

**Impact:** ~50% reduction in note rendering time.
**Risk:** None.

### P1: Skip curved rendering for negligible arc heights

**File:** `HighwayRenderer.cpp:263`

Already has `abs(arcHeight) < 1.5f` threshold. Could extend with size-based check:
if `rect.getWidth() < 30px`, skip curvature (the arc is sub-pixel at that size).

Far-distance notes are tiny and curved rendering is overkill. Many of the 150 notes
in stress scenario are far away.

**Impact:** Eliminates 30-40% of drawCurved calls in heavy/stress scenarios.
**Risk:** Very low.

### Note curvature caching (SKIP — SVG solves this)

Pre-rendering curved notes at discrete position buckets (8 arc heights per glyph).
Would turn 13 drawImage calls → 1 per note. Biggest single win for raster notes.

**Not worth building** — SVG path transforms will handle curvature naturally with
a single `fillPath` per note instead of 13 `drawImage` calls.

### Gridline image downsize (SKIP — SVG solves this)

Current 1024×64 PNG is oversized for what's needed. A 256×16 source would cut
per-pixel resampling by ~16x. But gridlines as SVG paths will be simple filled
rectangles — no image scaling at all.

---

## Execution Plan

### Phase 0 — Measure (do first)

Add timing instrumentation to `drawTexture()` to confirm where time goes:
```cpp
auto t0 = Clock::now();  // offscreen.clear()
auto t1 = Clock::now();  // strip loop
auto t2 = Clock::now();  // composite
```

This tells us if the cost is in clear, strips, or composite before optimizing blindly.

### Phase 1 — Highway texture (biggest durable wins)

P9-C (low quality strips) → P9-D (cap/adaptive strips) → P9-A (pause skip)

These are constant/threshold changes, no refactoring. Combined target: 50%+ reduction
in texture rendering time (~3.5ms saved at 1080p, ~13ms at 4K).

### Phase 2 — Perspective math

P5-A (constexpr denominator) → P5-B or P5-C (LUT or fast pow) → P4 (eliminate redundant edge)

Helps everything — notes, gridlines, sustains, texture strips. Target: ~1-2ms saved.

### Phase 3 — Frame caching

P6 (ValueTree cache) → minimal effort, eliminates hundreds of hash lookups.

### Phase 4 — Structural (only if needed)

P8 (DrawCallMap refactor) — largest change, defer unless Phases 1-3 aren't sufficient.
