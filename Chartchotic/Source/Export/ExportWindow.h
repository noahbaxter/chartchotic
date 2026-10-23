#pragma once

#include <JuceHeader.h>
#include <memory>

#include "ExportDialogComponent.h"

/**
    A real window for the export dialog.

    It started as an overlay inside the plugin editor, which was wrong: the
    editor is whatever size the host gives it, often docked and narrow, and the
    export UI is a song list plus two columns of fields that simply does not
    fit inside it. An overlay can only ever scale down until it is unusable.

    A window of its own is also the honest shape for the job: you are setting
    up an album, not adjusting the highway.

    One size, and it does not resize. Everything in here is sized to what goes
    in it: a year is four characters, a cover is a cover, and a song list is a
    song list. None of that wants more room when the window is dragged wider,
    so a resizable window only ever produced empty space and a layout with
    four more ways to be wrong.

    Kept above the plugin editor, because REAPER's FX window floats and a
    window that opens behind it looks like nothing happened.
*/
class ExportWindow : public juce::DocumentWindow
{
public:
    ExportWindow(ExportDialogComponent::Context context, std::function<void()> onClosed)
        : juce::DocumentWindow("Export Chart",
                               juce::Colour(Theme::darkBg),
                               juce::DocumentWindow::closeButton),
          closed(std::move(onClosed))
    {
        auto* content = new ExportDialogComponent(std::move(context));
        dialog = content;

        setUsingNativeTitleBar(true);
        setContentOwned(content, true);
        setResizable(false, false);
        setAlwaysOnTop(true);
        centreWithSize(ExportDialogComponent::kWidth, ExportDialogComponent::kHeight);
        setVisible(true);
    }

    /** The dialog inside, for wiring up its callbacks. */
    ExportDialogComponent& content() { return *dialog; }

    // The title bar's close button, which has to behave exactly like Cancel.
    void closeButtonPressed() override { if (closed) closed(); }

private:
    ExportDialogComponent* dialog = nullptr;
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportWindow)
};
