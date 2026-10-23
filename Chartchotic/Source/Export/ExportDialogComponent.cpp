#include "ExportDialogComponent.h"

#include "ChartSettings.h"

namespace
{
    // Chart difficulty ratings run 0 to 6. -1 is the unrated marker, and it is
    // deliberately not reachable by stepping: getting there by accident is how
    // a chart ships unrated.
    constexpr int kMinRating = 0;
    constexpr int kMaxRating = 6;

    const char* const kArtExtensions[] = { ".png", ".jpg", ".jpeg", nullptr };

    /**
        The whole of the window's geometry, in one place.

        Every number the layout uses is here. Nothing below writes a bare
        literal into a rect: ten rounds of nudging one section at a time is
        what produced the fourteen scattered constants this replaces, and the
        only way that does not happen again is for there to be nowhere to put
        a nudge except this block.
    */
    namespace Layout
    {
        /**
            Two dials over the whole window.

            Text and space pull against each other: bigger type needs more
            room to breathe, and trimming the room is what stops the window
            growing to hold it. Keeping them apart means one can go up while
            the other comes down, which is exactly the adjustment this window
            keeps wanting, and it is one edit rather than forty.
        */
        constexpr float textScale  = 1.14f;
        constexpr float spaceScale = 0.85f;

        constexpr int space(int base) { return (int)((float)base * spaceScale + 0.5f); }
        constexpr float type(float base) { return base * textScale; }

        /** A row whose text is sized off its own height, so it scales with type. */
        constexpr int textRow(int base) { return (int)((float)base * textScale + 0.5f); }

        constexpr int pad        = space(18);
        constexpr int gapXS      = space(4);
        constexpr int gapS       = space(8);
        constexpr int gapM       = space(12);
        constexpr int sectionGap = space(16);

        constexpr int titleHeight   = textRow(26);
        constexpr int headingHeight = textRow(15);
        constexpr int lineHeight    = textRow(18);

        /** A metadata row. Shorter than a control: it is a line of text. */
        constexpr int fieldHeight   = textRow(26);

        /** Anything you press. */
        constexpr int controlHeight = textRow(30);
        constexpr int buttonHeight  = textRow(32);

        /**
            The charter row, which is taller than the rows above it. It carries
            the icon, and an icon the size of a line of text is not a picture
            anyone can recognise.
        */
        constexpr int charterHeight = 34;

        constexpr int listRowHeight = textRow(36);

        /** The top strip: one lane of regions over its ruler. */
        constexpr int stripHeight  = 26;
        constexpr int rulerHeight  = 14;

        /** The time selection, as a bar under the lane it covers. */
        constexpr int selectionRule = 3;

        /** Fixed, not a share of the width. A song list does not need to grow. */
        constexpr int railWidth   = 248;

        /** The cover, and the background is the same width at 16:9. */
        constexpr int artWidth = 300;

        /** The icon preview: its thumbnail, and the name it resolved to. */
        constexpr int iconPreviewWidth = 158;

        /** Right-aligned label gutter, and the gap to the box beside it. */
        constexpr int labelWidth  = 58;
        constexpr int labelGap    = 8;

        /**
            One box width for every tag, whatever goes in it.

            A column of boxes that all stop in the same place reads as a list
            of values; boxes that each stop somewhere different read as a form
            that has been filled in badly. The short values get their own row
            shared between them instead of their own width.
        */
        constexpr int boxMeta   = 382;
        constexpr int boxNumber = 56;
        constexpr int boxRating = 48;

        /** Labels on the second and third field of a shared row, mid-row. */
        constexpr int labelTrack = 46;
        constexpr int labelGenre = 52;

        constexpr int exportWidth = 148;
        constexpr int cancelWidth = 92;

        /** The hover pencil, which is a glyph and not a button-sized thing. */
        constexpr int pencilSize = 22;

        /** The charter icon in the collapsed bar. A picture, with no room for
            the message the preview shows beside it when it is open. */
        constexpr int charterIcon = 30;

        /** The tick column, sized to the box rather than to the row. */
        constexpr int tickColumn = 30;

        /** The row being edited: a wash of coral, and a bar down its edge. */
        constexpr float selectedTint = 0.14f;
        constexpr int   selectedEdge = 3;

        constexpr float panelRadius = 4.0f;

        constexpr float fontTitle   = type(20.0f);
        constexpr float fontBody    = type(11.5f);
        constexpr float fontSmall   = type(10.5f);
        constexpr float fontTiny    = type(9.5f);
    }

    using namespace Layout;

    /**
        Give a checkbox exactly the room its label needs, and take it off the row.

        Its width depends on its height, so the height goes on first and the
        width is asked for afterwards. Handing it a round number instead is
        what turned "5-lane drums" into "5-lane dr...".
    */
    void placeToggle(CheckboxToggle& toggle, juce::Rectangle<int>& row, int height)
    {
        toggle.setBounds(row.getX(), row.getY() + (row.getHeight() - height) / 2, 1, height);
        toggle.setSize(toggle.preferredWidth(), height);
        row.removeFromLeft(toggle.getWidth());
    }

    /** How long ago, in the roughest terms that are still useful. */
    juce::String exportedWhen(const juce::File& file)
    {
        const auto age = juce::Time::getCurrentTime() - file.getLastModificationTime();
        if (age.inMinutes() < 1)  return "just now";
        if (age.inHours() < 1)    return juce::String((int)age.inMinutes()) + "m ago";
        if (age.inDays() < 1)     return juce::String((int)age.inHours()) + "h ago";
        return juce::String((int)age.inDays()) + "d ago";
    }

    /**
        Where generated backgrounds wait until the chart is written.

        The exporter copies artwork into the chart folder, so these are
        staging and nothing else. The dialog clears the folder on the way in
        and on the way out: they are cheap to remake and leaving them behind
        litters the temp directory with every background ever previewed.
    */
    juce::File backgroundStagingFolder()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("chartchotic-backgrounds");
    }

    /**
        What a chart already on disk says about itself.

        song.ini is "key = value" a line at a time, so it is read here rather
        than through PropertiesFile, which would want to own the file.
    */
    juce::StringPairArray readExportedIni(const juce::File& chart)
    {
        juce::StringPairArray values;

        // A .sng keeps its metadata inside the container, which means parsing
        // the index to reach it. Only loose folders are read for now.
        auto ini = chart.getChildFile("song.ini");
        if (!ini.existsAsFile()) return values;

        for (const auto& line : juce::StringArray::fromLines(ini.loadFileAsString()))
        {
            const auto trimmed = line.trim();
            if (trimmed.startsWith("[") || !trimmed.contains("=")) continue;

            values.set(trimmed.upToFirstOccurrenceOf("=", false, false).trim().toLowerCase(),
                       trimmed.fromFirstOccurrenceOf("=", false, false).trim());
        }

        return values;
    }

    /** Where a song starts, in the form the REAPER ruler shows it. */
    juce::String timecodeOf(double seconds)
    {
        const int whole = (int)std::max(0.0, seconds);
        return juce::String(whole / 60) + ":" + juce::String(whole % 60).paddedLeft('0', 2);
    }

    /** First album.png / background.jpg style file found under any search path. */
    juce::File findArt(const juce::Array<juce::File>& paths, const juce::String& stem)
    {
        for (const auto& dir : paths)
        {
            if (!dir.isDirectory()) continue;
            for (int i = 0; kArtExtensions[i] != nullptr; ++i)
            {
                auto candidate = dir.getChildFile(stem + kArtExtensions[i]);
                if (candidate.existsAsFile()) return candidate;
            }
        }
        return {};
    }

    // Album-wide values live on the root; each song hangs off it under its
    // region GUID, so renaming or dragging a region keeps its settings.
    const juce::Identifier kRemembered { "exportDialog" };
    const juce::Identifier kSong { "song" };
    const juce::Identifier kGuid { "guid" };
    const juce::Identifier kTitle { "title" }, kArtist { "artist" }, kAlbum { "album" };
    const juce::Identifier kGenre { "genre" }, kYear { "year" }, kTrack { "track" };
    const juce::Identifier kDiffDrums { "diffDrums" }, kDiffPro { "diffPro" };
    const juce::Identifier kDiffElite { "diffElite" };
    const juce::Identifier kProDrums { "proDrums" }, kFiveLane { "fiveLane" };
    const juce::Identifier kSelected { "selected" };
    const juce::Identifier kAlbumArt { "albumArt" }, kBackgroundArt { "backgroundArt" };
    const juce::Identifier kPackAsSng { "packAsSng" }, kAudioFormat { "audioFormat" };
    const juce::Identifier kDestination { "destination" };
    const juce::Identifier kDrumTypeForced { "drumTypeForced" };
    const juce::Identifier kRenderAudio { "renderAudio" };

    /** Where the song sat, so it can be found again if its GUID cannot. */
    const juce::Identifier kStart { "startSec" }, kEnd { "endSec" };

    /**
        Identity for the song that is a bare time selection rather than a
        region. It needs one: without a key nothing about it is saved, so the
        difficulty you just typed is gone the moment the window closes.
    */
    const juce::String kSelectionKey { "selection" };
}

//==============================================================================
void ExportDialogComponent::MixedTextEditor::paintOverChildren(juce::Graphics& g)
{
    if (!mixed || !isEmpty()) return;

    // Squeezed rather than truncated: a year box is four characters wide and
    // was rendering the marker as "mi...", which says nothing at all.
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getMixedFont(getFont().getHeight()));
    g.drawFittedText(Theme::mixedText, getLocalBounds().reduced(5, 0),
                     juce::Justification::centredLeft, 1, 0.4f);
}

//==============================================================================
ExportDialogComponent::Field::Field(const juce::String& labelText, bool numeric)
    : label(labelText)
{
    // Barely there until it is focused. A tag editor is a list of values you
    // read down, not a stack of boxes, and an outline on every row is what
    // made six short values look like a form to fill in.
    editor.setColour(juce::TextEditor::backgroundColourId,
                     juce::Colour(Theme::darkBgLighter).withAlpha(0.5f));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::coral));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
    editor.setColour(juce::TextEditor::highlightColourId, juce::Colour(Theme::coral).withAlpha(0.3f));
    if (numeric)
        editor.setInputRestrictions(4, "0123456789");

    // Down the middle of the box. A single-line editor sits its text at the
    // top by default, which on a row taller than the text reads as though the
    // value is falling out of the box.
    editor.setJustification(juce::Justification::centredLeft);
    addAndMakeVisible(editor);

    applyAll.onClick = [this]() { if (onApplyToAll) onApplyToAll(); };
    addChildComponent(applyAll);
}

void ExportDialogComponent::Field::paint(juce::Graphics& g)
{
    auto labelArea = getLocalBounds().removeFromLeft(labelWidth);
    if (labelRight) labelArea.removeFromRight(labelGap);

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.42f));
    g.drawText(label, labelArea,
               labelRight ? juce::Justification::centredRight
                          : juce::Justification::centredLeft);
}

void ExportDialogComponent::Field::refreshFont()
{
    const float height = (float)getHeight() * 0.46f;

    // The same italic the mixed marker uses, since it is the same claim: what
    // is in the box is not yet true of everything selected.
    editor.applyFontToAllText(editor.partial ? Theme::getMixedFont(height)
                                             : Theme::getUIFont(height));
    editor.repaint();
}

void ExportDialogComponent::Field::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(labelWidth);

    // Sized to the value where one is asked for, and only filling what is
    // left when nothing better is known.
    if (boxWidth > 0)
        bounds = bounds.removeFromLeft(boxWidth);

    editor.setBounds(bounds);

    // Inside the right end of the box, not past it: placed outside, it fell
    // beyond this component and was clipped to a sliver. Values that reach a
    // tick are short enough that overlapping their tail costs nothing.
    applyAll.setBounds(bounds.removeFromRight(getHeight()));

    refreshFont();
}

//==============================================================================
ExportDialogComponent::TimelineStrip::TimelineStrip()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void ExportDialogComponent::TimelineStrip::mouseDown(const juce::MouseEvent& e)
{
    if (endSec <= 0.0 || onSelect == nullptr) return;

    // Back through the same mapping paint() uses, rather than hit-testing
    // rectangles it would then have to keep in step with.
    auto lane = getLocalBounds().withTrimmedBottom(rulerHeight);
    if (lane.getWidth() <= 0) return;

    const double at = endSec * (double)(e.x - lane.getX()) / (double)lane.getWidth();

    for (const auto& block : blocks)
        if (at >= block.startSec && at < block.endSec && block.song >= 0)
        {
            onSelect(block.song, e.mods);
            return;
        }
}

void ExportDialogComponent::TimelineStrip::setBlocks(std::vector<Block> newBlocks,
                                                     const ChartExporter::TimeRange& newSelection)
{
    blocks = std::move(newBlocks);
    selection = newSelection;

    // The project runs to whatever ends last, the selection included, so
    // dragging a selection past the last region does not push it off the end.
    endSec = selection.endSec;
    for (const auto& block : blocks)
        endSec = juce::jmax(endSec, block.endSec);

    repaint();
}

void ExportDialogComponent::TimelineStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto ruler = bounds.removeFromBottom(rulerHeight);
    auto lane = bounds;

    g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.6f));
    g.fillRoundedRectangle(lane.toFloat(), 2.0f);

    if (endSec <= 0.0) return;

    auto xFor = [&lane, this](double seconds)
    {
        return (float)lane.getX()
             + (float)(seconds / endSec) * (float)lane.getWidth();
    };

    for (const auto& block : blocks)
    {
        // A one-pixel region is still worth seeing, so a short one is widened
        // rather than rounded away.
        const float left = xFor(block.startSec);
        const float width = juce::jmax(2.0f, xFor(block.endSec) - left);
        auto rect = juce::Rectangle<float>(left, (float)lane.getY(), width, (float)lane.getHeight())
                        .reduced(1.0f);

        g.setColour(juce::Colour(block.current ? Theme::coral
                                 : block.ticked ? Theme::darkBgHover
                                                : Theme::darkBg).withAlpha(block.ticked ? 1.0f : 0.8f));
        g.fillRoundedRectangle(rect, 2.0f);

        g.setColour(juce::Colours::white.withAlpha(block.current ? 0.35f : Theme::borderAlpha));
        g.drawRoundedRectangle(rect, 2.0f, 1.0f);

        // Names only where one fits. A truncated word in a 12 pixel block
        // says less than the block itself does.
        if (rect.getWidth() < 34.0f) continue;

        g.setColour(juce::Colour(block.current ? Theme::textWhite : Theme::textDim));
        g.setFont(Theme::getUIFont(fontSmall));
        g.drawText(block.name, rect.reduced((float)gapXS, 0.0f).toNearestInt(),
                   juce::Justification::centredLeft, true);
    }

    // What the timeline is showing, as a rule under the lane rather than a
    // wash behind it: drawn behind, it came through the regions it crossed
    // and read as though those regions were half selected.
    if (selection.exists())
    {
        g.setColour(juce::Colour(Theme::coral));
        g.fillRect(juce::Rectangle<float>(
            xFor(selection.startSec), (float)ruler.getY(),
            juce::jmax(2.0f, xFor(selection.endSec) - xFor(selection.startSec)),
            (float)selectionRule));
    }

    g.setColour(juce::Colour(Theme::textDim).withAlpha(0.6f));
    g.setFont(Theme::getUIFont(fontTiny));
    g.drawText(timecodeOf(0.0), ruler, juce::Justification::bottomLeft);
    g.drawText(timecodeOf(endSec), ruler, juce::Justification::bottomRight);
}

//==============================================================================
ExportDialogComponent::ArtSlot::ArtSlot(const juce::String& labelText)
    : label(labelText)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    applyAll.onClick = [this]() { if (onApplyToAll) onApplyToAll(); };
    addChildComponent(applyAll);
}

void ExportDialogComponent::ArtSlot::resized()
{
    // Over the corner of the picture opposite the clear badge, so the offer
    // and the discard never sit on top of one another.
    auto preview = previewBounds();
    applyAll.setBounds(preview.removeFromBottom(applyAllSize).removeFromRight(applyAllSize));
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::previewBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(headingHeight);
    bounds.removeFromBottom(lineHeight);          // filename
    return bounds;
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::clearBounds() const
{
    return previewBounds().removeFromTop(22).removeFromRight(22).reduced(3);
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::optionsBounds() const
{
    return previewBounds().removeFromTop(22).removeFromLeft(22).reduced(3);
}

void ExportDialogComponent::ArtSlot::mouseMove(const juce::MouseEvent& e)
{
    const bool over = file.existsAsFile() && clearBounds().contains(e.getPosition());
    if (over == overClear) return;
    overClear = over;
    repaint();
}

void ExportDialogComponent::ArtSlot::mouseDown(const juce::MouseEvent& e)
{
    if (!previewBounds().contains(e.getPosition())) return;

    // Right-click carries pictures between songs. An album's songs mostly
    // share a cover, and choosing the same file eleven times is not how
    // anyone wants to say that.
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    if (hasOptions && optionsBounds().contains(e.getPosition()))
    {
        if (onOptions) onOptions();
        return;
    }

    if (file.existsAsFile() && clearBounds().contains(e.getPosition()))
    {
        setFile({});
        if (onFileChanged) onFileChanged({});
        return;
    }

    chooser = std::make_unique<juce::FileChooser>("Choose " + label.toLowerCase(),
                                                  file.existsAsFile() ? file : juce::File(),
                                                  "*.png;*.jpg;*.jpeg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc)
                         {
                             auto picked = fc.getResult();
                             if (!picked.existsAsFile()) return;
                             setFile(picked);
                             if (onFileChanged) onFileChanged(picked);
                         });
}

void ExportDialogComponent::ArtSlot::showContextMenu()
{
    const auto pasted = onPaste ? onPaste() : juce::File();

    enum { copyItem = 1, pasteItem, applyItem, clearItem };

    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(copyItem, "Copy image", file.existsAsFile());
    menu.addItem(pasteItem, "Paste image", pasted.existsAsFile());

    // Only where it means something: everything selected already has it, or
    // there is nothing selected to spread it to.
    if (partial)
    {
        menu.addSeparator();
        menu.addItem(applyItem, "Use for every selected song");
    }

    menu.addSeparator();
    menu.addItem(clearItem, "Clear", file.existsAsFile());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [this, pasted](int choice)
                       {
                           switch (choice)
                           {
                               case copyItem:
                                   if (onCopy) onCopy(file);
                                   break;

                               case pasteItem:
                                   setFile(pasted);
                                   if (onFileChanged) onFileChanged(pasted);
                                   break;

                               case applyItem:
                                   if (onApplyToAll) onApplyToAll();
                                   break;

                               case clearItem:
                                   setFile({});
                                   if (onFileChanged) onFileChanged({});
                                   break;

                               default: break;
                           }
                       });
}

void ExportDialogComponent::ArtSlot::setFile(const juce::File& newFile)
{
    file = newFile;
    thumbnail = file.existsAsFile() ? juce::ImageFileFormat::loadFrom(file) : juce::Image();
    repaint();
}

void ExportDialogComponent::ArtSlot::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(10.0f));
    g.drawText(label, getLocalBounds().removeFromTop(headingHeight),
               juce::Justification::centredLeft);

    auto preview = previewBounds();

    g.setColour(juce::Colour(Theme::darkBgLighter));
    g.fillRect(preview);
    if (thumbnail.isValid())
    {
        g.drawImage(thumbnail, preview.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else if (mixed)
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getMixedFont(10.0f));
        g.drawText(Theme::mixedText, preview, juce::Justification::centred);
    }
    else
    {
        // The picture is the control, so an empty one says what to do with it.
        g.setColour(juce::Colour(Theme::textDim).withAlpha(hovering ? 0.9f : 0.5f));
        g.setFont(Theme::getUIFont(10.0f));
        g.drawText("click to choose", preview, juce::Justification::centred);
    }

    // Bordered green while only part of the selection has this picture: the
    // same thing the fields say by leaning, said in the only way a picture
    // can say it.
    if (partial)
    {
        g.setColour(juce::Colour(Theme::green));
        g.drawRect(preview, 2);
    }
    else
    {
        g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                             : juce::Colours::white.withAlpha(Theme::borderAlpha));
        g.drawRect(preview);
    }

    // Clearing is a hover affordance rather than a button competing with the
    // picture for space.
    if (hovering && file.existsAsFile())
    {
        auto badge = clearBounds();
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.85f));
        g.fillRoundedRectangle(badge.toFloat(), 2.0f);
        g.setColour(juce::Colour(overClear ? Theme::coral : Theme::textDim));
        g.drawRoundedRectangle(badge.toFloat(), 2.0f, 1.0f);

        const auto cross = badge.reduced(4).toFloat();
        g.drawLine(cross.getX(), cross.getY(), cross.getRight(), cross.getBottom(), 1.2f);
        g.drawLine(cross.getX(), cross.getBottom(), cross.getRight(), cross.getY(), 1.2f);
    }

    // The gear lives opposite the clear badge, so the two never collide.
    if (hasOptions && hovering)
    {
        auto badge = optionsBounds();
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.85f));
        g.fillRoundedRectangle(badge.toFloat(), 2.0f);
        g.setColour(juce::Colour(Theme::textDim));
        g.drawRoundedRectangle(badge.toFloat(), 2.0f, 1.0f);

        const auto centre = badge.toFloat().getCentre();
        const float outer = badge.getWidth() * 0.30f;
        for (int tooth = 0; tooth < 6; ++tooth)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float)tooth / 6.0f;
            g.drawLine(centre.x + std::cos(angle) * outer * 0.55f,
                       centre.y + std::sin(angle) * outer * 0.55f,
                       centre.x + std::cos(angle) * outer,
                       centre.y + std::sin(angle) * outer, 1.2f);
        }
        g.drawEllipse(centre.x - outer * 0.45f, centre.y - outer * 0.45f,
                      outer * 0.9f, outer * 0.9f, 1.2f);
    }

    if (file.existsAsFile())
    {
        auto footer = getLocalBounds().removeFromBottom(14);

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(9.5f));
        if (thumbnail.isValid())
        {
            auto size = juce::String(thumbnail.getWidth()) + "\xc3\x97"
                      + juce::String(thumbnail.getHeight());
            g.drawText(size, footer.removeFromRight(70), juce::Justification::centredRight);
        }

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::getUIFont(10.0f));
        g.drawText(caption.isNotEmpty() ? caption : file.getFileName(), footer,
                   juce::Justification::centredLeft, true);
    }
}

//==============================================================================
ExportDialogComponent::SongRow::SongRow()
{
    addAndMakeVisible(tick);
}

void ExportDialogComponent::SongRow::paint(juce::Graphics& g)
{
    if (current)
    {
        // Lit rather than merely lighter. Against a panel that is already a
        // shade up from the window, darkBgLighter was almost the same colour
        // as the rows either side of it.
        g.setColour(juce::Colour(Theme::darkBgHover));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colour(Theme::coral).withAlpha(selectedTint));
        g.fillRect(getLocalBounds());

        // Coral edge: reads at a glance where the fill alone does not.
        g.setColour(juce::Colour(Theme::coral));
        g.fillRect(getLocalBounds().removeFromLeft(selectedEdge));
    }

    auto bounds = getLocalBounds();
    bounds.removeFromLeft(tickColumn);

    auto numberArea = bounds.removeFromLeft((int)(getHeight() * 0.52f));
    g.setColour(juce::Colour(current ? Theme::coral : Theme::textDim).withAlpha(current ? 1.0f : 0.6f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.30f));
    g.drawText(number, numberArea, juce::Justification::centredLeft);

    auto top = bounds.removeFromTop(getHeight() * 3 / 5).reduced(2, 0);

    // The timecode is what tells two untitled rows apart, so it is on the
    // title line rather than tucked under it.
    g.setColour(juce::Colour(Theme::textDim).withAlpha(0.7f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    auto timeArea = top.removeFromRight(
        juce::jmin(top.getWidth() / 2,
                   (int)Theme::getUIFont((float)getHeight() * 0.28f).getStringWidthFloat(timecode) + 6));
    g.drawText(timecode, timeArea, juce::Justification::bottomRight);

    g.setColour(juce::Colour(untitled ? Theme::textDim
                                      : (current ? Theme::textWhite : Theme::textDim))
                    .withMultipliedAlpha(untitled ? 0.6f : 1.0f));
    g.setFont(untitled ? Theme::getMixedFont((float)getHeight() * 0.36f)
                       : Theme::getUIFont((float)getHeight() * 0.36f));
    g.drawText(name, top, juce::Justification::bottomLeft, true);

    auto lower = bounds.reduced(2, 0);

    g.setColour(juce::Colour(Theme::textDim).withAlpha(0.6f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.26f));
    auto sourceArea = lower.removeFromRight(
        juce::jmin(lower.getWidth() / 2,
                   (int)Theme::getUIFont((float)getHeight() * 0.26f).getStringWidthFloat(source) + 6));
    g.drawText(source, sourceArea, juce::Justification::topRight, true);

    g.setColour(juce::Colour(!ready ? Theme::coral : exported ? Theme::textDim : Theme::green));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    g.drawText(status, lower, juce::Justification::topLeft, true);
}

void ExportDialogComponent::SongRow::resized()
{
    // A square, so the box inside it cannot come out wider than the column.
    // The box is a share of the height, and on a row taller than the column
    // is wide that put it through the track number beside it.
    const int side = juce::jmin(tickColumn, getHeight()) - gapS;
    tick.setBounds(getLocalBounds().removeFromLeft(tickColumn)
                       .withSizeKeepingCentre(side, side));
}

//==============================================================================
juce::String ExportDialogComponent::Song::blocker(bool charterNamed, bool drums) const
{
    // Asked of the song rather than of the window, so a session can hold one
    // finished record and one half-typed single and still export the record.
    if (title.isEmpty()) return "needs a title";
    if (artist.isEmpty()) return "needs an artist";
    if (album.isEmpty()) return "needs an album";
    if (track.isEmpty()) return "needs a track number";
    if (!charterNamed) return "needs a charter";
    if (drums && (diffDrums == ChartExporter::ExportOptions::kUnrated
               || diffPro == ChartExporter::ExportOptions::kUnrated))
        return "needs a rating";
    return {};
}

//==============================================================================
ExportDialogComponent::ExportDialogComponent(Context ctx)
    : context(std::move(ctx))
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    // Mouse moves from every descendant as well as from the window itself.
    // The charter line carries the icon preview, so pointing at it delivered
    // the move to the icon and the line never learned the pointer was there:
    // the pencil only appeared in the gaps between its own controls.
    addMouseListener(this, true);

    hasDrums = std::any_of(context.trackNames.begin(), context.trackNames.end(),
                           [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });

    // Charter identity comes from the machine, not the project, so it is typed
    // once ever rather than once per song.
    charterField.setText(context.charter);
    iconField.setText(context.icon);

    // Typing resolves a mixed field, for everything selected. Each field
    // writes only its own member, leaving the others alone.
    auto edits = [this](Field& field, juce::String Song::*member)
    {
        field.editor.onTextChange = [this, &field, member]()
        {
            // Typing writes to everything selected, so a value that arrived
            // as "some of them" stops being that the moment it is edited.
            field.clearPartial();

            auto value = field.text();
            applyToSelection([member, &value](Song& song) { song.*member = value; });
        };
    };

    edits(titleField, &Song::title);
    edits(trackField, &Song::track);
    edits(artistField, &Song::artist);
    edits(albumField, &Song::album);
    edits(genreField, &Song::genre);
    edits(yearField, &Song::year);

    for (auto* field : { &titleField, &trackField, &artistField, &albumField,
                         &genreField, &yearField, &charterField, &iconField })
        addAndMakeVisible(*field);

    // Kept as it is typed, not only when an export happens to run. Typing a
    // charter and closing the window without exporting lost it, which is the
    // opposite of a setting you name once and never touch again.
    charterField.editor.onTextChange = [this]()
    {
        ChartSettings::setCharter(charterField.text());
        refreshRows();
    };

    // An unsaved project has no folder to sit an export folder beside, so
    // there is nothing to show and the box says what to do about it instead.
    outputField.setText(context.destinationRoot.getFullPathName());
    outputField.editor.setTextToShowWhenEmpty("save the project, or type a folder",
                                              juce::Colour(Theme::coral));
    outputField.editor.onTextChange = [this]()
    {
        context.destinationRoot = juce::File::createFileWithoutCheckingPath(outputField.text());
        refreshRows();
    };
    addAndMakeVisible(outputField);

    iconField.editor.onTextChange = [this]()
    {
        ChartSettings::setIcon(iconField.text());
        iconPreview.setIconName(iconField.text());
        refreshValidity();
        repaint();
    };
    iconPreview.setIconName(iconField.text());
    addAndMakeVisible(iconPreview);


    // Rolling the wheel over a badge rates that instrument. Stepping from a
    // mixed or unrated selection lands on the lowest rating rather than
    // refusing to move, so one notch always does something.
    difficulties.onAdjustRating = [this](int index, int delta)
    {
        if (songs.empty() || editing.empty()) return;

        const auto member = ratingMemberFor(difficulties.getEntries()[(size_t)index]);
        if (member == nullptr) return;

        const int from = songs[(size_t)*editing.begin()].*member;
        const int stepped = from == ChartExporter::ExportOptions::kUnrated ? kMinRating - 1 : from;
        const int to = juce::jlimit(kMinRating, kMaxRating, stepped + delta);

        applyToSelection([member, to](Song& song) { song.*member = to; });

        // Back through the one path that fills controls, so the badge and the
        // box beneath it cannot end up saying different numbers.
        loadSelection();
    };

    difficulties.onSetRating = [this](int index, int rating)
    {
        if (songs.empty() || editing.empty()) return;

        const auto member = ratingMemberFor(difficulties.getEntries()[(size_t)index]);
        if (member == nullptr) return;

        applyToSelection([member, rating](Song& song) { song.*member = rating; });
        loadSelection();
    };

    // Labels sit against their boxes, tag-editor style, so the panel has one
    // ragged edge instead of a left gutter and a right one.
    for (auto* field : { &titleField, &artistField, &albumField, &yearField,
                         &trackField, &genreField })
        field->labelRight = true;

    // Every menu in here opens off one of these, and JUCE takes a popup's
    // look from the component it is attached to. Set on the openers rather
    // than on the window, which would restyle every button in it too.
    for (auto* opener : std::initializer_list<juce::Component*>{
             &formatMenu, &packagingMenu, &difficulties, &albumArt, &backgroundArt })
        opener->setLookAndFeel(&menuLook);

    instruments.setTrackNames(context.trackNames);
    addAndMakeVisible(instruments);

    // Clicking the strip is clicking the rail. Two views of one list, so
    // picking a song off either has to mean the same thing.
    timeline.onSelect = [this](int song, const juce::ModifierKeys& mods)
    { selectSong(song, mods); };
    addAndMakeVisible(timeline);

    // Clicking the picture picks a picture, as it does on the art slots. It
    // cannot open a file chooser: the icon is a name looked up in a
    // repository, so the gallery of them is where you go to pick one, and
    // that is what the browse button used to be for.
    iconPreview.onClick = []() { IconPreview::browseUrl().launchInDefaultBrowser(); };

    // Both are song.ini tags that force how the drums are read, overriding
    // what the game would work out from the notes. They start from what was
    // detected, so they are corrections rather than decisions, and both being
    // set at once is an invalid chart, which is why they exclude each other.
    proDrumsHelp.setText("Forces pro drums even when the chart has no tom markers.");
    fiveLaneHelp.setText("Forces Guitar Hero style 5-lane drums: a fifth lane for "
                         "green, and no cymbals.");

    for (auto* help : { &proDrumsHelp, &fiveLaneHelp })
        help->setPlacement(InfoTooltip::Placement::below);

    proDrumsHelp.attachTo(proDrumsToggle);
    fiveLaneHelp.attachTo(fiveLaneToggle);

    proDrumsToggle.onClick = [this]()
    {
        // Mutually exclusive: the format calls both being true invalid.
        const bool on = proDrumsToggle.getToggleState();
        if (on) fiveLaneToggle.setToggleState(false);
        applyToSelection([on](Song& song)
                         {
                             song.proDrums = on;
                             if (on) song.fiveLane = false;

                             // Said by hand, so reading the notes stops
                             // having an opinion about it.
                             song.drumTypeForced = true;
                         });
    };
    fiveLaneToggle.onClick = [this]()
    {
        const bool on = fiveLaneToggle.getToggleState();
        if (on) proDrumsToggle.setToggleState(false);
        applyToSelection([on](Song& song)
                         {
                             song.fiveLane = on;
                             if (on) song.proDrums = false;
                             song.drumTypeForced = true;
                         });
    };

    albumArt.onFileChanged = [this](const juce::File& file)
    {
        applyToSelection([&file](Song& song) { song.albumArtFile = file; });

        // A cover arriving where there is no background makes one, in the
        // template already chosen. Only for songs with nothing there: an
        // existing background was put there deliberately.
        if (file.existsAsFile())
            generateBackgrounds(true);
    };
    backgroundArt.onFileChanged = [this](const juce::File& file)
    { applyToSelection([&file](Song& song) { song.backgroundFile = file; }); };

    // One picture held between the slots, so a cover copied off one song
    // pastes onto another, and onto the background slot if that is wanted.
    for (auto* slot : { &albumArt, &backgroundArt })
    {
        slot->onCopy = [this](const juce::File& file) { copiedArtwork = file; };
        slot->onPaste = [this]() { return copiedArtwork; };
    }

    // REAPER's sink descriptions are sentences ("MP3 (encoder by LAME
    // project)"), which do not fit a segmented button.
    auto shortFormatName = [](const juce::String& description)
    {
        for (const auto* name : { "Opus", "Vorbis", "MP3", "FLAC", "WAV" })
            if (description.containsIgnoreCase(name)) return juce::String(name);
        return description.upToFirstOccurrenceOf(" ", false, false);
    };

    juce::StringArray formatLabels;
    for (const auto& sink : context.audioFormats)
        formatLabels.add(shortFormatName(sink.description));
    formatMenu.setItems(formatLabels);

    int opusIndex = 0;
    for (int i = 0; i < (int)context.audioFormats.size(); ++i)
        if (context.audioFormats[(size_t)i].description.containsIgnoreCase("opus")) opusIndex = i;
    formatMenu.setSelectedIndex(opusIndex);

    packagingMenu.setItems({ "folder", "sng" });
    packagingMenu.setSelectedIndex(0);

    // Reuse cannot work against a .sng: the audio is inside the container and
    // reading it back means parsing the index. Forced on until that exists.
    auto followPackaging = [this]()
    {
        const bool sng = packagingMenu.getSelectedIndex() == 1;
        if (sng) renderAudioToggle.setToggleState(true);
        renderAudioToggle.setEnabled(!sng);
        formatMenu.setEnabled(renderAudioToggle.getToggleState());
        repaint();
    };

    packagingMenu.onSelectionChanged = [followPackaging](int) { followPackaging(); };

    renderAudioToggle.setToggleState(true);
    renderAudioToggle.onClick = [this]()
    {
        formatMenu.setEnabled(renderAudioToggle.getToggleState());
        repaint();
    };

    // Behind a gear on the slot itself: three template names on permanent
    // display took a row of the window to say something set once.
    backgroundArt.hasOptions = true;
    backgroundArt.onOptions = [this]()
    {
        enum { autoItem = 1, generateItem, firstStyle = 10 };

        juce::PopupMenu menu;
        menu.setLookAndFeel(&menuLook);

        menu.addItem(autoItem, "Make one from the cover automatically", true,
                     ChartSettings::autoBackground());

        // Because automatic is not always: the cover arrived before the
        // setting was on, or the background was cleared, or it was never
        // going to happen and there was no way to ask for it.
        menu.addItem(generateItem, "Generate now", albumArt.file.existsAsFile());

        menu.addSeparator();
        menu.addSectionHeader("Style");

        const auto names = BackgroundGenerator::styleNames();
        const int chosen = ChartSettings::backgroundStyle();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem(firstStyle + i, names[i], true, i == chosen);

        // From the gear itself, not from under the picture. A menu that opens
        // a long way from what was clicked reads as belonging to something
        // else.
        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&backgroundArt)
                               .withTargetScreenArea(backgroundArt.optionsBounds()
                                                     + backgroundArt.getScreenPosition()),
                           [this](int result)
                           {
                               if (result == autoItem)
                               {
                                   ChartSettings::setAutoBackground(!ChartSettings::autoBackground());
                               }
                               else if (result == generateItem)
                               {
                                   generateBackgrounds();
                               }
                               else if (result >= firstStyle)
                               {
                                   // Picking a template is what makes it, so the
                                   // three can be compared by choosing them.
                                   ChartSettings::setBackgroundStyle(result - firstStyle);
                                   generateBackgrounds();
                               }
                           });
    };

    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::coral).withAlpha(0.15f));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    exportButton.onClick = [this]()
    {
        // Ticked rows that are not ready are skipped rather than blocking the
        // rest: ten finished charts should not wait on the eleventh.
        std::vector<ChartExporter::SongExport> batch;
        for (const auto& song : songs)
        {
            if (!song.selected) continue;
            if (song.blocker(albumBlocker().isEmpty(), hasDrums).isNotEmpty()) continue;

            // The sentinel is ours. SongExport::regionGuid must be empty when
            // there is no region, which is how the exporter picks what to
            // make a region for.
            const bool isRegion = song.region.guid != kSelectionKey;
            batch.push_back({ song.region.range(), optionsFor(song),
                              isRegion ? song.region.guid : juce::String() });
        }

        if (onExport && !batch.empty()) onExport(batch);
    };

    cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    cancelButton.onClick = [this]() { if (onDismiss) onDismiss(); };

    charterPencil.onClick = [this]()
    {
        settingsOpen = !settingsOpen;
        resized();
        repaint();
    };
    addAndMakeVisible(charterPencil);

    createRegionButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(Theme::coral).withAlpha(0.15f));
    createRegionButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    createRegionButton.onClick = [this]()
    {
        if (!createRegion || !context.selection.plausible()) return;

        // A song already sitting on this selection is the one being made real,
        // and its title names the region. Otherwise this is a new region and
        // the next poll picks it up as a song.
        auto pending = std::find_if(songs.begin(), songs.end(),
                                    [](const Song& s) { return s.region.guid == kSelectionKey; });

        auto name = (pending != songs.end() && pending->title.isNotEmpty())
                      ? pending->title
                      : context.inferred.title.isNotEmpty() ? context.inferred.title
                                                            : juce::String("song");

        auto guid = createRegion(context.selection, name);
        if (guid.isEmpty()) return;

        if (pending != songs.end())
        {
            pending->region.guid = guid;
            pending->region.name = name;
        }

        refreshRows();
        resized();
    };
    addAndMakeVisible(createRegionButton);

    listView.setViewedComponent(&listContent, false);
    listView.setScrollBarsShown(true, false);
    addAndMakeVisible(listView);

    const std::initializer_list<juce::Component*> everything {
        &proDrumsToggle, &fiveLaneToggle, &difficulties, &albumArt, &backgroundArt,
        &renderAudioToggle, &formatMenu, &packagingMenu, &exportButton, &cancelButton
    };
    for (auto* component : everything)
        addAndMakeVisible(*component);

    // A project with no drums in it has nothing to force about them.
    for (auto* component : { &proDrumsToggle, &fiveLaneToggle })
        component->setVisible(hasDrums);

    prefillArtwork();

    // Built through the same path the timer uses, so opening and updating
    // cannot drift apart.
    auto initial = songs;
    Snapshot snapshot { context.selection, context.regions, context.trackNames };
    context.regions.clear();          // force the first apply to do its work
    applySnapshot(snapshot);

    restore(context.remembered);
    regroup();
    loadSelection();
    refreshRows();

    // After restore, since that is what decides whether we reopened into .sng.
    followPackaging();

    startTimer(400);
}

ExportDialogComponent::~ExportDialogComponent()
{
    // Before the look itself goes: it is declared after these, so it would
    // otherwise be destroyed while they still point at it.
    for (auto* opener : std::initializer_list<juce::Component*>{
             &formatMenu, &packagingMenu, &difficulties, &albumArt, &backgroundArt })
        opener->setLookAndFeel(nullptr);

    // The exporter has already copied whatever shipped into the chart folder,
    // so anything still here is a preview nobody needs.
    auto staging = backgroundStagingFolder();
    if (staging.isDirectory()
        && staging.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        staging.deleteRecursively();
}

bool ExportDialogComponent::differsFromCurrent(const Snapshot& snapshot) const
{
    if (snapshot.trackNames != context.trackNames) return true;

    // Always, not only when there are no regions. The readout and the create
    // button both track the selection, so a project that already has regions
    // still has to notice it moving.
    if (snapshot.selection.startSec != context.selection.startSec
        || snapshot.selection.endSec != context.selection.endSec)
        return true;

    if (snapshot.regions.size() != context.regions.size()) return true;
    for (size_t i = 0; i < snapshot.regions.size(); ++i)
    {
        const auto& a = snapshot.regions[i];
        const auto& b = context.regions[i];
        if (a.guid != b.guid || a.name != b.name
            || a.startSec != b.startSec || a.endSec != b.endSec)
            return true;
    }
    return false;
}

void ExportDialogComponent::applySnapshot(const Snapshot& snapshot)
{
    const bool rangesChanged = snapshot.regions.size() != context.regions.size()
                            || snapshot.selection.startSec != context.selection.startSec
                            || snapshot.selection.endSec != context.selection.endSec;

    context.selection = snapshot.selection;
    context.regions = snapshot.regions;
    context.trackNames = snapshot.trackNames;

    hasDrums = std::any_of(context.trackNames.begin(), context.trackNames.end(),
                           [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });

    if (rangesChanged && analyseDrums)
        context.drums = analyseDrums(context.regions.empty() ? context.selection
                                                             : context.regions.front().range());

    // Carried across so a region moving or being renamed keeps whatever was
    // typed for it. Identity first; failing that, the range, which is what
    // carries a time selection over at the moment it becomes a region. Typed
    // values are never dropped on the floor for want of a matching GUID.
    auto previous = songs;
    std::vector<bool> claimed(previous.size(), false);

    auto carry = [&previous, &claimed](const ChartExporter::Region& region) -> const Song*
    {
        for (size_t i = 0; i < previous.size(); ++i)
            if (!claimed[i] && region.guid.isNotEmpty() && previous[i].region.guid == region.guid)
            {
                claimed[i] = true;
                return &previous[i];
            }

        // One previous song can only become one region, so a claimed match is
        // never handed out twice.
        for (size_t i = 0; i < previous.size(); ++i)
            if (!claimed[i]
                && std::abs(previous[i].region.startSec - region.startSec) < 0.01
                && std::abs(previous[i].region.endSec - region.endSec) < 0.01)
            {
                claimed[i] = true;
                return &previous[i];
            }

        // Failing that, the best overlap. A region drawn by hand never lands
        // exactly on the selection it came from, and an exact match was
        // dropping everything typed on the floor for a few milliseconds.
        int best = -1;
        double bestOverlap = 0.0;

        for (size_t i = 0; i < previous.size(); ++i)
        {
            if (claimed[i]) continue;

            const auto& was = previous[i].region;
            const double overlap = juce::jmin(was.endSec, region.endSec)
                                 - juce::jmax(was.startSec, region.startSec);
            if (overlap <= 0.0) continue;

            // Over half the shorter of the two, so a region cannot steal a
            // song it merely brushes against.
            const double shorter = juce::jmin(was.endSec - was.startSec,
                                              region.endSec - region.startSec);
            if (shorter <= 0.0 || overlap < shorter * 0.5) continue;

            if (overlap > bestOverlap) { bestOverlap = overlap; best = (int)i; }
        }

        if (best >= 0)
        {
            claimed[(size_t)best] = true;
            return &previous[(size_t)best];
        }

        return nullptr;
    };

    /**
        What the songs already here agree on, for seeding a new one.

        Typing an album name once and having the next region come up blank is
        the whole of "why do I keep typing this". Only where they agree: on a
        compilation they will not, and guessing one song's artist for the next
        is worse than leaving it empty.
    */
    auto agreed = [&previous](juce::String Song::*member, const juce::String& fallback)
    {
        juce::String value;
        for (const auto& song : previous)
        {
            const auto& theirs = song.*member;
            if (theirs.isEmpty()) continue;
            if (value.isEmpty()) value = theirs;
            else if (value != theirs) return fallback;
        }
        return value.isNotEmpty() ? value : fallback;
    };

    auto agreedFile = [&previous](juce::File Song::*member, const juce::File& fallback)
    {
        juce::File value;
        for (const auto& song : previous)
        {
            const auto& theirs = song.*member;
            if (theirs == juce::File()) continue;
            if (value == juce::File()) value = theirs;
            else if (value != theirs) return fallback;
        }
        return value != juce::File() ? value : fallback;
    };

    // Everything a new song starts as, in one place: the two that make one
    // were seeding themselves separately and had already drifted apart.
    auto seed = [&](Song& song)
    {
        // Pro unless the project looks like 5-lane, and detection corrects it
        // per song a moment later. Never read off tom markers: a 4-lane .mid
        // chart is pro whether or not it happens to contain a tom.
        song.fiveLane = context.drums.fiveLane;
        song.proDrums = !context.drums.fiveLane;
        song.drumTypeForced = false;

        song.artist = agreed(&Song::artist, context.inferred.artist);
        song.album = agreed(&Song::album, context.inferred.album);
        song.genre = agreed(&Song::genre, context.inferred.genre);
        song.year = agreed(&Song::year, context.inferred.year);
        song.albumArtFile = agreedFile(&Song::albumArtFile, prefilledAlbumArt);
        song.backgroundFile = agreedFile(&Song::backgroundFile, prefilledBackground);
    };

    songs.clear();
    for (const auto& region : context.regions)
    {
        Song song;
        song.region = region;

        if (const auto* old = carry(region))
        {
            song = *old;
            song.region = region;

            // The region's name only fills a title nobody has typed.
            if (song.title.isEmpty()) song.title = region.name.trim();
        }
        else
        {
            // The region name is a default, not a convention: it is the most
            // likely title and nothing downstream depends on its shape.
            song.title = region.name.trim();
            seed(song);
        }

        songs.push_back(song);
    }

    // A song that is not a region yet.
    //
    // It outlives the time selection that made it: clicking the ruler
    // collapses the selection, and dropping the song then took everything
    // typed into it along with it.
    auto carried = std::find_if(previous.begin(), previous.end(),
                                [](const Song& s) { return s.region.guid == kSelectionKey; });
    const bool hadProvisional = carried != previous.end();

    // Only when there is nothing else: once the project has regions, they are
    // the songs, and the button below is how a selection becomes another one.
    // An existing one is kept regardless, since it holds what was typed.
    if (hadProvisional || (songs.empty() && context.selection.plausible()))
    {
        Song song;
        if (hadProvisional)
        {
            song = *carried;
        }
        else
        {
            song.title = context.inferred.title;
            song.track = context.inferred.track;

            // Seeded like any other new song. Leaving this out is why a
            // remade selection came back asking for an artist.
            seed(song);
        }

        song.region.name = "time selection";
        song.region.guid = kSelectionKey;

        // Only a real selection moves it. A collapsed one leaves the range
        // where it was, so the row keeps pointing at the same music.
        if (context.selection.plausible())
        {
            song.region.startSec = context.selection.startSec;
            song.region.endSec = context.selection.endSec;
        }

        song.selected = true;
        songs.push_back(song);
    }

    // Track numbers default to timeline order, which is album order whenever
    // the project is laid out the way the album plays.
    // Numbers already in use are skipped, so filling a gap cannot hand two
    // songs the same track number.
    juce::SortedSet<int> taken;
    for (const auto& song : songs)
        if (song.track.isNotEmpty()) taken.add(song.track.getIntValue());

    int number = 1;
    for (auto& song : songs)
    {
        if (song.track.isNotEmpty()) continue;
        while (taken.contains(number)) ++number;
        taken.add(number);
        song.track = juce::String(number).paddedLeft('0', 2);
    }

    // Drop selected songs that went away; never leave the selection empty.
    std::set<int> surviving;
    for (int index : editing)
        if (juce::isPositiveAndBelow(index, (int)songs.size()))
            surviving.insert(index);
    editing = surviving;
    anchor = juce::jlimit(0, juce::jmax(0, (int)songs.size() - 1), anchor);

    // Selecting a song is selecting its time, so a selection that has moved
    // off it drops the row rather than leaving it looking selected. Several
    // songs at once is its own state and is left alone.
    if (editing.size() == 1)
    {
        const auto& range = songs[(size_t)*editing.begin()].region;
        const bool matches = context.selection.plausible()
                          && std::abs(context.selection.startSec - range.startSec) < 0.05
                          && std::abs(context.selection.endSec - range.endSec) < 0.05;
        if (!matches) editing.clear();
    }

    rebuildRows();
    loadSelection();
    refreshRows();
    resized();
}

void ExportDialogComponent::rebuildRows()
{
    rows.clear();
    listContent.removeAllChildren();

    for (size_t i = 0; i < songs.size(); ++i)
    {
        auto row = std::make_unique<SongRow>();
        row->onSelect = [this, i](const juce::ModifierKeys& mods) { selectSong((int)i, mods); };
        row->tick.onClick = [this, i]()
        {
            songs[i].selected = rows[i]->tick.getToggleState();
            refreshValidity();
            repaint();
        };
        listContent.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    regroup();
}

void ExportDialogComponent::regroup()
{
    // Album first, artist second: two singles by one artist belong together
    // more than they belong apart, and an album is the tighter claim of the
    // two when both are there.
    auto keyFor = [](const Song& song)
    {
        if (song.album.isNotEmpty()) return "a:" + song.album.trim().toLowerCase();
        if (song.artist.isNotEmpty()) return "b:" + song.artist.trim().toLowerCase();
        return juce::String();
    };

    groups.clear();
    displayOrder.clear();

    juce::StringArray seen;
    for (size_t i = 0; i < songs.size(); ++i)
    {
        auto key = keyFor(songs[i]);

        // Songs with nothing to group by each stand alone rather than being
        // herded into one "unknown" pile that would select them together.
        if (key.isEmpty())
        {
            groups.push_back({ (int)i });
            seen.add({});
            continue;
        }

        const int at = seen.indexOf(key);
        if (at >= 0) groups[(size_t)at].push_back((int)i);
        else { groups.push_back({ (int)i }); seen.add(key); }
    }

    for (const auto& group : groups)
        for (int index : group)
            displayOrder.push_back(index);

}

void ExportDialogComponent::selectSong(int index, const juce::ModifierKeys& mods)
{
    if (!juce::isPositiveAndBelow(index, (int)songs.size())) return;

    auto positionOf = [this](int songIndex)
    {
        for (size_t i = 0; i < displayOrder.size(); ++i)
            if (displayOrder[i] == songIndex) return (int)i;
        return 0;
    };

    if (mods.isShiftDown() && !editing.empty())
    {
        // Measured down the rail as it is drawn, not through the underlying
        // indices: a range means the rows between these two on screen.
        const int from = positionOf(anchor);
        const int to = positionOf(index);
        editing.clear();
        for (int p = juce::jmin(from, to); p <= juce::jmax(from, to); ++p)
            editing.insert(displayOrder[(size_t)p]);
    }
    else if (mods.isCommandDown() || mods.isCtrlDown())
    {
        if (editing.count(index) && editing.size() > 1) editing.erase(index);
        else editing.insert(index);
        anchor = index;
    }
    else
    {
        editing = { index };
        anchor = index;

        // One song selected is one range on the timeline. Several is a state
        // no single range describes, so the timeline is left alone.
        if (setTimeSelection)
        {
            const auto& range = songs[(size_t)index].region;
            context.selection.startSec = range.startSec;
            context.selection.endSec = range.endSec;
            lastPolledSelection = context.selection;
            setTimeSelection(context.selection);
        }
    }

    loadSelection();
    refreshRows();
}

void ExportDialogComponent::selectAllForEditing()
{
    editing.clear();
    for (size_t i = 0; i < songs.size(); ++i)
        editing.insert((int)i);

    loadSelection();
    refreshRows();
}

void ExportDialogComponent::applyToSelection(std::function<void(Song&)> change)
{
    for (int index : editing)
        if (juce::isPositiveAndBelow(index, (int)songs.size()))
            change(songs[(size_t)index]);

    // The only path that writes a song, so it is the only place that has to
    // notice one was written.
    stateDirty = true;

    regroup();
    refreshRows();
    resized();
}

void ExportDialogComponent::loadSelection()
{
    // Nothing selected: the panel shows nothing rather than the last song's
    // values, which would read as though that song were still being edited.
    if (songs.empty() || editing.empty())
    {
        for (auto* field : { &titleField, &trackField, &artistField, &albumField,
                             &genreField, &yearField })
            field->setText({});

        proDrumsToggle.setMixed(false);
        fiveLaneToggle.setMixed(false);
        albumArt.setMixed(false);
        albumArt.setFile({});
        backgroundArt.setMixed(false);
        backgroundArt.setFile({});

        // Which instruments the project has is true of the project, not of
        // whatever is selected, so the badges stay up when nothing is.
        refreshDifficulties();
        return;
    }

    // What kind of drum chart these are, off their own notes, before anything
    // is filled in from them.
    detectDrumTypes();

    // Every control fills the same way: agree and show the value, disagree
    // and mark it mixed. Nothing here writes back.
    /**
        Three states, not two.

        A value every song agrees on is just the value. A value some of them
        carry while the rest carry nothing has one obvious answer, so it is
        shown and offered. Two different real values have no obvious answer at
        all, and offering to pick one would be picking for the user.
    */
    auto text = [this](Field& field, juce::String Song::*member)
    {
        juce::String value;
        bool disagree = false;
        bool anyEmpty = false;

        for (int i : editing)
        {
            const auto& theirs = songs[(size_t)i].*member;
            if (theirs.isEmpty()) { anyEmpty = true; continue; }
            if (value.isEmpty())  value = theirs;
            else if (value != theirs) disagree = true;
        }

        if (disagree)             field.setMixed();
        else if (value.isEmpty()) field.setText({});
        else if (anyEmpty)        field.setPartial(value);
        else                      field.setText(value);

        // The tick writes the shown value to everything selected, which is
        // what every other edit in this window already does.
        field.onApplyToAll = [this, &field, member]()
        {
            const auto shared = field.text();
            applyToSelection([member, shared](Song& song) { song.*member = shared; });
            loadSelection();
        };
    };

    text(titleField, &Song::title);
    text(trackField, &Song::track);
    text(artistField, &Song::artist);
    text(albumField, &Song::album);
    text(genreField, &Song::genre);
    text(yearField, &Song::year);

    auto flag = [this](CheckboxToggle& toggle, bool Song::*member)
    {
        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        toggle.setMixed(!same);
        if (same) toggle.setToggleState(first.*member);
    };

    flag(proDrumsToggle, &Song::proDrums);
    flag(fiveLaneToggle, &Song::fiveLane);

    // Only when the selection agrees. A PRO stamp is a claim about the chart,
    // and half a selection being pro is not one.
    instruments.setProDrums(proDrumsToggle.getToggleState() && !proDrumsToggle.isMixed());
    refreshDifficulties();

    // The same three states the fields use. A picture cannot lean, so partial
    // is a border rather than an italic, but it is the same claim.
    auto art = [this](ArtSlot& slot, juce::File Song::*member)
    {
        // Named for where it lands in the chart, not for the staging file it
        // happens to be sitting in now.
        slot.caption = (&slot == &albumArt) ? "album.png" : "background.png";

        juce::File value;
        bool disagree = false;
        bool anyEmpty = false;

        for (int i : editing)
        {
            const auto& theirs = songs[(size_t)i].*member;
            if (theirs == juce::File()) { anyEmpty = true; continue; }
            if (value == juce::File())  value = theirs;
            else if (value != theirs) disagree = true;
        }

        slot.setFile(disagree ? juce::File() : value);
        slot.setMixed(disagree);
        slot.setPartial(!disagree && anyEmpty && value != juce::File());

        slot.onApplyToAll = [this, &slot, member]()
        {
            const auto shared = slot.file;
            applyToSelection([member, shared](Song& song) { song.*member = shared; });
            loadSelection();
        };
    };

    art(albumArt, &Song::albumArtFile);
    art(backgroundArt, &Song::backgroundFile);
}

void ExportDialogComponent::timerCallback()
{
    // Coalesced onto the tick rather than written per keystroke: the tree is
    // rebuilt whole each time, and typing a title is a dozen of these. Ahead
    // of the poll, so it still saves in a session that has nothing to poll.
    if (stateDirty && onStateChanged)
    {
        onStateChanged(toValueTree());
        stateDirty = false;
    }

    if (!pollProject) return;

    auto snapshot = pollProject();
    if (differsFromCurrent(snapshot))
        applySnapshot(snapshot);

    auto same = [](const ChartExporter::TimeRange& a, const ChartExporter::TimeRange& b)
    {
        return std::abs(a.startSec - b.startSec) < 0.001
            && std::abs(a.endSec - b.endSec) < 0.001;
    };

    // One tick of stillness before reading the notes, so dragging a selection
    // does not analyse the project on every frame of the drag.
    if (analyseDrums
        && snapshot.selection.plausible()
        && same(snapshot.selection, lastPolledSelection)
        && !same(snapshot.selection, analysedSelection))
    {
        selectionDrums = analyseDrums(snapshot.selection);
        analysedSelection = snapshot.selection;
        repaint();
    }

    lastPolledSelection = snapshot.selection;
}

void ExportDialogComponent::refreshRows()
{
    const bool named = albumBlocker().isEmpty();
    for (size_t i = 0; i < rows.size() && i < songs.size(); ++i)
    {
        auto& song = songs[i];
        auto blocked = song.blocker(named, hasDrums);

        // Whether this song has been exported before, under the name it would
        // export as now. Renaming it makes the old one invisible here, which
        // is correct: a different name is a different chart.
        song.exported = juce::File();
        const auto folderName = optionsFor(song).folderName;
        if (folderName.isNotEmpty())
        {
            auto folder = context.destinationRoot.getChildFile(folderName);
            auto packed = context.destinationRoot.getChildFile(folderName + ".sng");
            if (folder.isDirectory())        song.exported = folder;
            else if (packed.existsAsFile())  song.exported = packed;
        }

        adoptExported(song);

        rows[i]->number = song.track;

        const bool isSelection = song.region.guid == kSelectionKey;
        rows[i]->untitled = song.title.isEmpty() && song.region.name.isEmpty();
        rows[i]->name = song.title.isNotEmpty() ? song.title
                      : song.region.name.isNotEmpty() ? song.region.name
                                                      : juce::String("untitled");
        // Where it is and how long it runs, which is what tells two rows
        // apart. The release is on the fields, not repeated down the list.
        rows[i]->timecode = timecodeOf(song.region.startSec) + " \xe2\x80\x93 "
                          + timecodeOf(song.region.endSec);
        rows[i]->source = isSelection
                            ? juce::String("time selection")
                            : timecodeOf(song.region.endSec - song.region.startSec);
        rows[i]->status = blocked.isNotEmpty() ? blocked
                        : song.exported.exists() ? "exported " + exportedWhen(song.exported)
                                                        : "ready";
        rows[i]->ready = blocked.isEmpty();
        rows[i]->exported = song.exported.exists();
        rows[i]->current = editing.count((int)i) > 0;
        rows[i]->tick.setToggleState(song.selected);
        rows[i]->repaint();
    }

    // Offered whenever the timeline is showing something that is not a song
    // yet, so making one is never something you have to already know about.
    const bool offer = context.selection.plausible() && !selectionInsideRegion();

    // The rail gives up a row for it, so appearing has to relayout.
    if (offer != createRegionButton.isVisible())
    {
        createRegionButton.setVisible(offer);
        resized();
    }

    rebuildTimeline();
    refreshValidity();
    repaint();
}

int ExportDialogComponent::Song::* ExportDialogComponent::ratingMemberFor(
    const DifficultyGrid::Entry& entry) const
{
    if (entry.part == Part::ELITE_DRUMS) return &Song::diffElite;
    if (entry.part != Part::DRUMS) return nullptr;

    return entry.variant == "PRO" ? &Song::diffPro : &Song::diffDrums;
}

void ExportDialogComponent::refreshDifficulties()
{
    // Every instrument the format has a slot for, in the order the games put
    // them in. Ones this project has no track for still appear, greyed: that
    // a chart has no guitar is worth being able to see.
    struct Slot { Part part; const char* variant; };
    static const Slot slots[] = {
        { Part::GUITAR,      "" },
        { Part::BASS,        "" },
        { Part::KEYS,        "" },
        { Part::VOCALS,      "" },
        { Part::DRUMS,       "" },
        { Part::DRUMS,       "PRO" },
        { Part::ELITE_DRUMS, "ELITE" },
    };

    // What the selected songs actually contain, not what the project has a
    // track for. An album keeps every instrument on one set of tracks and
    // only some songs use each of them.
    juce::StringArray present;
    if (analyseParts)
    {
        for (int index : editing)
        {
            const auto& range = songs[(size_t)index].region.range();
            if (!range.exists()) continue;

            for (const auto& name : analyseParts(range))
                present.addIfNotAlreadyThere(name);
        }
    }
    else
    {
        present = context.trackNames;
    }

    auto trackFor = [&present](Part part)
    {
        for (const auto& entry : getImplementedTrackNames())
            if (entry.part == part && present.contains(entry.name, true))
                return true;
        return false;
    };

    // One rating shown only when the selected songs agree on it, the same
    // rule the fields follow. Disagreement is not a number.
    auto agreedRating = [this](int Song::*member)
    {
        if (songs.empty() || editing.empty()) return ChartExporter::ExportOptions::kUnrated;

        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        return same ? first.*member : ChartExporter::ExportOptions::kUnrated;
    };

    const bool pro = proDrumsToggle.getToggleState() && !proDrumsToggle.isMixed();

    std::vector<DifficultyGrid::Entry> entries;
    for (const auto& slot : slots)
    {
        DifficultyGrid::Entry entry;
        entry.part = slot.part;
        entry.variant = slot.variant;
        entry.present = trackFor(slot.part);

        // The pro drums badge is the drums track read a second way, so it is
        // only there when the song says the drums are pro.
        if (juce::String(slot.variant) == "PRO")
            entry.present = entry.present && pro;

        entry.active = entry.present;

        // Whether a track has notes inside one song's range is not something
        // this window knows: reading them is the expensive pass, and it runs
        // for the time selection only. Until it runs per song, a track that
        // exists is taken at its word.
        entry.hasNotes = entry.present;

        // Through the same mapping the wheel writes back along, so what a
        // badge shows and what a notch changes cannot come apart.
        const auto member = ratingMemberFor(entry);
        entry.rating = member != nullptr ? agreedRating(member)
                                         : ChartExporter::ExportOptions::kUnrated;
        entries.push_back(entry);
    }

    difficulties.setEntries(std::move(entries));
}

void ExportDialogComponent::detectDrumTypes()
{
    if (!analyseDrums) return;

    // Only what is being looked at, and every time it is looked at. The chart
    // is being edited while this window sits open, so an answer from when the
    // song first appeared is an answer about a chart that has moved on.
    for (int index : editing)
    {
        auto& song = songs[(size_t)index];
        if (song.drumTypeForced || !song.region.range().exists()) continue;

        const auto profile = analyseDrums(song.region.range());

        // Nothing to go on. A range with no drum notes says nothing about how
        // its drums ought to be read.
        if (!profile.hasDrums) continue;

        // The only question the notes can answer is 5-lane or not.
        //
        // Whether a 4-lane chart is "pro" is not something to detect: in .mid
        // a 4-lane note is a cymbal and a tom marker turns it into a tom, so
        // a 4-lane chart is pro by construction. Reading pro off the presence
        // of tom markers is what makes an all-cymbals chart claim it is not
        // pro, and then its cymbals stop working in game.
        song.fiveLane = profile.fiveLane;
        song.proDrums = !profile.fiveLane;
    }
}

void ExportDialogComponent::adoptExported(Song& song)
{
    // Once per song. The chart on disk does not change while this window is
    // open unless this window is what changed it.
    if (song.adoptedFromDisk || !song.exported.isDirectory()) return;
    song.adoptedFromDisk = true;

    const auto values = readExportedIni(song.exported);
    if (values.size() == 0) return;

    // Only what is still unanswered here. The chart on disk is what was
    // shipped last time, which beats guessing from a filename, and loses to
    // anything typed into this window since.
    auto text = [&values](juce::String& target, const char* key)
    {
        if (target.isNotEmpty()) return;
        target = values.getValue(key, {});
    };

    text(song.title, "name");
    text(song.artist, "artist");
    text(song.album, "album");
    text(song.genre, "genre");
    text(song.year, "year");
    text(song.track, "album_track");

    auto rating = [&values](int& target, const char* key)
    {
        if (target != ChartExporter::ExportOptions::kUnrated) return;

        const auto value = values.getValue(key, {});
        if (value.isNotEmpty()) target = value.getIntValue();
    };

    // The whole reason for reading the file: a chart that has been exported
    // already carries its own ratings, and asking for them again is asking
    // someone to remember what they shipped.
    rating(song.diffDrums, "diff_drums");
    rating(song.diffPro, "diff_drums_real");
    rating(song.diffElite, "diff_elite_drums");

    // A tag in a shipped chart is a decision that was made about it, so
    // reading the notes does not get to argue with it.
    if (values.containsKey("five_lane_drums") || values.containsKey("pro_drums"))
    {
        song.fiveLane = values.getValue("five_lane_drums", "0").getIntValue() != 0;
        song.proDrums = values.getValue("pro_drums", "0").getIntValue() != 0;
        song.drumTypeForced = true;
    }
}

void ExportDialogComponent::rebuildTimeline()
{
    std::vector<TimelineStrip::Block> blocks;
    blocks.reserve(songs.size());

    for (size_t i = 0; i < songs.size(); ++i)
    {
        const auto& song = songs[i];

        // The song that is only a time selection is already drawn as the
        // selection, and drawing it twice says there are two of it.
        if (!song.region.range().exists()) continue;

        blocks.push_back({ song.title.isNotEmpty() ? song.title : song.region.name,
                           song.region.startSec, song.region.endSec,
                           song.selected, editing.count((int)i) > 0, (int)i });
    }

    timeline.setBlocks(std::move(blocks), context.selection);
}

juce::String ExportDialogComponent::albumBlocker() const
{
    // All that is left of the window-wide checks. Everything else moved onto
    // the song, because only the charter is a fact about the session rather
    // than about the music in it.
    if (charterField.text().isEmpty()) return "needs a charter";
    return {};
}

void ExportDialogComponent::generateBackgrounds(bool onlyMissing)
{
    BackgroundGenerator::Options options;
    options.style = (BackgroundGenerator::Style)juce::jmax(0, ChartSettings::backgroundStyle());

    auto folder = backgroundStagingFolder();
    folder.createDirectory();

    for (int index : editing)
    {
        if (!juce::isPositiveAndBelow(index, (int)songs.size())) continue;

        auto& song = songs[(size_t)index];
        if (!song.albumArtFile.existsAsFile()) continue;
        if (onlyMissing && song.backgroundFile.existsAsFile()) continue;

        // Named for the song so two of them cannot land on the same file.
        auto target = folder.getChildFile(juce::String(song.region.guid.hashCode64())
                                            + "-background.png");
        if (BackgroundGenerator::writeTo(target, song.albumArtFile, options))
            song.backgroundFile = target;
    }

    loadSelection();
    refreshRows();
}

bool ExportDialogComponent::selectionInsideRegion() const
{
    if (!context.selection.plausible()) return false;

    // Contained, not merely overlapping: a selection spilling past a region is
    // a different stretch of music and may well want to be its own song.
    for (const auto& region : context.regions)
        if (context.selection.startSec >= region.startSec - 0.01
            && context.selection.endSec <= region.endSec + 0.01)
            return true;

    return false;
}

int ExportDialogComponent::readyCount() const
{
    const bool named = albumBlocker().isEmpty();
    return (int)std::count_if(songs.begin(), songs.end(),
                              [this, named](const Song& song)
                              { return song.selected && song.blocker(named, hasDrums).isEmpty(); });
}

void ExportDialogComponent::refreshValidity()
{
    const int ready = readyCount();
    exportButton.setEnabled(ready > 0);
    exportButton.setButtonText(ready > 0 ? "Export " + juce::String(ready)
                                             + (ready == 1 ? " song" : " songs")
                                         : "Export");
}

void ExportDialogComponent::prefillArtwork()
{
    // Held, not shown: artwork is per song, so this seeds each new song.
    prefilledAlbumArt = findArt(context.artworkSearchPaths, "album");
    prefilledBackground = findArt(context.artworkSearchPaths, "background");
}

ChartExporter::ExportOptions ExportDialogComponent::optionsFor(const Song& song) const
{
    ChartExporter::ExportOptions options;
    options.title = song.title;
    options.track = song.track;
    options.artist = song.artist;
    options.album = song.album;
    options.genre = song.genre;
    options.year = song.year;
    options.charter = charterField.text();
    options.icon = iconField.text();

    options.diffDrums = hasDrums ? song.diffDrums : ChartExporter::ExportOptions::kUnrated;
    options.diffDrumsReal = hasDrums ? song.diffPro : ChartExporter::ExportOptions::kUnrated;
    options.diffEliteDrums = song.diffElite;
    options.proDrums = song.proDrums;
    options.fiveLaneDrums = song.fiveLane;

    options.albumArt = song.albumArtFile;
    options.backgroundArt = song.backgroundFile;
    // Generating is something the dialog does when asked, so the exporter is
    // never left to invent a background nobody has looked at.
    options.generateBackground = false;
    options.backgroundOptions.style =
        (BackgroundGenerator::Style)juce::jmax(0, ChartSettings::backgroundStyle());

    const int format = formatMenu.getSelectedIndex();
    if (juce::isPositiveAndBelow(format, (int)context.audioFormats.size()))
        options.audioFormatCode = context.audioFormats[(size_t)format].formatCode();

    options.packAsSng = packagingMenu.getSelectedIndex() == 1;
    options.destinationRoot = context.destinationRoot;

    ChartExporter::ChartName name;
    name.artist = options.artist;
    name.album = options.album;
    name.track = options.track;
    name.title = options.title;
    options.folderName = name.folderName();

    // A chart with no audio yet always needs a render, whatever the toggle
    // says: the toggle is about skipping work already done, not about
    // shipping a chart with nothing to play.
    const auto existing = ChartExporter::existingAudio(
        context.destinationRoot.getChildFile(options.folderName));
    options.renderAudio = renderAudioToggle.getToggleState() || !existing.existsAsFile();

    return options;
}

//==============================================================================
void ExportDialogComponent::adoptRegions(const juce::StringPairArray& folderNameToGuid)
{
    for (auto& song : songs)
    {
        auto guid = folderNameToGuid[optionsFor(song).folderName];
        if (guid.isNotEmpty()) song.region.guid = guid;
    }
}

juce::ValueTree ExportDialogComponent::toValueTree() const
{
    juce::ValueTree tree(kRemembered);
    tree.setProperty(kPackAsSng, packagingMenu.getSelectedIndex() == 1, nullptr);
    tree.setProperty(kAudioFormat, formatMenu.getSelectedIndex(), nullptr);
    tree.setProperty(kDestination, outputField.text(), nullptr);

    // Whether the audio gets re-rendered is the setting most likely to be
    // turned off and expected to stay off: charting a song means exporting it
    // over and over, and re-encoding every time is the slow part.
    tree.setProperty(kRenderAudio, renderAudioToggle.getToggleState(), nullptr);

    for (const auto& song : songs)
    {
        if (song.region.guid.isEmpty()) continue;

        juce::ValueTree child(kSong);
        child.setProperty(kGuid, song.region.guid, nullptr);

        // Written alongside the GUID as a second way back to this song. A
        // GUID is the right key right up until it is not the same one.
        child.setProperty(kStart, song.region.startSec, nullptr);
        child.setProperty(kEnd, song.region.endSec, nullptr);
        child.setProperty(kTitle, song.title, nullptr);
        child.setProperty(kTrack, song.track, nullptr);
        child.setProperty(kArtist, song.artist, nullptr);
        child.setProperty(kAlbum, song.album, nullptr);
        child.setProperty(kGenre, song.genre, nullptr);
        child.setProperty(kYear, song.year, nullptr);
        child.setProperty(kAlbumArt, song.albumArtFile.getFullPathName(), nullptr);
        child.setProperty(kBackgroundArt, song.backgroundFile.getFullPathName(), nullptr);
        child.setProperty(kDiffDrums, song.diffDrums, nullptr);
        child.setProperty(kDiffPro, song.diffPro, nullptr);
        child.setProperty(kDiffElite, song.diffElite, nullptr);
        child.setProperty(kProDrums, song.proDrums, nullptr);
        child.setProperty(kFiveLane, song.fiveLane, nullptr);
        child.setProperty(kDrumTypeForced, song.drumTypeForced, nullptr);
        child.setProperty(kSelected, song.selected, nullptr);
        tree.appendChild(child, nullptr);
    }

    return tree;
}

void ExportDialogComponent::restore(const juce::ValueTree& tree)
{
    if (!tree.isValid()) return;

    if (tree.hasProperty(kPackAsSng))
        packagingMenu.setSelectedIndex((bool)tree.getProperty(kPackAsSng) ? 1 : 0);
    if (tree.hasProperty(kAudioFormat))
        formatMenu.setSelectedIndex((int)tree.getProperty(kAudioFormat));

    if (tree.hasProperty(kRenderAudio))
        renderAudioToggle.setToggleState((bool)tree.getProperty(kRenderAudio));

    // Only when it was actually typed somewhere else. An empty one would
    // overwrite the folder beside the project with nothing.
    if (auto destination = tree.getProperty(kDestination).toString(); destination.isNotEmpty())
    {
        context.destinationRoot = juce::File::createFileWithoutCheckingPath(destination);
        outputField.setText(destination);
    }

    // Trees written before the metadata moved onto the song carry artist,
    // album, genre, year and artwork on the root, meant for every song at
    // once. Read them first so reopening an old project finds what was typed
    // into it, then let each song's own values override.
    for (auto& song : songs)
    {
        auto legacy = [&tree](juce::String& target, const juce::Identifier& key)
        {
            auto value = tree.getProperty(key).toString();
            if (value.isNotEmpty()) target = value;
        };

        legacy(song.artist, kArtist);
        legacy(song.album, kAlbum);
        legacy(song.genre, kGenre);
        legacy(song.year, kYear);

        juce::File legacyArt(tree.getProperty(kAlbumArt).toString());
        if (legacyArt.existsAsFile()) song.albumArtFile = legacyArt;
        juce::File legacyBackground(tree.getProperty(kBackgroundArt).toString());
        if (legacyBackground.existsAsFile()) song.backgroundFile = legacyBackground;
    }

    for (auto& song : songs)
    {
        auto child = tree.getChildWithProperty(kGuid, song.region.guid);

        // Trees written before the GUID lookup worked are keyed by the name
        // fallback. Only for named regions: every unnamed one shares the key
        // "name:" and would all restore from whichever child came first.
        if (!child.isValid() && song.region.name.isNotEmpty())
            child = tree.getChildWithProperty(kGuid, "name:" + song.region.name);

        // Failing both, the range, which is how applySnapshot already carries
        // a song across a rebuild. A GUID that did not survive the trip out to
        // the project file and back is otherwise everything typed for that
        // song silently gone, and the only sign is a window that forgot.
        if (!child.isValid())
        {
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                const auto candidate = tree.getChild(i);
                if (!candidate.hasProperty(kStart) || !candidate.hasProperty(kEnd)) continue;

                if (std::abs((double)candidate.getProperty(kStart) - song.region.startSec) < 0.01
                    && std::abs((double)candidate.getProperty(kEnd) - song.region.endSec) < 0.01)
                {
                    child = candidate;
                    break;
                }
            }
        }

        if (!child.isValid()) continue;

        // Remembered values win over inferred ones: anything in here was typed
        // by hand, which is a stronger claim than anything read off a filename.
        auto put = [&child](juce::String& target, const juce::Identifier& key)
        {
            auto value = child.getProperty(key).toString();
            if (value.isNotEmpty()) target = value;
        };

        put(song.title, kTitle);
        put(song.track, kTrack);
        put(song.artist, kArtist);
        put(song.album, kAlbum);
        put(song.genre, kGenre);
        put(song.year, kYear);

        // Art paths can go stale between sessions, so a file that has since
        // moved falls back to prefill rather than showing a dead path.
        juce::File art(child.getProperty(kAlbumArt).toString());
        if (art.existsAsFile()) song.albumArtFile = art;
        juce::File background(child.getProperty(kBackgroundArt).toString());
        if (background.existsAsFile()) song.backgroundFile = background;

        if (child.hasProperty(kDiffDrums)) song.diffDrums = (int)child.getProperty(kDiffDrums);
        if (child.hasProperty(kDiffPro))   song.diffPro = (int)child.getProperty(kDiffPro);
        if (child.hasProperty(kDiffElite)) song.diffElite = (int)child.getProperty(kDiffElite);
        // Only a type that was set by hand outranks reading the notes. The
        // saved values themselves are written every time, detected or not, so
        // trusting their presence would mark every reopened song as forced
        // and detection would never run again.
        if (child.hasProperty(kDrumTypeForced))
            song.drumTypeForced = (bool)child.getProperty(kDrumTypeForced);

        if (song.drumTypeForced)
        {
            if (child.hasProperty(kProDrums)) song.proDrums = (bool)child.getProperty(kProDrums);
            if (child.hasProperty(kFiveLane)) song.fiveLane = (bool)child.getProperty(kFiveLane);
        }
        if (child.hasProperty(kSelected))  song.selected = (bool)child.getProperty(kSelected);
    }
}

//==============================================================================
void ExportDialogComponent::drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    if (bounds.isEmpty()) return;

    g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.35f));
    g.fillRoundedRectangle(bounds.toFloat(), panelRadius);

    g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), panelRadius, 1.0f);
}

void ExportDialogComponent::drawHeading(juce::Graphics& g, juce::Rectangle<int> area,
                                        const juce::String& text) const
{
    if (area.isEmpty()) return;

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(fontSmall));
    g.drawText(text, area, juce::Justification::centredLeft, true);
}

void ExportDialogComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::darkBg));

    g.setColour(juce::Colour(Theme::coral));
    g.setFont(Theme::getUIFont(fontTitle));
    g.drawText("Export Chart", ui.title, juce::Justification::centredLeft);

    // What the project holds, in one line. The track names used to be spelled
    // out here; the strip below and the icons beside the title say it faster.
    juce::String summary;
    if (songs.empty())
    {
        // Worth saying out loud rather than refusing to open: the fix is in
        // REAPER, not in here.
        summary << "no regions in this project and no time selection, "
                << "so there is nothing to export";
    }
    else
    {
        summary << (int)songs.size() << (songs.size() == 1 ? " song" : " songs")
                << " \xc2\xb7 " << readyCount() << " ready";
        if (context.trackNames.isEmpty())
            summary << " \xc2\xb7 NO CHART TRACKS FOUND";
    }

    g.setColour(juce::Colour(songs.empty() ? Theme::coral : Theme::textDim));
    g.setFont(Theme::getUIFont(fontBody));
    g.drawText(summary, ui.projectSummary, juce::Justification::centredRight, true);

    drawPanel(g, ui.railPanel);
    drawPanel(g, ui.songPanel);

    drawHeading(g, ui.railHeading, "SONGS");
    drawHeading(g, ui.metaHeading, "METADATA");
    drawHeading(g, ui.chartHeading, "DIFFICULTIES");
    drawHeading(g, ui.drumsHeading, "MISC");

    // Which song the fields belong to is the one thing this window has to say
    // plainly, so the heading counts what is being edited rather than just
    // naming the section.
    juce::String songHeading("SONG");
    if (editing.empty())         songHeading = "NO SONG SELECTED";
    else if (editing.size() > 1) songHeading = juce::String((int)editing.size()) + " SONGS SELECTED";
    drawHeading(g, ui.songHeading, songHeading);

    // What the timeline is showing right now: the range, how long it is, and
    // what is actually in it, so making it a region is an informed click.
    if (!ui.selectionHeading.isEmpty())
    {
        drawHeading(g, ui.selectionHeading, "TIME SELECTION");

        const auto& range = context.selection;

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::getUIFont(fontBody));
        g.drawText(timecodeOf(range.startSec) + " \xe2\x80\x93 " + timecodeOf(range.endSec),
                   ui.selectionRange, juce::Justification::centredLeft);

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(fontSmall));
        g.drawText(timecodeOf(range.endSec - range.startSec),
                   ui.selectionLength, juce::Justification::centredRight);

        // Reported off the notes in this range, not off the project.
        juce::String contents;
        const bool analysed = std::abs(analysedSelection.startSec - range.startSec) < 0.001
                           && std::abs(analysedSelection.endSec - range.endSec) < 0.001;

        if (!analysed)                     contents = "reading notes";
        else if (!selectionDrums.hasDrums) contents = "no drum notes here";
        else
        {
            contents = "drums";
            if (selectionDrums.proDrums) contents << " \xc2\xb7 pro";
            if (selectionDrums.fiveLane) contents << " \xc2\xb7 5-lane";
            if (selectionDrums.tomMarkers > 0)
                contents << " \xc2\xb7 " << selectionDrums.tomMarkers << " toms";
        }

        g.setColour(juce::Colour(analysed && !selectionDrums.hasDrums ? Theme::coral
                                                                     : Theme::textDim));
        g.setFont(Theme::getUIFont(fontSmall));
        g.drawText(contents, ui.selectionContents, juce::Justification::centredLeft, true);
    }

    // Collapsed: who we are and where this lands, in one line. A missing
    // charter is the one thing in here that stops an export, so it is what
    // the line says instead when it is missing. The icon is drawn by the
    // preview beside it rather than named here.
    if (!ui.summaryLine.isEmpty())
    {
        auto blocked = albumBlocker();

        g.setColour(juce::Colour(blocked.isNotEmpty() ? Theme::coral : Theme::textWhite));
        g.setFont(Theme::getUIFont(fontBody));
        g.drawText(blocked.isNotEmpty() ? "every song " + blocked : charterField.text(),
                   ui.summaryLine, juce::Justification::centredLeft, true);
    }

}

void ExportDialogComponent::layoutHeader(juce::Rectangle<int> area)
{
    auto row = area.removeFromTop(titleHeight);

    const auto titleFont = Theme::getUIFont(fontTitle);
    ui.title = row.removeFromLeft((int)titleFont.getStringWidthFloat("Export Chart") + gapM);

    // The instruments sit with the title, since they describe the project and
    // not any one song. The hovered name draws into the room to their right.
    instruments.setBounds(row.removeFromLeft(
        juce::jmin(row.getWidth(), instruments.badgesWidth() + boxMeta)));
    ui.projectSummary = row;

    area.removeFromTop(gapS);
    timeline.setBounds(area.removeFromTop(stripHeight + rulerHeight));
}

void ExportDialogComponent::layoutFooter(juce::Rectangle<int>& content)
{
    auto buttons = content.removeFromBottom(buttonHeight);
    cancelButton.setBounds(buttons.removeFromRight(cancelWidth));
    buttons.removeFromRight(gapS);
    exportButton.setBounds(buttons.removeFromRight(exportWidth));
    content.removeFromBottom(gapS);

    // Only who we are hides behind the button. What this export does is not
    // set once and forgotten: whether the audio is re-rendered is a decision
    // per export, and charting a song means exporting it over and over with
    // no reason to encode the audio again each time.
    for (auto* component : { &charterField, &iconField })
        component->setVisible(settingsOpen);

    // The icon shows either way. Collapsed it is what identifies the charter,
    // and the picture is what anyone recognises: the name beside it was a
    // filename doing the job of a thumbnail.
    iconPreview.setVisible(true);

    auto line = content.removeFromBottom(charterHeight);
    ui.charterLine = line;

    // The icon leads either way. It is the picture that says whose chart this
    // is, and clicking it is how you change it.
    iconPreview.setBounds(line.removeFromLeft(settingsOpen ? iconPreviewWidth : charterIcon)
                              .withSizeKeepingCentre(settingsOpen ? iconPreviewWidth : charterIcon,
                                                     charterIcon));
    line.removeFromLeft(gapS);

    if (settingsOpen)
    {
        iconField.labelWidth = labelTrack;
        iconField.boxWidth = boxNumber + boxRating;
        iconField.labelRight = true;
        iconField.setBounds(line.removeFromLeft(labelTrack + boxNumber + boxRating));
        line.removeFromLeft(sectionGap);

        charterField.labelWidth = labelWidth;
        charterField.boxWidth = boxMeta / 2;
        charterField.labelRight = true;
        charterField.setBounds(line.removeFromLeft(labelWidth + boxMeta / 2));
        line.removeFromLeft(gapS);

        ui.summaryLine = {};
    }
    else
    {
        // The name, and the pencil straight after it rather than off at the
        // far edge: it edits this, so it belongs against this.
        const auto width = Theme::getUIFont(fontBody).getStringWidthFloat(charterField.text());
        ui.summaryLine = line.removeFromLeft(juce::jmax(charterIcon, (int)width + gapS));
        line.removeFromLeft(gapXS);
    }

    // Always right where the charter details are, open or closed, so the way
    // back out is where the way in was.
    charterPencil.setBounds(line.removeFromLeft(pencilSize)
                                .withSizeKeepingCentre(pencilSize, pencilSize));
    // A tick while the row is open, since that click accepts rather than
    // edits, and only there at all while the row is being pointed at.
    charterPencil.setDone(settingsOpen);
    charterPencil.setVisible(settingsOpen || overCharterLine);

    content.removeFromBottom(gapS);

    // What this export does, always reachable. Sized to their content rather
    // than stretched edge to edge, so two unrelated choices stop reading as
    // one bar.
    auto audio = content.removeFromBottom(controlHeight);
    audio.removeFromLeft(labelWidth + labelGap);

    // Three answers on one line, so they read at one size. The shared control
    // font made the checkbox twice the height of the words beside it, which
    // said the checkbox was the important one.
    renderAudioToggle.setLabelHeight(fontBody);
    placeToggle(renderAudioToggle, audio, controlHeight);
    audio.removeFromLeft(sectionGap);

    // Nothing is encoded when nothing is rendered, so the codec stops being
    // a question rather than sitting there looking like a live one.
    formatMenu.setEnabled(renderAudioToggle.getToggleState());
    formatMenu.setBounds(audio.removeFromLeft(formatMenu.preferredWidth(controlHeight)));
    audio.removeFromLeft(sectionGap);
    packagingMenu.setBounds(audio.removeFromLeft(packagingMenu.preferredWidth(controlHeight)));
    content.removeFromBottom(gapS);

    // Where the files land, always on screen and always editable. Collapsed
    // it used to be a folder name folded into a summary, which is the one
    // thing on that line you cannot check by looking at it.
    outputField.labelWidth = labelWidth;
    outputField.boxWidth = 0;
    outputField.labelRight = true;
    outputField.setBounds(content.removeFromBottom(fieldHeight));
    content.removeFromBottom(sectionGap);
}

void ExportDialogComponent::layoutRail(juce::Rectangle<int> area)
{
    ui.railPanel = area;

    auto inner = area.reduced(gapM, gapS);
    ui.railHeading = inner.removeFromTop(headingHeight);
    inner.removeFromTop(gapS);

    // Under the rail: what the timeline is showing, then what to do with it.
    if (createRegionButton.isVisible())
    {
        createRegionButton.setBounds(inner.removeFromBottom(buttonHeight));
        inner.removeFromBottom(gapS);
    }

    if (context.selection.plausible())
    {
        auto panel = inner.removeFromBottom(headingHeight + lineHeight * 2);
        inner.removeFromBottom(gapS);

        ui.selectionHeading = panel.removeFromTop(headingHeight);

        auto range = panel.removeFromTop(lineHeight);
        ui.selectionLength = range.removeFromRight(boxNumber);
        ui.selectionRange = range;
        ui.selectionContents = panel.removeFromTop(lineHeight);
    }

    listView.setBounds(inner);

    // Room for the scrollbar only when there is a scrollbar. Reserving it
    // either way left every row ending a scrollbar's width short of the
    // panel, which is the gutter down the right of a list that never scrolls.
    const int wanted = (int)rows.size() * listRowHeight;
    const int width = wanted > inner.getHeight()
                        ? inner.getWidth() - listView.getScrollBarThickness()
                        : inner.getWidth();

    // Grouping still decides the order, so a release stays together, but it
    // does not put the release's name down the list beside every song in it.
    listContent.setSize(juce::jmax(1, width), juce::jmax(1, wanted));

    int y = 0;
    for (int index : displayOrder)
    {
        rows[(size_t)index]->setBounds(0, y, listContent.getWidth(), listRowHeight);
        y += listRowHeight;
    }
    listContent.setSize(listContent.getWidth(), juce::jmax(1, y));
}

void ExportDialogComponent::layoutSongPane(juce::Rectangle<int> area)
{
    ui.songPanel = area;

    auto inner = area.reduced(gapM, gapS);
    ui.songHeading = inner.removeFromTop(headingHeight);
    inner.removeFromTop(gapS);

    //==========================================================================
    // Artwork down the right, so the metadata stops being handed the whole
    // width for values four characters long.

    auto art = inner.removeFromRight(artWidth);
    inner.removeFromRight(sectionGap);

    // Each picture is its own shape: a cover is square, a background is 16:9,
    // and seeing that is half of knowing which slot is which.
    albumArt.setBounds(art.removeFromTop(headingHeight + artWidth + lineHeight));
    art.removeFromTop(gapS);
    backgroundArt.setBounds(art.removeFromTop(headingHeight + (artWidth * 9) / 16 + lineHeight));

    //==========================================================================
    // The tags, one to a line, the way anything else that edits them sets
    // them out. Labels right against their boxes.
    ui.metaHeading = inner.removeFromTop(headingHeight);
    inner.removeFromTop(gapXS);

    // Every row is the same width, whatever is in it. A column of boxes that
    // all stop in the same place reads as a list of values.
    auto tagRow = [&inner](Field& field)
    {
        field.labelWidth = labelWidth;
        field.boxWidth = boxMeta;
        field.setBounds(inner.removeFromTop(fieldHeight));
        inner.removeFromTop(gapXS);
    };

    tagRow(titleField);
    tagRow(artistField);
    tagRow(albumField);

    // The three short ones share the row rather than each taking a full one,
    // and between them they still measure exactly as wide as the rows above.
    {
        auto row = inner.removeFromTop(fieldHeight);
        inner.removeFromTop(gapXS);

        yearField.labelWidth = labelWidth;
        yearField.boxWidth = boxNumber;
        yearField.setBounds(row.removeFromLeft(labelWidth + boxNumber));
        row.removeFromLeft(gapM);

        trackField.labelWidth = labelTrack;
        trackField.boxWidth = boxNumber;
        trackField.setBounds(row.removeFromLeft(labelTrack + boxNumber));
        row.removeFromLeft(gapM);

        // Genre takes what is left, which is what lands the row's right edge
        // on the same pixel as the rows above it.
        genreField.labelWidth = labelGenre;
        genreField.boxWidth = 0;
        genreField.setBounds(row);
    }

    //==========================================================================
    // What the chart is, as against what the song is. Ratings and drum flags
    // are not metadata and were reading as though they were.
    if (!hasDrums)
    {
        ui.chartHeading = {};
        return;
    }

    inner.removeFromTop(sectionGap);
    ui.chartHeading = inner.removeFromTop(headingHeight);
    inner.removeFromTop(gapS);

    // Every instrument, with its rating on it. This is where you see what is
    // in a chart, which is why the badges are big enough to tell apart.
    difficulties.setBounds(inner.removeFromTop(DifficultyGrid::heightNeeded()));
    inner.removeFromTop(sectionGap);

    //==========================================================================
    // The switches that are not about any one number: format tags you
    // occasionally have to force. The ratings that used to sit here said the
    // same thing the badges above already say, in emptier boxes.
    ui.drumsHeading = inner.removeFromTop(headingHeight);
    inner.removeFromTop(gapXS);

    // At the size of the rows around them, not at control size. These are
    // overrides of something already detected, not the main event.
    auto toggles = inner.removeFromTop(fieldHeight);
    toggles.removeFromLeft(labelWidth + labelGap);

    for (auto* toggle : { &proDrumsToggle, &fiveLaneToggle })
        toggle->setLabelHeight(fontBody);

    placeToggle(proDrumsToggle, toggles, fieldHeight);
    toggles.removeFromLeft(sectionGap);
    placeToggle(fiveLaneToggle, toggles, fieldHeight);
}

void ExportDialogComponent::resized()
{
    ui = {};

    auto content = getLocalBounds().reduced(pad);

    // The header and the footer are fixed height and the middle flexes, so
    // both ends come off first and whatever is left is the middle.
    layoutHeader(content.removeFromTop(titleHeight + gapS + stripHeight + rulerHeight));
    content.removeFromTop(sectionGap);

    layoutFooter(content);

    layoutRail(content.removeFromLeft(railWidth));
    content.removeFromLeft(sectionGap);
    layoutSongPane(content);
}

void ExportDialogComponent::mouseMove(const juce::MouseEvent& e)
{
    // Relative to this, whatever it actually happened over. The icon sits on
    // that line and is a component of its own, so events there arrive in the
    // icon's coordinates and the line never saw the pointer at all.
    const bool over = ui.charterLine.contains(e.getEventRelativeTo(this).getPosition());
    if (over == overCharterLine) return;

    overCharterLine = over;
    charterPencil.setVisible(settingsOpen || overCharterLine);
}

void ExportDialogComponent::mouseDown(const juce::MouseEvent& e)
{
    // The whole line opens it, not just the glyph. A pencil that only appears
    // on hover is not something to aim at, and the name beside it is the
    // thing being edited anyway.
    if (settingsOpen || !ui.charterLine.contains(e.getEventRelativeTo(this).getPosition()))
        return;

    charterPencil.triggerClick();
}

void ExportDialogComponent::mouseExit(const juce::MouseEvent&)
{
    if (!overCharterLine) return;

    overCharterLine = false;
    charterPencil.setVisible(settingsOpen);
}

bool ExportDialogComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDismiss) onDismiss();
        return true;
    }
    return false;
}
