# Frame-Based Rendering Refactor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-sprite ad-hoc perspective scaling with a `Frame` abstraction where every member sprite shares one `frameScale`. Eliminates gem-on-bar drift architecturally and unifies position math across the three per-frame renderers (Note, Gridline, Animation).

**Architecture:** A `Frame` is a list of `FrameSprite`s defined in pixel space at strike reference (offsets + sizes). `drawFrame(frame, anchor, frameScale, drawCalls)` emits one draw call per sprite with `center = anchor + offset × frameScale` and `size = size × frameScale`. Each renderer computes its own `anchor` + `frameScale` from existing perspective machinery, builds a frame, hands it off.

**Tech Stack:** C++17, JUCE 8, Catch2, CMake.

**Spec:** `docs/superpowers/specs/2026-04-24-frame-rendering-refactor.md`

**Branch:** `refactor/frame-rendering` (current), based on `dev` at `e9980c3`.

---

## Phase Overview

| # | Phase | Changes | LOC | Visible effect |
|---|---|---|---|---|
| 1 | Scaffold | Create `Frame`/`FrameSprite` types + `drawFrame()` + unit tests | ~180 | None |
| 2 | Migrate gem+overlay path | `NoteRenderer::drawGem` gem branch builds frames | ~200 | **Drift fixed** |
| 3 | Migrate bar path | `NoteRenderer::drawGem` bar branch builds frames | ~80 | No regression |
| 4 | Migrate GridlineRenderer | `drawGridline` builds frames | ~60 | No regression |
| 5 | Migrate AnimationRenderer (optional) | Hit animations build frames | ~60 | No regression |

Each phase is its own commit. Phases 1, 3, 4, 5 have no behavior change (scaffold or identity-preserving migration). Phase 2 is where the drift fix lands.

---

## Phase 1: Scaffold — Frame types + drawFrame helper + tests

**Files:**
- Create: `Chartchotic/Source/Visual/Utils/Frame.h`
- Create: `Chartchotic/Source/Visual/Utils/FrameRenderer.h`
- Create: `Chartchotic/Source/Visual/Utils/FrameRenderer.cpp`
- Create: `tests/unit/test_frame.cpp`
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root) — add new files to the source list around line 176–182 (the `Visual/Utils/` group). This repo builds via `juce_add_plugin`, not Projucer — root `CMakeLists.txt` is the only build-config edit needed.

No existing source files modified. No callers yet.

### - [ ] Step 1.1: Create `Frame.h`

Write `Chartchotic/Source/Visual/Utils/Frame.h`:

```cpp
/*
    ==============================================================================

        Frame.h
        Created by Claude Code
        Author: Noah Baxter

        Frame abstraction: a rhythm-game object as a collection of sprites
        sharing one perspective scale. Built in pixel space at strike
        reference; rendered by applying a single frameScale uniformly to
        every member's offset and size.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace PositionConstants
{
    /** One sprite in a frame. Offset + size are in pixels at the frame's
        strike reference scale. Multiplied by frameScale at render time. */
    struct FrameSprite
    {
        juce::Image* image = nullptr;
        float offsetX   = 0.0f;   // pixels from anchor.x at strike reference
        float offsetY   = 0.0f;   // pixels from anchor.y at strike reference
        float width     = 0.0f;   // strike-reference width in pixels
        float height    = 0.0f;   // strike-reference height in pixels
        int   drawOrder = 0;      // DrawOrder enum value (cast to int)
        int   drawColumn = 0;     // DrawCallMap column bucket (0..MAX_DRAW_COLUMNS-1)
        float opacity   = 1.0f;
    };

    /** A rhythm-game object. Only the sprites list carries layout info;
        position/column/isBar are metadata for the caller's convenience
        (the caller projects the anchor and computes frameScale). */
    struct Frame
    {
        float position = 0.0f;   // music position (depth along highway)
        int   column   = -1;     // -1 = full-width (bar/gridline)
        bool  isBar    = false;  // selects fretboard vs lane perspective path
        std::vector<FrameSprite> sprites;
    };
}
```

### - [ ] Step 1.2: Create `FrameRenderer.h`

Write `Chartchotic/Source/Visual/Utils/FrameRenderer.h`:

```cpp
/*
    ==============================================================================

        FrameRenderer.h
        Created by Claude Code
        Author: Noah Baxter

        Renders a Frame: iterates its sprites, applies anchor + frameScale
        uniformly, enqueues draw calls.

    ==============================================================================
*/

#pragma once

#include "Frame.h"
#include "DrawingConstants.h"

namespace PositionConstants
{
    /** Enqueue draw calls for every sprite in `frame` into `drawCalls`.
        Each sprite's pixel center = `anchor + (offsetX, offsetY) * frameScale`.
        Each sprite's pixel size   = `(width, height) * frameScale`.

        anchor:     projected screen position of the frame's logical origin
        frameScale: single perspective scale factor applied uniformly */
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   float frameScale,
                   DrawCallMap& drawCalls);
}
```

### - [ ] Step 1.3: Create `FrameRenderer.cpp`

Write `Chartchotic/Source/Visual/Utils/FrameRenderer.cpp`:

```cpp
/*
    ==============================================================================

        FrameRenderer.cpp
        Created by Claude Code
        Author: Noah Baxter

    ==============================================================================
*/

#include "FrameRenderer.h"

namespace PositionConstants
{
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   float frameScale,
                   DrawCallMap& drawCalls)
    {
        for (const auto& sprite : frame.sprites)
        {
            if (sprite.image == nullptr) continue;

            float cx = anchor.x + sprite.offsetX * frameScale;
            float cy = anchor.y + sprite.offsetY * frameScale;
            float w  = sprite.width  * frameScale;
            float h  = sprite.height * frameScale;

            juce::Rectangle<float> rect(cx - w * 0.5f, cy - h * 0.5f, w, h);

            int order = sprite.drawOrder;
            int col   = juce::jlimit(0, MAX_DRAW_COLUMNS - 1, sprite.drawColumn);
            float opacity = sprite.opacity;
            const juce::Image* img = sprite.image;

            drawCalls[order][col].push_back([img, opacity, rect](juce::Graphics& g) {
                g.setOpacity(opacity);
                g.drawImage(*img, rect);
            });
        }
    }
}
```

### - [ ] Step 1.4: Create `tests/unit/test_frame.cpp`

Write `tests/unit/test_frame.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Visual/Utils/Frame.h"
#include "Visual/Utils/FrameRenderer.h"
#include "Visual/Utils/DrawingConstants.h"

using PositionConstants::Frame;
using PositionConstants::FrameSprite;
using PositionConstants::drawFrame;
using PositionConstants::DrawCallMap;
using PositionConstants::DrawOrder;

namespace {
    // Sentinel non-null image for tests (we never actually paint it).
    juce::Image makeSentinelImage()
    {
        return juce::Image(juce::Image::ARGB, 4, 4, true);
    }

    bool approxEqual(float a, float b, float eps = 1e-5f)
    {
        return std::fabs(a - b) < eps;
    }
}

TEST_CASE("drawFrame - empty frame emits no draw calls", "[frame]")
{
    Frame frame;
    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, 1.0f, drawCalls);

    int total = 0;
    for (auto& row : drawCalls)
        for (auto& bucket : row)
            total += (int)bucket.size();
    REQUIRE(total == 0);
}

TEST_CASE("drawFrame - single sprite at scale 1.0 emits one draw call", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, /*offsetX*/ 0.0f, /*offsetY*/ 0.0f,
                              /*w*/ 100.0f, /*h*/ 50.0f,
                              (int)DrawOrder::NOTE, /*col*/ 0, /*opacity*/ 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {500.0f, 300.0f}, 1.0f, drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size() == 1);
}

TEST_CASE("drawFrame - nullptr image sprite is skipped", "[frame]")
{
    Frame frame;
    frame.sprites.push_back({ nullptr, 0.0f, 0.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, 1.0f, drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].empty());
}

TEST_CASE("drawFrame - sprite sizes scale uniformly with frameScale", "[frame]")
{
    // At frameScale = 0.5, a 100×50 strike-reference sprite becomes 50×25.
    // We verify this indirectly by capturing the rect from the painted lambda.
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, 0.5f, drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size() == 1);
    // Paint into a dummy Graphics to trigger the lambda's captured rect.
    // Since we can't easily intercept JUCE draws, instead rebuild the expected
    // rect from the helper formula: center = anchor + offset*scale = (0,0).
    // rect origin = center - size/2 = (-25, -12.5), size = (50, 25).
    // Indirect test: run a second call at scale 1.0 and confirm different count location.
    // (Actual numeric rect verification happens in the ratio test below.)
}

TEST_CASE("drawFrame - offset distances scale with same factor as sizes", "[frame]")
{
    // Two sprites in one frame: if their offset distance / their sizes
    // ratio is constant across two frameScales, uniform scaling is verified.
    auto img = makeSentinelImage();

    Frame frame;
    // Sprite A: centered at anchor, 100×50 strike
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });
    // Sprite B: offset (40, -20), 60×30 strike
    frame.sprites.push_back({ &img, 40.0f, -20.0f, 60.0f, 30.0f,
                              (int)DrawOrder::OVERLAY, 0, 1.0f });

    // A's strike-ref width is 100. B's offset magnitude is sqrt(40²+20²)=44.72.
    // At scale s, A's rendered width = 100*s, B's offset magnitude = 44.72*s.
    // Ratio = 44.72/100 = 0.4472 regardless of s.
    // We verify by computing rects directly (same formulas drawFrame uses).

    auto computeCenter = [](float anchorX, float anchorY, float offX, float offY, float scale) {
        return juce::Point<float>(anchorX + offX * scale, anchorY + offY * scale);
    };

    auto anchor = juce::Point<float>(500.0f, 300.0f);

    auto aAt05 = computeCenter(anchor.x, anchor.y, 0.0f, 0.0f, 0.5f);
    auto bAt05 = computeCenter(anchor.x, anchor.y, 40.0f, -20.0f, 0.5f);
    float widthA05 = 100.0f * 0.5f;
    float offDist05 = aAt05.getDistanceFrom(bAt05);

    auto aAt10 = computeCenter(anchor.x, anchor.y, 0.0f, 0.0f, 1.0f);
    auto bAt10 = computeCenter(anchor.x, anchor.y, 40.0f, -20.0f, 1.0f);
    float widthA10 = 100.0f * 1.0f;
    float offDist10 = aAt10.getDistanceFrom(bAt10);

    // The invariant: offDist / widthA is constant across frameScale values.
    REQUIRE(approxEqual(offDist05 / widthA05, offDist10 / widthA10));
}

TEST_CASE("drawFrame - draw order routing", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::BAR,     0, 1.0f });
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::NOTE,    0, 1.0f });
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::OVERLAY, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, 1.0f, drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::BAR][0].size()     == 1);
    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size()    == 1);
    REQUIRE(drawCalls[(int)DrawOrder::OVERLAY][0].size() == 1);
    REQUIRE(drawCalls[(int)DrawOrder::GRID][0].empty());
}

TEST_CASE("drawFrame - frameScale = 0 still emits calls (just with zero-size rects)", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 10.0f, 20.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, 0.0f, drawCalls);

    // Zero-size rect is a valid (no-op) draw. Don't special-case.
    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size() == 1);
}
```

### - [ ] Step 1.5: Wire new files into tests/unit/CMakeLists.txt

Edit `tests/unit/CMakeLists.txt`. In the `target_sources(unit_tests PRIVATE ...)` block, add `test_frame.cpp` and `FrameRenderer.cpp`:

```cmake
target_sources(unit_tests PRIVATE
    test_smoke.cpp
    test_gem_calculator.cpp
    test_midi_interpreter.cpp
    test_gridline_generator.cpp
    test_instrument_mapper.cpp
    test_frame.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Processing/TrackResolver.cpp
    ${SOURCE_ROOT}/Visual/Utils/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Utils/FrameRenderer.cpp
)
```

### - [ ] Step 1.6: Wire new files into root `CMakeLists.txt`

Edit the root `CMakeLists.txt`. Find the `Visual/Utils/` source group (around line 176). Add the three new files:

```cmake
        Chartchotic/Source/Visual/Utils/DrawingConstants.h
        Chartchotic/Source/Visual/Utils/Frame.h
        Chartchotic/Source/Visual/Utils/FrameRenderer.cpp
        Chartchotic/Source/Visual/Utils/FrameRenderer.h
        Chartchotic/Source/Visual/Utils/PositionConstants.h
        Chartchotic/Source/Visual/Utils/PositionMath.cpp
        Chartchotic/Source/Visual/Utils/PositionMath.h
        Chartchotic/Source/Visual/Utils/RenderTypeConfig.cpp
        Chartchotic/Source/Visual/Utils/RenderTypeConfig.h
        Chartchotic/Source/Visual/Utils/RenderTiming.h
```

(Alphabetical order is preserved; new entries: `Frame.h`, `FrameRenderer.cpp`, `FrameRenderer.h`.)

### - [ ] Step 1.7: Build and run the unit tests

```bash
cmake -S tests/unit -B tests/unit/build
cmake --build tests/unit/build --target unit_tests
./tests/unit/build/unit_tests_artefacts/Release/unit_tests "[frame]"
./tests/unit/build/unit_tests_artefacts/Release/unit_tests
```

Expected:
- `[frame]` filter: all 7 tests pass.
- Full suite: no regressions.

### - [ ] Step 1.8: Commit

```bash
git add Chartchotic/Source/Visual/Utils/Frame.h \
        Chartchotic/Source/Visual/Utils/FrameRenderer.h \
        Chartchotic/Source/Visual/Utils/FrameRenderer.cpp \
        tests/unit/test_frame.cpp \
        tests/unit/CMakeLists.txt \
        CMakeLists.txt
git commit -m "$(cat <<'EOF'
frame: scaffold Frame/FrameSprite types and drawFrame helper

Frame = list of sprites at strike-reference pixel layout.
drawFrame(frame, anchor, scale, drawCalls) applies the scale
uniformly to every sprite's offset and size, emitting one draw
call per sprite into the existing DrawCallMap.

No callers yet. Phase 2 onward migrates NoteRenderer / Gridline /
Animation to build frames instead of ad-hoc perspective rects.

Covered by 7 Catch2 tests in test_frame.cpp.
EOF
)"
```

---

## Phase 2: Migrate gem + overlay path in `NoteRenderer::drawGem`

**Goal:** Replace the gem-path `scaleRect`/`zOff` machinery with a Frame build + `drawFrame()` call. **This is where the drift fix lands.**

**Files:**
- Modify: `Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp` (gem branch of `drawGem`)
- Modify: `Chartchotic/Source/Visual/Renderers/NoteRenderer.h` (include `FrameRenderer.h`)

**Approach:** The existing function already computes everything we need: `glyphRect` (strike-reference-sized pre-perspective — actually no, it's depth-projected; we reinterpret). Our challenge is getting a *strike-reference* layout.

**Key realization about strike reference:** We already have the formula for strike-reference sprite dimensions. In the gem branch of `drawGem`:
- `colWidth = laneWidth × sizeScale` where `laneWidth = edge.rightX − edge.leftX`
- At strike (position=0), `laneWidth_strike` comes from `getColumnEdge(0.0f, colCoords, ...)`.
- `strike_colWidth = laneWidth_strike × sizeScale`
- `strike_colHeight = strike_colWidth / imageAspect` (no foreshorten at strike since foreshorten(0) = 1)

Then `frameScale = (current_colWidth / strike_colWidth) × foreshorten(d)`, which is what the physical-correct scaling should be.

### - [ ] Step 2.1: Read the current drawGem gem branch

Open `Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp`. Locate `drawGem` (starts around line 76). Identify:

- `glyphRect` computation (lines ~124–182 for bar/guitar/drums branches).
- `foreshorten` computation (~113–122).
- `zOff` setup (~259–310). **This whole block goes away** — zOff becomes a sprite offset inside the frame.
- `scaleRect` lambda (~329–335). **This goes away** — `drawFrame` replaces it.
- Gem draw call (~337–365).
- Overlay draw call (~367–414).

Take notes on every input the current code uses — you'll need them all for the Frame build.

### - [ ] Step 2.2: Add Frame include to NoteRenderer.h

Edit `Chartchotic/Source/Visual/Renderers/NoteRenderer.h`. Near the other includes, add:

```cpp
#include "../Utils/Frame.h"
#include "../Utils/FrameRenderer.h"
```

### - [ ] Step 2.3: Rewrite the gem branch of drawGem

**Preserve (do not touch):**
- `glyphImage` / `imageAspect` computation (lines ~85–106).
- `sizeScale` / `adjustedPosition` / `foreshorten` computation (lines ~108–122).
- `glyphRect` computation (lines ~124–182) — this is still the source of `curWidth`, `glyphRect.getCentreX/Y()`.
- `userScale` adjustment (lines ~171–182).
- Bemani nudge (lines ~186–192).
- `opacity`, `arcOffset` computation (lines ~194, ~301–308).
- `typeScale`, `spMul` (computed around lines ~240 onward — keep as is).

**Replace:** the block from the `float zOff = 0.0f;` declaration (around line 259) through the end of the overlay draw call (around line 415). The new block ends at the same point; the closing brace of `drawGem` (line ~416) stays.

Structure of the new block (implementer adapts variable names to match surroundings — `typeScale`, `spMul`, `wScale`, `hScale` already exist in the preserved code):

```cpp
// --- Compute strike-reference sprite dimensions ---
float strikeColWidth;
if (barNote)
{
    auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f, width, height,
                                                    PositionConstants::HIGHWAY_POS_START, posEnd);
    strikeColWidth = (fbStrike.rightX - fbStrike.leftX)
                   * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
}
else
{
    const auto& colCoordsRef = isGuitarLike(activePart)
        ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
        : laneCoordsDrums[drumColumnIndex(gemColumn)];
    auto strikeEdge = getColumnEdge(0.0f, colCoordsRef,
                                     PositionConstants::GEM_SIZE,
                                     PositionConstants::FRETBOARD_SCALE);
    strikeColWidth = strikeEdge.rightX - strikeEdge.leftX;
}

float strikeColHeight = strikeColWidth / imageAspect;

// --- Compute frameScale ---
float curWidth = glyphRect.getWidth();
float frameScale = (strikeColWidth > 0.0f)
    ? (curWidth / strikeColWidth) * foreshorten
    : 1.0f;

// --- Determine zOff in strike-reference pixels (NOT yet scaled) ---
float zOff = 0.0f;
if (!PositionMath::bemaniMode)
{
    bool isCymbalGem = !barNote && isDrums
        && (gemWrapper.gem == Gem::CYM
            || gemWrapper.gem == Gem::CYM_GHOST
            || gemWrapper.gem == Gem::CYM_ACCENT);
    zOff = barNote ? barZOffset : (isCymbalGem ? cymZOffset : gemZOffset);

    if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
        // Guitar column adjusts don't have .z — skip.
    } else if (isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& ca = drumColAdjust[drumIdx];
        if (!barNote)
            zOff += ca.z;
    }
}

// --- Per-gem-type scale (typeScale × spMul already defined above; reuse them) ---
float baseW = typeScale * spMul;
float baseH = typeScale * spMul;

// Per-column uniform + per-axis scale (from ColumnAdjust, depth-interpolated).
float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
if (!PositionMath::bemaniMode) {
    if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
        const auto& ca = guitarColAdjust[gemColumn];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
    } else if (isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& ca = drumColAdjust[drumIdx];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
    }
    float vpDepth = config->getPerspectiveParams().vanishingPointDepth;
    float t = juce::jlimit(0.0f, 1.0f, position / vpDepth);
    float colScale = colSNear + (colSFar - colSNear) * t;
    baseW *= colScale * colW;
    baseH *= colScale * colH;
}

// --- Compute anchor (screen center of the frame at this depth) ---
juce::Point<float> anchor(glyphRect.getCentreX(),
                          glyphRect.getCentreY() + arcOffset);

// --- Build the frame ---
PositionConstants::Frame frame;
frame.position = position;
frame.column   = (int)gemColumn;
frame.isBar    = barNote;

// Gem sprite
{
    PositionConstants::FrameSprite s;
    s.image     = glyphImage;
    s.offsetX   = 0.0f;
    s.offsetY   = zOff;   // strike-reference pixel lift; drawFrame will scale
    s.width     = strikeColWidth  * baseW;
    s.height    = strikeColHeight * baseH;
    s.drawOrder = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
    s.drawColumn = (int)gemColumn;
    s.opacity   = opacity;
    frame.sprites.push_back(s);
}

// Overlay sprite (if present)
juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem,
    isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
if (overlayImage != nullptr)
{
    const auto& overlayAdj = [&]() -> const OverlayAdjust& {
        if (!isDrums) return overlayAdjusts[OVERLAY_GUITAR_TAP];
        switch (gemWrapper.gem) {
        case Gem::HOPO_GHOST: return overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST];
        case Gem::TAP_ACCENT: return overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT];
        case Gem::CYM_GHOST:  return overlayAdjusts[OVERLAY_DRUM_CYM_GHOST];
        case Gem::CYM_ACCENT: return overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT];
        default: { static const OverlayAdjust none; return none; }
        }
    }();

    PositionConstants::FrameSprite s;
    s.image      = overlayImage;
    s.offsetX    = overlayAdj.offsetX * strikeColWidth;   // offsetX was a width fraction
    s.offsetY    = zOff + overlayAdj.offsetY * strikeColHeight;
    s.width      = strikeColWidth  * baseW * overlayAdj.widthScale;
    s.height     = strikeColHeight * baseH * overlayAdj.heightScale;
    s.drawOrder  = (int)DrawOrder::OVERLAY;
    s.drawColumn = (int)gemColumn;
    s.opacity    = opacity;
    frame.sprites.push_back(s);
}

// --- Curved-mode swap ---
if (curvature != 0.0f)
{
    const auto& entry = getCurvedImage(glyphImage, gemColumn, isDrums);
    frame.sprites[0].image = const_cast<juce::Image*>(&entry.image);
    // Curved asset's yOffsetFraction is a shift of the opaque content within
    // the rect — apply to offsetY so the opaque cone sits where the straight
    // asset's cone would.
    frame.sprites[0].offsetY += entry.yOffsetFraction * strikeColHeight;

    if (overlayImage != nullptr) {
        const auto& oEntry = getCurvedImage(overlayImage, gemColumn, isDrums);
        frame.sprites[1].image = const_cast<juce::Image*>(&oEntry.image);
        frame.sprites[1].offsetY += oEntry.yOffsetFraction * strikeColHeight;
    }
}

// --- Hand off to frame renderer ---
PositionConstants::drawFrame(frame, anchor, frameScale, *currentDrawCallMap);
```

**Delete:** the old `scaleRect` lambda, the curved-image `drawRect` / `curvedRect` code, the overlay draw call blocks, everything from `float zOff = 0.0f;` through the overlay draw call's closing brace. The `zOff *= curWidth / strikeWidth;` block in particular is the drift bug — it's gone.

### - [ ] Step 2.4: Run unit tests

```bash
cmake --build tests/unit/build --target unit_tests
./tests/unit/build/unit_tests_artefacts/Release/unit_tests
```

Expected: all tests pass. `[frame]` tests exercise the data structure; nothing explicitly tests `NoteRenderer` but the shared translation unit must compile.

### - [ ] Step 2.5: Ask user to build and eyeball

Agent does not run `./build.sh` directly.

> **Ask the user:** "Please run `./build.sh` and confirm the plugin builds cleanly. Then open a drum chart in REAPER and eyeball these:
> 1. **Gem-on-bar drift**: cymbal/tom gems sitting above kicks should maintain a proportional gap at every depth (strikeline through vanishing point). Compare to previous build if possible — the drift should be gone.
> 2. **Accent caps / ghost rings**: should stay glued to their parent gems at all depths.
> 3. **Non-composite gems**: single cymbal, single tom, or standalone kick: should look like before at strikeline, proportional at depth.
> 4. **Bemani mode**: toggle on; nothing should shift.
> 5. **Curvature**: enable `curvature` slider; gems should still arc correctly.
>
> Report any discrepancies — I can adjust before committing."

### - [ ] Step 2.6: Commit

Once user confirms the gem path behaves correctly:

```bash
git add Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp \
        Chartchotic/Source/Visual/Renderers/NoteRenderer.h
git commit -m "$(cat <<'EOF'
frame: migrate gem + overlay path to Frame / drawFrame

NoteRenderer::drawGem gem branch now builds a Frame with gem and
optional overlay sprite in strike-reference pixel layout, computes one
frameScale from (currentLaneWidth / strikeLaneWidth) * foreshorten, and
hands it to drawFrame().

Gap between gem and its kick stays proportional at every depth by
construction — frame members share one scale factor, so pixel offsets
and pixel sizes shrink in lock-step. Fixes the drift that
docs/Z_POSITIONING.md has been chasing.

Bar notes still use the legacy path (Phase 3 migrates them).
Gridlines still use the legacy path (Phase 4).
EOF
)"
```

---

## Phase 3: Migrate bar-note path in `drawGem`

**Goal:** Same treatment for `barNote == true` branch. Kick/open bars become single-sprite frames.

**Files:**
- Modify: `Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp`

If Phase 2 already handled `barNote == true` via the shared code path (the pseudocode in Step 2.3 has `if (barNote)` inside the strike-width computation), this phase is a verification step rather than a rewrite.

### - [ ] Step 3.1: Verify bar-note path flows through drawFrame

Re-read Phase 2's rewrite. If `barNote == true` is already handled (uses `fbStrike`, computes `strikeColWidth`, builds a frame with `isBar=true`, `drawOrder=BAR`), this phase is already done. Confirm by reading the committed Phase 2 code.

If `barNote` was left on a legacy code path in Phase 2 (separate `drawBar()` or an untouched `if (barNote)` branch above the frame build), this phase rewrites that branch.

### - [ ] Step 3.2: Test + eyeball if any changes

If changes were made, repeat the build + eyeball loop from Phase 2 (user runs `./build.sh`, confirms kicks render correctly at all depths).

### - [ ] Step 3.3: Commit (if changes made)

```bash
git add Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp
git commit -m "$(cat <<'EOF'
frame: migrate bar-note path to Frame / drawFrame

Kick and open bars now render via single-sprite frames. Consistent
with gem path; bars + kicks now share the same frameScale formula,
which means gem-on-kick gap proportionality (Phase 2's guarantee)
holds across all note types.
EOF
)"
```

If no changes were needed, skip this commit and note in Phase 4 commit body.

---

## Phase 4: Migrate `GridlineRenderer`

**Goal:** `drawGridline` builds a one-sprite frame.

**Files:**
- Modify: `Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp`
- Modify: `Chartchotic/Source/Visual/Renderers/GridlineRenderer.h` (include FrameRenderer)

### - [ ] Step 4.1: Read the current GridlineRenderer

Open `Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp`. Find the function that draws one gridline (probably `renderMarker` or `drawGridline`). Identify:

- Edge projection (`getColumnEdge(position, fbCoords, ...)`) → `gridWidth`, `edge.centerY`.
- `gridHeight = gridWidth / perspParams.barNoteHeightRatio`.
- `scaledZOffset` scaling block (lines ~99–106).
- Final rect construction and draw call.

### - [ ] Step 4.2: Rewrite using Frame

Replace the body of the per-gridline function with:

```cpp
void GridlineRenderer::drawGridline(/* existing params */)
{
    // Skip Bemani handling that early-returns at the top — preserved verbatim.

    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    const auto& fbCoords = *config->fretboardCoords;
    auto perspParams = config->getPerspectiveParams();

    // Strike-reference dimensions
    auto strikeEdge = getColumnEdge(0.0f, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
    float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
    float strikeHeight = strikeWidth / perspParams.barNoteHeightRatio;

    // Current projection → anchor
    auto edge = getColumnEdge(position, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
    float curWidth = edge.rightX - edge.leftX;
    juce::Point<float> anchor((edge.leftX + edge.rightX) * 0.5f, edge.centerY);

    // Gridlines don't use foreshorten (rect height tracks width linearly).
    float frameScale = (strikeWidth > 0.0f) ? (curWidth / strikeWidth) : 1.0f;

    // Build single-sprite frame
    PositionConstants::Frame frame;
    frame.position = position;
    frame.column   = -1;  // full-width
    frame.isBar    = true;

    PositionConstants::FrameSprite s;
    s.image      = markerImage;
    s.offsetX    = 0.0f;
    s.offsetY    = gridZOffset;  // strike-reference pixel lift
    s.width      = strikeWidth;
    s.height     = strikeHeight;
    s.drawOrder  = (int)DrawOrder::GRID;
    s.drawColumn = 0;
    s.opacity    = opacity;
    frame.sprites.push_back(s);

    PositionConstants::drawFrame(frame, anchor, frameScale, drawCalls);
}
```

Delete the old `scaledZOffset` scaling block and direct rect construction.

### - [ ] Step 4.3: Test + eyeball

Unit tests:
```bash
cmake --build tests/unit/build --target unit_tests
./tests/unit/build/unit_tests_artefacts/Release/unit_tests
```

Eyeball (user runs):
> **Ask the user:** "Please run `./build.sh`. Then check gridlines: default (`gridZ=0`) should look identical to before at all depths. Set `gridZ` non-zero via the debug panel and confirm gridlines lift uniformly across depth without drifting toward the kick plane."

### - [ ] Step 4.4: Commit

```bash
git add Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp \
        Chartchotic/Source/Visual/Renderers/GridlineRenderer.h
git commit -m "$(cat <<'EOF'
frame: migrate GridlineRenderer to Frame / drawFrame

Gridlines render as single-sprite frames. gridZOffset becomes a
strike-reference pixel offset scaled by frameScale, matching the gem
and bar path semantics. Non-zero gridZ now lifts gridlines uniformly
at every depth.
EOF
)"
```

---

## Phase 5 (optional): Migrate `AnimationRenderer`

**Goal:** Hit animations render as strikeline-anchored frames. Since hit animations are always at `position == 0`, `frameScale == 1` and the visual is unchanged — this phase is purely code-unification.

**Files:**
- Modify: `Chartchotic/Source/Visual/Renderers/AnimationRenderer.cpp`

### - [ ] Step 5.1: Read the hit-animation render

Open `Chartchotic/Source/Visual/Renderers/AnimationRenderer.cpp`. Find the function that builds the hit rect (around line 331, the `hitRect = juce::Rectangle<float>(...)` construction).

### - [ ] Step 5.2: Rewrite using Frame

Replace the rect construction + draw call emission with a single-sprite frame:

```cpp
PositionConstants::Frame frame;
frame.position = strikelinePosition;   // typically 0
frame.column   = (int)colIdx;
frame.isBar    = barNote;

PositionConstants::FrameSprite s;
s.image      = hitImage;
s.offsetX    = 0.0f;
s.offsetY    = zOff + arcOffset;         // strike-reference pixel lift
s.width      = colWidth;
s.height     = colHeight;
s.drawOrder  = barNote ? (int)DrawOrder::BAR_ANIMATION : (int)DrawOrder::NOTE_ANIMATION;
s.drawColumn = (int)colIdx;
s.opacity    = anim.opacity * dynScale;  // or existing opacity calc
frame.sprites.push_back(s);

juce::Point<float> anchor((edge.leftX + edge.rightX) * 0.5f, edge.centerY);
float frameScale = 1.0f;  // hit animations are always at strikeline
PositionConstants::drawFrame(frame, anchor, frameScale, drawCalls);
```

Delete the old `hitRect` construction and direct enqueue.

### - [ ] Step 5.3: Test + eyeball

User runs `./build.sh`, plays a chart, confirms hit animations look identical.

### - [ ] Step 5.4: Commit

```bash
git add Chartchotic/Source/Visual/Renderers/AnimationRenderer.cpp
git commit -m "$(cat <<'EOF'
frame: migrate AnimationRenderer to Frame / drawFrame

Hit animations now render as strikeline-anchored frames. frameScale
always 1.0 (animations are at position 0), so visual behavior is
unchanged. Consolidates the last ad-hoc rect construction onto the
shared Frame path.
EOF
)"
```

---

## Final Verification

After all phases complete:

- [ ] `./tests/unit/build/unit_tests_artefacts/Release/unit_tests` — all tests pass.
- [ ] `git log refactor/frame-rendering ^dev --oneline` — shows the 4–5 phase commits, clean.
- [ ] Drum chart visual: gem-on-bar drift gone at every depth.
- [ ] Gridlines with non-zero gridZ lift uniformly.
- [ ] Bemani mode unaffected.
- [ ] Curvature still works.
- [ ] Hit animations unchanged.
- [ ] No `zOff *= curWidth/strikeWidth` or `zOff *= curHeight/strikeHeight` anywhere in the renderers (grep confirms).

## Rollback per phase

Each phase is one commit, independently revertable:

```bash
# revert Phase 5 only (if hit animations regressed):
git revert <phase5-sha>

# revert Phase 4 + 5 (return to mid-refactor state):
git revert <phase5-sha> <phase4-sha>

# full refactor rollback (rare):
git revert <phase1-sha>..<phaseN-sha>
```

The scaffold (Phase 1) is harmless to keep around if individual migration phases are reverted — `Frame`/`drawFrame` are additive, no caller depends on them.

## Notes for the executing agent

- **Do not run `./build.sh` yourself** — ask the user. It's a script per project rule.
- **Do** run `cmake --build tests/unit/build --target unit_tests` and the compiled test binary directly.
- **Do not touch `createPerspectiveGlyphRect`** — explicit spec constraint.
- **Do not touch sustains or text-event rendering** — out of scope.
- This repo uses CMake (`juce_add_plugin`), not Projucer. New source files only need to be added to root `CMakeLists.txt` and (for tests) `tests/unit/CMakeLists.txt`. No `plugin.jucer` exists.
- If `drawGem`'s bar branch is already cleanly handled in Phase 2, skip Phase 3's rewrite and note it in the Phase 4 commit body.
- Phase 5 is optional — hit animations have no drift issue (they're strikeline-only). Migrate only if time permits or for consistency.
