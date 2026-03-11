#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class ChartchoticLogo : public juce::Component
{
public:
    ChartchoticLogo() { setPaintingIsUnclipped(true); }

    void setFontSize(float size)
    {
        if (fontSize == size) return;
        fontSize = size;
        repaint();
    }

    float getIdealWidth() const
    {
        auto fonts = getFonts();
        juce::Font boldFont(fonts.bold);
        boldFont.setHeight(fontSize);
        juce::Font medFont(fonts.medium);
        medFont.setHeight(fontSize);

        float chartW = boldFont.getStringWidthFloat("CHART");
        float choticW = medFont.getStringWidthFloat("CHOTIC");
        float dotClusterW = getDotClusterWidth();
        float gapW = fontSize * logoGapRatio * 2.0f;

        return chartW + gapW + dotClusterW + choticW;
    }

    void paint(juce::Graphics& g) override
    {
        if (fontSize < 1.0f) return;

        auto fonts = getFonts();
        juce::Font boldFont(fonts.bold);
        boldFont.setHeight(fontSize);
        juce::Font medFont(fonts.medium);
        medFont.setHeight(fontSize);

        float chartW = boldFont.getStringWidthFloat("CHART");
        float choticW = medFont.getStringWidthFloat("CHOTIC");
        float dotClusterW = getDotClusterWidth();
        float gapW = fontSize * logoGapRatio;

        float totalW = chartW + gapW + dotClusterW + gapW + choticW;
        float x = ((float)getWidth() - totalW) * 0.5f;
        float cy = (float)getHeight() * 0.5f;

        // "CHART" — white bold
        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(boldFont);
        g.drawText("CHART", juce::Rectangle<float>(x, 0.0f, chartW, (float)getHeight()),
                   juce::Justification::centredLeft, false);
        x += chartW + gapW;

        // Dot cluster — vertically centered, nudged left under the T
        drawDotCluster(g, x + fontSize * dotNudge, cy);
        x += dotClusterW + gapW;

        // "CHOTIC" — red-orange medium
        g.setColour(juce::Colour(Theme::coral));
        g.setFont(medFont);
        g.drawText("CHOTIC", juce::Rectangle<float>(x, 0.0f, choticW, (float)getHeight()),
                   juce::Justification::centredLeft, false);
    }

    float logoGapRatio = 0.1f;    // gap between text and dots (debug-tunable)
    float dotNudge = -0.04f;      // dot cluster X nudge as ratio of fontSize (negative = left)

private:
    float fontSize = 13.0f;
    static constexpr float dotSizeRatio = 0.16f;  // dot diameter relative to fontSize
    static constexpr float dotGapRatio = 0.065f;   // gap between dots relative to fontSize

    static constexpr juce::uint32 dotColors[6] = {
        Theme::green,
        Theme::red,
        Theme::yellow,
        Theme::blue,
        Theme::orange,
        Theme::textWhite  // Star power
    };

    struct FontPair
    {
        juce::Font bold;
        juce::Font medium;
    };

    FontPair getFonts() const
    {
        static auto boldTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::TurretRoadBold_ttf,
            BinaryData::TurretRoadBold_ttfSize);
        static auto mediumTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::TurretRoadMedium_ttf,
            BinaryData::TurretRoadMedium_ttfSize);

        juce::Font bold = boldTypeface != nullptr
            ? juce::Font(boldTypeface) : juce::Font(fontSize, juce::Font::bold);
        juce::Font med = mediumTypeface != nullptr
            ? juce::Font(mediumTypeface) : juce::Font(fontSize);

        return { bold, med };
    }

    float getDotClusterWidth() const
    {
        float dotSize = fontSize * dotSizeRatio;
        float dotGap = fontSize * dotGapRatio;
        return dotSize * 3.0f + dotGap * 2.0f;
    }

    void drawDotCluster(juce::Graphics& g, float x, float cy) const
    {
        float dotSize = fontSize * dotSizeRatio;
        float dotGap = fontSize * dotGapRatio;
        float clusterH = dotSize * 2.0f + dotGap;
        float top = cy - clusterH * 0.5f;

        // 3 columns x 2 rows: [G R Y] / [B O dark]
        for (int row = 0; row < 2; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                int idx = row * 3 + col;
                float dx = x + (float)col * (dotSize + dotGap);
                float dy = top + (float)row * (dotSize + dotGap);
                g.setColour(juce::Colour(dotColors[idx]));
                g.fillEllipse(dx, dy, dotSize, dotSize);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChartchoticLogo)
};
