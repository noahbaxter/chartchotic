/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <unordered_map>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Visual/HighwaySlot.h"
#include "Visual/TrackImageCache.h"
#include "Visual/UtilityStripComponent.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/ChartTypes.h"
#include "Midi/Utils/TimeConverter.h"
#include "Visual/Utils/LatencySmoother.h"
#include "UI/UpdateChecker.h"
#include "UI/LookAndFeel/ChartchoticLookAndFeel.h"
#include "UI/ToolbarComponent.h"
#include "UI/Controls/WriteModeIcon.h"
#include "UI/UpdateBannerComponent.h"
#include "UI/FooterComponent.h"
#include "UI/PartsMenuComponent.h"
#include "UI/ConfirmPrompt.h"
#include "Editor/AssetController.h"
#include "Editor/SessionController.h"
#include "Editor/FrameDataBuilder.h"
#include "Editor/InteractionController.h"
#ifdef DEBUG
#include "DebugTools/DebugEditorController.h"
#endif

//==============================================================================

class ChartchoticAudioProcessorEditor  :
    public juce::AudioProcessorEditor
{
public:
    ChartchoticAudioProcessorEditor (ChartchoticAudioProcessor&, juce::ValueTree &state);
    ~ChartchoticAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Ahead of the write-mode keys: the strip is editor chrome, not an authoring tool,
        // so it toggles whether or not write mode is on.
        if (!key.getModifiers().isAnyModifierKeyDown())
        {
            if (key.getKeyCode() == 'U') { toggleUtilityStrip(); return true; }
            if (key.getKeyCode() == 'M' && utilityStripVisible)
            {
                utilityStrip.toggleZoom();
                return true;
            }

            // Next to the view toggle because they are the same kind of thing: getting
            // around the project rather than editing it. Brackets would have been the
            // obvious pair, but write mode owns those for the step division.
            if (key.getKeyCode() == ',' && utilityStripVisible)
            {
                utilityStrip.gotoAdjacentChart(-1);
                return true;
            }
            if (key.getKeyCode() == '.' && utilityStripVisible)
            {
                utilityStrip.gotoAdjacentChart(1);
                return true;
            }
        }

        if (interactionController.onKeyPress(key))
            return true;

#ifdef DEBUG
        if (debug.keyPressed(key, toolbar))
            return true;
#endif

        if (forwardToReaper(key))
            return true;

        return false;
    }

    void handleHighwayScroll(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
    {
        // Alt+wheel owns the write grid unconditionally. It never falls through
        // to seeking: moving the playhead when you meant to change the grid is
        // worse than doing nothing, so this consumes the event either way.
        // Sits ahead of the debug playhead nudge so it works in standalone too.
        // Ctrl is deliberately unused: macOS takes Ctrl+wheel for system zoom.
        if (event.mods.isAltDown())
        {
            double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
            if (wheelDelta != 0.0)
            {
                int div = interactionController.stepDivision();
                // Up is finer, matching ']'. setStepDivision clamps to 1..64.
                interactionController.setStepDivision(wheelDelta > 0 ? div * 2
                                                                     : div / 2);
            }
            return;
        }

#ifdef DEBUG
        if (debug.mouseWheelMove(wheel, event.mods.isShiftDown(),
                                            SCROLL_NORMAL_BEATS, SCROLL_SHIFT_BEATS))
            return;
#endif

        if (event.mods.isCommandDown())
        {
            double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
            int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;
            noteSpeed = juce::jlimit(NOTE_SPEED_MIN, NOTE_SPEED_MAX, noteSpeed + (wheelDelta > 0 ? 1 : -1));
            state.setProperty("noteSpeed", noteSpeed, nullptr);
            toolbar.getNoteSpeedStepper().setDisplayValue(noteSpeed);
            toolbar.getNoteSpeedStepper().setAtMin(noteSpeed <= NOTE_SPEED_MIN);
            toolbar.getNoteSpeedStepper().setAtMax(noteSpeed >= NOTE_SPEED_MAX);
            if (toolbar.onNoteSpeedChanged) toolbar.onNoteSpeedChanged(noteSpeed);
            return;
        }

        if (auto* playHead = audioProcessor.getPlayHead())
        {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue())
            {
                double currentPPQ = positionInfo->getPpqPosition().orFallback(0.0);
                double jumpBeats = event.mods.isShiftDown() ? SCROLL_SHIFT_BEATS : SCROLL_NORMAL_BEATS;

                double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
                double jumpAmount = wheelDelta * jumpBeats;

                double newPPQ = currentPPQ + jumpAmount;
                newPPQ = std::max(0.0, newPPQ);

                audioProcessor.requestTimelinePositionChange(PPQ(newPPQ));
            }
        }
    }

    bool forwardToReaper(const juce::KeyPress& key)
    {
        auto& apis = audioProcessor.getReaperMidiProvider().getAPIs();
        if (!apis.Main_OnCommand) return false;

        int code = key.getKeyCode();
        bool cmd = key.getModifiers().isCommandDown();
        bool shift = key.getModifiers().isShiftDown();

        // Transport
        if (code == juce::KeyPress::spaceKey)
            { apis.Main_OnCommand(40044, 0); return true; }

        if (!cmd) return false;

        // Undo / redo
        if (code == 'Z' && !shift)
            { apis.Main_OnCommand(40029, 0); interactionController.clearEditSelection(); return true; }
        if (code == 'Z' && shift)
            { apis.Main_OnCommand(40030, 0); interactionController.clearEditSelection(); return true; }

        // Save
        if (code == 'S')
            { apis.Main_OnCommand(40026, 0); return true; }

        return false;
    }

private:
    juce::ValueTree& state;

    ChartchoticAudioProcessor& audioProcessor;
    AssetManager assetManager;

    static constexpr int MAX_HIGHWAY_SLOTS = 4;
    std::array<HighwaySlot, MAX_HIGHWAY_SLOTS> slots;
    int activeSlotCount = 0;

    // The highway the mouse is working in. Authoring and the ghost both follow
    // it, so a multi-highway layout edits one chart at a time.
    int focusedSlot = 0;
    void focusSlot(int index);
    // Layout changes can retire the focused slot, so every read clamps.
    int effectiveFocusSlot() const
    { return juce::jlimit(0, juce::jmax(0, activeSlotCount - 1), focusedSlot); }

    // The slot the cursor is over, which is where the ghost is drawn and which lanes a hover is
    // read against. Separate from focus so moving across highways previews on each without the
    // write bar rewriting itself under the cursor: focus follows a click, or a hover that stays.
    // Only ever set from a pointer MOVE, which JUCE does not send while a button is down, so a
    // drag wandering over a neighbour cannot retarget the stroke halfway through.
    int hoveredSlot = -1;
    void setHoveredSlot(int index);

    // A cursor that settles on one highway and stays there is asking for that highway, so the
    // write bar stops describing one the mouse left. Crossing a highway on the way to the
    // toolbar never gets there, because leaving drops the hover and the clock with it, and the
    // dwell has to be served on one slot without a break. Both modes: an idle cursor is not
    // mid-edit, and a press is its own claim on the highway without waiting.
    static constexpr juce::uint32 hoverFocusDwellMs = 1200;
    // When the current hover began, or 0 for a hover that can no longer earn focus.
    juce::uint32 hoverDwellStartMs = 0;
    void updateHoverFocusDwell();
    // Which slot the controller is currently resolving points against. Only the draw ghost
    // follows the cursor: an edit-mode selection is held against one part's tracks, so pointing
    // the controller at a neighbouring highway mid-selection would aim it at the wrong notes.
    int pointerContextSlot() const
    {
        if (interactionController.subMode() != SubMode::Draw) return effectiveFocusSlot();
        return hoveredSlot >= 0 && hoveredSlot < activeSlotCount ? hoveredSlot : effectiveFocusSlot();
    }

    // Shared track image cache (guitar + drums baked once at full resolution)
    TrackImageCache trackImageCache;
    std::unique_ptr<TrackRenderer> cacheRenderer;
    void requestAsyncRebake();

    HighwayComponent& primaryHighway() { return *slots[0].highway; }
    MidiInterpreter& primaryInterpreter() { return *slots[0].interpreter; }

    template <typename Fn>
    void forEachHighway(Fn&& fn)
    {
        for (int i = 0; i < activeSlotCount; i++)
            fn(*slots[i].highway);
    }

    template <typename Fn>
    void forEachActiveSlot(Fn&& fn)
    {
        for (int i = 0; i < activeSlotCount; i++)
            fn(slots[i]);
    }

    // Iterate ALL pooled highways (including inactive/hidden ones).
    // Use for texture/asset operations that must be set on all slots upfront.
    template <typename Fn>
    void forAllHighways(Fn&& fn)
    {
        for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
            fn(*slots[i].highway);
    }

    void propagateToSlots(const juce::Identifier& prop, const juce::var& value)
    {
        for (int i = 0; i < activeSlotCount; i++)
            if (slots[i].ownedState)
                slots[i].ownedState->setProperty(prop, value, nullptr);
    }

    // Custom look and feel
    ChartchoticLookAndFeel chartPreviewLnF;

    // Interaction controller (must be declared before toolbar — toolbar holds a reference)
    InteractionController interactionController;

    // UI Components
    ToolbarComponent toolbar;
    // Nothing in the app displayed a tooltip before this: the controls stored them and no window
    // existed to show them. Owned by the editor so every control under it is covered.
    juce::TooltipWindow tooltipWindow { this, TOOLTIP_DELAY_MS };

    // Floating mode chip — owned by the editor so it sits over the highway
    // (top-left, just below the toolbar). Click toggles write mode; key
    // shortcuts (W/Q) still work via WriteController.
    WriteModeIcon writeModeIcon;

    // Song-scoped time axis beside the highways. Toggleable, so the grid reflows for it
    // rather than reserving space.
    UtilityStripComponent utilityStrip;
    bool utilityStripVisible = true;
    static constexpr int utilityStripMargin = 6;

    // The strip is a unit with a shape of its own, not a fixed slab. Width takes a share of
    // the editor and everything the strip draws is sized off it, but the aspect bounds hold
    // that share to a column: a short wide window gets no stubby fat ruler, a tall narrow
    // one no hairline. Same fit-it-into-the-space move the toolbar and the bemani highways
    // make, run on both ends.
    static constexpr double utilityStripWidthRatio = 0.14;
    static constexpr double utilityStripMinAspect = 0.16;  // width:height, floor
    static constexpr double utilityStripMaxAspect = 0.38;  // width:height, ceiling
    // The aspect floor pushes width up on a tall window, which is the one direction it can
    // eat the highway, so the share it may take of the editor caps it either way.
    static constexpr double utilityStripMaxShare = 0.22;
    static constexpr int utilityStripMinWidth = 170;
    static constexpr int utilityStripMaxWidth = 420;

    // Resolved in resized() before any geometry derives from it.
    int utilityStripWidth = utilityStripMinWidth;

    int computeUtilityStripWidth() const
    {
        double byAspect = juce::jlimit(getHeight() * utilityStripMinAspect,
                                       getHeight() * utilityStripMaxAspect,
                                       getWidth() * utilityStripWidthRatio);
        double capped = std::min(byAspect, getWidth() * utilityStripMaxShare);
        return juce::jlimit(utilityStripMinWidth, utilityStripMaxWidth,
                            juce::roundToInt(capped));
    }

    void toggleUtilityStrip()
    {
        utilityStripVisible = !utilityStripVisible;
        state.setProperty("showUtilityStrip", utilityStripVisible, nullptr);
        resized();
    }

    // The section the playhead is inside, spelled over the highway the way the games title
    // it. It rides on the strip: no strip and it is gone.
    bool sectionBannerVisible = true;

    void toggleSectionBanner()
    {
        sectionBannerVisible = !sectionBannerVisible;
        state.setProperty("showSectionBanner", sectionBannerVisible, nullptr);
        resized();   // the grid may be holding room for it
        repaint();
    }

    bool sectionBannerShown() const { return sectionBannerVisible && utilityStripVisible; }
    float sectionBannerSize() const
    { return std::max(sectionBannerMinPt, getHeight() * sectionBannerHeightRatio); }

    // Height the banner covers below the toolbar, song and section lines both. Reserved
    // whether or not either name is set right now, so the grid does not jump as you play.
    int sectionBannerReserve() const
    {
        return juce::roundToInt(sectionBannerSize()
                                * (sectionBannerTopGap + songBannerScale * bannerLineGap
                                   + bannerLineGap));
    }

    void drawSectionBanner(juce::Graphics& g);
    int drawBannerLine(juce::Graphics& g, const juce::String& name, float size, int left, int y);

    static constexpr float sectionBannerHeightRatio = 0.034f;
    static constexpr float sectionBannerMinPt = 13.0f;
    static constexpr float sectionBannerTopGap = 0.5f;    // of its own text height
    static constexpr int sectionBannerShadowPx = 2;
    // Enough below the section to read as its heading rather than as a second title.
    static constexpr float songBannerScale = 0.62f;
    static constexpr float bannerLineGap = 1.15f;         // of the line's own text height
    // The white the logo sets CHART in, over the shadow it needs to stay readable on
    // whatever background art is loaded.
    inline static const juce::Colour sectionBannerText = juce::Colour(Theme::textWhite);
    inline static const juce::Colour sectionBannerShadow = juce::Colour(0x99000000);

    //==============================================================================
    // UI Elements
    static constexpr int defaultWidth = 600;
    static constexpr int defaultHeight = 600;
    static constexpr int minWidth = 200;
    static constexpr int minHeight = 200;
    static constexpr double sceneAspectRatio = 4.0 / 3.0;
    static constexpr int highwayGridPadding = 4;        // px between highways and window edges in multi-slot grid
    static constexpr double bemaniMinAspect = 1.0 / 2.0; // minimum width:height ratio for Bemani highways

    // Virtual scene dimensions (maintain internal 4:3 ratio)
    int sceneWidth = defaultWidth;
    int sceneHeight = defaultHeight;

    // Asset management (backgrounds, textures, logos)
    AssetController assets;

    // Session/slot management (multi-highway, instrument/difficulty selection)
    SessionController session;

    UpdateBannerComponent updateBanner;
    FooterComponent footer { updateBanner };

    // The map itself lives on the processor, since discovery reads it and has to keep
    // working with the window shut. This is only the view on it.
    PartsMenuComponent partsMenu;
    ConfirmPrompt confirmPrompt;
    static constexpr int partsMenuMargin = 16;
    void togglePartsMenu();
    void createTrackForPart(Part part);
    void confirmDeleteTrack(int trackIndex, const juce::String& name);
    void deleteTrack(int trackIndex);
    void openPartsMenu();
    void layoutPartsMenu();
    void refreshPartsMenuTracks();
    // REAPER changes tracks out from under the open menu, an undo of a delete included, so
    // the list is re-read while it is on screen rather than only when it opens. Throttled:
    // reading it walks every track's items, which is far more than a frame's worth of work
    // for an answer that changes when you go and change it.
    void pollPartsMenuTracks();
    static constexpr double partsMenuPollIntervalMs = 250.0;
    double lastPartsMenuPollMs = 0.0;
    void commitPartPins();
    UpdateChecker updateChecker;

    //==============================================================================

    void initToolbarCallbacks();
    void initBottomBar();
    void loadState();
    void updateTrackInfoDisplay();
    void updateFooterHelpText()
    {
        if (!interactionController.writeModeActive())
        {
            footer.setModeAccent(juce::Colour(Theme::coral));
            footer.setDefaultHelpText(makeHelp({
                key("Scroll"), dim("navigate"),
                key("Cmd+Scroll"), dim("adjust speed")
            }));
            return;
        }

        bool drawMode = interactionController.subMode() == SubMode::Draw;
        footer.setModeAccent(drawMode ? juce::Colour(Theme::purple) : juce::Colour(Theme::blue));

        bool stamp = drawMode && interactionController.hasStamp();
        bool bar = interactionController.barMode();
        HelpText h;

        if (drawMode)
        {
            if (stamp)
                h = makeHelp({
                    key("Click"), dim("place stamp"),
                    key("Shift+drag"), dim("paint"),
                    key("Left/Right"), dim("shift lanes"),
                    key("Esc"), dim("clear stamp")
                });
            else
                h = makeHelp({
                    key("Click"), dim("place note"),
                    key("Shift+drag"), dim("paint"),
                    key("Right-click"), dim("erase")
                });
        }
        else
        {
            h = makeHelp({
                key("Click"), dim("select"),
                key("Drag"), dim("marquee"),
                key("Double-click"), dim("create/delete"),
                key("Arrows"), dim("move")
            });
        }

        if (bar)
        {
            h.push_back(key("B"));
            h.push_back(dim("exit bar mode"));
        }

        footer.setDefaultHelpText(h);
    }
#ifdef DEBUG
    void rebuildSlots(const DebugMidiFilePlayer::LoadedChart& chart);
#endif
    void updateDisplaySizeFromSpeedSlider();
    void applyLatencySetting(int latencyValue);

    FrameContext buildFrameContext();

    float computeScrollOffset();

    float latencyInSeconds = 0.0;

    // Resize constraints
    juce::ComponentBoundsConstrainer constrainer;

    // Position tracking for cursor vs playhead separation
    PPQ lastKnownPosition = 0.0;
    bool lastPlayingState = false;

    PPQ displaySizeInPPQ = 1.5; // Only used for MIDI window fetching
    double displayWindowTimeSeconds = 1.0; // Actual render window time in seconds

    // Compute farFadeEnd from user slider value × per-instrument scale
    float computeFarFadeEnd(float userLength) const
    {
        bool drums = activeSlotCount == 0 ? isDrumLike(getPartFromState(state))
                                          : isDrumLike(slots[0].part);
        return userLength * getHwyScale(drums);
    }

    // VBlank-synced rendering
    juce::VBlankAttachment vblankAttachment;
    double targetFrameInterval = 0.0;  // 0 = native (no throttle), otherwise seconds between frames
    juce::int64 lastFrameTicks = 0;
    void onFrame();

    // FPS counter
    bool showFps = false;
    static constexpr int FPS_RING_SIZE = 60;
    double fpsRing[FPS_RING_SIZE] = {};
    int fpsRingIndex = 0;
    double lastFrameTimestamp = 0.0;
    void drawFpsOverlay(juce::Graphics& g);

    // Cache invalidation throttling (for REAPER MIDI edit detection while paused)
    juce::int64 lastCacheInvalidationTicks = 0;

    // Track info display (footer status bar)
    int lastDisplayedTrackNumber = -1;
    juce::String lastDisplayedTrackName;

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;
    static constexpr double SCROLL_SHIFT_BEATS  = 8.0;   // Shift+scroll: 4x normal

#ifdef DEBUG
    DebugEditorController debug;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartchoticAudioProcessorEditor)

    // Latency tracking
    PPQ defaultLatencyInPPQ = 0.0;
    double defaultBPM = 120.0;

    PPQ latencyInPPQ()
    {
        auto* playHead = audioProcessor.getPlayHead();
        if (playHead == nullptr) return defaultLatencyInPPQ;

        auto positionInfo = playHead->getPosition();
        if (!positionInfo.hasValue()) return defaultLatencyInPPQ;

        double bpm = positionInfo->getBpm().orFallback(defaultBPM);
        return PPQ(audioProcessor.latencyInSeconds * (bpm / 60.0));
    }

    // Multi-buffer latency smoothing
    LatencySmoother latencySmoother;
    PPQ smoothedLatencyInPPQ()
    {
        return latencySmoother.smooth(latencyInPPQ());
    }

    //==============================================================================
    // Prints

    void printCallback()
    {
        if (!audioProcessor.debugText.isEmpty())
        {
#ifdef DEBUG
            debug.getConsole().moveCaretToEnd();
            debug.getConsole().insertTextAtCaret(audioProcessor.debugText);
#endif
            audioProcessor.debugText.clear();
        }
    }
};
