/*
    ==============================================================================

        WaveformTuningPanel.h
        Author: Noah Baxter

        The waveform layer's controls. Rides with the layer: it appears when the
        waveform does and goes away with it.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include "Theme.h"
#include "../Visual/Utils/WaveformTuning.h"

/*
    Not the debug tuning panel: that one is DEBUG-only behind a toolbar menu, and the waveform
    has to be dialled in against real audio in a real session, which is a release build. These
    are the controls for a feature rather than a developer's probe, so they live with the
    feature and persist like any other setting.
*/
class WaveformTuningPanel : public juce::Component
{
public:
    // Every control on the panel, so the layout, the persistence and the reset all walk one
    // list rather than three that have to be kept in step.
    struct Control
    {
        const char* name;
        const char* key;        // what it is saved as
        float* value;
        float min, max, step;
        int decimals;
        // Snap to powers of two, for the control whose in-between values are a lie: the
        // renderer groups whole cache buckets, so 5ms and 6ms are the same picture, and a
        // slider reading 5 while drawing 4 is one that cannot be trusted for anything.
        bool snapToPowerOfTwo = false;
    };

    explicit WaveformTuningPanel(juce::ValueTree& stateToUse) : state(stateToUse)
    {
        auto& t = Waveform::tuning();

        controls = {
            { "Res ms",    "resolution", &t.resolutionMs,  1.0f, 128.0f, 1.0f,  0, true },
            // The two that decide whether a transient reads at all, so they sit directly
            // under Detail rather than at the bottom with the cosmetics.
            { "Floor dB",  "floordb",    &t.floorDb,     -72.0f,  -6.0f,  1.0f,  0 },
            { "Curve",     "curve",      &t.curve,         0.3f,   3.0f,  0.05f, 2 },
            { "Transients","transients", &t.transients,    0.0f,   1.0f,  0.01f, 2 },
            { "Smoothing", "smoothing",  &t.smoothing,     0.0f,   1.0f,  0.01f, 2 },
            { "Width",     "width",      &t.width,         0.05f,  1.0f,  0.01f, 2 },
            { "Gain",      "gain",       &t.gain,          0.1f,   4.0f,  0.05f, 2 },
            // ASCII only. These read "Fill α" in the source and drew as "Fill Î±" on screen,
            // because the label is a const char* and JUCE takes that as the platform's narrow
            // encoding rather than as UTF-8.
            { "Fill %",    "opacity",    &t.opacity,       0.0f,   1.0f,  0.01f, 2 },
            // Its own alpha rather than a share of the fill's: dropping the fill back so the
            // notes read over it is exactly when the edge has to stay solid.
            { "Outline px","linewidth",  &t.outlineWidth,  0.0f,  16.0f,  0.25f, 2 },
            { "Outline %", "linealpha",  &t.outlineOpacity, 0.0f,  1.0f,  0.01f, 2 },
        };

        for (auto& c : controls)
        {
            // A saved value only counts if it is still in range: a slider's limits can move
            // between versions and a stale value outside them would clamp to something the
            // user never chose.
            if (state.hasProperty(propertyFor(c.key)))
                *c.value = juce::jlimit(c.min, c.max, (float)state.getProperty(propertyFor(c.key)));

            // A value saved before this control started snapping, or from a build whose steps
            // differed, has to land on a real step rather than sit between two.
            if (c.snapToPowerOfTwo)
                *c.value = nearestPowerOfTwo(*c.value);

            auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                         juce::Slider::TextBoxRight);
            slider->setRange(c.min, c.max, c.step);
            slider->setNumDecimalPlacesToDisplay(c.decimals);
            slider->setValue(*c.value, juce::dontSendNotification);
            slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(Theme::textWhite));
            slider->setColour(juce::Slider::textBoxBackgroundColourId,
                              juce::Colour(Theme::darkBg).withAlpha(0.0f));
            slider->setColour(juce::Slider::textBoxOutlineColourId,
                              juce::Colour(Theme::panelNeutralLine).withAlpha(0.0f));
            slider->setColour(juce::Slider::thumbColourId, juce::Colour(Theme::coral));
            slider->setColour(juce::Slider::trackColourId, juce::Colour(Theme::coral).withAlpha(0.5f));
            slider->setColour(juce::Slider::backgroundColourId, juce::Colour(Theme::darkBgLighter));

            // The target and its key by value, not a reference into the vector: a reference
            // would outlive nothing today and dangle the moment a control is ever added.
            auto* raw = slider.get();
            float* target = c.value;
            const char* key = c.key;
            const bool snap = c.snapToPowerOfTwo;
            slider->onValueChange = [this, target, key, raw, snap]()
            {
                float chosen = (float)raw->getValue();

                if (snap)
                {
                    chosen = nearestPowerOfTwo(chosen);
                    // Written back so the slider sits where it actually landed. Without the
                    // notification guard this would call straight back into here.
                    raw->setValue(chosen, juce::dontSendNotification);
                }

                *target = chosen;
                state.setProperty(propertyFor(key), *target, nullptr);
            };

            addAndMakeVisible(*slider);
            sliders.push_back(std::move(slider));
        }

        // Normalising is gone: the board's edge is 0dBFS and nothing rescales to fit. Kept as a
        // dead toggle only because this whole panel is slated for deletion (see BACKLOG.md).
        normalizeToggle.setButtonText("Normalize to loudest");
        normalizeToggle.setEnabled(false);
        normalizeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(Theme::textDim));
        normalizeToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(Theme::coral));
        addAndMakeVisible(normalizeToggle);
    }

    // What the panel needs to be to fit its contents, so the editor places it without
    // knowing how many controls there are.
    int preferredHeight() const
    {
        return headerH + (int)controls.size() * rowH + toggleH + padding * 2;
    }

    static constexpr int preferredWidth = 260;

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::panelNeutralBg).withAlpha(Theme::panelBgAlpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), (float)corner);

        g.setColour(juce::Colour(Theme::panelNeutralLine).withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), (float)corner, 1.0f);

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(juce::FontOptions().withHeight((float)headerH * 0.62f));
        g.drawText("Waveform", padding, padding, getWidth() - padding * 2, headerH,
                   juce::Justification::centredLeft);

        g.setFont(juce::FontOptions().withHeight((float)rowH * 0.55f));
        for (size_t i = 0; i < controls.size(); ++i)
        {
            g.setColour(juce::Colour(Theme::textDim));
            g.drawText(controls[i].name,
                       padding, headerH + padding + (int)i * rowH, labelW, rowH,
                       juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        const int sliderX = padding + labelW;
        const int sliderW = getWidth() - sliderX - padding;

        for (size_t i = 0; i < sliders.size(); ++i)
            sliders[i]->setBounds(sliderX, headerH + padding + (int)i * rowH, sliderW, rowH);

        normalizeToggle.setBounds(padding, headerH + padding + (int)sliders.size() * rowH,
                                  getWidth() - padding * 2, toggleH);
    }

private:
    // Namespaced so the panel's values cannot collide with anything else in the tree.
    static juce::Identifier propertyFor(const char* key)
    {
        return juce::Identifier("waveform_" + juce::String(key));
    }

    // Nearest in the geometric sense, so 6 goes to 8 rather than to 4: the steps double, so
    // the midpoint people expect is the geometric one, not the arithmetic one.
    static float nearestPowerOfTwo(float value)
    {
        if (value <= 1.0f) return 1.0f;
        return (float)(1 << juce::jlimit(0, 16, (int)std::round(std::log2(value))));
    }

    static constexpr int padding = 8;
    static constexpr int headerH = 22;
    static constexpr int rowH    = 22;
    static constexpr int toggleH = 24;
    static constexpr int labelW  = 74;
    static constexpr int corner  = 6;

    juce::ValueTree& state;
    std::vector<Control> controls;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    juce::ToggleButton normalizeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformTuningPanel)
};
