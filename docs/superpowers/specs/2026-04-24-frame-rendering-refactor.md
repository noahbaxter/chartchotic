# Frame-Based Rendering Refactor — Design

**Status:** Design pending user review
**Date:** 2026-04-24
**Author:** Noah Baxter, with Claude Code
**Related:** `docs/Z_POSITIONING.md` (postmortem of prior drift-fix attempts), `BACKLOG.md` "Rendering — perspective limits" section.

---

## TL;DR

Introduce a `Frame` abstraction over rhythm-game objects (gems, bars, gridlines, overlays) so each object is a group of sprites with **one shared perspective scale factor**. The scale factor drives every pixel dimension of the frame — sprite size, Z lift, overlay offsets — so members of a frame cannot drift apart with depth by construction, not by tuning.

This simultaneously:
1. **Fixes the gem-on-bar drift bug** (Invontor, Discord Apr 3–10; two prior patches reverted; full postmortem at `docs/Z_POSITIONING.md`).
2. **Unifies per-sprite position math** currently duplicated across `NoteRenderer`, `GridlineRenderer`, and `AnimationRenderer`.
3. **Makes future composite elements** (solo sections, BRE markers, authoring ghost notes, section labels) drop-in additions rather than re-solving position math each time.

Scope: ~400–500 LOC, 5 phased commits, comparable to the `RenderTypeConfig` refactor (`ba7a4f7`→`44069d4`). Not a pipeline rewrite — `createPerspectiveGlyphRect` stays, sustains/text-events are unchanged, no JUCE OpenGL.

---

## Problem

### The immediate bug

Gems sitting above bars drift onto them at depth. Discord reports from Invontor (April 2026) triggered two prior fix attempts (`9128b93` position-space offset, `faa7133` pixel-space lift, both reverted — see `docs/Z_POSITIONING.md`). A third attempt inside the current session (swap width-ratio for height-ratio) was also wrong because it ignored the `foreshorten` multiplier on sprite height. All three attempts failed because they treated zOff as an independent scalar and tried to match its curve to sprite height — but which curve is "right" depends on details of how each sprite gets sized, which varies across renderers.

### The underlying pattern

Each rhythm-game element — gem, bar, overlay, gridline — is positioned and sized independently. Each reaches into the perspective math (`createPerspectiveGlyphRect`, lane coord lookups, `colScale`, `foreshorten`) and assembles its own rect. There is no notion of "these sprites belong together." A tiny mismatch in how any two of them sample the perspective curves produces drift.

Future composite elements (multi-sprite notes, decorated lanes, BRE markers) would face the same problem. Each would re-solve the "how do I stay proportional to my neighbors across depth" puzzle.

### The right abstraction

**A frame is a set of sprites that share one perspective scale.** Pixel offsets and sizes within the frame are fixed at a reference scale (strike). At render time, one scale factor — derived from the frame's depth — multiplies every pixel dimension uniformly. Members cannot drift apart because they aren't computed independently.

---

## Design

### Data types

```cpp
// Utils/Frame.h
namespace PositionConstants
{
    // One sprite within a frame, sized and positioned in pixel space at
    // strike reference. All coordinates are relative to the frame anchor.
    struct FrameSprite
    {
        juce::Image*   image;
        juce::Point<float>  offset;       // pixels from anchor at strike reference
        juce::Size<float>   size;         // pixels at strike reference
        int             drawOrder;         // from DrawOrder enum
        float           opacity = 1.0f;
    };

    // A rhythm-game object as a group of sprites sharing one perspective scale.
    struct Frame
    {
        float position;                    // music position (depth along highway)
        int   column;                      // lane column index (-1 = full-width / bar)
        bool  isBar = false;               // bar vs gem: selects perspective path
        std::vector<FrameSprite> sprites;
    };
}
```

No virtual dispatch, no inheritance, no heap allocations per frame. Frames are stack-built per gem/bar/gridline at render time, consumed immediately.

### Rendering

A new helper in `PositionMath` (or a new `FrameRenderer` utility — decide in plan phase):

```cpp
// Render a frame: project its anchor, compute one scale, apply uniformly to all sprites.
void drawFrame(const Frame& frame, RenderContext ctx, DrawCallMap& out);
```

Render steps:

1. **Project anchor** — existing `getFretboardEdge` (bars) or `getColumnPosition` (gems) gives the anchor's screen position and reference width at the frame's depth.
2. **Compute scale factor** — `frameScale = currentAnchorWidth / strikeAnchorWidth × foreshorten(depth)`. A single number. This is exactly the height ratio `glyphRect.getHeight() / strikeGlyphHeight` that makes gap-to-sprite ratio constant.
3. **For each sprite**:
   - `screenCenter = anchor + sprite.offset × frameScale`
   - `screenSize = sprite.size × frameScale`
   - Enqueue into `DrawCallMap` at `sprite.drawOrder`.

The drift bug is fixed in step 3: every sprite's offset and size scales by the same `frameScale`. The relationship between a cymbal and its kick (both sharing a frame, or both sharing their own frames at the same depth with matched anchor-projection math) is architecturally locked.

### Where existing concepts live in the frame model

| Current concept | Frame-model home |
|---|---|
| `gemZ`, `cymZ`, `barZ`, `gridZ` (Z offsets) | `FrameSprite.offset.y` for each sprite |
| `OverlayAdjust.{offsetX, offsetY, widthScale, heightScale}` | Additional `FrameSprite.offset` / `FrameSprite.size` for the overlay sprite in the same frame |
| `hScale` (per-gem-type scale e.g. ghost=1.0, cymbal=0.95) | Baked into `FrameSprite.size` when frame is built |
| `colScale` (per-column sNear/sFar depth ramp) | Multiplier on `frameScale` at projection time (one line) |
| `arcOffset` (curvature bow) | Added to anchor's screen position before `frameScale` is applied |
| `foreshorten` (note-height reduction) | Included in the `frameScale` formula (as it already does for sprite height) |
| Bemani mode | `frameScale = 1.0f`, flat anchor projection |
| Star power tint/opacity | `FrameSprite.opacity` |

Every tunable maps cleanly. Debug panel code doesn't need to change — the tunables still live in `InstrumentOffsets` and `ColumnAdjust`, they just feed into frame construction instead of being sprinkled through render logic.

### What the math guarantees

Two sprites **in the same frame** are drawn at:
- `spriteA_center = anchor + offsetA × frameScale`
- `spriteB_center = anchor + offsetB × frameScale`
- `spriteA_size = sizeA × frameScale`, `spriteB_size = sizeB × frameScale`

Ratios between them (e.g., `|offsetA − offsetB| / sizeA`) are **constant in `frameScale`** — they don't depend on depth. Strike-time layout is preserved at every depth.

Two frames **at the same music position** (e.g., a gem frame and a kick-bar frame) have:
- `frameScaleA = bufferA_width / strikeA_width × foreshorten(d)`
- `frameScaleB = bufferB_width / strikeB_width × foreshorten(d)`

Since all perspective lookups (`getColumnPosition`, `getFretboardEdge`) share the same `widthProgress` and `foreshorten` curves, `frameScaleA = frameScaleB × constant` — the ratio is depth-invariant. Gem-to-bar gap stays proportional.

---

## Migration

Incremental. Each phase is one commit, each leaves the codebase working.

### Phase 1 — Scaffold

- Add `FrameSprite` and `Frame` structs in `Chartchotic/Source/Visual/Utils/Frame.h`.
- Add `drawFrame()` helper in `Utils/FrameRenderer.{h,cpp}` (or as a free function in `PositionMath`). Uses existing perspective machinery.
- No call sites yet. No behavior change.
- Unit tests for frame scaling invariants: `(offset, size)` ratios preserved across depth.

### Phase 2 — Migrate gem path in `NoteRenderer::drawGem`

- Rewrite the gem + overlay branch (the main one, `!barNote`) to build a `Frame` and hand it to `drawFrame()`.
- Delete the current per-sprite `scaleRect` calls in the gem path.
- **Drift fix lands here** — because `drawFrame` uses the unified scale factor.
- Re-tune `gemZ`, `cymZ`, overlay offsets once (values mostly carry over; expect minor nudges from the math shift).

### Phase 3 — Migrate bar-note path

- Same pattern for `barNote == true` branch of `drawGem`.
- Kick/open bars become single-sprite frames.
- `barZ`, `gridZ` surface as sprite offsets.

### Phase 4 — Migrate `GridlineRenderer`

- `drawGridline` builds a one-sprite `Frame` with column = -1 (full-width).
- Delete the ad-hoc width-ratio scaling there.

### Phase 5 — Migrate `AnimationRenderer` (optional)

- Hit animations become strikeline-anchored frames.
- Since hit animations are strikeline-only, `frameScale = 1` — no behavior change, just code unification.

**Skip**: `SustainRenderer` (spans depths, doesn't fit the single-anchor model — leave as-is), `TextEventRenderer` (separate rendering concern, not drift-affected).

### Out of scope for this refactor

- True-3D pipeline rewrite (`BACKLOG.md` parking-lot item).
- Aspect-ratio drift correction at depth (orthogonal — the frame model preserves whatever aspect the asset has, it doesn't solve the W/H perspective mismatch).
- Sustain Z-offset (separate backlog item).
- Changes to `createPerspectiveGlyphRect` or the underlying perspective math.
- Pipeline/asset/DrawCallMap restructuring.

---

## Testing

Frame is data. That enables testing that wasn't practical before.

**Unit tests (Catch2, in `tests/unit/test_frame.cpp`):**
- Frame at strike (depth=0): sprite screen positions equal `anchor + offset`; sizes equal `size`.
- Frame at far (depth=1): positions and sizes all scale by the same factor.
- Two sprites in same frame: ratio `|offsetA − offsetB| / sizeA` is depth-invariant (the drift-regression test).
- Two frames at same position, different columns: scale factors match (gem-to-bar consistency).
- Bemani mode: `frameScale = 1` at all depths.
- Curvature non-zero: anchor shifts vertically, offsets and sizes unchanged relative to anchor.

**Manual/visual (still required):**
- Build + drum chart, eyeball gem-on-kick at all depths.
- Overlays (accent caps, ghost rings) stay glued to parent gems.
- Gridlines with non-zero `gridZ` lift uniformly across depth.
- Curvature + frame composition.
- Bemani mode unaffected.

## Risk & Rollback

**Risk level:** Moderate. Touches rendering code paths used in every frame. Phased migration + per-phase commit makes rollback easy. Unit tests catch math regressions; visual eyeball catches pixel regressions.

**Per-phase rollback:** `git revert <phase-sha>` restores the prior renderer verbatim, since each phase is self-contained.

**Compatibility check per phase:**
- After Phase 1: no visible change (scaffolding only).
- After Phase 2: drum/guitar gems render identically or with expected drift fix; bars/gridlines unchanged.
- After Phase 3: bars render identically or with expected drift fix.
- After Phase 4: gridlines render identically.
- After Phase 5: hit animations render identically.

If any phase breaks a non-target element, revert just that phase and diagnose.

---

## Files

**Create:**
- `Chartchotic/Source/Visual/Utils/Frame.h` — data types.
- `Chartchotic/Source/Visual/Utils/FrameRenderer.{h,cpp}` — `drawFrame()` implementation.
- `tests/unit/test_frame.cpp` — unit tests.

**Modify:**
- `Chartchotic/Source/Visual/Renderers/NoteRenderer.{h,cpp}` — gem + bar paths build frames.
- `Chartchotic/Source/Visual/Renderers/GridlineRenderer.{h,cpp}` — build frames.
- `Chartchotic/Source/Visual/Renderers/AnimationRenderer.{h,cpp}` — optional, Phase 5.
- `tests/unit/CMakeLists.txt` — wire in `test_frame.cpp`.

**Unchanged:**
- `Chartchotic/Source/Visual/Utils/PositionMath.{h,cpp}` — still the source of perspective math.
- `Chartchotic/Source/Visual/Utils/PositionConstants.h` — tunables live here as today.
- `Chartchotic/Source/Visual/Renderers/SustainRenderer.cpp`, `TextEventRenderer.cpp`.
- `createPerspectiveGlyphRect`, `getFretboardEdge`, `getColumnPosition` — used as-is.

---

## Open questions

None blocking. Will address during plan phase:
- Exact placement of `drawFrame` (PositionMath static, FrameRenderer class, or free function in Frame.h).
- Whether to introduce a `RenderContext` struct bundling `(config, perspParams, isDrums, posEnd)` or keep passing them individually.
- Whether to bundle gem+overlay into a single multi-sprite frame, or keep overlay as a sibling frame at the same anchor (both are valid; both give the drift fix).

These are implementation-detail decisions, not design decisions.
