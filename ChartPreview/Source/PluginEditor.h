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

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  :
    public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&, juce::ValueTree &state);
    ~ChartPreviewAudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override
    {
        printCallback();

        bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
        toolbar.setReaperMode(isReaperMode);
        toolbar.updateVisibility();

        // Track position changes for render logic
        if (auto* playHead = audioProcessor.getPlayHead()) {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue()) {
                PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
                bool isCurrentlyPlaying = positionInfo->getIsPlaying();

                // Update tracked position and playing state
                lastKnownPosition = currentPosition;
                lastPlayingState = isCurrentlyPlaying;

                // In REAPER mode, throttled cache invalidation while paused to pick up MIDI edits in real-time
                // Throttled to ~20 Hz (every 3 frames at 60 FPS) to keep responsiveness without overwhelming the host
                if (isReaperMode && !isCurrentlyPlaying)
                {
                    paused_frameCounterSinceLastInvalidation++;
                    if (paused_frameCounterSinceLastInvalidation >= 3)
                    {
                        paused_frameCounterSinceLastInvalidation = 0;
                        audioProcessor.invalidateReaperCache();
                    }
                }
                else
                {
                    paused_frameCounterSinceLastInvalidation = 0;  // Reset when playing
                }
            }
        }

        // Debug standalone playback: advance simulated playhead
#ifdef DEBUG
        if (debugPlayActive)
        {
            juce::int64 now = juce::Time::getHighResolutionTicks();
            double deltaSeconds = (now - debugPlayLastTick) / (double)juce::Time::getHighResolutionTicksPerSecond();
            debugPlayLastTick = now;

            // Advance at 120 BPM (2 beats per second)
            debugPlayPPQ += deltaSeconds * (120.0 / 60.0);

            lastKnownPosition = PPQ(debugPlayPPQ);
            lastPlayingState = true;
        }
#endif

        // Update highway texture scroll offset
        highwayRenderer.setScrollOffset(computeScrollOffset());

        repaint();
    }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    bool keyPressed(const juce::KeyPress& key) override
    {
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
    static constexpr int defaultWidth = 800;
    static constexpr int defaultHeight = 600;
    static constexpr double aspectRatio = (double)defaultWidth / defaultHeight;
    static constexpr int minWidth = 400;
    static constexpr int minHeight = 300;

    // Background Assets
    juce::Image backgroundImageDefault;
    juce::Image backgroundImageRed;
    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;
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

    // Cache invalidation throttling (for REAPER MIDI edit detection while paused)
    int paused_frameCounterSinceLastInvalidation = 0;  // Throttle to ~20 Hz (every 3 frames at 60 FPS)

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;   // Normal scroll: quarter note
    static constexpr double SCROLL_SHIFT_BEATS = 0.5;     // Shift+scroll: full beat

    // Debug standalone playback simulation
#ifdef DEBUG
    bool debugPlayActive = false;
    bool debugNotesActive = false;
    double debugPlayPPQ = 0.0;
    juce::int64 debugPlayLastTick = 0;

    TrackWindow generateDebugChart(PPQ startPPQ);
    SustainWindow generateDebugSustains(PPQ startPPQ);
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
