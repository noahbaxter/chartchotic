# Shared Track Baking + Pooled Highways

## Problem

`rebuildVisibleSlots()` destroys and recreates HighwayComponents on every difficulty/
instrument toggle. Each new component triggers `TrackRenderer::rebuild()` baking 5
perspective images (~10MB, 50-100ms). This causes visible stutters.

## Goals

1. **Zero frame stutter** on difficulty toggle (Expert→Hard, etc.)
2. **Zero frame stutter** on instrument toggle (Guitar→Drums)
3. **Minimal stutter** on layout change (1 highway→4 highways) — at most one
   debounced rebake, not per-highway
4. **Correct rebake** on window resize and perspective param change (highway length)
5. No visual regression — scaled bakes must be pixel-correct

## Two independent mechanisms

### Mechanism A: Shared TrackImageCache

Guitar and drums each need exactly one set of baked track images (fadedTrack + 4
layer overlays). The perspective math in `getFretboardEdge()` computes all positions
as proportions of (width, height). A bake at 600×450 scaled to 300×225 via
`drawImage` produces identical proportional geometry.

**Cache holds:** 2 entries (guitar, drums). Each has fadedTrack + 4 layer images.
Baked at full single-highway resolution using a scratch TrackRenderer.

**Invalidation:** Only on:
- Window resize (new full resolution)
- Perspective param change (highway length, fade curve)
NOT on difficulty toggle, instrument toggle, layout change.

**Memory:** ~10MB per instrument type = ~20MB total. Same as one highway today × 2.

### Mechanism B: Pool 4 HighwayComponents

`std::array<HighwaySlot, 4>` created in editor constructor, never destroyed.
Hidden highways (`setVisible(false)`) cost zero per-frame — JUCE skips `paint()`.

On toggle: reconfigure slot properties (part, interpreter, skillLevel) and
flip visibility. No component creation/destruction.

## Architecture: what happens on each event

### Difficulty toggle (e.g. Expert → Hard, same instrument)
1. `updateVisibleSlots()` reconfigures slot interpreters with new skillLevel
2. `setVisible(true/false)` as needed
3. If slot count changed → call `resized()` for layout only
4. Point each active highway's overlays at the correct cache entry
5. **No `loadState()`, no `rebuildTrack()`, no disk I/O**
6. Cost: ~0ms

### Instrument toggle (e.g. Guitar → Drums)
1. `updateVisibleSlots()` reconfigures slot parts + interpreters
2. `setActivePart()` on each active highway
3. Point overlays at the drums cache entry instead of guitar
4. **No `rebuildTrack()` needed** — cache already has both bakes
5. Cost: ~0ms

### Layout change (1 highway → 4 highways)
1. `updateVisibleSlots()` activates additional slots
2. `resized()` assigns new bounds → triggers HighwayComponent::resized() debounce
3. New highways paint using cached track images immediately (drawImage scales)
4. After 500ms debounce, per-highway texture prebake rebuilds (needed because
   scanline LUT depends on viewport size)
5. Cost: instant visual, texture catches up after debounce

### Window resize
1. `resized()` detects dimension change → `trackImageCache.invalidate()`
2. Debounce timer fires → `rebakeTrackCache()` at new resolution
3. Highways paint with stale (stretched) cache during debounce, then crisp after
4. Cost: same as today, but only 2 bakes (guitar+drums) instead of per-highway

### Highway length slider
1. `setHighwayLength()` changes perspective params on each highway
2. `trackImageCache.invalidate()` — params changed
3. Debounced rebake at new params
4. Cost: same as today

## What `rebuildTrack()` should do (with cache active)

When `trackImageCache` is set and valid:
- `sceneRenderer.rescaleAssets(w)` — only if width changed (it caches internally)
- `sceneRenderer.overlayYOffset = topOverflow`
- Set overlay pointers to cache entry for current instrument
- **Skip `trackRenderer.rebuild()` entirely** — geometry is only needed for texture
  prebake, which can be deferred to the debounce timer
- Update `bakedRenderW/H/Overflow`

When cache is NOT set (standalone mode, no REAPER session):
- Full `trackRenderer.rebuild()` as before (backward compatible)

## What `updateVisibleSlots()` should do

1. Hide all slots
2. Configure each needed slot (part, skillLevel, interpreter, ownedState)
3. `setActivePart()` on each highway
4. `setVisible(true)` on active slots
5. If visible slot count changed: `resized()` (layout only)
6. Set overlay pointers on each active highway to correct cache entry
7. Apply visual settings directly (NOT via `loadState()`)
8. **Never call `loadState()`** — that's for startup only

## What `loadState()` should be

Startup-only. Called once in constructor after everything is wired.
Should NOT be called from `updateVisibleSlots()` or any toggle path.

Visual settings that need to propagate to new slots should be applied
directly in `updateVisibleSlots()` by reading from `state` properties,
not by re-running the entire loadState sequence.

## Critical bugs in current implementation (as of this writing)

### Bug 1: `updateVisibleSlots()` calls `loadState()`
`loadState()` triggers disk I/O (texture reload), multiple `rebuildTrack()` calls,
and `setHighwayLength()` per highway. This completely undermines the cache and pool.
**Fix:** Remove `loadState()` call. Apply needed visual settings directly.

### Bug 2: `resized()` cache invalidation check is wrong
Compares `sceneWidth` against `trackImageCache.getBakedWidth()`, but the cache is
baked at `slots[0].highway->renderWidth` which differs from `sceneWidth` in
multi-slot mode (each slot is smaller).
**Fix:** Compare against the actual render dimensions that the cache was baked at.
Or better: only invalidate when the full-resolution bake dimensions would change
(i.e. sceneWidth/sceneHeight change, since single-highway uses those).

### Bug 3: `rebuildTrack()` still calls `trackRenderer.rebuild()` with cache active
Even with `geometryOnly=true`, `rebuild()` does geometry cache + `rebuildPrebake()`.
`rebuildPrebake()` allocates a mip atlas image and populates a scanline LUT. This
is unnecessary on a difficulty/instrument toggle where dimensions haven't changed.
The TrackRenderer's own early-out catches same-dimension calls, but an instrument
change flips `isDrums` which defeats the early-out.
**Fix:** When cache is active and no dimensions/params changed, skip
`trackRenderer.rebuild()` entirely. Only update overlay pointers.

### Bug 4: `onInstrumentChanged()` calls `trackRenderer.invalidate()` + `rebuildTrack()`
The `invalidate()` resets cached dimensions, defeating the early-out in `rebuild()`.
With the cache active, instrument changes should only swap overlay pointers.
**Fix:** When cache is active, `onInstrumentChanged()` should just update overlays.

## Implementation order

1. Fix Bug 1 — remove `loadState()` from toggle paths, apply settings directly
2. Fix Bug 3 — `rebuildTrack()` skips `trackRenderer.rebuild()` when cache valid
   and only dimensions/part drive whether we need to re-point overlays
3. Fix Bug 4 — `onInstrumentChanged()` becomes overlay-swap-only with cache
4. Fix Bug 2 — correct the invalidation dimension check
5. Verify: difficulty toggle = zero rebuild work
6. Verify: instrument toggle = zero rebuild work
7. Verify: resize = single debounced rebake of cache
8. Verify: highway length change = cache invalidation + debounced rebake
