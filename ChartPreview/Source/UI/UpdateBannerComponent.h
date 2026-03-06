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
        badge.pulsePhasePtr = &pulsePhase;
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

    bool hasUpdate() const { return updateVersion.isNotEmpty(); }
    const juce::String& getDownloadUrl() const { return downloadUrl; }
    void setBadgeHovered(bool h) { badge.setHovered(h); }

    void showPrompt()
    {
        auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
        if (editor == nullptr) return;

        auto* overlay = new OverlayComponent();
        overlay->setVersion(updateVersion);
        auto urlCopy = downloadUrl.isNotEmpty() ? downloadUrl
            : juce::String("https://github.com/noahbaxter/chart-preview/releases");
        overlay->onDownload = [overlay, urlCopy]()
        {
            // Defer deletion — button is a child of overlay, can't delete mid-callback
            juce::MessageManager::callAsync([overlay, urlCopy]()
            {
                delete overlay;
                juce::URL(urlCopy).launchInDefaultBrowser();
            });
        };
        overlay->onDismiss = [overlay]()
        {
            juce::MessageManager::callAsync([overlay]() { delete overlay; });
        };
        editor->addAndMakeVisible(overlay);
        overlay->setBounds(editor->getLocalBounds());
        overlay->toFront(true);
    }

    void resized() override
    {
        badge.setBounds(getLocalBounds());
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

    //==========================================================================
    struct BadgeComponent : public juce::Component,
                           public juce::SettableTooltipClient
    {
        std::function<void()> onClick;
        float* pulsePhasePtr = nullptr;
        bool hovered = false;

        void setHovered(bool h) { if (hovered != h) { hovered = h; repaint(); } }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            float pulse = pulsePhasePtr ? (0.5f + 0.5f * std::sin(*pulsePhasePtr)) : 0.5f;
            auto orange = juce::Colour(Theme::orange);

            auto coral = juce::Colour(Theme::coral);
            if (hovered)
            {
                g.setColour(coral.withAlpha(0.5f));
                g.fillEllipse(bounds.expanded(2.0f));
                g.setColour(coral);
            }
            else
            {
                float glowAlpha = 0.1f + 0.1f * pulse;
                g.setColour(coral.withAlpha(glowAlpha));
                g.fillEllipse(bounds.expanded(2.0f));
                g.setColour(coral.withAlpha(0.5f));
            }
            g.fillEllipse(bounds);

            g.setColour(juce::Colour(Theme::textWhite));
            g.setFont(Theme::getUIFont(bounds.getHeight() * 0.5f));
            g.drawText("!", bounds, juce::Justification::centred);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            setHovered(true);
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            setHovered(false);
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

            dismissBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
            dismissBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
            dismissBtn.onClick = [this]() { if (onDismiss) onDismiss(); };
            addAndMakeVisible(dismissBtn);

            downloadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::orange).withAlpha(0.15f));
            downloadBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::orange));
            downloadBtn.onClick = [this]() { if (onDownload) onDownload(); };
            addAndMakeVisible(downloadBtn);
        }

        void paint(juce::Graphics& g) override
        {
            float s = getHeight() / 600.0f;

            g.fillAll(juce::Colours::black.withAlpha(Theme::overlayDimAlpha));

            auto card = getCardBounds();

            g.setColour(juce::Colour(Theme::darkBg));
            g.fillRoundedRectangle(card, Theme::cardRadius);

            g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
            g.drawRoundedRectangle(card, Theme::cardRadius, 1.0f);

            auto inner = card;
            float pad = 28.0f * s;
            auto titleArea = inner.removeFromTop(inner.getHeight() * 0.36f).reduced(pad, pad * 0.3f);
            g.setColour(juce::Colour(Theme::orange));
            g.setFont(Theme::getUIFont(24.0f * s));
            g.drawText("Update Available", titleArea, juce::Justification::centredBottom);

            auto msgArea = inner.removeFromTop(inner.getHeight() * 0.32f).reduced(pad, 0.0f);
            g.setColour(juce::Colour(Theme::textWhite));
            g.setFont(Theme::getUIFont(16.0f * s));
            g.drawText("Chart Preview " + version + " is out.", msgArea, juce::Justification::centredTop);
        }

        void resized() override
        {
            float s = getHeight() / 600.0f;

            auto card = getCardBounds();
            float pad = 28.0f * s;
            auto buttonArea = card.removeFromBottom(card.getHeight() * 0.36f).reduced(pad, pad * 0.3f);
            int btnGap = juce::roundToInt(12.0f * s);
            int btnW = ((int)buttonArea.getWidth() - btnGap) / 2;
            int btnH = juce::roundToInt(36.0f * s);
            int btnY = (int)buttonArea.getY() + ((int)buttonArea.getHeight() - btnH) / 2;

            dismissBtn.setButtonText("Dismiss");
            dismissBtn.setBounds((int)buttonArea.getX(), btnY, btnW, btnH);
            downloadBtn.setButtonText("Download");
            downloadBtn.setBounds((int)buttonArea.getX() + btnW + btnGap, btnY, btnW, btnH);
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
            float s = getHeight() / 600.0f;
            float cardW = juce::jlimit(280.0f * s, 420.0f * s, getWidth() * 0.48f);
            float cardH = juce::jlimit(160.0f * s, 240.0f * s, getHeight() * 0.35f);
            return juce::Rectangle<float>(cardW, cardH)
                .withCentre(getLocalBounds().getCentre().toFloat());
        }
    };

    BadgeComponent badge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateBannerComponent)
};
