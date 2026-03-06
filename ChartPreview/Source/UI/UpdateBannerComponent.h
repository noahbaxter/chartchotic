#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class UpdateBannerComponent : public juce::Component,
                              private juce::Timer
{
public:
    UpdateBannerComponent()
    {
        badge.onClick = [this]() { showPrompt(); };
        badge.setVisible(false);
        addAndMakeVisible(badge);
    }

    void setUpdateInfo(const juce::String& version, const juce::String& url)
    {
        updateVersion = version;
        downloadUrl = url;
        badge.setVisible(true);
        badge.setTooltip("Update " + version + " available");
        startTimerHz(30);
        showPrompt();
    }

    void resized() override
    {
        badge.setBounds(getLocalBounds());
    }

    void parentHierarchyChanged() override
    {
        badge.pulsePhasePtr = &pulsePhase;
    }

private:
    juce::String updateVersion;
    juce::String downloadUrl;
    float pulsePhase = 0.0f;

    void timerCallback() override
    {
        pulsePhase += 0.07f;
        if (pulsePhase > juce::MathConstants<float>::twoPi)
            pulsePhase -= juce::MathConstants<float>::twoPi;
        badge.repaint();
    }

    void showPrompt()
    {
        auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
        if (editor == nullptr) return;

        auto* overlay = new OverlayComponent();
        overlay->setVersion(updateVersion);
        overlay->onDownload = [this, overlay]()
        {
            if (downloadUrl.isNotEmpty())
                juce::URL(downloadUrl).launchInDefaultBrowser();
            delete overlay;
        };
        overlay->onDismiss = [overlay]()
        {
            delete overlay;
        };
        editor->addAndMakeVisible(overlay);
        overlay->setBounds(editor->getLocalBounds());
        overlay->toFront(true);
    }

    //==========================================================================
    struct BadgeComponent : public juce::Component
    {
        std::function<void()> onClick;
        float* pulsePhasePtr = nullptr;

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            float pulse = pulsePhasePtr ? (0.5f + 0.5f * std::sin(*pulsePhasePtr)) : 0.5f;
            auto orange = juce::Colour(Theme::orange);

            float glowAlpha = 0.3f + 0.3f * pulse;
            g.setColour(orange.withAlpha(glowAlpha));
            g.fillEllipse(bounds.expanded(2.0f));

            g.setColour(orange);
            g.fillEllipse(bounds);

            g.setColour(juce::Colours::white);
            g.setFont(Theme::getUIFont(bounds.getHeight() * 0.55f));
            g.drawText("!", bounds, juce::Justification::centred);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
    };

    //==========================================================================
    struct OverlayComponent : public juce::Component
    {
        std::function<void()> onDownload;
        std::function<void()> onDismiss;

        void setVersion(const juce::String& v) { version = v; }

        OverlayComponent()
        {
            setInterceptsMouseClicks(true, true);

            dismissBtn.setButtonText("Dismiss");
            dismissBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
            dismissBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
            dismissBtn.onClick = [this]() { if (onDismiss) onDismiss(); };
            addAndMakeVisible(dismissBtn);

            downloadBtn.setButtonText("Download");
            downloadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::orange).withAlpha(0.15f));
            downloadBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::orange));
            downloadBtn.onClick = [this]() { if (onDownload) onDownload(); };
            addAndMakeVisible(downloadBtn);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.6f));

            auto card = getCardBounds();

            g.setColour(juce::Colour(Theme::darkBg));
            g.fillRoundedRectangle(card, 6.0f);

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(card, 6.0f, 1.0f);

            auto inner = card;
            auto titleArea = inner.removeFromTop(inner.getHeight() * 0.38f).reduced(16.0f, 8.0f);
            g.setColour(juce::Colour(Theme::orange));
            g.setFont(Theme::getUIFont(14.0f));
            g.drawText("Update Available", titleArea, juce::Justification::centredBottom);

            auto msgArea = inner.removeFromTop(inner.getHeight() * 0.35f).reduced(16.0f, 0.0f);
            g.setColour(juce::Colour(Theme::textDim));
            g.setFont(Theme::getUIFont(11.0f));
            g.drawText("Chart Preview " + version + " is out.", msgArea, juce::Justification::centredTop);
        }

        void resized() override
        {
            auto card = getCardBounds();
            auto buttonArea = card.removeFromBottom(card.getHeight() * 0.38f).reduced(16.0f, 8.0f);
            int btnW = ((int)buttonArea.getWidth() - 8) / 2;
            int btnH = juce::jlimit(22, 30, (int)buttonArea.getHeight() - 8);
            int btnY = (int)buttonArea.getY() + ((int)buttonArea.getHeight() - btnH) / 2;

            dismissBtn.setBounds((int)buttonArea.getX(), btnY, btnW, btnH);
            downloadBtn.setBounds((int)buttonArea.getX() + btnW + 8, btnY, btnW, btnH);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!getCardBounds().contains(e.getPosition().toFloat()))
                if (onDismiss) onDismiss();
        }

    private:
        juce::String version;
        juce::TextButton dismissBtn, downloadBtn;

        juce::Rectangle<float> getCardBounds() const
        {
            float cardW = juce::jlimit(200.0f, 280.0f, getWidth() * 0.38f);
            float cardH = juce::jlimit(110.0f, 160.0f, getHeight() * 0.28f);
            return juce::Rectangle<float>(cardW, cardH)
                .withCentre(getLocalBounds().getCentre().toFloat());
        }
    };

    BadgeComponent badge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateBannerComponent)
};
