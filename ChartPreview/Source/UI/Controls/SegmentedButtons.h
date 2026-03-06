#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// A horizontal group of connected buttons where exactly one is selected.
// Selected button gets coral fill, others are dark with subtle borders.
// Looks like a single rounded-rect bar with internal dividers.
class SegmentedButtons : public juce::Component
{
public:
    SegmentedButtons() {}

    void setItems(const juce::StringArray& labels)
    {
        items = labels;
        repaint();
    }

    void setSelectedIndex(int index, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (selectedIndex == index) return;
        selectedIndex = index;
        repaint();
        if (notification != juce::dontSendNotification && onSelectionChanged)
            onSelectionChanged(selectedIndex);
    }

    int getSelectedIndex() const { return selectedIndex; }

    // Returns 1-based ID to match ComboBox convention
    int getSelectedId() const { return selectedIndex + 1; }

    std::function<void(int index)> onSelectionChanged;

    void paint(juce::Graphics& g) override
    {
        if (items.isEmpty()) return;

        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        auto cornerSize = bounds.getHeight() * Theme::pillRadius;
        int count = items.size();
        float segW = bounds.getWidth() / (float)count;

        // Overall background
        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(bounds, cornerSize);

        // Draw each segment
        for (int i = 0; i < count; i++)
        {
            auto segBounds = juce::Rectangle<float>(bounds.getX() + i * segW, bounds.getY(),
                                                     segW, bounds.getHeight());
            bool selected = (i == selectedIndex);
            bool hovering = (i == hoverIndex && !selected);

            if (selected)
            {
                // Clip to rounded rect for first/last segments
                juce::Path clip;
                if (count == 1)
                {
                    clip.addRoundedRectangle(segBounds, cornerSize);
                }
                else if (i == 0)
                {
                    clip.addRoundedRectangle(segBounds.getX(), segBounds.getY(),
                                              segBounds.getWidth(), segBounds.getHeight(),
                                              cornerSize, cornerSize, true, false, true, false);
                }
                else if (i == count - 1)
                {
                    clip.addRoundedRectangle(segBounds.getX(), segBounds.getY(),
                                              segBounds.getWidth(), segBounds.getHeight(),
                                              cornerSize, cornerSize, false, true, false, true);
                }
                else
                {
                    clip.addRectangle(segBounds);
                }

                g.saveState();
                g.reduceClipRegion(clip);
                g.setColour(juce::Colour(Theme::coral));
                g.fillRect(segBounds);
                g.restoreState();

                g.setColour(juce::Colours::white);
            }
            else
            {
                if (hovering)
                {
                    g.setColour(juce::Colour(Theme::darkBgHover));

                    juce::Path clip;
                    if (i == 0)
                        clip.addRoundedRectangle(segBounds.getX(), segBounds.getY(),
                                                  segBounds.getWidth(), segBounds.getHeight(),
                                                  cornerSize, cornerSize, true, false, true, false);
                    else if (i == count - 1)
                        clip.addRoundedRectangle(segBounds.getX(), segBounds.getY(),
                                                  segBounds.getWidth(), segBounds.getHeight(),
                                                  cornerSize, cornerSize, false, true, false, true);
                    else
                        clip.addRectangle(segBounds);

                    g.saveState();
                    g.reduceClipRegion(clip);
                    g.fillRect(segBounds);
                    g.restoreState();
                }

                g.setColour(juce::Colour(Theme::textDim));
            }

            g.setFont(juce::Font(bounds.getHeight() * 0.393f));
            g.drawText(items[i], segBounds.toNearestInt(), juce::Justification::centred);

            // Divider line between segments (not after last)
            if (i < count - 1 && i != selectedIndex && i + 1 != selectedIndex)
            {
                float divX = segBounds.getRight();
                g.setColour(juce::Colour(Theme::textDim).withAlpha(0.3f));
                g.drawLine(divX, bounds.getY() + 4.0f, divX, bounds.getBottom() - 4.0f, 0.5f);
            }
        }

        // Overall outline
        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition()) || items.isEmpty()) return;

        int count = items.size();
        float segW = (float)getWidth() / (float)count;
        int clicked = juce::jlimit(0, count - 1, (int)(e.x / segW));

        if (clicked != selectedIndex)
        {
            selectedIndex = clicked;
            repaint();
            if (onSelectionChanged) onSelectionChanged(selectedIndex);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (items.isEmpty()) return;
        int count = items.size();
        float segW = (float)getWidth() / (float)count;
        int newHover = juce::jlimit(0, count - 1, (int)(e.x / segW));
        if (newHover != hoverIndex) { hoverIndex = newHover; repaint(); }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverIndex >= 0) { hoverIndex = -1; repaint(); }
    }

private:
    juce::StringArray items;
    int selectedIndex = 0;
    int hoverIndex = -1;
};
