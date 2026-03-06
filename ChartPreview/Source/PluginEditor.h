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
#include "UI/LookAndFeel/ChartPreviewLookAndFeel.h"
#include "UI/ToolbarComponent.h"
#ifdef DEBUG
#include "DebugTools/DebugEditorController.h"
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
        if (debugController.keyPressed(key, toolbar))
            return true;
#endif

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        // Ignore scroll on slider controls
        if (toolbar.getLatencyOffsetSlider().isMouseOver(true))
            return;

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

    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;
    SceneRenderer sceneRenderer;
    TrackRenderer trackRenderer;

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
    juce::Image backgroundImageCurrent;
    std::unique_ptr<juce::Drawable> reaperLogo;

    // Background image folder
    juce::StringArray backgroundNames;
    juce::File backgroundDirectory;
    void scanBackgrounds();
    void loadBackground(const juce::String& filename);

    juce::Label versionLabel;

    UpdateChecker updateChecker;
    juce::TextButton updateBanner;

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
    DebugEditorController debugController;
    double lastRepaintTimestamp = 0.0;
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
