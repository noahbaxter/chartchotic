/*
    ==============================================================================

        HighwayComponent.h
        Author: Noah Baxter

        Encapsulates one complete chart highway view as a JUCE Component.
        PluginEditor positions it with setBounds().

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/TimeConverter.h"
#include "../Editor/AuthoringTypes.h"
#include "PhraseMath.h"
#include "ViewLayers.h"
#include "Renderers/SceneRenderer.h"
#include "Art/TrackRenderer.h"
#include "Managers/AssetManager.h"
#include "Utils/DrawingConstants.h"
#include "Geometry/HitTestMapper.h"

class TrackImageCache;

struct HighwayFrameData {
    TimeBasedTrackWindow trackWindow;
    TimeBasedSustainWindow sustainWindow;
    TimeBasedSustainWindow phraseWindow;   // star power bands and fill lanes, for the editor views
    TimeBasedGridlineMap gridlines;
    TimeBasedFlipRegions flipRegions;
    TimeBasedEventMarkers eventMarkers;   // section, tempo and time signature gates
    double windowStartTime = 0.0;
    double windowEndTime = 1.0;
    float scrollOffset = 0.0f;
    double deltaSeconds = 0.0;
    bool isPlaying = false;
    bool discoFlipActive = false;
    Part builtForPart = Part::GUITAR;
};

class HighwayComponent : public juce::Component, private juce::Timer
{
public:
    HighwayComponent(juce::ValueTree& state, AssetManager& assetManager);

    void setActivePart(Part part);
    Part getActivePart() const { return activePart; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Mouse input — dispatches into write-mode callbacks (M1.3).
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp          (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (onMouseWheel) onMouseWheel(e, wheel);
    }

    // Authoring dispatch hooks. Wired by PluginEditor; null when unset.
    using PointerCallback = std::function<void(const AuthoringPoint&, const AuthoringContext&)>;
    void setOnPointerMove  (PointerCallback cb) { onPointerMove   = std::move(cb); }
    void setOnPointerDown  (PointerCallback cb) { onPointerDown   = std::move(cb); }
    void setOnPointerDrag  (PointerCallback cb) { onPointerDrag   = std::move(cb); }
    void setOnPointerUp    (PointerCallback cb) { onPointerUp     = std::move(cb); }
    void setOnPointerExit  (std::function<void()> cb) { onPointerExit   = std::move(cb); }
    void setOnPointerCancel(std::function<void()> cb) { onPointerCancel = std::move(cb); }
    void setOnPointerDoubleClick(PointerCallback cb) { onPointerDoubleClick = std::move(cb); }

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onMouseWheel;

    // Coordinate-domain conversion at the dispatch boundary (M0-G design rule).
    // Takes a "seconds offset from cursor" (the timeFromCursor returned by HitTestMapper)
    // and returns project QN. If unset, mouse handlers fall back to rawProjectQN = 0.
    void setSecondsToProjectQN(std::function<double(double)> fn) { secondsToProjectQN = std::move(fn); }

    // Inverse of secondsToProjectQN — given a project QN, return the seconds-offset-from-cursor
    // used by the rendering window. Required for drawing the hover ghost at the snapped QN.
    void setProjectQNToSeconds(std::function<double(double)> fn) { projectQNToSeconds = std::move(fn); }

    // Read-only access to the WriteController's overlay state for the ghost cursor.
    using OverlayStateGetter = std::function<const OverlayState&()>;
    void setOverlayStateGetter(OverlayStateGetter g) { overlayStateGetter = std::move(g); }
    void setPatchBuffer(const OptimisticPatchBuffer* buf) { patchBuffer = buf; }

    // Format a project QN as "M.B" position label (e.g. "37.2.5"). Wired by PluginEditor.
    void setFormatPositionQN(std::function<juce::String(double)> fn) { formatPositionQN = std::move(fn); }

    // By value so a caller with a dead local can move its windows in rather than deep-copying
    // seven containers, one of them a std::map with a node per visible note, every frame per slot.
    void setFrameData(HighwayFrameData data);
    void rebuildTrack();

    // The board's colours changed under us. rebuildTrack alone is not enough: TrackRenderer
    // skips a rebuild whose dimensions have not moved, so the strikeline would keep the pads
    // it baked in the old palette while everything else repainted in the new one.
    void repaintTrackArt()
    {
        trackRenderer.invalidate();
        rebuildTrack();
        repaint();
    }

    // Everything the View menu controls on a highway, in one go.
    void applyView(const ViewOptions& view)
    {
        const auto has = [&view](ViewLayers::Layer l)
        { return view.has(l, ViewLayers::Surface::Highway); };
        using L = ViewLayers::Layer;

        sceneRenderer.showGems = has(L::Gems);
        sceneRenderer.showBars = has(L::Bars);
        sceneRenderer.showSustains = has(L::Sustains);
        sceneRenderer.showLanes = has(L::Lanes);
        sceneRenderer.showFills = has(L::Fills);
        sceneRenderer.showBre = has(L::Bre);
        sceneRenderer.showSolos = has(L::Solos);
        sceneRenderer.showGridlines = has(L::BeatLines);
        sceneRenderer.showMeasureNumbers = has(L::MeasureNumbers);
        sceneRenderer.showTrack = has(L::SideRails);
        sceneRenderer.showLaneSeparators = has(L::LaneLines);
        sceneRenderer.showStrikeline = has(L::Strikeline);
        showHighway = has(L::Surface);
        gateLayerOn = { has(L::SectionGates), has(L::TimeSigGates),
                        has(L::TempoGates), has(L::DiscoGates) };

        // Also drives whether the activation gem grows.
        showActivationOn = has(L::Activation);
        sceneRenderer.showActivationLane = showActivationOn;
        applyEditorView();
        repaint();
    }
    void setLaneToolActive(bool on)     { laneToolOn = on; applyEditorView(); repaint(); }
    void setWriteMode(bool on)          { writeModeOn = on; sceneRenderer.setWriteMode(on); applyEditorView(); repaint(); }

    // The editor view decides which phrase overlays draw. They are a write-mode thing, so
    // with write mode off every view looks like Notes.
    void setEditorView(EditorView v)    { editorView = v; applyEditorView(); repaint(); }

    void setHighwayLength(float length) { sceneRenderer.farFadeEnd = length; PositionMath::bemaniHwyScale = length; rebuildTrack(); repaint(); }
    void setTexture(const juce::Image& img) { trackRenderer.setTexture(img); }
    void clearTexture()                 { trackRenderer.clearTexture(); }
    void setTextureScale(float s)       { trackRenderer.textureScale = s; repaint(); }
    void setTextureOpacity(float o)     { trackRenderer.textureOpacity = o; repaint(); }
    void setGemScale(float)             { repaint(); }
    void setBarScale(float)             { repaint(); }

    static constexpr float labelIconSize = 40.0f;

public:
    // Authoring state that every highway shows, so it cannot come from the focus-gated overlay.
    void setBarMode(bool on) { barModeOn = on; }

    // A BRE is one song-global phrase written to every part at once, so its preview has to show
    // on every highway rather than only the one the cursor is over. The overlay cannot carry it:
    // that is gated to the focused slot and comes back blank everywhere else.
    //
    // Pulled at paint, not pushed: the hover moves with the cursor, and onStateChanged (the only
    // push channel) does not fire on pointer move or exit, so a pushed hover both lagged the
    // cursor and outlived it.
    struct GlobalPhrase
    {
        OverlayState::PhrasePreview drag;
        OverlayState::PhrasePreview hover;
    };
    using GlobalPhraseGetter = std::function<GlobalPhrase()>;
    void setGlobalPhraseGetter(GlobalPhraseGetter g) { globalPhraseGetter = std::move(g); }

private:
    bool writeModeOn = false;
    bool barModeOn = false;
    bool showActivationOn = DEFAULT_SHOW_ACTIVATION;
    // What the View menu asks for, by GateKind. The editor view can add to it at paint time.
    std::array<bool, GATE_KIND_COUNT> gateLayerOn { false, false, false, true };
    bool laneToolOn = false;
    GlobalPhraseGetter globalPhraseGetter;
    EditorView editorView = EditorView::Notes;
    EditorView shownView() const { return writeModeOn ? editorView : EditorView::Notes; }
    // The lane tool and bar mode are write-mode overlays like the view is, so they stop showing
    // with the mode that owns them. Without this, leaving write mode while one was on left the
    // board faded out for a tool that could no longer do anything. The flags themselves are
    // kept, so coming back into write mode puts you back on the tool you were using.
    bool laneToolShown() const { return writeModeOn && laneToolOn; }
    bool barModeShown()  const { return writeModeOn && barModeOn; }
    bool phraseViewActive() const
    {
        auto v = shownView();
        return v == EditorView::StarPower || v == EditorView::Phrase;
    }
    void applyEditorView()
    {
        // The lane and the grown gem are one toggle in both modes. Reading a chart, which note
        // takes star power is worth as much as it is while authoring, and the lane alone does not
        // say it: the grown gem is what picks that note out of the fill it sits in.
        sceneRenderer.setShowActivation(showActivationOn);
        // Lane mode dims the gems because the lane is what you are working on and it is drawn
        // behind notes that would otherwise sit on top of it. The Phrase view is not the same
        // case: a fill or BRE drawn at phrase-view strength already reads, and dimming the notes
        // over it only made the phrase look like it had come in front of them.
        sceneRenderer.setPhraseDim(shownView() == EditorView::StarPower
                                   || shownView() == EditorView::Sections || laneToolShown()
                                   ? PHRASE_VIEW_GEM_DIM : 1.0f);
        sceneRenderer.setPhraseViewActive(shownView() == EditorView::Phrase);
    }

public:

    bool showPartLabel = false;
    bool showDifficultyLabel = false;
    SkillLevel displaySkillLevel = SkillLevel::EXPERT;

    /** When cache is active, instrument change only swaps overlay pointers — no rebuild. */
    void onInstrumentChanged();

    void setTrackImageCache(TrackImageCache* cache) { trackImageCache = cache; }

    // Accessors for debug wiring
    SceneRenderer& getSceneRenderer()   { return sceneRenderer; }
    TrackRenderer& getTrackRenderer()   { return trackRenderer; }

    int getTopOverflow() const { return topOverflow; }
    void updateOverflow();

    bool showHighway = true;
    bool stretchToFill = false;

    int renderWidth = 0, renderHeight = 0;
    // Device pixels per point for the display this highway is on, recorded during paint. The
    // track bake reads it so its images land 1:1 on a retina backing store instead of being
    // interpolated up at blit time.
    float physicalScale = 1.0f;

    /** What paint()'s transform scales the scene by on its way into the component, for the
        component's CURRENT bounds. Derived rather than remembered from the last paint: the
        window is resizable, and resized() asks for this before the next paint has run, so a
        cached value would bake every resize at the previous size's scale.

        paint() computes the same thing inline rather than calling this, because while the
        resize debounce is running it deliberately scales the STALE baked dimensions instead
        of the live ones. This always answers for the live ones, which is what the next bake
        wants. */
    void getDrawScale(float& outX, float& outY) const
    {
        outX = outY = 1.0f;
        const int w = renderWidth > 0 ? renderWidth : getWidth();
        const int totalH = (renderHeight > 0 ? renderHeight : getHeight()) + topOverflow;
        if (w <= 0 || totalH <= 0 || getWidth() <= 0 || getHeight() <= 0) return;

        if (stretchToFill && !PositionMath::bemaniMode)
        {
            outX = (float)getWidth()  / (float)w;
            outY = (float)getHeight() / (float)totalH;
        }
        else
        {
            outX = outY = std::min((float)getWidth()  / (float)w,
                                   (float)getHeight() / (float)totalH);
        }
    }
    std::function<void()> onOverflowChanged;

    /** Defer an expensive rebuild (e.g. window resize).
        Starts the debounce timer; paint() uses old track images until rebuild fires. */
    void deferRebuild() { startTimer(rebuildDebounceMs); repaint(); }

private:
    static constexpr int rebuildDebounceMs = 500;

    // The one place the active lane table is chosen. Everything that needs to agree with the
    // drawn columns (lane lines, hit testing, marquee) goes through these, so nothing can pick
    // the 4-lane table for elite or read the static constants while the renderer reads the
    // debug-tunable copies. isDrumLike() is true for elite, so elite must be tested first.
    const PositionConstants::NormalizedCoordinates* activeLaneCoords() const;
    int activeLaneCount() const;

    Part activePart = Part::GUITAR;
    Part pendingPart = Part::GUITAR;
    void commitPendingPart();
    juce::ValueTree& state;
    AssetManager& assetManager;
    SceneRenderer sceneRenderer;
    TrackRenderer trackRenderer;
    TrackImageCache* trackImageCache = nullptr;

    HighwayFrameData frameData;

    // M1.3 mouse dispatch
    HitTestMapper hitTestMapper;
    PointerCallback onPointerMove;
    PointerCallback onPointerDown;
    PointerCallback onPointerDrag;
    PointerCallback onPointerUp;
    std::function<void()> onPointerExit;
    std::function<void()> onPointerCancel;
    PointerCallback onPointerDoubleClick;
    std::function<double(double)> secondsToProjectQN;
    std::function<double(double)> projectQNToSeconds;
    OverlayStateGetter            overlayStateGetter;
    const OptimisticPatchBuffer*  patchBuffer = nullptr;
    std::function<juce::String(double)> formatPositionQN;

    // Last ghost state seen at end of mouseMove. Used to throttle repaint —
    // vblank already drives 60fps, mouseMove fires at 100+ Hz on fast pointers,
    // so re-rendering on every cursor pixel saturates the renderer at high
    // step subdivisions (cost scales with gridline count).
    struct LastGhostState
    {
        bool   visible = false;
        int    lane = -1;
        double qn = 0.0;
    };
    LastGhostState lastGhost;

    // Build authoring payloads from a JUCE mouse event.
    void buildAuthoringPayload(const juce::MouseEvent& e,
                               AuthoringPoint& outPoint,
                               AuthoringContext& outContext) const;

    // Map a component-local screen-space point into the render-space coordinates
    // that PositionMath / HitTestMapper operate in. Inverse of the paint()
    // transform: render canvas is renderWidth x (renderHeight + topOverflow),
    // mapped to the component via either independent X/Y stretch (stretchToFill)
    // or uniform letterboxed scale.
    juce::Point<float> screenToRenderCoords(juce::Point<float> screen) const;

    int topOverflow = 0;

    // Dimensions of the last full rebuild (track bake + asset rescale)
    int bakedRenderW = 0, bakedRenderH = 0, bakedOverflow = 0;

#ifdef DEBUG
public:
    bool showDebugColour = false;
    bool showClickZones = false;
    mutable float debugLastHitY = -1.0f;
    mutable double debugLastResolvedQN = 0.0;
    mutable int debugLastLane = -1;
    double debugTrackRender_us = 0.0;
    double debugHighwayPaint_us = 0.0;
private:
    juce::Colour debugColour;
#endif
};
