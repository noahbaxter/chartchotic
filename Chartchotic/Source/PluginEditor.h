/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/Processing/MidiInterpreter.h"
#include "Visual/Renderers/SceneRenderer.h"
#include "Visual/Renderers/TrackRenderer.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/Utils.h"
#include "Utils/TimeConverter.h"
#include "Visual/Utils/LatencySmoother.h"
#include "UpdateChecker.h"
#include "UI/LookAndFeel/ChartchoticLookAndFeel.h"
#include "UI/ToolbarComponent.h"
#include "UI/UpdateBannerComponent.h"
#ifdef DEBUG
#include "DebugTools/DebugEditorController.h"
#endif

//==============================================================================
/**
*/
class ChartchoticAudioProcessorEditor  :
    public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    ChartchoticAudioProcessorEditor (ChartchoticAudioProcessor&, juce::ValueTree &state);
    ~ChartchoticAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    bool keyPressed(const juce::KeyPress& key) override
    {
#ifdef DEBUG
        if (debugController.keyPressed(key, toolbar))
            return true;
#endif

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {

#ifdef DEBUG
        if (debugController.mouseWheelMove(wheel, event.mods.isShiftDown(),
                                            SCROLL_NORMAL_BEATS, SCROLL_SHIFT_BEATS))
            return;
#endif

        // Get the current playhead position
        if (auto* playHead = audioProcessor.getPlayHead())
        {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue())
            {
                double currentPPQ = positionInfo->getPpqPosition().orFallback(0.0);
                double jumpBeats = event.mods.isShiftDown() ? SCROLL_SHIFT_BEATS : SCROLL_NORMAL_BEATS;

                // Note: when shift is held, deltaY might be in deltaX instead
                double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
                double jumpAmount = wheelDelta * jumpBeats;

                double newPPQ = currentPPQ + jumpAmount;
                newPPQ = std::max(0.0, newPPQ);  // Clamp to 0

                audioProcessor.requestTimelinePositionChange(PPQ(newPPQ));
            }
        }
    }

private:
    juce::ValueTree& state;

    ChartchoticAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;
    SceneRenderer sceneRenderer;
    TrackRenderer trackRenderer;

    // Custom look and feel
    ChartchoticLookAndFeel chartPreviewLnF;

    // UI Components
    ToolbarComponent toolbar;

    //==============================================================================
    // UI Elements
    static constexpr int defaultWidth = 800;
    static constexpr int defaultHeight = 600;
    static constexpr int minWidth = 400;
    static constexpr int minHeight = 200;
    static constexpr int resizeDebounceMs = 150;
    static constexpr double sceneAspectRatio = 4.0 / 3.0;

    // Virtual scene dimensions (maintain internal 4:3 ratio)
    int sceneWidth = defaultWidth;
    int sceneHeight = defaultHeight;
    int sceneOffsetY = 0;

    // Background Assets
    juce::Image backgroundImageDefault;
    juce::Image backgroundImageCurrent;
    std::unique_ptr<juce::Drawable> reaperLogo;

    // Background image folder
    juce::StringArray backgroundNames;
    juce::File backgroundDirectory;
    void scanBackgrounds();
    void loadBackground(const juce::String& filename);

    struct ClickableLabel : public juce::Label
    {
        std::function<bool()> isClickable;
        std::function<void()> onClick;
        std::function<void(bool)> onHover;
        juce::Colour normalColour, hoverColour;

        void mouseDown(const juce::MouseEvent&) override
        {
            if (isClickable && isClickable() && onClick) onClick();
        }
        void mouseEnter(const juce::MouseEvent&) override
        {
            if (isClickable && isClickable())
            {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                setColour(juce::Label::textColourId, hoverColour);
                repaint();
                if (onHover) onHover(true);
            }
        }
        void mouseExit(const juce::MouseEvent&) override
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
            setColour(juce::Label::textColourId, normalColour);
            repaint();
            if (onHover) onHover(false);
        }
    };
    ClickableLabel versionLabel;

    UpdateChecker updateChecker;
    UpdateBannerComponent updateBanner;

    //==============================================================================

    void initAssets();
    void initToolbarCallbacks();
    void initBottomBar();
    void loadState();
    void updateDisplaySizeFromSpeedSlider();
    void applyLatencySetting(int latencyValue);

    void paintReaperMode(juce::Graphics& g);
    void paintStandardMode(juce::Graphics& g);
    void rebuildFadedTrackImage();

    // Highway texture overlay
    juce::StringArray highwayTextureNames;
    juce::File highwayTextureDirectory;
    void scanHighwayTextures();
    void loadHighwayTexture(const juce::String& filename);
    float computeScrollOffset();

    bool showHighway = true;
    float latencyInSeconds = 0.0;

    // Resize constraints
    juce::ComponentBoundsConstrainer constrainer;

    // Position tracking for cursor vs playhead separation
    PPQ lastKnownPosition = 0.0;
    bool lastPlayingState = false;

    PPQ displaySizeInPPQ = 1.5; // Only used for MIDI window fetching
    double displayWindowTimeSeconds = 1.0; // Actual render window time in seconds

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

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;   // Normal scroll: quarter note
    static constexpr double SCROLL_SHIFT_BEATS = 0.5;     // Shift+scroll: full beat

#ifdef DEBUG
    DebugEditorController debugController;
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

    void print(const juce::String &line)
    {
        audioProcessor.debugText += line + "\n";
    }

    std::string gemsToString(std::array<Gem, 7> gems)
    {
        std::string str = "(";
        for (const auto &gem : gems)
        {
            str += std::to_string((int)gem) + ",";
        }
        str += ")";
        return str;
    }

    void printCallback()
    {
        if (!audioProcessor.debugText.isEmpty())
        {
#ifdef DEBUG
            debugController.getConsole().moveCaretToEnd();
            debugController.getConsole().insertTextAtCaret(audioProcessor.debugText);
#endif
            audioProcessor.debugText.clear();
        }
    }

    std::string midiMessagesToString(const std::vector<juce::MidiMessage> &messages)
    {
        std::string str = "";
        for (const auto &message : messages)
        {
            str += message.getDescription().toStdString() + " ";
        }
        return str;
    }
};
