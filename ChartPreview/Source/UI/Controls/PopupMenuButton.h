#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// A panel that appears below a button and holds arbitrary child controls.
// Dismisses when clicking outside via a desktop-level mouse listener.
class PopupPanel : public juce::Component
{
public:
    PopupPanel() { setAlwaysOnTop(true); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }
};

// A button that toggles a PopupPanel below itself.
// Uses a separate mouse listener to detect clicks outside for dismissal.
class PopupMenuButton : public juce::TextButton
{
public:
    PopupMenuButton(const juce::String& buttonText) : juce::TextButton(buttonText) {}

    ~PopupMenuButton() override { dismissPanel(); }

    void addPanelChild(juce::Component* child) { panelChildren.push_back(child); }

    void setPanelSize(int w, int h) { panelWidth = w; panelHeight = h; }

    bool isPanelVisible() const { return panel != nullptr && panel->isVisible(); }

    void showPanel()
    {
        if (panel != nullptr) { dismissPanel(); return; }

        panel = std::make_unique<PopupPanel>();
        panel->setSize(panelWidth, panelHeight);

        for (auto* child : panelChildren)
            panel->addAndMakeVisible(child);

        if (onLayoutPanel)
            onLayoutPanel(panel.get());

        auto screenPos = localPointToGlobal(juce::Point<int>(getWidth(), getHeight()));
        panel->setTopLeftPosition(screenPos.x - panelWidth, screenPos.y + 2);
        panel->setLookAndFeel(&getLookAndFeel());
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);
        panel->toFront(true);

        dismissListener = std::make_unique<DismissListener>(*this);
        juce::Desktop::getInstance().addGlobalMouseListener(dismissListener.get());

        setToggleState(true, juce::dontSendNotification);
        repaint();
    }

    void dismissPanel()
    {
        if (dismissListener != nullptr)
        {
            juce::Desktop::getInstance().removeGlobalMouseListener(dismissListener.get());
            dismissListener.reset();
        }
        if (panel != nullptr)
        {
            for (auto* child : panelChildren)
                panel->removeChildComponent(child);
            panel.reset();
        }

        setToggleState(false, juce::dontSendNotification);
        repaint();
    }

    void relayoutPanel()
    {
        if (panel != nullptr && onLayoutPanel)
        {
            onLayoutPanel(panel.get());
            panel->repaint();
        }
    }

    std::function<void(juce::Component* panel)> onLayoutPanel;

    void clicked() override { showPanel(); }

private:
    struct DismissListener : public juce::MouseListener
    {
        PopupMenuButton& owner;
        DismissListener(PopupMenuButton& o) : owner(o) {}

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (owner.panel == nullptr) return;
            auto pos = e.getScreenPosition();
            if (!owner.panel->getScreenBounds().contains(pos) && !owner.getScreenBounds().contains(pos))
                juce::MessageManager::callAsync([this]() { owner.dismissPanel(); });
        }
    };

    std::unique_ptr<DismissListener> dismissListener;
    std::unique_ptr<PopupPanel> panel;
    std::vector<juce::Component*> panelChildren;
    int panelWidth = 160;
    int panelHeight = 200;
};
