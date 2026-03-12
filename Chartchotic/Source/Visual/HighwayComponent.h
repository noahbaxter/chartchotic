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
#include "../Utils/Utils.h"
#include "../Utils/TimeConverter.h"
#include "Renderers/SceneRenderer.h"
#include "Renderers/TrackRenderer.h"
#include "Managers/AssetManager.h"
#include "Utils/DrawingConstants.h"

struct HighwayFrameData {
    TimeBasedTrackWindow trackWindow;
    TimeBasedSustainWindow sustainWindow;
    TimeBasedGridlineMap gridlines;
    double windowStartTime = 0.0;
    double windowEndTime = 1.0;
    float scrollOffset = 0.0f;
    double deltaSeconds = 0.0;
    bool isPlaying = false;
    bool discoFlipActive = false;
};

class HighwayComponent : public juce::Component, private juce::Timer
{
public:
    HighwayComponent(juce::ValueTree& state, AssetManager& assetManager);

    void setActivePart(Part part);
    Part getActivePart() const { return activePart; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setFrameData(const HighwayFrameData& data);
    void rebuildTrack();

    // Visibility flags
    void setShowGems(bool on)           { sceneRenderer.showGems = on; repaint(); }
    void setShowBars(bool on)           { sceneRenderer.showBars = on; repaint(); }
    void setShowSustains(bool on)       { sceneRenderer.showSustains = on; repaint(); }
    void setShowLanes(bool on)          { sceneRenderer.showLanes = on; repaint(); }
    void setShowGridlines(bool on)      { sceneRenderer.showGridlines = on; repaint(); }
    void setShowTrack(bool on)          { sceneRenderer.showTrack = on; repaint(); }
    void setShowLaneSeparators(bool on) { sceneRenderer.showLaneSeparators = on; repaint(); }
    void setShowStrikeline(bool on)     { sceneRenderer.showStrikeline = on; repaint(); }
    void setShowHighway(bool on)        { showHighway = on; repaint(); }

    void setHighwayLength(float length) { sceneRenderer.farFadeEnd = length; rebuildTrack(); repaint(); }
    void setTexture(const juce::Image& img) { trackRenderer.setTexture(img); }
    void clearTexture()                 { trackRenderer.clearTexture(); }
    void setTextureScale(float s)       { trackRenderer.textureScale = s; repaint(); }
    void setTextureOpacity(float o)     { trackRenderer.textureOpacity = o; repaint(); }
    void setGemScale(float)             { repaint(); }
    void setBarScale(float)             { repaint(); }

    void onInstrumentChanged()          { setActivePart(isPart(state, Part::DRUMS) ? Part::DRUMS : Part::GUITAR); rebuildTrack(); repaint(); }

    // Accessors for debug wiring
    SceneRenderer& getSceneRenderer()   { return sceneRenderer; }
    TrackRenderer& getTrackRenderer()   { return trackRenderer; }

    int getTopOverflow() const { return topOverflow; }
    void updateOverflow();

    bool showHighway = true;
    bool stretchToFill = false;

    int renderWidth = 0, renderHeight = 0;
    std::function<void()> onOverflowChanged;

    /** Defer an expensive rebuild (e.g. window resize).
        Starts the debounce timer; paint() uses old track images until rebuild fires. */
    void deferRebuild() { startTimer(rebuildDebounceMs); repaint(); }

private:
    static constexpr int rebuildDebounceMs = 500;

    Part activePart = Part::GUITAR;
    juce::ValueTree& state;
    AssetManager& assetManager;
    SceneRenderer sceneRenderer;
    TrackRenderer trackRenderer;

    HighwayFrameData frameData;

    int topOverflow = 0;

    // Dimensions of the last full rebuild (track bake + asset rescale)
    int bakedRenderW = 0, bakedRenderH = 0, bakedOverflow = 0;

#ifdef DEBUG
public:
    bool showDebugColour = false;
private:
    juce::Colour debugColour;
#endif
};
