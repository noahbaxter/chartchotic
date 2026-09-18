/*
    ==============================================================================

        GemIconArt.h
        Author: Noah Baxter

        The chart's gem art drawn as small icons, for controls that should look like
        the note they place. Kept apart from any component so render_harness can dump
        the exact icons the write bar draws.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <tuple>
#include "../Managers/AssetManager.h"
#include "../Geometry/PositionConstants.h"

class GemIconArt
{
public:
    explicit GemIconArt(AssetManager& am) : assets(am) {}

    // Gem art columns the icons borrow: green on guitar, red on drums, the first hand lane.
    static constexpr uint GUITAR_COLUMN = 1;
    static constexpr uint DRUM_COLUMN = 1;

    // Every icon draws from the unscaled master, never the live highway copy. The live ones are
    // swapped out by AssetManager::rescaleForWidth as the viewport changes, which had the write
    // bar's icons resizing themselves whenever the chart was. Generated art (runtime recolours)
    // has no master and is used as it comes.
    juce::Image* stable(juce::Image* live) const
    {
        auto* master = assets.masterFor(live);
        return master != nullptr ? const_cast<juce::Image*>(master) : live;
    }

    // Gem art sits in a canvas with room around it for the highway's perspective, so fitting
    // the whole image leaves a sliver of gem in the middle of the button. Only the visible
    // pixels are fitted. Answers the rect it drew into, empty when there was nothing to draw.
    juce::Rectangle<float> drawImage(juce::Graphics& g, juce::Rectangle<float> r, juce::Image* img) const
    {
        if (img == nullptr || !img->isValid()) return {};

        auto src = visibleBounds(*img).toFloat();
        float scale = juce::jmin(r.getWidth() / src.getWidth(), r.getHeight() / src.getHeight());
        auto dest = juce::Rectangle<float>(src.getWidth() * scale, src.getHeight() * scale)
                        .withCentre(r.getCentre());

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(*img, juce::roundToInt(dest.getX()), juce::roundToInt(dest.getY()),
                    juce::roundToInt(dest.getWidth()), juce::roundToInt(dest.getHeight()),
                    (int)src.getX(), (int)src.getY(), (int)src.getWidth(), (int)src.getHeight());
        return dest;
    }

    bool drawGuitarGem(juce::Graphics& g, juce::Rectangle<float> r, Gem gem) const
    {
        return drawGem(g, r, stable(assets.getGuitarGlyphImage(GemWrapper(gem), GUITAR_COLUMN, false)),
                       stable(assets.getOverlayImage(gem, Part::GUITAR)), PositionConstants::OVERLAY_GUITAR_TAP);
    }

    bool drawDrumGem(juce::Graphics& g, juce::Rectangle<float> r, Gem gem) const
    {
        return drawGem(g, r, stable(assets.getDrumGlyphImage(GemWrapper(gem), DRUM_COLUMN, false)),
                       stable(assets.getOverlayImage(gem, Part::DRUMS)),
                       gem == Gem::TAP_ACCENT ? PositionConstants::OVERLAY_DRUM_NOTE_ACCENT
                                              : PositionConstants::OVERLAY_DRUM_NOTE_GHOST);
    }

    // A flam is a grace note just ahead of the hit, so the icon is two gems, the grace one
    // smaller and behind.
    bool drawFlam(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        auto* gem = stable(assets.getDrumGlyphImage(GemWrapper(Gem::NOTE), DRUM_COLUMN, false));
        if (gem == nullptr || !gem->isValid()) return false;

        auto grace = r.withTrimmedRight(r.getWidth() * FLAM_OFFSET).withTrimmedBottom(r.getHeight() * FLAM_OFFSET);
        auto hit   = r.withTrimmedLeft(r.getWidth() * FLAM_OFFSET).withTrimmedTop(r.getHeight() * FLAM_OFFSET);
        {
            juce::Graphics::ScopedSaveState save(g);
            g.setOpacity(FLAM_GRACE_ALPHA);
            drawImage(g, grace, gem);
        }
        drawImage(g, hit, gem);
        return true;
    }

    // A lane: the band, with the run of gems it covers sitting on top of it. Drawn rather than
    // lettered because a lane has a shape a chart author already reads on the highway.
    // `bands` is how many lanes the marker covers: one for a roll on a single pad, two for
    // alternation between a pair. Drawn as what it is rather than labelled 1 and 2, since the
    // two markers are called different things on different instruments.
    bool drawLane(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour laneColour,
                  int bands = 1) const
    {
        // Vertical, because that is how a lane runs on the highway. Laid out sideways it read
        // as a sustain. Just the bands: gems sitting on them were too small to read as gems at
        // button size, and the coloured run is what the eye picks out on the board anyway. No
        // asset involved, so there is nothing here that can fail to draw.
        bands = juce::jmax(1, bands);

        // The count is spelled out beside the bands as well as drawn, because one thin band and
        // two thin bands are not far apart at button size.
        const float digitW   = r.getWidth() * LANE_DIGIT_WIDTH;
        const auto  bandArea = r.withWidth(r.getWidth() - digitW);

        const float bandW = bandArea.getWidth() * LANE_BAND_WIDTH
                          / (1.0f + (float)(bands - 1) * (1.0f + LANE_BAND_GAP));
        const float step  = bandW * (1.0f + LANE_BAND_GAP);
        const float runW  = step * (float)(bands - 1) + bandW;

        float left = bandArea.getCentreX() - runW * 0.5f;
        g.setColour(laneColour);
        for (int i = 0; i < bands; ++i)
        {
            auto band = juce::Rectangle<float>(bandW, r.getHeight() * LANE_BAND_HEIGHT)
                            .withCentre({ left + bandW * 0.5f + step * (float)i, r.getCentreY() });
            g.fillRoundedRectangle(band, bandW * 0.5f);
        }

        g.setFont(juce::Font(juce::FontOptions()
                                 .withHeight(r.getHeight() * LANE_DIGIT_HEIGHT)
                                 .withStyle("Bold")));
        g.drawText(juce::String(bands),
                   r.withWidth(r.getWidth()).withTrimmedLeft(r.getWidth() - digitW),
                   juce::Justification::centred, false);
        return true;
    }

private:
    static constexpr juce::uint8 VISIBLE_ALPHA = 8;
    static constexpr float FLAM_OFFSET = 0.3f;       // of the icon, how far the two gems sit apart
    static constexpr float FLAM_GRACE_ALPHA = 0.55f;
    static constexpr float LANE_BAND_WIDTH  = 0.34f;  // of the band area, the whole run of bands
    static constexpr float LANE_BAND_HEIGHT = 0.92f;  // of the icon, so it clears the edges
    // Of a band, the space between two of them. The run keeps one width whatever the count, so
    // a two-lane icon reads as a pair side by side rather than as one wider lane.
    static constexpr float LANE_BAND_GAP    = 0.5f;
    static constexpr float LANE_DIGIT_WIDTH  = 0.38f; // of the icon, reserved beside the bands
    static constexpr float LANE_DIGIT_HEIGHT = 0.5f;  // of the icon

    // Scanned once per image and kept. The size is part of the key, not just the buffer
    // address: rescaling frees the old pixel data and the allocator is free to hand the same
    // address straight back, which served stale bounds for an image that had changed size.
    // Icons draw from masters now, which do not rescale, but the key stays honest either way.
    juce::Rectangle<int> visibleBounds(const juce::Image& img) const
    {
        const auto key = std::make_tuple(img.getPixelData().get(), img.getWidth(), img.getHeight());
        auto hit = cache.find(key);
        if (hit != cache.end()) return hit->second;

        juce::Image::BitmapData bmp(img, juce::Image::BitmapData::readOnly);
        int x0 = img.getWidth(), y0 = img.getHeight(), x1 = -1, y1 = -1;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                if (bmp.getPixelColour(x, y).getAlpha() > VISIBLE_ALPHA)
                {
                    x0 = juce::jmin(x0, x); x1 = juce::jmax(x1, x);
                    y0 = juce::jmin(y0, y); y1 = juce::jmax(y1, y);
                }

        auto bounds = x1 < 0 ? img.getBounds() : juce::Rectangle<int>(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        cache[key] = bounds;
        return bounds;
    }

    // The overlay (tap mark, ghost ring, accent chevron) is registered to the gem's whole
    // canvas the way NoteRenderer draws it: the same rect as the gem, scaled about its centre
    // and nudged by OVERLAY_DEFAULTS. The gem's visible pixels decide the fit, so every gem
    // icon is the same size whatever overlay it carries, and an overlay reaching past the
    // gem is clipped to the button rather than shrinking the gem to make room.
    bool drawGem(juce::Graphics& g, juce::Rectangle<float> r, juce::Image* gem,
                 juce::Image* overlay, PositionConstants::OverlayType overlayType) const
    {
        if (gem == nullptr || !gem->isValid()) return false;

        const auto canvas = gem->getBounds().toFloat();
        const auto visible = visibleBounds(*gem).toFloat();

        float k = juce::jmin(r.getWidth() / visible.getWidth(), r.getHeight() / visible.getHeight());
        auto fitted = juce::Rectangle<float>(visible.getWidth() * k, visible.getHeight() * k)
                          .withCentre(r.getCentre());
        auto toButton = [&](juce::Rectangle<float> p) {
            return juce::Rectangle<float>(fitted.getX() + (p.getX() - visible.getX()) * k,
                                          fitted.getY() + (p.getY() - visible.getY()) * k,
                                          p.getWidth() * k, p.getHeight() * k);
        };

        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(r.getSmallestIntegerContainer());
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(*gem, toButton(canvas));

        if (overlay != nullptr && overlay->isValid())
        {
            const auto& adj = PositionConstants::OVERLAY_DEFAULTS[overlayType];
            auto overlayRect = canvas.withSizeKeepingCentre(canvas.getWidth() * adj.scaleX * adj.scale,
                                                            canvas.getHeight() * adj.scaleY * adj.scale);
            overlayRect = overlayRect.translated(overlayRect.getWidth() * adj.offsetX,
                                                 overlayRect.getHeight() * adj.offsetY);
            g.drawImage(*overlay, toButton(overlayRect), juce::RectanglePlacement::stretchToFit);
        }
        return true;
    }

    AssetManager& assets;
    mutable std::map<std::tuple<const juce::ImagePixelData*, int, int>, juce::Rectangle<int>> cache;
};
