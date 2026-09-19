#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <JuceHeader.h>
#include "Art/TrackRenderer.h"
#include "Geometry/PositionMath.h"
#include "Utils/LaneColours.h"
#include "Utils/PartPalettes.h"

class TrackImageCache
{
public:
    TrackImageCache() : alive(std::make_shared<std::atomic<bool>>(true)) {}
    ~TrackImageCache() { shutdown(); }

    /** One slot geometry to bake for. A 2x2 layout holding an elite highway has two of these,
        because elite's width weight changes its row's width-per-unit and so the other slot in
        that row is a different size from the slots in the row below. */
    struct BakeSize
    {
        int width = 0, height = 0;
        int overflowGuitar = 0, overflowDrums = 0;

        bool operator==(const BakeSize& o) const
        {
            return width == o.width && height == o.height
                && overflowGuitar == o.overflowGuitar && overflowDrums == o.overflowDrums;
        }
    };

    struct BakedTrack {
        juce::Image fadedTrack;
        juce::Image layers[TrackRenderer::NUM_LAYERS];
        int overflow = 0;
        bool valid = false;
        // The colours this was baked with. There are two bakes for a whole project, one per
        // board shape, and the strikeline inside them is painted in a part's palette: without
        // this, every 5-fret part wore guitar's pads and every drum part wore drums'.
        LaneColours::Palette palette = LaneColours::defaultPalette();
    };

    /** The bake for this part at this slot size, or null when there is none to use: a size
        nobody baked, or one made in someone else's colours. Both cases fall through to the
        caller's own live render, which is correct, just slower.

        Size is part of the key because a bake blitted into a destination it does not match is
        resampled, and a 2x downscale of a full-viewport layer measured 4050us against 133us
        for the same layer baked 1:1 — 30x, and about 42% of a stress frame across the five
        track composites. */
    BakedTrack* getFor(Part part, int width, int height)
    {
        auto* entry = findEntry(width, height);
        if (entry == nullptr) return nullptr;

        auto& baked = isDrumLike(part) ? entry->drums : entry->guitar;
        if (!baked.valid) return nullptr;
        return baked.palette == PartPalettes::global().forPart(part) ? &baked : nullptr;
    }

    /** Request an async bake of every listed slot size on a background thread.
        Drops the request if a bake is already in flight.
        onComplete is called on the message thread after staging is ready. */
    void requestBake(TrackRenderer& renderer, std::vector<BakeSize> sizes,
                     float farFadeEnd, float farFadeLen, float farFadeCurve, float posEnd,
                     const PositionConstants::NormalizedCoordinates* guitarLaneCoords, int guitarLaneCount,
                     const PositionConstants::NormalizedCoordinates* drumLaneCoords, int drumLaneCount,
                     float texScale, float texOpacity,
                     std::function<void()> onComplete)
    {
        if (sizes.empty()) return;

        // Don't overlap bakes
        bool expected = false;
        if (!baking.compare_exchange_strong(expected, true))
            return;

        // Snapshot generation at request time
        int requestGen = gen.load(std::memory_order_relaxed);

        // Copy lane coords so background thread doesn't alias GUI-thread arrays
        std::array<PositionConstants::NormalizedCoordinates, PositionConstants::GUITAR_LANE_COUNT> gLanes;
        std::array<PositionConstants::NormalizedCoordinates, PositionConstants::DRUM_LANE_COUNT> dLanes;
        std::copy_n(guitarLaneCoords, guitarLaneCount, gLanes.begin());
        std::copy_n(drumLaneCoords, drumLaneCount, dLanes.begin());

        // Copy texture settings to scratch renderer before spawning thread
        renderer.textureScale = texScale;
        renderer.textureOpacity = texOpacity;

        // Join any previous thread
        if (bakeThread.joinable())
            bakeThread.join();

        auto aliveFlag = alive;  // shared_ptr copy for the lambda chain

        bakeThread = std::thread([this, &renderer, sizes = std::move(sizes),
                                  farFadeEnd, farFadeLen, farFadeCurve, posEnd,
                                  gLanes, dLanes, guitarLaneCount, drumLaneCount,
                                  requestGen, aliveFlag,
                                  onComplete = std::move(onComplete)]() mutable {
            std::vector<Entry> staged;
            staged.reserve(sizes.size());

            for (const auto& size : sizes)
            {
                Entry e;
                e.size = size;
                bakeInto(renderer, e.guitar, e.drums, size,
                         farFadeEnd, farFadeLen, farFadeCurve, posEnd,
                         gLanes.data(), guitarLaneCount, dLanes.data(), drumLaneCount);
                staged.push_back(std::move(e));
            }

            juce::MessageManager::callAsync([this, requestGen, onComplete, aliveFlag,
                                             st = std::move(staged)]() mutable {
                // Owner destroyed — discard silently
                if (!aliveFlag->load(std::memory_order_relaxed))
                    return;

                baking.store(false, std::memory_order_relaxed);

                // Discard if a newer invalidation happened since we started
                if (requestGen != gen.load(std::memory_order_relaxed))
                {
                    if (onComplete)
                        onComplete();
                    return;
                }

                // Commit staged → active
                entries = std::move(st);
                dirty = false;

                if (onComplete)
                    onComplete();
            });
        });
    }

    void invalidate()
    {
        dirty = true;
        gen.fetch_add(1, std::memory_order_relaxed);
    }

    bool isDirty() const { return dirty; }
    bool isValid() const { return !dirty && !entries.empty(); }

    /** True when the live set of slot sizes is exactly what is baked. A layout change that
        resizes any slot has to rebake, or that slot goes back to blitting a mismatched
        source, which is the cost this cache exists to avoid. */
    bool covers(const std::vector<BakeSize>& sizes) const
    {
        if (sizes.size() != entries.size()) return false;
        for (const auto& s : sizes)
        {
            bool found = false;
            for (const auto& e : entries)
                if (e.size == s) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }

    void shutdown()
    {
        alive->store(false, std::memory_order_relaxed);
        if (bakeThread.joinable())
            bakeThread.join();
    }

private:
    struct Entry
    {
        BakeSize size;
        BakedTrack guitar, drums;
    };

    Entry* findEntry(int width, int height)
    {
        for (auto& e : entries)
            if (e.size.width == width && e.size.height == height)
                return &e;
        return nullptr;
    }

    /** Bake both guitar and drums at one slot size into the provided output structs. */
    static void bakeInto(TrackRenderer& renderer, BakedTrack& outGuitar, BakedTrack& outDrums,
                         const BakeSize& size,
                         float farFadeEnd, float farFadeLen, float farFadeCurve, float posEnd,
                         const PositionConstants::NormalizedCoordinates* guitarLaneCoords, int guitarLaneCount,
                         const PositionConstants::NormalizedCoordinates* drumLaneCoords, int drumLaneCount)
    {
        auto bakeOne = [&](bool isDrums, BakedTrack& out) {
            // One bake per board shape, painted in that shape's canonical part. Which colours
            // those were is recorded below, so a part that disagrees can tell.
            renderer.activePart = isDrums ? Part::DRUMS : Part::GUITAR;
            auto* coords = isDrums ? drumLaneCoords : guitarLaneCoords;
            int count = isDrums ? drumLaneCount : guitarLaneCount;
            int ov = isDrums ? size.overflowDrums : size.overflowGuitar;
            renderer.setLaneCoords(coords, count);
            renderer.invalidate();
            renderer.rebuild(size.width, size.height, ov, farFadeEnd, farFadeLen, farFadeCurve, posEnd);

            out.fadedTrack = renderer.getFadedTrackImage().createCopy();
            for (int i = 0; i < TrackRenderer::NUM_LAYERS; i++)
                out.layers[i] = renderer.getLayerImage((TrackRenderer::Layer)i).createCopy();
            out.overflow = ov;
            out.palette = PartPalettes::global().forPart(renderer.activePart);
            out.valid = true;
        };

        bakeOne(false, outGuitar);
        bakeOne(true, outDrums);
    }

    std::vector<Entry> entries;
    bool dirty = true;
    std::atomic<int> gen{0};
    std::atomic<bool> baking{false};
    std::shared_ptr<std::atomic<bool>> alive;
    std::thread bakeThread;
};
