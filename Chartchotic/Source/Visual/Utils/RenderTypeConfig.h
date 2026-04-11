/*
    ==============================================================================

        RenderTypeConfig.h
        Author: Noah Baxter

        Per-render-style data bundle. Replaces the paired guitar / drum lookup
        tables and isDrums?X:Y branches in renderers with a single const-pointer
        indirection: `config->X`.

        Phase 1: struct definition + getRenderTypeConfig() factory for the two
        currently-implemented styles. Zero renderer changes in this phase.

    ==============================================================================
*/

#pragma once

#include "PositionConstants.h"
#include "../../UI/ControlConstants.h"

namespace PositionConstants
{
    // POD bundle of per-render-style data. Instances are static-lifetime
    // singletons returned by getRenderTypeConfig().
    //
    // Everything here is "data identity" only — which table to read, which
    // accessor to call. Genuine behavior branches (cymbal-specific gem logic,
    // HOPO rendering, etc.) stay in renderer code and keep using isDrumLike()
    // / isGuitarLike(); those come out in Phase 4.
    struct RenderTypeConfig
    {
        size_t laneCount;

        const NormalizedCoordinates* fretboardCoords;    // single value
        const NormalizedCoordinates* bezierLaneCoords;   // base of laneCount-sized array
        const InstrumentOffsets*     instOffsets;
        const ColumnAdjust*          colAdjust;          // base of laneCount-sized array
        const CoordinateOffset*      animOffsets;        // base of laneCount-sized array

        // Perspective params. Returns by value so it works uniformly in
        // DEBUG (reads PositionMath::debugPerspParams* for live tuning) and
        // RELEASE (reads the constexpr defaults). Replaces the existing
        // #ifdef DEBUG / #else pair at each callsite.
        PerspectiveParams (*getPerspectiveParams)();

        // Bemani accessors — each reads the corresponding field on the
        // global live-tunable bemaniConfig. Present as function pointers
        // rather than pre-resolved floats so live debug tuning keeps working.
        float (*bemaniGemNudge)();
        float (*bemaniLaneEndPx)();
        float (*bemaniBarLaneEndPx)();
        float (*bemaniHwyScale)();
        float (*bemaniHitBarNudge)();
    };

    // Returns nullptr for unsupported RenderTypes. Phase 1 only supplies
    // configs for FIVE_FRET and FOUR_LANE_DRUMS — the only highway-renderable
    // render types today. Callers are already gated upstream by
    // isHighwayRenderable(), so nullptr should never reach a renderer in
    // practice. If it does, you'll crash on deref — that's intentional,
    // to flag the gap loudly rather than silently render a wrong style.
    const RenderTypeConfig* getRenderTypeConfig(RenderType type);
}
