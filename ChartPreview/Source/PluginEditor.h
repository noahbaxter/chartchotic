/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/Processing/MidiInterpreter.h"
#include "Visual/Renderers/HighwayRenderer.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/Utils.h"
#include "Utils/TimeConverter.h"
#include "UpdateChecker.h"
#include "UI/ChartPreviewLookAndFeel.h"
#include "UI/ToolbarComponent.h"
#ifdef DEBUG
#include "DebugTools/DebugPlaybackController.h"
#endif

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  :
    public juce::AudioProcessorEditor
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&, juce::ValueTree &state);
    ~ChartPreviewAudioProcessorEditor() override;

    //==============================================================================

    void paint (juce::Graphics&) override;
    void resized() override;

    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    bool keyPressed(const juce::KeyPress& key) override
    {
#ifdef DEBUG
        if (debugStandalone && key == juce::KeyPress::spaceKey)
        {
            debugController.togglePlay();
            toolbar.setDebugPlay(debugController.isPlaying());
            return true;
        }
#endif

        // Handle arrow keys for latency offset when text box has focus
        auto& latencyOffsetInput = toolbar.getLatencyOffsetInput();
        if (latencyOffsetInput.hasKeyboardFocus(true))
        {
            int currentOffset = latencyOffsetInput.getText().getIntValue();

            // Get pipeline type to determine valid range
            bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
            int minValue = isReaperMode ? -2000 : 0;
            int maxValue = 2000;

            if (key == juce::KeyPress::upKey)
            {
                if (currentOffset < maxValue)
                {
                    int newValue = currentOffset + 10;  // Increment by 10ms
                    if (newValue > maxValue) newValue = maxValue;
                    latencyOffsetInput.setText(juce::String(newValue), false);
                    applyLatencyOffsetChange();
                }
                return true;
            }
            else if (key == juce::KeyPress::downKey)
            {
                if (currentOffset > minValue)
                {
                    int newValue = currentOffset - 10;  // Decrement by 10ms
                    if (newValue < minValue) newValue = minValue;
                    latencyOffsetInput.setText(juce::String(newValue), false);
                    applyLatencyOffsetChange();
                }
                return true;
            }
        }

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        // Ignore scroll on text input fields
        auto& latencyOffsetInput = toolbar.getLatencyOffsetInput();
        if (latencyOffsetInput.isMouseOver(true))
            return;

#ifdef DEBUG
        if (debugStandalone)
        {
            double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
            double jumpBeats = event.mods.isShiftDown() ? SCROLL_SHIFT_BEATS : SCROLL_NORMAL_BEATS;
            debugController.nudgePlayhead(wheelDelta * jumpBeats);
            return;
        }
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

    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;
    HighwayRenderer highwayRenderer;

    // Custom look and feel
    ChartPreviewLookAndFeel chartPreviewLnF;

    // UI Components
    ToolbarComponent toolbar;

    //==============================================================================
    // UI Elements
    static constexpr int defaultWidth = 600;
    static constexpr int defaultHeight = 600;
    static constexpr int minWidth = 300;
    static constexpr int minHeight = 300;

    // Background Assets
    juce::Image backgroundImageDefault;
    juce::Image backgroundImageRed;
    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;
    std::unique_ptr<juce::Drawable> trackDrumSvg;
    std::unique_ptr<juce::Drawable> trackGuitarSvg;
    bool useSvgTracks = false;
    float svgTrackScale = 1.0f;
    float svgTrackYOffset = 0.215f;
    float svgTrackFade = 0.3f;
    juce::Image svgTrackCached;
    juce::Image canvasOffscreen;
    int svgTrackCachedW = 0, svgTrackCachedH = 0;
    const juce::Drawable* svgTrackCachedSrc = nullptr;
    std::unique_ptr<juce::Drawable> reaperLogo;

    // Highway texture overlay
    juce::StringArray highwayTextureNames;
    juce::File highwayTextureDir;
    void scanHighwayTextures();
    void loadHighwayTexture(const juce::String& filename);
    double computeScrollOffset();

    juce::Label versionLabel;

    juce::TextEditor consoleOutput;
    juce::TextButton clearLogsButton;

    UpdateChecker updateChecker;
    juce::TextButton updateBanner;

    //==============================================================================

    void initAssets();
    void initToolbarCallbacks();
    void initBottomBar();
    void loadState();
    void updateDisplaySizeFromSpeedSlider();
    void applyLatencySetting(int latencyValue);
    void applyLatencyOffsetChange();

    void paintReaperMode(juce::Graphics& g);
    void paintStandardMode(juce::Graphics& g);

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

    // Cache invalidation throttling (for REAPER MIDI edit detection while paused)
    juce::int64 lastCacheInvalidationTicks = 0;

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;   // Normal scroll: quarter note
    static constexpr double SCROLL_SHIFT_BEATS = 0.5;     // Shift+scroll: full beat

#ifdef DEBUG
    DebugPlaybackController debugController;
    bool debugStandalone = false;
    float highwayScaleOverride = 0.74f;
    float highwayYPosition = 0.815f;

    // Debug chart loading
    TempoTimeSignatureMap debugMidiTempoMap;
    double debugChartLengthInBeats = 0.0;
    void loadDebugChart(int index);

    struct DebugChartEntry { const char* data; int size; };
    static const DebugChartEntry debugChartRegistry[];
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

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

    // Multi-buffer smoothing state
    PPQ lastSmoothedLatencyPPQ = 0.0;        // Current smoothed latency value
    PPQ smoothingTargetLatencyPPQ = 0.0;     // Target we're smoothing towards
    PPQ smoothingStartLatencyPPQ = 0.0;      // Starting point of current smooth
    double smoothingProgress = 1.0;          // 0.0 to 1.0, 1.0 means complete
    double smoothingDurationSeconds = 2.0;   // How long to spread adjustment over
    juce::int64 lastSmoothingUpdateTime = 0; // For timing calculations
    void setSmoothingDurationSeconds(double duration) { smoothingDurationSeconds = duration; }
    PPQ smoothedLatencyInPPQ()
    {
        PPQ targetLatency = latencyInPPQ();
        juce::int64 currentTime = juce::Time::getHighResolutionTicks();

        // Check if target has changed significantly or this is first frame
        double targetDifference = std::abs((targetLatency - smoothingTargetLatencyPPQ).toDouble());
        bool targetChanged = targetDifference > 0.01;  // Increased threshold to reduce jitter
        bool isFirstFrame = lastSmoothedLatencyPPQ == 0.0;

        if (isFirstFrame || targetChanged)
        {
            if (isFirstFrame)
            {
                // Initialize everything to target on first frame
                lastSmoothedLatencyPPQ = targetLatency;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingStartLatencyPPQ = targetLatency;
                smoothingProgress = 1.0;
                lastSmoothingUpdateTime = currentTime;
                return targetLatency;
            }
            else
            {
                // Start new smoothing transition
                smoothingStartLatencyPPQ = lastSmoothedLatencyPPQ;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingProgress = 0.0;
                lastSmoothingUpdateTime = currentTime;
            }
        }

        // Calculate time elapsed since last update
        double timeElapsedSeconds = (currentTime - lastSmoothingUpdateTime) / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        lastSmoothingUpdateTime = currentTime;

        // Update progress based on time elapsed
        if (smoothingProgress < 1.0)
        {
            double progressIncrement = timeElapsedSeconds / smoothingDurationSeconds;
            smoothingProgress = std::min(1.0, smoothingProgress + progressIncrement);

            // Use simple linear interpolation for more predictable behavior
            double totalAdjustment = (smoothingTargetLatencyPPQ - smoothingStartLatencyPPQ).toDouble();
            PPQ currentAdjustment = PPQ(totalAdjustment * smoothingProgress);
            lastSmoothedLatencyPPQ = smoothingStartLatencyPPQ + currentAdjustment;
        }
        else
        {
            // Smoothing complete, use target directly
            lastSmoothedLatencyPPQ = smoothingTargetLatencyPPQ;
        }

        return lastSmoothedLatencyPPQ;
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
            consoleOutput.moveCaretToEnd();
            consoleOutput.insertTextAtCaret(audioProcessor.debugText);
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
