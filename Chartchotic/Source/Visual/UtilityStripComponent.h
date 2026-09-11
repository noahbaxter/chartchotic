/*
    ==============================================================================

        UtilityStripComponent.h
        Author: Noah Baxter

        Vertical time-axis strip beside the highways, for everything that is not a
        gem on a lane: sections, [coda], and (later) star power, solo and fill.

        Zoom runs from 8 bars to the whole song. It follows the playhead and is
        where song structure is read, so bar lines are drawn in chunks rather than
        per bar: a line every bar is a wash, a line every 4 or 8 is a ruler.

        Design: docs/superpowers/specs/2026-09-10-utility-strip-design.md

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Midi/SongSections.h"
#include "../Midi/Utils/TimeConverter.h"   // defines TempoTimeSignatureMap, which the helper assumes
#include "../Midi/Utils/TempoTimeSignatureEventHelper.h"
#include "../Midi/Providers/REAPER/MidiCache.h"
#include "../Midi/Utils/InstrumentMapper.h"
#include "Managers/AssetManager.h"
#include "../UI/ControlConstants.h"

// Neutral and dark, in the same family as the neck rather than a UI blue.
namespace UtilityColours
{
    inline const juce::Colour background   = juce::Colour(0xFF121214);
    inline const juce::Colour border       = juce::Colour(0xFF303036);
    inline const juce::Colour chunkLine    = juce::Colour(0xFF3A3A42);
    inline const juce::Colour chunkLineBig = juce::Colour(0xFF56565F);
    // Structure stays neutral like the neck; the section label is the one accent.
    inline const juce::Colour sectionLine  = juce::Colour(0xFF6E8FB5);
    inline const juce::Colour sectionBand  = juce::Colour(0x142F5F96);
    inline const juce::Colour sectionText  = juce::Colour(0xFF9CC4F5);
    inline const juce::Colour uiText       = juce::Colour(0xFFE2E2E8);
    inline const juce::Colour hoverLine    = juce::Colour(0xFFBFD4EC);
    inline const juce::Colour measureText  = juce::Colour(0xFF5E5E68);
    inline const juce::Colour codaLine     = juce::Colour(0xFFC98A5A);
    inline const juce::Colour playhead     = juce::Colour(0xFFEAEAEA);
    inline const juce::Colour label        = juce::Colour(0xFF7D7D85);
    inline const juce::Colour panelFill    = juce::Colour(0xFF1B1B1F);
    inline const juce::Colour noteMark     = juce::Colour(0xFF6E6E7A);
}

class UtilityStripComponent : public juce::Component
{
public:
    UtilityStripComponent() { setInterceptsMouseClicks(true, false); }

    void setSections(const SongSections* s)          { sections = s; }
    void setTempoMap(const TempoTimeSignatureMap* m) { tempoMap = m; }
    void setPlayhead(PPQ ppq)                        { playheadPPQ = ppq; }

    // Whole-song notes for the focused part, drawn as a density map rather than a chart.
    // One per visible highway. The map splits its width between them, so a single
    // instrument spreads across the strip and four share it.
    struct NoteSource
    {
        const std::vector<MidiCache::CachedNote>* notes = nullptr;
        Part part = Part::GUITAR;
        SkillLevel skill = SkillLevel::EXPERT;
    };

    void setNoteSources(std::vector<NoteSource> s) { noteSources = std::move(s); }

    // Lane colours come from the same place the gems and strikeline pads get them, so the
    // map cannot drift from the highway it summarises.
    void setAssetManager(AssetManager* am) { assetManager = am; }

    // 0 is the whole song. 64 bars is where structure and precision both hold.
    static constexpr int ZOOM_STEPS[] = { 8, 16, 32, 64, 128, 256, 0 };
    static constexpr int DEFAULT_ZOOM_INDEX = 3;

    int barsVisible() const { return ZOOM_STEPS[zoomIndex]; }
    void zoomIn()  { setZoomIndex(zoomIndex - 1); }
    void zoomOut() { setZoomIndex(zoomIndex + 1); }
    void toggleZoom()
    {
        setZoomIndex(barsVisible() == 0 ? DEFAULT_ZOOM_INDEX
                                        : (int)std::size(ZOOM_STEPS) - 1);
    }

    // Scrolling the map seeks. The jump is a fraction of the visible span rather than a
    // fixed beat count, so one notch covers the same proportion of screen at every zoom.
    std::function<void(double deltaQN)> onSeek;

    // Clicking a bar in the map jumps there.
    std::function<void(double targetQN)> onSeekTo;

    void paint(juce::Graphics& g) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    static constexpr double PLAYHEAD_FROM_BOTTOM = 0.12;
    // Faster than the highway's fixed 2 beats, because the map is for covering ground.
    static constexpr double SEEK_FRACTION_PER_NOTCH = 0.05;
    static constexpr int FOOTER_H = 20;
    static constexpr int BUTTON_W = 18;
    static constexpr int FILTER_ROW_H = 18;
    static constexpr int FILTER_HEADER_H = 20;

    struct Filter { const char* label; bool on; };
    // Sections are the reason the strip exists, so they are on and stay first.
    Filter filters[3] = { { "sections", true }, { "notes", false }, { "measures", true } };
    bool filtersExpanded = false;

    bool filterOn(int i) const { return filters[i].on; }

    int filterAreaHeight() const
    {
        return FILTER_HEADER_H + (filtersExpanded ? (int)std::size(filters) * FILTER_ROW_H : 0);
    }

    struct View { double startQN, endQN; };
    View visibleRange() const;

    void setZoomIndex(int i)
    {
        int clamped = juce::jlimit(0, (int)std::size(ZOOM_STEPS) - 1, i);
        if (clamped == zoomIndex) return;
        zoomIndex = clamped;
        repaint();
    }

    int trackTop() const { return filterAreaHeight(); }
    int trackHeight() const { return std::max(1, getHeight() - FOOTER_H - trackTop()); }

    double yForQN(double qn, const View& v) const
    {
        double span = std::max(1.0e-6, v.endQN - v.startQN);
        return trackTop() + trackHeight() - (qn - v.startQN) / span * trackHeight();
    }

    double qnForY(double y, const View& v) const
    {
        double span = std::max(1.0e-6, v.endQN - v.startQN);
        return v.startQN + (trackTop() + trackHeight() - y) / trackHeight() * span;
    }

    // Bar under `y`, snapped to the bar line, since bars are the unit everything here uses.
    double snappedBarQN(int y, const View& v) const;

    bool inTrackArea(juce::Point<int> p) const
    {
        return p.y >= trackTop() && p.y <= trackTop() + trackHeight();
    }

    void drawHover(juce::Graphics& g, const View& v) const;

    // Measure the containing section starts on, or 1 before the first section.
    int sectionStartMeasure(double qn) const;

    juce::Rectangle<int> minusBounds() const
    { return { 4, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    juce::Rectangle<int> plusBounds() const
    { return { 4 + BUTTON_W + 2, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    juce::Rectangle<int> filterHeaderBounds() const
    { return { 0, 0, getWidth(), FILTER_HEADER_H }; }
    juce::Rectangle<int> filterRowBounds(int i) const
    { return { 0, FILTER_HEADER_H + i * FILTER_ROW_H, getWidth(), FILTER_ROW_H }; }

    void drawChunkLines(juce::Graphics& g, const View& v) const;
    void drawSections(juce::Graphics& g, const View& v) const;
    void drawNotes(juce::Graphics& g, const View& v) const;
    void drawFilters(juce::Graphics& g) const;
    void drawFooter(juce::Graphics& g) const;

    const SongSections* sections = nullptr;
    const TempoTimeSignatureMap* tempoMap = nullptr;
    std::vector<NoteSource> noteSources;
    AssetManager* assetManager = nullptr;
    PPQ playheadPPQ { 0.0 };
    int zoomIndex = DEFAULT_ZOOM_INDEX;
    int hoverY = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UtilityStripComponent)
};
