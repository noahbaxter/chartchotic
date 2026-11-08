/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Host/ReaperTrackDetector.h"
#include "DebugTools/Perf.h"
#include "Midi/Processing/NoteProcessor.h"
#include "Visual/Geometry/PositionMath.h"

// CI injects CHARTCHOTIC_VERSION_STRING with full version (e.g. 0.9.5-dev.20260226.abc1234)
// Falls back to JucePlugin_VersionString from .jucer (base semver), then "dev" for unset builds
#ifdef CHARTCHOTIC_VERSION_STRING
  #define CHARTCHOTIC_VERSION CHARTCHOTIC_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHARTCHOTIC_VERSION JucePlugin_VersionString
#else
  #define CHARTCHOTIC_VERSION "dev"
#endif

#if __has_include("BuildInfo.h")
  #include "BuildInfo.h"
#endif

//==============================================================================
ChartchoticAudioProcessorEditor::ChartchoticAudioProcessorEditor(ChartchoticAudioProcessor &p, juce::ValueTree &state)
    : AudioProcessorEditor(&p),
      state(state),
      audioProcessor(p),
      assetManager(),
      interactionController(state),
      toolbar(state, interactionController)
{
    // The person's colours, before anything draws with them. Global rather than part of the
    // project state, so they are already right the first time a window opens.
    PartPalettes::global().load();

    // And the board has to be baked against them. The track art is only rebuilt when its
    // dimensions move, so a window reopening at the size it closed at would otherwise keep
    // whatever the strikeline looked like before the palettes were read. Dirtying the cache
    // here makes the first layout bake rather than reuse.
    trackImageCache.invalidate();

    // Snap counts steps from the same time signature anchors the gridlines do, so the two
    // agree after an odd-length bar.
    interactionController.setGridSectionSource([this](double qn) {
        const juce::ScopedLock lock(audioProcessor.getTempoLock());
        const auto& map = audioProcessor.getTempoTimeSignatureMap();
        GridSection section;
        section.anchorQN = TempoTimeSignatureEventHelper::getLastTimeSigResetPosition(map, PPQ(qn)).toDouble();
        if (auto end = TempoTimeSignatureEventHelper::getNextTimeSigResetPosition(map, PPQ(qn)))
            section.endQN = end->toDouble();
        return section;
    });

    // Create scratch renderer for shared track image cache
    cacheRenderer = std::make_unique<TrackRenderer>(state);

    // Pre-allocate all 4 highway slots (pooled, never destroyed)
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway = std::make_unique<HighwayComponent>(state, assetManager);
        slots[i].highway->setTrackImageCache(&trackImageCache);
        slots[i].highway->setVisible(false);

        // Wire mouse dispatch into the interaction controller.
        auto& hw = *slots[i].highway;
        // Claim focus before dispatching so the edit lands on the highway the
        // mouse is over. Drags do not re-claim, that would retarget the stroke.
        // A move only claims the HOVER, which aims the controller at this highway's lanes so the
        // ghost lands in the right place, without touching focus. That keeps the write bar on
        // the instrument a click will write to, instead of rewriting itself as the cursor
        // crosses a highway of the other family.
        hw.setOnPointerMove       ([this, i](const AuthoringPoint& p, const AuthoringContext& c) { setHoveredSlot(i); interactionController.onPointerMove(p, c); });
        hw.setOnPointerDown       ([this, i](const AuthoringPoint& p, const AuthoringContext& c) { focusSlot(i); interactionController.onPointerDown(p, c); });
        hw.setOnPointerDrag       ([this](const AuthoringPoint& p, const AuthoringContext& c) { interactionController.onPointerDrag(p, c); });
        hw.setOnPointerUp         ([this](const AuthoringPoint& p, const AuthoringContext& c) { interactionController.onPointerUp(p, c); });
        hw.setOnPointerExit       ([this, i]() { if (hoveredSlot == i) { hoveredSlot = -1; hoverDwellStartMs = 0; } interactionController.onPointerExit(); });
        hw.setOnPointerCancel     ([this]() { interactionController.onPointerCancel(); });
        hw.setOnPointerDoubleClick([this, i](const AuthoringPoint& p, const AuthoringContext& c) { focusSlot(i); interactionController.onPointerDoubleClick(p, c); });
        hw.onMouseWheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { handleHighwayScroll(e, w); };

        // Coordinate-domain conversion: HitTestMapper returns "seconds offset
        // from cursor"; the controller wants project QN. Convert at the
        // dispatch boundary (M0-G design rule).
        hw.setSecondsToProjectQN([this](double secondsFromCursor) -> double {
            double cursorQN = lastKnownPosition.toDouble();
            auto& reaperProvider = audioProcessor.getReaperMidiProvider();
            if (reaperProvider.isReaperApiAvailable())
            {
                double cursorTime = reaperProvider.ppqToTime(cursorQN);
                return reaperProvider.timeToPpq(cursorTime + secondsFromCursor);
            }
            // Standard fallback: use instantaneous BPM.
            double bpm = defaultBPM;
            if (auto* playHead = audioProcessor.getPlayHead())
            {
                auto positionInfo = playHead->getPosition();
                if (positionInfo.hasValue())
                    bpm = positionInfo->getBpm().orFallback(defaultBPM);
            }
            return cursorQN + secondsFromCursor * (bpm / 60.0);
        });

        // Inverse of the above — used by the hover-ghost renderer to convert a
        // snapped project QN back into a seconds-from-cursor offset (then into a
        // normalized highway position).
        hw.setProjectQNToSeconds([this](double projectQN) -> double {
            double cursorQN = lastKnownPosition.toDouble();
            auto& reaperProvider = audioProcessor.getReaperMidiProvider();
            if (reaperProvider.isReaperApiAvailable())
            {
                double cursorTime = reaperProvider.ppqToTime(cursorQN);
                double targetTime = reaperProvider.ppqToTime(projectQN);
                return targetTime - cursorTime;
            }
            double bpm = defaultBPM;
            if (auto* playHead = audioProcessor.getPlayHead())
            {
                auto positionInfo = playHead->getPosition();
                if (positionInfo.hasValue())
                    bpm = positionInfo->getBpm().orFallback(defaultBPM);
            }
            return (projectQN - cursorQN) * (60.0 / bpm);
        });

        // One controller feeds every slot, so gate it or the cursor mirrors.
        hw.setOverlayStateGetter([this, i]() -> const OverlayState& {
            static const OverlayState blank;
            // The slot under the cursor, not the focused one: the ghost belongs on the highway
            // being pointed at, which is how you can line a note up on a highway before the
            // click that claims it.
            return i == pointerContextSlot() ? interactionController.getOverlayState() : blank;
        });

        // The one thing that deliberately escapes that gate. A BRE lands on every part at once,
        // so both its drag and its hover show on every highway, not just the focused one.
        hw.setGlobalPhraseGetter([this]() -> HighwayComponent::GlobalPhrase {
            const auto& ov = interactionController.getOverlayState();
            auto ifBre = [](const OverlayState::PhrasePreview& p) {
                return p.visible && (PhraseMath::Kind)p.kind == PhraseMath::Kind::Bre
                     ? p : OverlayState::PhrasePreview{};
            };
            return { ifBre(ov.phrasePreview), ifBre(ov.phraseHover) };
        });

        // Format a project QN as "M.B" for the ghost cursor position label.
        hw.setFormatPositionQN([this](double projectQN) -> juce::String {
            const juce::ScopedLock lock(audioProcessor.getTempoLock());
            auto& map = audioProcessor.getTempoTimeSignatureMap();
            auto mb = TempoTimeSignatureEventHelper::pPqToMeasureBeat(PPQ(projectQN), map);
            if (map.empty()) return juce::String(mb.measure + 1);
            return TempoTimeSignatureEventHelper::formatMeasureBeat(mb.measure + 1,
                                                                    mb.beatInMeasure);
        });
    }

    // Activate slot 0 as default
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;
        interactionController.setActivePart(slot.part);
        toolbar.refreshFromWriteController();
    }
    setLookAndFeel(&chartPreviewLnF);

    // Set up resize constraints
    constrainer.setMinimumSize(minWidth, minHeight);
    setConstrainer(&constrainer);
    setResizable(true, true);

    // Restore saved window size
    int savedWidth = state.hasProperty("editorWidth") ? (int)state["editorWidth"] : defaultWidth;
    int savedHeight = state.hasProperty("editorHeight") ? (int)state["editorHeight"] : defaultHeight;
    savedWidth = juce::jlimit(minWidth, 4096, savedWidth);
    savedHeight = juce::jlimit(minHeight, 4096, savedHeight);
    setSize(savedWidth, savedHeight);

    setWantsKeyboardFocus(true);

    latencyInSeconds = audioProcessor.latencyInSeconds;

#ifdef DEBUG
    bool isStandalone = juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
    debug.onChartLoaded = [this](const DebugMidiFilePlayer::LoadedChart& chart) {
        rebuildSlots(chart);
    };
    debug.init(*this, audioProcessor, state, isStandalone);
#endif

    assets.init(toolbar, [this](auto fn) { forAllHighways(fn); });
    session.init(state, toolbar, audioProcessor,
                 slots.data(), MAX_HIGHWAY_SLOTS, activeSlotCount,
                 {
                     [this]() { resized(); },
                     [this]() { loadState(); },
                     [this](float v) { return computeFarFadeEnd(v); },
                     [this](auto fn) { forEachHighway(fn); },
                     [this](auto fn) { forAllHighways(fn); },
                     [this]() { forAllHighways([this](auto& hw) { hw.applyView(view); }); }
                 });
    initToolbarCallbacks();
    toolbar.setLatencyOffsetRange(CALIBRATION_MIN_MS, CALIBRATION_MAX_MS);

#ifdef DEBUG
    toolbar.getTuningPanel().setAssetManager(assetManager);
#endif

    // Wire and add all pooled highway slots to component tree (hidden by default).
    // In standalone debug mode, rebuildSlots may have already replaced
    // the default slot during init — just re-wire to be safe.
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway->onOverflowChanged = [this]() { resized(); };
        addChildComponent(*slots[i].highway);
    }
    // Make active slots visible
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->setVisible(true);
    addAndMakeVisible(toolbar);

    // Floating write-mode chip — sits over the highway (top-left). Click
    // toggles write mode (same effect as W); state is pushed via the
    // WriteController::onStateChanged callback wired below.
    writeModeIcon.setState(interactionController.writeModeActive(), interactionController.subMode());
    writeModeIcon.onClick = [this]() {
        interactionController.setWriteModeActive(!interactionController.writeModeActive());
    };
    writeModeIcon.onHoverHelp = [this](const HelpText& h) {
        footer.setHelpText(h);
    };
    writeModeIcon.onHoverHelpClear = [this]() {
        footer.clearHelpText();
    };
    addAndMakeVisible(writeModeIcon);
    addAndMakeVisible(utilityStrip);
    utilityStrip.setTempoMap(&audioProcessor.getTempoTimeSignatureMap());
    utilityStrip.setAssetManager(&assetManager);
    utilityStrip.onSeekTo = [this](double targetQN)
    {
        audioProcessor.requestTimelinePositionChange(PPQ(std::max(0.0, targetQN)));
    };
    // Qualified because the constructor's own `state` parameter shadows the member here.
    utilityStrip.onFollowChanged = [this](bool on)
    {
        this->state.setProperty("stripFollow", on, nullptr);
    };
    utilityStrip.onSongRenamed = [this](double startQN, const juce::String& name)
    {
        auto* writer = audioProcessor.getReaperMidiProvider().getWriter();
        if (writer == nullptr || !writer->isAvailable()) return;

        if (!writer->setSongName(startQN, name.toStdString())) return;

        // The name lives on the EVENTS item, so the strip only sees it after a re-read.
        if (auto* session = audioProcessor.getInstrumentSession())
        {
            const auto& tracks = session->getTracks();
            for (int i = 0; i < (int)tracks.size(); ++i)
                if (tracks[i].part == Part::EVENTS)
                    session->invalidateTrack(i);
        }
    };
    writeModeIcon.toFront(false);

    initBottomBar();

#ifdef DEBUG
    // Ensure debug console renders on top of highways
    debug.getConsole().toFront(false);
    debug.getClearButton().toFront(false);
    debug.getCopyButton().toFront(false);
#endif

    // If REAPER session already exists (editor recreated after track move), rebuild from it
    if (audioProcessor.isReaperHost)
    {
        toolbar.setReaperMode(true);
        toolbar.updateVisibility();
        if (auto* instrSession = audioProcessor.getInstrumentSession())
            session.rebuildFromSession(*instrSession);
    }

    loadState();
    resized();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartchoticAudioProcessorEditor::~ChartchoticAudioProcessorEditor()
{
    // Before anything else: it holds callbacks that capture this.
    exportWindow.reset();

    // Shut down async bake thread before destroying members it references
    trackImageCache.shutdown();

    setLookAndFeel(nullptr);

    // Clear static typeface refs so JUCE leak detector doesn't fire on shutdown.
    // Safe in plugin mode too — typefaces are lazily recreated on next use.
    Theme::clearTypefaces();
    ChartchoticLogo::clearTypefaces();
}

// The sub-toolbar follows focus, so the modifier buttons always describe the
// instrument the next click writes to.
void ChartchoticAudioProcessorEditor::focusSlot(int index)
{
    if (index < 0 || index >= activeSlotCount || index == focusedSlot) return;

    focusedSlot = index;
    const auto& slot = slots[index];
    interactionController.setActivePart(slot.part);
    interactionController.setActiveSkill(slot.skillLevel);
    interactionController.setFocusPart(slot.part);
    toolbar.refreshFromWriteController();
    repaint();
}

// Aiming the controller at this highway's lanes, so the move that follows reads against the
// right columns. Deliberately not focusSlot: no toolbar refresh, so the bar stays on the part a
// click would write to rather than flickering between instruments as the cursor travels.
void ChartchoticAudioProcessorEditor::setHoveredSlot(int index)
{
    if (index < 0 || index >= activeSlotCount || index == hoveredSlot) return;

    hoveredSlot = index;
    // A new slot restarts the dwell: it is time spent on ONE highway that earns focus, so
    // travelling across three of them arrives with nothing banked.
    hoverDwellStartMs = juce::Time::getMillisecondCounter();

    // Tracking the hover is for both modes, since the dwell takes focus in both. Aiming the
    // controller at the hovered lanes is draw only, for the reason pointerContextSlot gives:
    // edit mode's selection belongs to the focused highway and must not be retargeted by the
    // cursor passing over another.
    if (interactionController.subMode() != SubMode::Draw) return;

    const auto& slot = slots[index];
    interactionController.setActivePart(slot.part);
    interactionController.setActiveSkill(slot.skillLevel);
}

// Hand focus to a highway the cursor has settled on. Called every frame rather than from the
// move, because the thing being measured is the cursor NOT moving on to somewhere else.
void ChartchoticAudioProcessorEditor::updateHoverFocusDwell()
{
    if (hoverDwellStartMs == 0 || hoveredSlot < 0 || hoveredSlot >= activeSlotCount) return;

    // Already there. Drop the clock rather than hold it, so the next hover starts its own.
    if (hoveredSlot == effectiveFocusSlot())
    {
        hoverDwellStartMs = 0;
        return;
    }

    // Only an idle cursor earns focus this way. Interacting is its own answer: a press takes
    // the highway immediately, so the dwell has nothing to settle. Holding still with a button
    // down would otherwise let the clock run out and retarget the stroke from under it.
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) return;

    if (juce::Time::getMillisecondCounter() - hoverDwellStartMs < hoverFocusDwellMs) return;

    hoverDwellStartMs = 0;
    focusSlot(hoveredSlot);
}

// Read the project's tracks straight from REAPER rather than from discovery, which only
// reports tracks it already recognised. The whole point of the menu is the ones it did not.
void ChartchoticAudioProcessorEditor::refreshPartsMenuTracks()
{
    if (audioProcessor.reaperGetFunc == nullptr) return;
    auto getFunc = [this](const char* name) { return audioProcessor.reaperGetFunc(name); };

    std::vector<PartsMenuComponent::TrackRow> rows;
    const int count = ReaperTrackDetector::getTrackCount(getFunc);
    const int hostTrack = ReaperTrackDetector::detectPluginTrack(getFunc);
    for (int i = 0; i < count; ++i)
    {
        auto name = juce::String(ReaperTrackDetector::getTrackName(getFunc, i));
        // Passed through rather than dropped: the menu hides utility tracks by default and
        // shows them under All tracks, which it cannot do if they never arrive.
        const auto content = ReaperTrackDetector::getTrackContent(getFunc, i);
        rows.push_back({ i, name, content.midi, content.audio,
                         PartMapping::isUtilityTrackName(name), i == hostTrack });
    }

    // Only relayout when the list actually moved: the poll runs this while the menu is open,
    // and resizing the panel every time would fight the scroll position for no reason.
    if (partsMenu.setTracks(std::move(rows)) && partsMenu.isVisible())
        layoutPartsMenu();
}

void ChartchoticAudioProcessorEditor::pollPartsMenuTracks()
{
    if (!partsMenu.isVisible()) return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastPartsMenuPollMs < partsMenuPollIntervalMs) return;

    lastPartsMenuPollMs = now;
    refreshPartsMenuTracks();
}

// The readout is the menu's one control: clicking it again puts the menu away, the way any
// toggle does, so the panel never needs a button of its own to close.
void ChartchoticAudioProcessorEditor::togglePartsMenu()
{
    if (partsMenu.isVisible()) { partsMenu.setVisible(false); return; }
    openPartsMenu();
}

// Make the track REAPER does not have yet and point the part straight at it, so the plus is
// one action rather than "now go and find it in the list".
void ChartchoticAudioProcessorEditor::createTrackForPart(Part part)
{
    if (audioProcessor.reaperGetFunc == nullptr) return;
    auto getFunc = [this](const char* name) { return audioProcessor.reaperGetFunc(name); };

    const auto name = trackNameFor(part);
    if (name.isEmpty()) return;

    const int index = ReaperTrackDetector::createTrackNamed(getFunc, name.toStdString());
    if (index < 0) return;

    audioProcessor.getPartMap().pin(part, index);
    commitPartPins();
    refreshPartsMenuTracks();
    layoutPartsMenu();          // one more row to make room for
}

// Deleting a REAPER track takes its MIDI with it, so it asks first and does nothing on its
// own. Just the question: the X was aimed at a named row a moment ago, and a paragraph
// explaining REAPER's undo is not what anyone reads here.
void ChartchoticAudioProcessorEditor::confirmDeleteTrack(int trackIndex, const juce::String& name)
{
    confirmPrompt.ask("Delete \"" + name + "\"?", {},
                      "Delete", true,
                      [this, trackIndex](bool confirmed)
                      {
                          if (confirmed) deleteTrack(trackIndex);
                      },
                      this);
}

void ChartchoticAudioProcessorEditor::showExportDialog()
{
    // Already open: bring it forward rather than opening a second one.
    if (exportWindow != nullptr)
    {
        exportWindow->toFront(true);
        return;
    }

    auto& provider = audioProcessor.getReaperMidiProvider();
    ChartExporter exporter(provider);
    exporter.logContext();

    auto range = exporter.timeSelection();
    auto regions = exporter.regions();

    // Drums are read off whichever range is to hand, since the answer is a
    // property of how the project is charted rather than of one song.
    auto probe = regions.empty() ? range : regions.front().range();

    ExportDialogComponent::Context context;
    context.selection = range;
    context.regions = regions;
    context.inferred = exporter.inferChartName(probe);
    context.trackNames = exporter.chartTrackNames();
    context.audioFormats = exporter.compressedSinks();
    context.destinationRoot = exporter.exportRoot();
    context.artworkSearchPaths = exporter.artworkSearchPaths(probe);
    context.drums = exporter.drumProfile(probe);
    context.remembered = state.getChildWithName("exportDialog");
    context.charter = ChartSettings::charter();
    context.icon = ChartSettings::icon();

    // Opened even with nothing to export, because a button that does nothing
    // at all reads as broken. The window says what is missing instead.
    auto close = [this]()
    {
        if (exportWindow == nullptr) return;

        // Kept in the project state rather than the settings file: a title and
        // a difficulty belong to this song, not to every song this machine
        // will ever export.
        auto remembered = exportWindow->content().toValueTree();
        state.removeChild(state.getChildWithName("exportDialog"), nullptr);
        state.appendChild(remembered, nullptr);

        // Deferred: the click that closed it is still being delivered to one
        // of the window's own children.
        juce::MessageManager::callAsync([this]() { exportWindow.reset(); });
    };

    exportWindow = std::make_unique<ExportWindow>(std::move(context), close);
    exportWindow->content().onDismiss = close;

    // Saved as it is typed, not only on the way out. Closing is one of the
    // ways a session ends, and the others were losing everything in here.
    exportWindow->content().onStateChanged = [this](const juce::ValueTree& remembered)
    {
        state.removeChild(state.getChildWithName("exportDialog"), nullptr);
        state.appendChild(remembered.createCopy(), nullptr);
    };

    // The project keeps being edited while this is open, so the window reads
    // it rather than holding the snapshot it opened with.
    exportWindow->content().pollProject = [this]()
    {
        ChartExporter live(audioProcessor.getReaperMidiProvider());
        ExportDialogComponent::Snapshot snapshot;
        snapshot.selection = live.timeSelection();
        snapshot.regions = live.regions();
        snapshot.trackNames = live.chartTrackNames();
        return snapshot;
    };

    exportWindow->content().createRegion = [this](const ChartExporter::TimeRange& range,
                                                  const juce::String& name)
    {
        ChartExporter live(audioProcessor.getReaperMidiProvider());
        return live.createRegion(range, name);
    };

    exportWindow->content().setTimeSelection = [this](const ChartExporter::TimeRange& range)
    {
        ChartExporter live(audioProcessor.getReaperMidiProvider());
        live.setTimeSelection(range);
    };

    exportWindow->content().analyseDrums = [this](const ChartExporter::TimeRange& probeRange)
    {
        ChartExporter live(audioProcessor.getReaperMidiProvider());
        return live.drumProfile(probeRange);
    };

    exportWindow->content().analyseParts = [this](const ChartExporter::TimeRange& probeRange)
    {
        ChartExporter live(audioProcessor.getReaperMidiProvider());
        return live.chartTrackNamesIn(probeRange);
    };
    exportWindow->content().onExport = [this, close](const std::vector<ChartExporter::SongExport>& batch)
    {
        // Persisted on export rather than per keystroke, so a half-typed name
        // never becomes the default for every future project.
        if (!batch.empty())
        {
            ChartSettings::setCharter(batch.front().options.charter);
            ChartSettings::setIcon(batch.front().options.icon);
        }

        // A fresh exporter: the window may have been sitting open long enough
        // for the project to have moved on.
        ChartExporter runner(audioProcessor.getReaperMidiProvider());
        auto outcome = runner.runBatch(batch);
        ChartExporter::log(juce::String("[export] ")
                           + (outcome.ok ? "DONE " : "FAILED ") + outcome.message);

        // Before close() saves them: settings key to the new regions, not to
        // the time selection they were exported from.
        if (exportWindow != nullptr)
            exportWindow->content().adoptRegions(outcome.createdRegions);
        for (const auto& failure : outcome.failures)
            ChartExporter::log("[export] " + failure);

        // The folder rather than each chart: eleven Finder windows is not a
        // result anybody wants.
        if (outcome.ok && !outcome.outputs.isEmpty())
            outcome.outputs.getFirst().getParentDirectory().revealToUser();

        close();
    };
}

void ChartchoticAudioProcessorEditor::deleteTrack(int trackIndex)
{
    if (audioProcessor.reaperGetFunc == nullptr) return;
    auto getFunc = [this](const char* name) { return audioProcessor.reaperGetFunc(name); };

    if (!ReaperTrackDetector::deleteTrack(getFunc, trackIndex)) return;

    // Everything after it just renumbered, and pins are held by index, so they have to move
    // with it or they end up pointing at a neighbour.
    audioProcessor.getPartMap().trackDeleted(trackIndex);

    commitPartPins();
    refreshPartsMenuTracks();
    layoutPartsMenu();
}

void ChartchoticAudioProcessorEditor::openPartsMenu()
{
    refreshPartsMenuTracks();
    layoutPartsMenu();
    partsMenu.setVisible(true);
    partsMenu.toFront(true);
    partsMenu.grabKeyboardFocus();   // so Escape closes it
}

// Out of the corner it was opened from, at the size the table actually needs, and never
// past the window: a big project fills the plugin, a small one stays a panel.
void ChartchoticAudioProcessorEditor::layoutPartsMenu()
{
    // Between the toolbar and the footer. The menu sits over the scene and everything in it,
    // the write mode button included, but the toolbar stays reachable: a panel that covers
    // its own header has nothing left to anchor it, which is what a tall project did.
    const int top = toolbar.getBottom() + partsMenuMargin;
    const int bottom = getHeight() - footer.getHeight() - partsMenuMargin;
    // Starts clear of the strip rather than over it: the strip is how you know where you are
    // in the song, and the menu is a thing you open while reading it.
    const int left = (utilityStripVisible ? utilityStrip.getRight() : 0) + partsMenuMargin;
    const auto available = juce::Rectangle<int>(left, top,
                                                juce::jmax(0, getWidth() - left - partsMenuMargin),
                                                juce::jmax(0, bottom - top));

    // The toolbar's own scale, so a part icon in the table is the same size as the one in the
    // header. Taken before asking what the panel needs, since it sets the row height.
    partsMenu.setChromeScale((float)toolbar.getStripHeight()
                             / (float)ToolbarComponent::referenceHeight);

    // Full width always, so the columns have every pixel going and only scroll when even that
    // is not enough. Height is what the rows need, capped, with the rest scrolling.
    const int h = juce::jmin(partsMenu.preferredSize().y, available.getHeight());

    partsMenu.setBounds(available.getX(), available.getBottom() - h, available.getWidth(), h);

    // Last word on z-order. The write mode chip and the strip both raise themselves during
    // layout, so the menu has to re-claim the front after them or it opens underneath.
    if (partsMenu.isVisible()) partsMenu.toFront(false);
}

// Save the change and put it to work in one step. Discovery reads the same map, so a pin has
// to re-run it or the highways keep showing whatever the last scan decided.
void ChartchoticAudioProcessorEditor::commitPartPins()
{
    auto pins = state.getOrCreateChildWithName("partPins", nullptr);
    audioProcessor.getPartMap().writeTo(pins);
    audioProcessor.rediscoverParts();

    // Same path the 50ms poll takes when a track changes under us, so a pin lands the way
    // any other discovery change does rather than through a route of its own.
    if (auto* instrSession = audioProcessor.getInstrumentSession())
        session.rebuildFromSession(*instrSession);

    partsMenu.refresh();
    repaint();
}

void ChartchoticAudioProcessorEditor::updateMonitor()
{
    auto& monitor = audioProcessor.getMonitor();
    const bool clickOn = (bool)state.getProperty("monitorClick", false);
    const bool notesOn = (bool)state.getProperty("monitorNotes", false);
    monitor.setClickEnabled(clickOn);
    monitor.setNotesEnabled(notesOn);
    if (!clickOn && !notesOn) return;

    auto* session = audioProcessor.getInstrumentSession();
    if (session == nullptr || activeSlotCount <= 0) return;

    const auto& slot = slots[effectiveFocusSlot()];
    const bool kick2x = !state.hasProperty("kick2x") || (bool)state["kick2x"];
    const bool proDrums = (DrumType)(int)state.getProperty("drumType") == DrumType::PRO;
    const bool dynamics = (bool)state.getProperty("dynamics");

    // A change in what plays rebuilds at once; edits to the chart are caught by the timer.
    const juce::String key = juce::String((int)slot.part) + "/" + juce::String((int)slot.skillLevel)
                           + "/" + juce::String((int)kick2x) + "/" + juce::String((int)proDrums)
                           + "/" + juce::String((int)dynamics);
    const juce::int64 now = juce::Time::getHighResolutionTicks();
    const double elapsed = (now - lastMonitorBuildTicks)
                         / (double)juce::Time::getHighResolutionTicksPerSecond();
    if (key == lastMonitorKey && elapsed < 0.5) return;
    lastMonitorKey = key;
    lastMonitorBuildTicks = now;

    auto built = std::make_shared<MonitorTriggers>();

    const auto& tracks = session->getTracks();
    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        if (tracks[i].part != slot.part) continue;
        const auto& notes = session->getNotes(i);
        if (slot.part == Part::ELITE_DRUMS)
            built->notes = MonitorTriggerBuilder::eliteDrums(notes, slot.skillLevel, kick2x, dynamics);
        else if (isDrumLike(slot.part))
            built->notes = MonitorTriggerBuilder::drums(notes, slot.skillLevel, kick2x, proDrums, dynamics);
        else if (isGuitarLike(slot.part))
            built->notes = MonitorTriggerBuilder::guitar(notes, slot.skillLevel);
        break;
    }

    // The click runs to the end of the last chart, so it covers the whole project.
    const auto& spans = session->getClipSpans();
    const double endQN = spans.empty() ? 0.0 : spans.back().endQN;
    // Copied out first, so the audio thread never waits on a whole project's worth of clicks.
    TempoTimeSignatureMap tempoMap;
    {
        const juce::ScopedLock lock(audioProcessor.getTempoLock());
        tempoMap = audioProcessor.getTempoTimeSignatureMap();
    }
    built->click = MonitorTriggerBuilder::metronome(tempoMap, 0.0, endQN);

    monitor.publish(std::move(built));
}

void ChartchoticAudioProcessorEditor::onFrame()
{
    if (targetFrameInterval > 0.0)
    {
        juce::int64 now = juce::Time::getHighResolutionTicks();
        double ticksPerSec = static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        double elapsed = (now - lastFrameTicks) / ticksPerSec;
        if (elapsed < targetFrameInterval)
            return;

        // The frame boundary. Everything from here to the next one belongs to this frame,
        // including the paints JUCE runs after this tick returns.
        Perf::beginFrame();

        // Advance by target interval to stay on a stable grid.
        // If we overshot by more than a full interval (e.g. app was suspended), snap to now.
        juce::int64 intervalTicks = static_cast<juce::int64>(targetFrameInterval * ticksPerSec);
        lastFrameTicks += intervalTicks;
        if ((now - lastFrameTicks) / ticksPerSec > targetFrameInterval)
            lastFrameTicks = now;
    }

    // Closed by hand at buildFrameContext. Everything before that point is the per-frame sync
    // and pollForChanges, which is the REAPER API traffic no offline harness can see.
    Perf::Scope tickScope { "tick/sync" };

    printCallback();

    updateHoverFocusDwell();

    // Frame delta tracking (used by FPS overlay and debug profiler)
    double frameDelta_us_local = 0.0;
    {
        double now = juce::Time::getHighResolutionTicks()
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (lastFrameTimestamp > 0.0)
        {
            frameDelta_us_local = (now - lastFrameTimestamp) * 1000000.0;
            fpsRing[fpsRingIndex] = frameDelta_us_local;
            fpsRingIndex = (fpsRingIndex + 1) % FPS_RING_SIZE;
        }
        lastFrameTimestamp = now;
    }

#ifdef DEBUG
    debug.beginFrame(frameDelta_us_local);
    debug.onFrame(lastKnownPosition, lastPlayingState);
#endif

    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    if (isReaperMode && !toolbar.isReaperModeActive())
    {
        toolbar.setReaperMode(true);
        toolbar.updateVisibility();
    }

    // Per-frame sync — wires MidiWriter, InstrumentSession, playingState,
    // part, skill into both sub-controllers, then ticks frame timers.
    if (activeSlotCount > 0)
    {
        // Multi-difficulty layouts share a part across slots and differ only by
        // skill, so take both from the slot rather than the global setting.
        // Patches are keyed by lane and QN with no track, so an unfocused
        // highway reading them flashes the note. Routed per frame: the layout
        // changes in too many places to hook each one.
        const int fs = effectiveFocusSlot();
        for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
            slots[i].highway->setPatchBuffer(
                i == fs ? &interactionController.getPatchBuffer() : nullptr);

        // Points resolve against the highway under the cursor so its ghost is right, while the
        // write bar keeps describing the focused slot. Re-asserted every frame because the
        // ghost is recomputed on the frame tick too, not only when the mouse moves.
        const auto& context = slots[pointerContextSlot()];
        interactionController.setFocusPart(slots[fs].part);
        interactionController.onFrame(
            audioProcessor.getReaperMidiProvider().getWriter(),
            audioProcessor.getInstrumentSession(),
            &lastPlayingState,
            context.part,
            context.skillLevel,
            lastKnownPosition.toDouble());
    }

    updateTrackInfoDisplay();
    pollPartsMenuTracks();

    // Create InstrumentSession on first REAPER detection (if multi-highway enabled)
    bool trackDiscovery = !state.hasProperty("trackDiscovery") || (bool)state["trackDiscovery"];
    if (isReaperMode && trackDiscovery && !audioProcessor.getInstrumentSession())
    {
        audioProcessor.createInstrumentSession();
        if (auto* instrSession = audioProcessor.getInstrumentSession())
            session.rebuildFromSession(*instrSession);
    }

    // Track position changes for render logic
    if (auto* playHead = audioProcessor.getPlayHead()) {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue()) {
            PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
            bool isCurrentlyPlaying = positionInfo->getIsPlaying();

            lastKnownPosition = currentPosition;
            lastPlayingState = isCurrentlyPlaying;

            // REAPER mode: single owner of REAPER API traffic. Must run whether
            // playing or paused — the audio thread no longer calls REAPER API
            // (would deadlock during track duplication).
            // Hash-gated: pollReaperMidiHash only refetches when the track hash
            // actually changes, so the hot path is cheap during playback.
            if (isReaperMode)
            {
                juce::int64 now = juce::Time::getHighResolutionTicks();
                double elapsed = (now - lastCacheInvalidationTicks)
                    / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
                if (elapsed >= 0.05)
                {
                    lastCacheInvalidationTicks = now;
                    audioProcessor.pollReaperMidiHash();

                    // Poll InstrumentSession for changes (re-fetch affected tracks)
                    if (auto* instrSession = audioProcessor.getInstrumentSession())
                    {
                        if (instrSession->pollForChanges())
                            session.rebuildFromSession(*instrSession);
                    }
                }
            }
            else
            {
                lastCacheInvalidationTicks = 0;
            }
        }
    }

    updateMonitor();

    // Build frame data and push to each active highway slot
#ifdef DEBUG
    auto dataBuildStart = std::chrono::high_resolution_clock::now();
#endif
    tickScope.close();
    PERF_SCOPE("tick/ctx");
    auto ctx = buildFrameContext();
    HighwayFrameData primaryFrameData;

    if (utilityStripVisible)
    {
        auto* instrSession = audioProcessor.getInstrumentSession();
        utilityStrip.setSections(instrSession ? &instrSession->getSongSections() : nullptr);
        utilityStrip.setClipSpans(instrSession ? &instrSession->getClipSpans() : nullptr);
        utilityStrip.setSongs(instrSession ? &instrSession->getSongs() : nullptr);

        // One note column per visible highway, so a single instrument spreads across the
        // strip and a full band shares it.
        std::vector<UtilityStripComponent::NoteSource> sources;
        if (instrSession != nullptr)
        {
            const auto& tracks = instrSession->getTracks();
            for (int s = 0; s < activeSlotCount; ++s)
            {
                UtilityStripComponent::NoteSource src;
                src.part = slots[s].part;
                src.skill = slots[s].skillLevel;
                for (int i = 0; i < (int)tracks.size(); ++i)
                    if (tracks[i].part == src.part)
                    {
                        src.notes = &instrSession->getNotes(i);
                        break;
                    }
                sources.push_back(src);
            }
        }

        utilityStrip.setNoteSources(std::move(sources));

        // The view is a write-mode overlay, so with write mode off the strip reads as Notes.
        utilityStrip.setEditorView(interactionController.writeModeActive()
                                   ? interactionController.view() : EditorView::Notes);

        utilityStrip.setPlaying(audioProcessor.isPlaying);
        utilityStrip.setPlayhead(lastKnownPosition);
        utilityStrip.repaint();
    }

#ifdef DEBUG
    // Standalone debug mode: bypass normal pipeline
    if (!isReaperMode && debug.isStandalone() && debug.isNotesActive())
    {
        HighwayFrameData frameData;
        debug.buildStandaloneFrameData(frameData, primaryHighway().getSceneRenderer(),
                                       primaryInterpreter(), displaySizeInPPQ.toDouble(),
                                       displayWindowTimeSeconds);
        frameData.scrollOffset = computeScrollOffset();
        frameData.builtForPart = primaryInterpreter().instrumentPart;
        primaryFrameData = frameData;
        slots[0].highway->setFrameData(frameData);
        slots[0].highway->repaint();
    }
    else
#endif
    if (isReaperMode && activeSlotCount > 1)
    {
        FrameDataBuilder::buildReaperBatched(primaryFrameData, ctx);
    }
    else
    {
        PERF_SCOPE("tick/buildFrameData");
        for (int i = 0; i < activeSlotCount; i++)
        {
            auto& slot = slots[i];
            HighwayFrameData frameData;
            if (isReaperMode)
                FrameDataBuilder::buildReaper(frameData, ctx, *slot.interpreter);
            else
                FrameDataBuilder::buildStandard(frameData, ctx, *slot.interpreter);
            frameData.builtForPart = slot.interpreter->instrumentPart;

#ifdef DEBUG
            // Only the frame profiler reads this back after the loop, so in release the copy
            // was pure waste and the windows can go straight into the slot.
            if (i == 0)
                primaryFrameData = frameData;
#endif
            slot.highway->setFrameData(std::move(frameData));
            slot.highway->repaint();
        }
    }
#ifdef DEBUG
    double dataBuild_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - dataBuildStart).count();
    {
        SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
        Part part = activeSlotCount == 0 ? getPartFromState(state) : primaryHighway().getActivePart();
        int vpW = activeSlotCount == 0 ? 0 : primaryHighway().renderWidth;
        int vpH = activeSlotCount == 0 ? 0 : primaryHighway().renderHeight;
        debug.recordFrameData(primaryFrameData, dataBuild_us,
            activeSlotCount, part, skill, vpW, vpH, lastPlayingState);
    }
#endif

    // The editor's own painting is a full-window backdrop plus the banner, and none of it moves
    // between section boundaries, so repainting the lot every vblank dragged the background,
    // toolbar, footer and strip through the rasteriser for nothing. Repaint only what changes.
    // Debug overlays are the exception: they redraw their own numbers every frame.
    constexpr bool debugOverlaysNeedFullRepaint =
       #ifdef DEBUG
        true;
       #else
        false;
       #endif

    if (debugOverlaysNeedFullRepaint)
    {
        repaint();
        return;
    }

    // Slack covers the rounded pill drawFpsOverlay paints outside the text rect.
    if (showFps)
        repaint(fpsOverlayBounds().expanded(8));

    if (sectionBannerShown())
    {
        auto song = utilityStrip.currentSongName();
        auto section = utilityStrip.currentSectionName();
        if (song != lastBannerSong || section != lastBannerSection)
        {
            lastBannerSong = song;
            lastBannerSection = section;
            repaint(sectionBannerBounds());
        }
    }
}

void ChartchoticAudioProcessorEditor::initToolbarCallbacks()
{
    toolbar.onSkillChanged = [this](int id) {
        state.setProperty("skillLevel", id, nullptr);
        propagateToSlots("skillLevel", id);
        if (session.isActive())
        {
            session.getEnabledDifficulties() = { (SkillLevel)id };
            toolbar.setEnabledDifficulties(session.getEnabledDifficulties());
            session.rebuildVisibleSlots();
        }
    };

    toolbar.onDifficultyClicked = [this](SkillLevel skill, bool modifierHeld) {
        if (!session.isActive()) return;

        auto& diffs = session.getEnabledDifficulties();
        if (modifierHeld)
        {
            if (diffs.count(skill))
                diffs.erase(skill);
            else
                diffs.insert(skill);
        }
        else
        {
            diffs.clear();
            diffs.insert(skill);
        }

        if (diffs.size() > 1)
            session.forceSingleInstrument();

        if (!diffs.empty())
            state.setProperty("skillLevel", (int)*diffs.begin(), nullptr);

        toolbar.setEnabledDifficulties(diffs);
        session.rebuildVisibleSlots();
    };

    toolbar.onAllDifficultiesClicked = [this]() {
        if (!session.isActive()) return;
        session.getEnabledDifficulties() = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                SkillLevel::HARD, SkillLevel::EXPERT };

        session.forceSingleInstrument();

        toolbar.setEnabledDifficulties(session.getEnabledDifficulties());
        session.rebuildVisibleSlots();
    };

    toolbar.onInstrumentClicked = [this](Part part, bool modifierHeld) {
        if (!session.isActive()) return;

        auto& parts = session.getEnabledParts();
        if (modifierHeld)
        {
            if (parts.count(part))
                parts.erase(part);
            else
                parts.insert(part);
        }
        else
        {
            parts.clear();
            parts.insert(part);
        }

        if (parts.size() > 1)
            session.forceSingleDifficulty();

        toolbar.setEnabledParts(parts);
        session.rebuildVisibleSlots();
        if (activeSlotCount > 0)
        {
            interactionController.setActivePart(slots[0].part);
            toolbar.refreshFromWriteController();
        }
    };

    toolbar.onAllInstrumentsClicked = [this]() {
        if (!session.isActive()) return;
        auto& parts = session.getEnabledParts();
        parts.clear();
        for (auto p : session.getDiscoveredParts())
            parts.insert(p);

        session.forceSingleDifficulty();

        toolbar.setEnabledParts(parts);
        session.rebuildVisibleSlots();
        if (activeSlotCount > 0)
        {
            interactionController.setActivePart(slots[0].part);
            toolbar.refreshFromWriteController();
        }
    };

    toolbar.onPartChanged = [this](int id) {
        if (activeSlotCount == 0) return;
        state.setProperty("part", id, nullptr);
        Part newPart = getPartFromState(state);
        primaryInterpreter().instrumentPart = newPart;
        slots[0].part = newPart;
        interactionController.setActivePart(newPart);
        toolbar.refreshFromWriteController();

        if (!PositionMath::bemaniMode)
        {
            float userLength = state.hasProperty("highwayLength")
                ? (float)state["highwayLength"] : FAR_FADE_DEFAULT;
            float scaledLength = computeFarFadeEnd(userLength);
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            trackImageCache.invalidate();
            requestAsyncRebake();
        }
        updateDisplaySizeFromSpeedSlider();

        forEachHighway([](auto& hw) { hw.onInstrumentChanged(); });
#ifdef DEBUG
        toolbar.getTuningPanel().setDrums(isDrumLike(getPartFromState(state)));
#endif
        propagateToSlots("part", id);
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        propagateToSlots("drumType", id);
    };

    toolbar.onAutoHopoChanged = [this](bool enabled) {
        state.setProperty("autoHopo", enabled, nullptr);
        propagateToSlots("autoHopo", enabled);
    };
    toolbar.onHopoThresholdChanged = [this](int thresholdIndex) {
        state.setProperty("hopoThresh", thresholdIndex, nullptr);
        propagateToSlots("hopoThresh", thresholdIndex);
    };

    toolbar.onLaneToolActive = [this](bool on) { forEachHighway([on](auto& hw) { hw.setLaneToolActive(on); }); };
    toolbar.onViewChanged = [this](const ViewOptions& v) { view = v; applyView(); };
    toolbar.onUtilityStripToggled = [this](bool on) { setUtilityStripVisible(on); };

    toolbar.onKick2xChanged = [this](bool on) { state.setProperty("kick2x", on, nullptr); propagateToSlots("kick2x", on); };
    toolbar.onExportChart = [this]() { showExportDialog(); };
    toolbar.onDiscoFlipChanged = [this](bool on) { state.setProperty("discoFlip", on, nullptr); propagateToSlots("discoFlip", on); };
    toolbar.onDynamicsChanged = [this](bool on) { state.setProperty("dynamics", on, nullptr); propagateToSlots("dynamics", on); };

    toolbar.onNoteSpeedChanged = [this](int speed) {
        state.setProperty("noteSpeed", speed, nullptr);
        updateDisplaySizeFromSpeedSlider();
        if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            audioProcessor.invalidateReaperCache();
    };

    toolbar.onFramerateChanged = [this](int id) {
        state.setProperty("framerate", id, nullptr);
        switch (id) {
        case 1: targetFrameInterval = 1.0 / 15.0; break;
        case 2: targetFrameInterval = 1.0 / 30.0; break;
        case 3: targetFrameInterval = 1.0 / 60.0; break;
        default: targetFrameInterval = 0.0; break;
        }
    };

    toolbar.onShowFpsChanged = [this](bool on) { showFps = on; };
    toolbar.onShowBackgroundChanged = [this](bool on) { applyBackgroundVisibility(on); };

    toolbar.onTrackDiscoveryChanged = [this](bool on) {
        lastDisplayedTrackNumber = -1; // force status bar refresh
        lastDisplayedTrackName = {};
        if (on)
        {
            // Re-create InstrumentSession and rebuild from discovery
            if (!audioProcessor.getInstrumentSession())
            {
                audioProcessor.createInstrumentSession();
                if (auto* instrSession = audioProcessor.getInstrumentSession())
                    session.rebuildFromSession(*instrSession);
            }
        }
        else
        {
            // Tear down discovery, fall back to single-instrument mode.
            // Uses the processor's own noteStateMapArray (fed by ReaperMidiPipeline
            // reading from the plugin's track). Multi-difficulty still works.
            audioProcessor.destroyInstrumentSession();

            for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
            {
                slots[i].active = false;
                slots[i].highway->setVisible(false);
            }

            auto& slot = slots[0];
            Part currentPart = getPartFromState(state);
            Part fallbackPart = isDrumLike(currentPart) ? Part::DRUMS : Part::GUITAR;
            state.setProperty("part", (int)fallbackPart, nullptr);
            slot.part = fallbackPart;
            slot.active = true;
            slot.ownedState.reset();
            slot.interpreter = std::make_unique<MidiInterpreter>(
                state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
            slot.highway->setActivePart(slot.part);
            slot.highway->showPartLabel = false;
            slot.highway->showDifficultyLabel = false;
            slot.highway->setVisible(true);
            activeSlotCount = 1;
            interactionController.setActivePart(slot.part);
            toolbar.refreshFromWriteController();

            toolbar.setMultiInstrumentMode(false);
            toolbar.resetToManualMode();
            resized();
            loadState();
        }
    };

    toolbar.onLatencyChanged = [this](int id) { state.setProperty("latency", id, nullptr); applyLatencySetting(id); };
    toolbar.onLatencyOffsetChanged = [this](int offsetMs) {
        if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            audioProcessor.invalidateReaperCache();
    };

    toolbar.onBackgroundChanged = [this](const juce::String& bgName) { state.setProperty("background", bgName, nullptr); assets.loadBackground(bgName); };

    toolbar.onGemScaleChanged = [this](float scale) { state.setProperty("gemScale", scale, nullptr); propagateToSlots("gemScale", scale); forEachHighway([scale](auto& hw) { hw.setGemScale(scale); }); };
    toolbar.onBarScaleChanged = [this](float scale) { state.setProperty("barScale", scale, nullptr); propagateToSlots("barScale", scale); forEachHighway([scale](auto& hw) { hw.setBarScale(scale); }); };

    toolbar.onHighwayLengthChanged = [this](float length) {
        state.setProperty("highwayLength", length, nullptr);
        if (!PositionMath::bemaniMode)
        {
            float scaledLength = computeFarFadeEnd(length);
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            trackImageCache.invalidate();
            requestAsyncRebake();
        }
        else
        {
            forEachHighway([this, length](auto& hw) { hw.setHighwayLength(computeFarFadeEnd(length)); });
        }
        updateDisplaySizeFromSpeedSlider();
    };

    toolbar.onStretchChanged = [this](bool on) { forEachHighway([on](auto& hw) { hw.stretchToFill = on; }); state.setProperty("stretchToFill", on, nullptr); resized(); };

    toolbar.onBemaniModeChanged = [this](bool on) {
        PositionMath::bemaniMode = on;
        PositionMath::bemaniHwyScale = 1.0f;
        state.setProperty("bemaniMode", on, nullptr);
        updateDisplaySizeFromSpeedSlider();
        trackImageCache.invalidate();
        resized();
        toolbar.resized();
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty()) forAllHighways([](auto& hw) { hw.clearTexture(); });
        else assets.loadHighwayTexture(textureName);
    };

    toolbar.onTextureScaleChanged = [this](float scale) { state.setProperty("textureScale", scale, nullptr); forAllHighways([scale](auto& hw) { hw.setTextureScale(scale); }); };
    toolbar.onTextureOpacityChanged = [this](float opacity) { state.setProperty("textureOpacity", opacity, nullptr); forAllHighways([opacity](auto& hw) { hw.setTextureOpacity(opacity); }); };

    toolbar.onOpenBackgroundFolder = [this]() { assets.getBackgroundDirectory().revealToUser(); };
    toolbar.onOpenTextureFolder = [this]() { assets.getHighwayTextureDirectory().revealToUser(); };

    // Refresh the floating mode chip + sub-toolbar whenever write-mode state
    // changes (W/Q toggles, [/]/S/T grid changes). When the sub-toolbar's
    // visibility flips, the toolbar reports a new height — trigger our own
    // resized() so the highway reclaims/yields the row's space.
    // A refused phrase says why in the footer, where the rest of the write-mode help goes.
    interactionController.setPhraseRefusedHandler([this](const juce::String& reason) {
        footer.setHelpText({ dim(reason) });
    });

    interactionController.setChartWrittenHandler([this]() {
        audioProcessor.invalidateReaperCache();
    });

    interactionController.setNameRequestHandler(
        [this](const juce::String& title, const juce::String& initial,
               std::function<void(const juce::String&)> done) {
            textPrompt.ask(title, initial, "OK", std::move(done), this);
        });

    interactionController.onStateChanged = [this]() {
        writeModeIcon.setState(interactionController.writeModeActive(), interactionController.subMode());
        bool wm = interactionController.writeModeActive();
        auto view = interactionController.view();
        bool bar = interactionController.barMode();
        // Pushed here with the rest of the write state, not only from the toolbar's own button:
        // the keyboard and the mode switch turn the tool off too, and a highway left holding a
        // stale flag keeps dimming the whole board for a tool that is no longer on.
        bool lane = interactionController.laneTool();

        forAllHighways([wm, view, bar, lane](auto& hw) {
            hw.setWriteMode(wm);
            hw.setEditorView(view);
            hw.setBarMode(bar);
            hw.setLaneToolActive(lane);
        });

        if (toolbar.refreshFromWriteController())
            resized();

        updateFooterHelpText();
        forEachHighway([](auto& hw) { hw.repaint(); });
    };

    toolbar.getWriteSubToolbar().setAssetManager(&assetManager);
    toolbar.getWriteSubToolbar().onHoverHelp = [this](const HelpText& h) {
        footer.setHelpText(h);
    };
    toolbar.getWriteSubToolbar().onHoverHelpClear = [this]() {
        footer.clearHelpText();
    };

#ifdef DEBUG
    debug.wireCallbacks(toolbar, primaryHighway(), [this]() { repaint(); });
#endif
}

void ChartchoticAudioProcessorEditor::initBottomBar()
{
#if defined(CHARTCHOTIC_BUILD_TIMESTAMP) && defined(CHARTCHOTIC_BUILD_SHA)
    footer.init(juce::String("v") + CHARTCHOTIC_VERSION + "." + CHARTCHOTIC_BUILD_TIMESTAMP + "." + CHARTCHOTIC_BUILD_SHA);
#else
    footer.init(juce::String("v") + CHARTCHOTIC_VERSION);
#endif
    updateFooterHelpText();
    addAndMakeVisible(footer);

    // The parts menu is only reachable in REAPER: outside it there is no project to map
    // tracks from, so the readout stays the inert status text it has always been.
    if (audioProcessor.isReaperHost)
    {
        auto& partMap = audioProcessor.getPartMap();
        partMap.readFrom(state.getOrCreateChildWithName("partPins", nullptr));
        audioProcessor.rediscoverParts();   // pins loaded from state have to take effect too

        footer.getStatusBar().onTrackLabelClicked = [this]() { togglePartsMenu(); };

        partsMenu.setMap(&partMap);
        partsMenu.onPin   = [this](Part p, int track)
                            { audioProcessor.getPartMap().pin(p, track); commitPartPins(); };
        partsMenu.onClearPart = [this](Part p)
                            { audioProcessor.getPartMap().clearPart(p);  commitPartPins(); };
        partsMenu.onCreateTrackForPart = [this](Part p) { createTrackForPart(p); };
        partsMenu.onDeleteTrack = [this](int index, juce::String name)
                                  { confirmDeleteTrack(index, name); };
        partsMenu.onPinWaveform   = [this](int track)
                            { audioProcessor.getPartMap().pinWaveform(track); commitPartPins(); };
        partsMenu.onUnpinWaveform = [this]()
                            { audioProcessor.getPartMap().unpinWaveform();    commitPartPins(); };
        partsMenu.onDismiss  = [this]() { partsMenu.setVisible(false); };
        partsMenu.onEditColours = [this](Part part)
        {
            // The menu covers the highway, and the highway is what you are picking colours
            // against, so opening the picker puts the menu away.
            partsMenu.setVisible(false);
            palettePicker.open(part, this);
        };
        // Every drag of the wheel repaints the board, so the colours are chosen against the
        // chart rather than against a swatch.
        palettePicker.onChanged = [this]()
        {
            // The strikeline pads are baked into the shared track image, so a repaint alone
            // would redraw the gems in their new colours over pads still in the old ones.
            trackImageCache.invalidate();
            requestAsyncRebake();
            forAllHighways([](auto& hw) { hw.repaintTrackArt(); });

            partsMenu.repaint();
            repaint();
        };
        // confirmPrompt is not a child: it puts itself on the desktop as its own window.
        addChildComponent(partsMenu);
    }

    updateBanner.onPromptDismissed = [this]() { audioProcessor.updatePromptDismissed = true; };
    updateChecker.onUpdateCheckComplete = [this](const UpdateChecker::UpdateInfo& info)
    {
        if (info.available)
        {
            bool autoPrompt = !audioProcessor.updatePromptDismissed;
            updateBanner.setUpdateInfo(info.version, info.downloadUrl, info.channelLabel, info.displayMessage, autoPrompt);
            footer.setUpdateAvailable();
            resized();
        }
    };
#ifndef DEBUG
    updateChecker.checkForUpdates();
#endif
}

void ChartchoticAudioProcessorEditor::updateTrackInfoDisplay()
{
    // Prefer generic track info (works in any DAW via JUCE/VST3)
    int trackNum = audioProcessor.detectedTrackNumber.load();
    juce::String trackName = audioProcessor.getDetectedTrackName();

    // REAPER fallback: use reaperTrack state property for track number
    if (trackNum < 0 && audioProcessor.isReaperHost)
        trackNum = (int)state.getProperty("reaperTrack"); // 1-based

    if (trackNum < 0)
    {
        if (lastDisplayedTrackNumber != -1)
        {
            footer.getStatusBar().clearTrackInfo();
            lastDisplayedTrackNumber = -1;
            lastDisplayedTrackName = {};
        }
        return;
    }

    // REAPER fallback: use REAPER API for track name if JUCE/VST3 didn't provide one
    if (trackName.isEmpty() && audioProcessor.isReaperHost
        && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
    {
        int trackIndex = (int)state.getProperty("reaperTrack") - 1;
        std::string reaperName = ReaperTrackDetector::getTrackName(
            audioProcessor.reaperMidiProvider.getReaperGetFunc(), trackIndex);
        trackName = juce::String(reaperName);
    }

    if (trackName.isEmpty())
        trackName = "Track " + juce::String(trackNum);

    // Only update UI when something changed
    if (trackNum == lastDisplayedTrackNumber && trackName == lastDisplayedTrackName)
        return;

    lastDisplayedTrackNumber = trackNum;
    lastDisplayedTrackName = trackName;
    footer.getStatusBar().setTrackInfo(trackNum, trackName);
}

//==============================================================================
void ChartchoticAudioProcessorEditor::paint(juce::Graphics& g)
{
    PERF_SCOPE("paint/editor");
    g.fillAll(juce::Colours::black);

    if (assets.isBackgroundVisible())
        g.drawImage(assets.getCurrentBackground(), getLocalBounds().toFloat());

    // DAW icon in footer
    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
        footer.setDawIcon(assets.getReaperLogo());
    else
        footer.setDawIcon(nullptr);
}

// Dim, high and centred over the highways, so it reads at a glance without competing with
// the gems coming at you. Centred on the highway area rather than the window, so the strip
// does not push it off-centre.
void ChartchoticAudioProcessorEditor::drawSectionBanner(juce::Graphics& g)
{
    if (!sectionBannerShown() || activeSlotCount == 0) return;

    auto song = utilityStrip.currentSongName();
    auto section = utilityStrip.currentSectionName();
    if (song.isEmpty() && section.isEmpty()) return;

    float size = sectionBannerSize();
    int left = utilityStripWidth;
    int y = toolbar.getHeight() + juce::roundToInt(size * sectionBannerTopGap);

    // Song first and smaller, so the pair reads as one label: which chart, then where in it.
    if (song.isNotEmpty())
        y = drawBannerLine(g, song, size * songBannerScale, left, y);

    if (section.isNotEmpty())
        drawBannerLine(g, section, size, left, y);
}

// One centred line with its drop shadow, answering with the y the next line starts at.
int ChartchoticAudioProcessorEditor::drawBannerLine(juce::Graphics& g, const juce::String& name,
                                                    float size, int left, int y)
{
    // Underscores are chart-file plumbing, not part of the name anyone reads.
    auto text = name.replaceCharacter('_', ' ').toUpperCase();
    auto area = juce::Rectangle<int>(left, y, getWidth() - left, juce::roundToInt(size * 1.4f));

    g.setFont(Theme::getUIFont(size));

    g.setColour(sectionBannerShadow);
    g.drawText(text, area.translated(0, sectionBannerShadowPx),
               juce::Justification::centredTop, false);
    g.setColour(sectionBannerText);
    g.drawText(text, area, juce::Justification::centredTop, false);

    return y + juce::roundToInt(size * bannerLineGap);
}

void ChartchoticAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    // Painted after every child, so toFront does not lift a panel above it. The parts menu is
    // the one thing meant to sit over the scene, so the scene's own labels stand down for it.
    if (partsMenu.isVisible()) return;

    drawSectionBanner(g);

    // "Nothing selected" or "no parts detected" placeholder
    if (activeSlotCount == 0)
    {
        int tbHeight = toolbar.getHeight();
        auto area = getLocalBounds().withTrimmedTop(tbHeight);

        if (audioProcessor.isReaperHost && session.getDiscoveredParts().empty())
        {
            // No instrument tracks found — show naming guide
            float s = (float)getHeight() / (float)defaultHeight;
            float headerSize = 20.0f * s;
            float listSize = 14.0f * s;
            int lineH = juce::roundToInt(listSize * 1.6f);

            juce::StringArray trackNames;
            for (auto& entry : getImplementedTrackNames())
                trackNames.addIfNotAlreadyThere(entry.name);

            int blockH = juce::roundToInt(headerSize * 2.0f) + lineH * trackNames.size();
            int y = area.getCentreY() - blockH / 2;

            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(Theme::getUIFont(headerSize));
            g.drawText("No instrument tracks detected", area.getX(), y, area.getWidth(),
                       juce::roundToInt(headerSize * 1.5f), juce::Justification::centred);
            y += juce::roundToInt(headerSize * 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.setFont(Theme::getUIFont(listSize));
            for (auto& name : trackNames)
            {
                g.drawText(name, area.getX(), y, area.getWidth(), lineH, juce::Justification::centred);
                y += lineH;
            }
        }
        else if (session.isActive())
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(Theme::getUIFont(16.0f));
            juce::String msg = session.getEnabledParts().empty() ? "Select an instrument"
                             : session.getEnabledDifficulties().empty() ? "Select a difficulty"
                             : "No data";
            g.drawText(msg, area, juce::Justification::centred);
        }
    }

    if (showFps)
        drawFpsOverlay(g);

#ifdef DEBUG
    {
        HighwayComponent* hwPtrs[MAX_PROFILED_HIGHWAYS] = {};
        int hwCount = std::min(activeSlotCount, (int)MAX_PROFILED_HIGHWAYS);
        for (int i = 0; i < hwCount; ++i)
            hwPtrs[i] = slots[i].highway.get();
        debug.paintOverChildren(g,
            activeSlotCount == 0 ? nullptr : &primaryHighway(),
            hwPtrs, hwCount, activeSlotCount > 0);
    }
#endif
}

void ChartchoticAudioProcessorEditor::resized()
{
    state.setProperty("editorWidth", getWidth(), nullptr);
    state.setProperty("editorHeight", getHeight(), nullptr);

    utilityStripWidth = computeUtilityStripWidth();

    // Virtual scene dimensions. The strip is not part of the scene, so it comes off the
    // width before the highway geometry is derived from it.
    sceneWidth = getWidth() - (utilityStripVisible ? utilityStripWidth : 0);
    // Toolbar scales with width, but also caps by height so panels fit vertically.
    // The 0.09 vertical ratio ensures toolbar + tallest panel + footer all fit.
    int fromWidth = juce::roundToInt(getWidth() * ToolbarComponent::toolbarRatio);
    int fromHeight = juce::roundToInt(getHeight() * 0.09f);
    int tbStripHeight = std::min({ fromWidth, fromHeight, ToolbarComponent::maxToolbarHeight });

    // Total toolbar height — grows when write mode is active (sub-toolbar row).
    int tbHeight = toolbar.getReportedHeight(tbStripHeight);

    if (PositionMath::bemaniMode)
    {
        // Bemani: enforce minimum aspect ratio, but grow taller to fill available space
        int minH = juce::roundToInt(sceneWidth / bemaniMinAspect);
        int available = getHeight() - tbHeight;
        sceneHeight = std::max(minH, available);
    }
    else
    {
        sceneHeight = juce::roundToInt(sceneWidth / sceneAspectRatio);
    }

    const int margin = 10;

    // Toolbar at top — scales with editor width. Height includes sub-toolbar
    // row when write mode is active.
    toolbar.setBounds(0, 0, getWidth(), tbHeight);

    // Floating mode chip — top-left of the highway, just below the toolbar.
    // Sized off the toolbar strip so it scales with the rest of the UI.
    {
        int chipSize = juce::jmax(20, juce::roundToInt(tbStripHeight * 0.95f));
        int chipInset = juce::jmax(6, juce::roundToInt(tbStripHeight * 0.22f));
        writeModeIcon.setBounds(chipInset, tbHeight + chipInset, chipSize, chipSize);
        writeModeIcon.toFront(false);
    }

    // Footer bar height
    int footerFromWidth = juce::roundToInt(getWidth() * FooterComponent::footerRatio);
    int footerFromHeight = juce::roundToInt(getHeight() * 0.08f);
    int footerH = std::min({ footerFromWidth, footerFromHeight, FooterComponent::maxFooterHeight });

    // Tell popup panels where the footer starts so they don't overlap it
    toolbar.setPanelBottomMargin(footerH);

    // Utility strip claims its width off the left before the grid is computed, so the
    // highways reflow around it rather than sitting under it.
    int stripW = utilityStripVisible ? utilityStripWidth : 0;
    utilityStrip.setVisible(utilityStripVisible);

    // The sub-toolbar starts where the strip ends, so turning write mode on no longer
    // shortens the strip to make room for a row it does not overlap anyway.
    toolbar.setSubToolbarLeftInset(stripW);

    if (utilityStripVisible)
    {
        // Hung off the top row alone rather than the whole toolbar: the sub-toolbar sits
        // beside the strip now, so counting its height here would give back the squish.
        utilityStrip.setBounds(utilityStripMargin, tbStripHeight + utilityStripMargin,
                               stripW - utilityStripMargin * 2,
                               getHeight() - tbStripHeight - footerH - utilityStripMargin * 2);
        utilityStrip.toFront(false);
    }

    // Layout highway slots below toolbar, above footer
    if (activeSlotCount > 0)
    {
        int contentW = getWidth() - stripW;
        int contentH = getHeight() - tbHeight - footerH;
        int numSlots = activeSlotCount;

        // Determine grid layout based on aspect ratio and slot count
        //   Bemani: always a single horizontal row
        //   wide:       single row
        //   tall:       single column
        //   square-ish: grid (2x2 for 3-4 slots, row for 2)
        //
        // A 2x2 gives each slot the container's own aspect, while a single row divides the
        // width by the slot count: four across only stops being a set of slivers once the
        // window is genuinely wide, so the grid holds well past the old 1.5 handover.
        constexpr float GRID_ROW_ASPECT  = 2.4f;  // above this, 3-4 slots go to one row
        constexpr float GRID_COL_ASPECT  = 0.7f;  // below this, they stack in one column
        constexpr float PAIR_ROW_ASPECT  = 1.0f;  // two slots: side by side above, stacked below
        float aspect = (float)contentW / (float)std::max(1, contentH);
        int cols = 1, rows = 1;

        if (numSlots == 1)
        {
            // One highway, one column.
        }
        else if (PositionMath::bemaniMode)
        {
            cols = numSlots;
            rows = 1;
        }
        else if (numSlots <= 2)
        {
            if (aspect > PAIR_ROW_ASPECT) { cols = numSlots; rows = 1; }
            else                          { cols = 1; rows = numSlots; }
        }
        else
        {
            // 3-4 slots
            if (aspect > GRID_ROW_ASPECT)      { cols = numSlots; rows = 1; }
            else if (aspect < GRID_COL_ASPECT) { cols = 1; rows = numSlots; }
            else                               { cols = 2; rows = (numSlots + 1) / 2; }
        }

        // The banner is centred over the highways. With an even number of columns it lands in
        // the gap between the middle two; with an odd number a highway sits right under it, so
        // the grid starts below it instead.
        int bannerReserve = sectionBannerShown() && cols % 2 == 1 ? sectionBannerReserve() : 0;
        int gridTop = tbHeight + bannerReserve;
        contentH -= bannerReserve;

        if (numSlots == 1)
        {
            auto& hw = *slots[0].highway;
            hw.renderWidth = sceneWidth;
            hw.renderHeight = sceneHeight;
            hw.updateOverflow();
            hw.setBounds(stripW, gridTop, contentW, contentH);
        }
        else
        {
            int pad = highwayGridPadding;

            // Horizontal width is distributed by per-highway weight: elite drums gets a wider slot
            // so its 8 lanes have room, at the SAME row height (a wider aspect) alongside the
            // others. This is the lever for a genuinely wider highway -- the board + its natural
            // rails scale to fill the wider slot, no clipping or rail warping. (The board-coord
            // widening, ELITE_BOARD_WIDTH_SCALE, additionally widens the board WITHIN any slot so a
            // solo elite highway is wider too, sized so its natural rails still fit.)
            int totalPadX = (cols + 1) * pad;
            int totalPadY = (rows + 1) * pad;
            int availW    = contentW - totalPadX;
            int slotBaseH = (contentH - totalPadY) / rows;

            auto widthWeight = [](Part p) {
                return p == Part::ELITE_DRUMS ? PositionConstants::ELITE_BOARD_WIDTH_SCALE : 1.0f;
            };

            // Per-row weight sum → width per unit weight (so each row still fills contentW).
            std::vector<float> rowWeight((size_t)rows, 0.0f);
            for (int i = 0; i < numSlots; ++i)
                rowWeight[(size_t)(i / cols)] += widthWeight(slots[i].part);

            std::vector<int> rowX((size_t)rows, stripW + pad);   // running x per row
            for (int i = 0; i < numSlots; ++i)
            {
                auto& hw = *slots[i].highway;
                int row = i / cols;
                float unitW = availW / std::max(1.0f, rowWeight[(size_t)row]);
                float w     = widthWeight(slots[i].part);

                int slotW = std::max(1, juce::roundToInt(w * unitW));
                int slotX = rowX[(size_t)row];
                rowX[(size_t)row] += slotW + pad;
                int slotY = gridTop + pad + row * (slotBaseH + pad);

                if (PositionMath::bemaniMode)
                {
                    hw.renderWidth = slotW;
                    int minH = juce::roundToInt(slotW / bemaniMinAspect);
                    hw.renderHeight = std::max(minH, slotBaseH);
                }
                else
                {
                    // Common render height across the row (unitW / aspect) → weighting the
                    // width alone widens the elite highway's aspect without making it taller.
                    hw.renderWidth  = slotW;
                    hw.renderHeight = juce::roundToInt(unitW / sceneAspectRatio);
                }

                hw.updateOverflow();
                hw.setBounds(slotX, slotY, slotW, slotBaseH);
            }
        }
    }

    footer.setBounds(0, getHeight() - footerH, getWidth(), footerH);

    if (partsMenu.isVisible()) layoutPartsMenu();

    // Rebake shared track cache when scene dimensions change or cache was externally invalidated
    if (trackImageCache.isDirty() || !trackImageCache.covers(collectBakeSizes()))
    {
        trackImageCache.invalidate();
        requestAsyncRebake();
    }

    #ifdef DEBUG
    // Debug console floats below the entire toolbar (including the
    // sub-toolbar row when write mode is active).
    int debugTop = tbHeight;
    debug.getClearButton().setBounds(margin, debugTop + 4, 100, 20);
    debug.getCopyButton().setBounds(margin + 104, debugTop + 4, 60, 20);
    debug.getConsole().setBounds(margin, debugTop + 28, getWidth() - (2 * margin), getHeight() - debugTop - 38);
    #endif

}

void ChartchoticAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;

    // Same formula for both modes; Bemani applies ratio so same speed value feels equivalent.
    // Flat viewport has no foreshortening, so it needs a longer time window to match
    // the readable note density of perspective mode.
    displayWindowTimeSeconds = 7.87 / (double)noteSpeed;
    if (PositionMath::bemaniMode)
        displayWindowTimeSeconds *= (double)NOTE_SPEED_BEMANI_RATIO;

    // PPQ window must cover the full visible highway (farFadeEnd * window time) at worst-case tempo.
    // At 300 BPM, 1 second = 5 quarter notes. Base window of 30 PPQ scaled by highway length.
    const double BASE_PPQ_WINDOW = 30.0;
    double hwScale = activeSlotCount == 0 ? 1.0 : std::max(1.0, (double)primaryHighway().getSceneRenderer().farFadeEnd);
    displaySizeInPPQ = PPQ(BASE_PPQ_WINDOW * hwScale);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartchoticAudioProcessorEditor::loadState()
{
    // Migrate old redBackground bool to new background string
    if (state.hasProperty("redBackground") && !state.hasProperty("background"))
    {
        if ((bool)state["redBackground"] && assets.getBackgroundNames().contains("background_red"))
            state.setProperty("background", "background_red", nullptr);
        state.removeProperty("redBackground", nullptr);
    }

    // Background visibility (default off)
    applyBackgroundVisibility(state.hasProperty("showBackground") && (bool)state["showBackground"]);

    // Load saved background image
    {
        juce::String savedBg = state.getProperty("background").toString();
        assets.loadBackground(savedBg);
    }

    // Default to gothic if no saved preference (first launch only)
    if (!state.hasProperty("highwayTexture") && assets.getHighwayTextureNames().contains("gothic_default"))
        state.setProperty("highwayTexture", "gothic_default", nullptr);

    toolbar.loadState();

    view = ViewOptions::fromState(state);
    utilityStripVisible = !state.hasProperty("showUtilityStrip") || (bool)state["showUtilityStrip"];
    toolbar.setUtilityStripVisible(utilityStripVisible);
    applyView();

    if (state.hasProperty("stripFollow"))
        utilityStrip.setFollowOn((bool)state["stripFollow"]);

    // Restore highway length (apply per-instrument scale)
    {
        float userLength = state.hasProperty("highwayLength")
            ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"])
            : FAR_FADE_DEFAULT;
        float scaledLength = computeFarFadeEnd(userLength);
        forEachHighway([scaledLength](auto& hw) {
            hw.getSceneRenderer().farFadeEnd = scaledLength;
        });
        PositionMath::bemaniHwyScale = scaledLength;
    }

    // Load highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty() && assets.getHighwayTextureNames().contains(savedTexture))
        assets.loadHighwayTexture(savedTexture);

    // Restore texture parameters (all pooled highways, not just active)
    if (state.hasProperty("textureScale"))
    {
        float ts = (float)state["textureScale"];
        forAllHighways([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state.hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);
        forAllHighways([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
    }

    // Apply side-effects that listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int frId = (int)state["framerate"];
    // Migrate old 120/144 FPS options (ids 4,5) and unset (0) to Native (id 4)
    if (frId < 1 || frId > 4)
        frId = 4;
    targetFrameInterval = (frId == 1) ? 1.0 / 15.0 :
                          (frId == 2) ? 1.0 / 30.0 :
                          (frId == 3) ? 1.0 / 60.0 : 0.0;

    showFps = (bool)state.getProperty("showFps", false);

    // Restore stretch mode
    {
        bool stretch = state.hasProperty("stretchToFill") && (bool)state["stretchToFill"];
        forEachHighway([stretch](auto& hw) { hw.stretchToFill = stretch; });
    }

    // Restore Bemani mode
    PositionMath::bemaniMode = state.hasProperty("bemaniMode") && (bool)state["bemaniMode"];
    PositionMath::bemaniHwyScale = 1.0f;

    updateDisplaySizeFromSpeedSlider();
    trackImageCache.invalidate();
    forEachHighway([](auto& hw) { hw.rebuildTrack(); });
}


std::vector<TrackImageCache::BakeSize> ChartchoticAudioProcessorEditor::collectBakeSizes()
{
    // One entry per distinct slot geometry actually on screen. Baking at sceneWidth instead,
    // which is what this did before, meant every slot in a multi-highway layout blitted a
    // roughly 2x-oversized source and paid a downscale resample on five full-viewport layers
    // per frame. Elite is skipped: it renders live, so a bake at its size would go unused.
    std::vector<TrackImageCache::BakeSize> sizes;

    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        if (!slots[i].active || slots[i].highway == nullptr) continue;
        if (getRenderType(slots[i].part) == RenderType::ELITE_DRUMS) continue;

        auto& hw = *slots[i].highway;
        if (hw.renderWidth <= 0 || hw.renderHeight <= 0) continue;

        auto& sr = hw.getSceneRenderer();
        auto overflowFor = [&](bool drums) {
            auto edge = PositionMath::getFretboardEdge(
                drums, sr.farFadeEnd, hw.renderWidth, hw.renderHeight,
                PositionConstants::HIGHWAY_POS_START, sr.highwayPosEnd);
            return std::max(0, (int)std::ceil(-edge.centerY));
        };

        TrackImageCache::BakeSize s;
        s.width = hw.renderWidth;
        s.height = hw.renderHeight;
        s.overflowGuitar = overflowFor(false);
        s.overflowDrums  = overflowFor(true);
        // Device scale times the shrink paint() applies at the CURRENT bounds. Derived from
        // the component rather than from the last paint, so a resize bakes at the size it is
        // about to be drawn instead of the size it just was. Floored so a slot mid-layout, or
        // one scaled to almost nothing, cannot ask for a degenerate bake.
        float dsx = 1.0f, dsy = 1.0f;
        hw.getDrawScale(dsx, dsy);
        s.scaleX = juce::jmax(0.1f, hw.physicalScale * dsx);
        s.scaleY = juce::jmax(0.1f, hw.physicalScale * dsy);

        bool seen = false;
        for (const auto& existing : sizes)
            if (existing == s) { seen = true; break; }
        if (!seen)
            sizes.push_back(s);
    }

    return sizes;
}

void ChartchoticAudioProcessorEditor::requestAsyncRebake()
{
    if (!trackImageCache.isDirty() || activeSlotCount == 0)
        return;

    auto sizes = collectBakeSizes();
    if (sizes.empty()) return;

    auto& sr = slots[0].highway->getSceneRenderer();
    auto& hw = *slots[0].highway;

    trackImageCache.requestBake(*cacheRenderer, std::move(sizes),
        sr.farFadeEnd, sr.farFadeLen, sr.farFadeCurve, sr.highwayPosEnd,
        sr.guitarLaneCoordsLocal, (int)PositionConstants::GUITAR_LANE_COUNT,
        sr.drumLaneCoordsLocal, (int)PositionConstants::DRUM_LANE_COUNT,
        hw.getTrackRenderer().textureScale, hw.getTrackRenderer().textureOpacity,
        [this]() {
            // If more invalidations arrived while we were baking, re-trigger
            if (trackImageCache.isDirty())
            {
                requestAsyncRebake();
                return;
            }

            // Called on message thread after staging committed
            forEachHighway([](auto& hw) { hw.rebuildTrack(); });
        });
}

#ifdef DEBUG
void ChartchoticAudioProcessorEditor::rebuildSlots(const DebugMidiFilePlayer::LoadedChart& chart)
{
    // Map Part enum to MIDI track name
    auto trackNameForPart = [](Part p) -> juce::String {
        switch (p) {
            case Part::GUITAR:     return "PART GUITAR";
            case Part::GUITAR_COOP: return "PART GUITAR COOP";
            case Part::RHYTHM:     return "PART RHYTHM";
            case Part::BASS:       return "PART BASS";
            case Part::KEYS:       return "PART KEYS";
            case Part::DRUMS:      return "PART DRUMS";
            case Part::VOCALS:     return "PART VOCALS";
            default:               return "PART GUITAR";
        }
    };

    // Hide all pooled slots
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].active = false;
        slots[i].highway->setVisible(false);
    }

    // If no parts found, configure slot 0 as default
    if (chart.foundParts.empty())
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.ownedState.reset();
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;
        interactionController.setActivePart(slot.part);
        toolbar.refreshFromWriteController();

        resized();
        loadState();
        return;
    }

    // Configure one slot per discovered part (up to MAX_HIGHWAY_SLOTS)
    int slotIdx = 0;
    for (Part part : chart.foundParts)
    {
        if (slotIdx >= MAX_HIGHWAY_SLOTS) break;

        auto& slot = slots[slotIdx];
        slot.part = part;
        slot.active = true;
        slot.ownedState.reset();

        slot.noteStateMapArray = std::make_shared<NoteStateMapArray>();
        slot.noteStateMapLock = std::make_shared<juce::CriticalSection>();

        // Process this instrument's notes into the slot's own NoteStateMapArray
        juce::String tName = trackNameForPart(part);
        auto notesIt = chart.trackNotes.find(tName);
        if (notesIt != chart.trackNotes.end())
        {
            const juce::ScopedLock lock(*slot.noteStateMapLock);
            for (auto& map : *slot.noteStateMapArray)
                map.clear();

            NoteProcessor noteProcessor;
            noteProcessor.processAllNotes(notesIt->second, *slot.noteStateMapArray);
        }

        slot.interpreter = std::make_unique<MidiInterpreter>(
            part, state, *slot.noteStateMapArray, *slot.noteStateMapLock);
        slot.highway->setActivePart(part);
        slot.highway->setVisible(true);
        slotIdx++;
    }

    activeSlotCount = slotIdx;

    // Sync write controller to the primary slot's part.
    if (activeSlotCount > 0)
    {
        interactionController.setActivePart(slots[0].part);
        toolbar.refreshFromWriteController();
    }

    // Multi-highway mode: show part labels and all toolbar options
    bool multiSlot = activeSlotCount > 1;
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->showPartLabel = multiSlot;
    toolbar.setMultiInstrumentMode(multiSlot);

    resized();
    loadState();
}
#endif

void ChartchoticAudioProcessorEditor::applyLatencySetting(int latencyValue)
{
    switch (latencyValue) {
    case 1: latencyInSeconds = 0.000; break;
    case 2: latencyInSeconds = 0.250; break;
    case 3: latencyInSeconds = 0.500; break;
    case 4: latencyInSeconds = 0.750; break;
    case 5: latencyInSeconds = 1.000; break;
    case 6: latencyInSeconds = 1.500; break;
    default: latencyInSeconds = 0.500; break;
    }
    audioProcessor.setLatencyInSeconds(latencyInSeconds);
}

juce::ComponentBoundsConstrainer* ChartchoticAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartchoticAudioProcessorEditor::parentSizeChanged()
{
    AudioProcessorEditor::parentSizeChanged();
}


float ChartchoticAudioProcessorEditor::computeScrollOffset()
{
#ifdef DEBUG
    {
        float debugOffset;
        if (debug.computeScrollOffset(debugOffset, displayWindowTimeSeconds))
            return debugOffset;
    }
#endif

    // Use absolute time from the playhead — immune to BPM/time sig changes
    double absoluteTime = 0.0;
    if (auto* playHead = audioProcessor.getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            absoluteTime = positionInfo->getTimeInSeconds().orFallback(0.0);
    }

    // Apply latency offset to match note rendering position
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    absoluteTime -= latencyOffsetMs / 1000.0;

    // Raw continuous scroll value — consumers do their own modulo against tile size.
    // No wrapping here avoids double-modulo precision snaps.
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    return (float)(-absoluteTime * scrollRate);
}

FrameContext ChartchoticAudioProcessorEditor::buildFrameContext()
{
    WriteGridConfig wgc;
    wgc.active        = interactionController.writeModeActive() && interactionController.snapEnabled();
    wgc.stepDivision  = interactionController.stepDivision();
    wgc.tuplet        = interactionController.tuplet();

    return FrameContext {
        audioProcessor,
        state,
        lastKnownPosition,
        displaySizeInPPQ,
        displayWindowTimeSeconds,
        computeScrollOffset(),
        smoothedLatencyInPPQ(),
        slots.data(),
        activeSlotCount,
        wgc
    };
}

void ChartchoticAudioProcessorEditor::drawFpsOverlay(juce::Graphics& g)
{
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < FPS_RING_SIZE; ++i)
        if (fpsRing[i] > 0.0) { sum += fpsRing[i]; ++count; }
    double avgDelta = count > 0 ? sum / count : 0.0;
    double fps = avgDelta > 0.0 ? 1000000.0 / avgDelta : 0.0;

    float s = (float)getHeight() / (float)defaultHeight;
    auto area = fpsOverlayBounds();

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(area.toFloat().expanded(4.0f * s, 2.0f * s), 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(Theme::getUIFont(12.0f * s));
    g.drawText(juce::String(fps, 1) + " FPS", area, juce::Justification::centredLeft);
}

