cmake_minimum_required(VERSION 3.22)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" CHARTPREVIEW_VERSION)
string(STRIP "${CHARTPREVIEW_VERSION}" CHARTPREVIEW_VERSION)

project(ChartPreview VERSION ${CHARTPREVIEW_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build channel: DEV or RELEASE (passed via -DBUILD_CHANNEL=...)
if(NOT DEFINED BUILD_CHANNEL)
    set(BUILD_CHANNEL "DEV")
endif()

# Version string: full version including prerelease suffix (passed via -DVERSION_STRING=...)
# For local dev builds, generate a timestamped version automatically
if(NOT DEFINED VERSION_STRING)
    if(BUILD_CHANNEL STREQUAL "DEV")
        string(TIMESTAMP _timestamp "%Y%m%d%H%M%S" UTC)
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _short_sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_short_sha STREQUAL "")
            set(_short_sha "local")
        endif()
        set(VERSION_STRING "${CHARTPREVIEW_VERSION}-dev.${_timestamp}.${_short_sha}")
    else()
        set(VERSION_STRING "${CHARTPREVIEW_VERSION}")
    endif()
endif()

# Suppress warnings from JUCE (third-party code)
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
add_subdirectory(third_party/JUCE)
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

# Plugin definition
juce_add_plugin(ChartPreview
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Dichotic Studios"
    BUNDLE_ID "com.dichoticstudios.chartpreview"
    PLUGIN_MANUFACTURER_CODE Dich
    PLUGIN_CODE ChPv
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Chart Preview"
    PLUGIN_DESC "A tool for visualizing midi as guitar/drum game charts"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_CURL TRUE

    VST3_CATEGORIES Tools
)

juce_generate_juce_header(ChartPreview)

# Source files
target_sources(ChartPreview
    PRIVATE
        ChartPreview/Source/PluginProcessor.cpp
        ChartPreview/Source/PluginProcessor.h
        ChartPreview/Source/PluginEditor.cpp
        ChartPreview/Source/PluginEditor.h
        ChartPreview/Source/ReaperVST3.h
        ChartPreview/Source/UpdateChecker.cpp
        ChartPreview/Source/UpdateChecker.h
        # DebugTools
        ChartPreview/Source/DebugTools/DebugEditorController.cpp
        ChartPreview/Source/DebugTools/DebugEditorController.h
        ChartPreview/Source/DebugTools/DebugMidiFilePlayer.h
        ChartPreview/Source/DebugTools/DebugPlaybackController.h
        ChartPreview/Source/DebugTools/DebugToolbarPanel.cpp
        ChartPreview/Source/DebugTools/DebugToolbarPanel.h
        ChartPreview/Source/DebugTools/DebugTuningPanel.cpp
        ChartPreview/Source/DebugTools/DebugTuningPanel.h
        ChartPreview/Source/DebugTools/Logger.h
        ChartPreview/Source/DebugTools/DataGenerators.h
        # Midi / Processing
        ChartPreview/Source/Midi/Processing/MidiInterpreter.cpp
        ChartPreview/Source/Midi/Processing/MidiInterpreter.h
        ChartPreview/Source/Midi/Processing/MidiProcessor.cpp
        ChartPreview/Source/Midi/Processing/MidiProcessor.h
        ChartPreview/Source/Midi/Processing/NoteProcessor.cpp
        ChartPreview/Source/Midi/Processing/NoteProcessor.h
        # Midi / Providers / REAPER
        ChartPreview/Source/Midi/Providers/REAPER/MidiCache.cpp
        ChartPreview/Source/Midi/Providers/REAPER/MidiCache.h
        ChartPreview/Source/Midi/Providers/REAPER/ReaperApiHelpers.h
        ChartPreview/Source/Midi/Providers/REAPER/ReaperMidiProvider.cpp
        ChartPreview/Source/Midi/Providers/REAPER/ReaperMidiProvider.h
        ChartPreview/Source/Midi/Providers/REAPER/ReaperNoteFetcher.cpp
        ChartPreview/Source/Midi/Providers/REAPER/ReaperNoteFetcher.h
        # Midi / Pipelines
        ChartPreview/Source/Midi/Pipelines/MidiPipeline.h
        ChartPreview/Source/Midi/Pipelines/MidiPipelineFactory.cpp
        ChartPreview/Source/Midi/Pipelines/MidiPipelineFactory.h
        ChartPreview/Source/Midi/Pipelines/ReaperMidiPipeline.cpp
        ChartPreview/Source/Midi/Pipelines/ReaperMidiPipeline.h
        ChartPreview/Source/Midi/Pipelines/StandardMidiPipeline.cpp
        ChartPreview/Source/Midi/Pipelines/StandardMidiPipeline.h
        # Midi / Utils
        ChartPreview/Source/Midi/Utils/ChordAnalyzer.cpp
        ChartPreview/Source/Midi/Utils/ChordAnalyzer.h
        ChartPreview/Source/Midi/Utils/GemCalculator.cpp
        ChartPreview/Source/Midi/Utils/GemCalculator.h
        ChartPreview/Source/Midi/Utils/InstrumentMapper.h
        ChartPreview/Source/Midi/Utils/LaneDetector.cpp
        ChartPreview/Source/Midi/Utils/LaneDetector.h
        ChartPreview/Source/Midi/Utils/MidiConstants.h
        ChartPreview/Source/Midi/Utils/MidiTypes.h
        # REAPER
        ChartPreview/Source/REAPER/ReaperIntegration.cpp
        ChartPreview/Source/REAPER/ReaperIntegration.h
        ChartPreview/Source/REAPER/ReaperTrackDetector.cpp
        ChartPreview/Source/REAPER/ReaperTrackDetector.h
        ChartPreview/Source/REAPER/ReaperVST2Extensions.cpp
        ChartPreview/Source/REAPER/ReaperVST2Extensions.h
        ChartPreview/Source/REAPER/ReaperVST3Extensions.cpp
        ChartPreview/Source/REAPER/ReaperVST3Extensions.h
        ChartPreview/Source/REAPER/TrackInfoListener.cpp
        ChartPreview/Source/REAPER/TrackInfoListener.h
        # Visual / Renderers
        ChartPreview/Source/Visual/Renderers/AnimationRenderer.cpp
        ChartPreview/Source/Visual/Renderers/AnimationRenderer.h
        ChartPreview/Source/Visual/Renderers/SceneRenderer.cpp
        ChartPreview/Source/Visual/Renderers/SceneRenderer.h
        ChartPreview/Source/Visual/Renderers/GridlineRenderer.cpp
        ChartPreview/Source/Visual/Renderers/GridlineRenderer.h
        ChartPreview/Source/Visual/Renderers/TrackRenderer.cpp
        ChartPreview/Source/Visual/Renderers/TrackRenderer.h
        ChartPreview/Source/Visual/Renderers/NoteRenderer.cpp
        ChartPreview/Source/Visual/Renderers/NoteRenderer.h
        ChartPreview/Source/Visual/Renderers/SustainRenderer.cpp
        ChartPreview/Source/Visual/Renderers/SustainRenderer.h
        # Visual / Utils
        ChartPreview/Source/Visual/Utils/DrawingConstants.h
        ChartPreview/Source/Visual/Utils/PositionConstants.h
        ChartPreview/Source/Visual/Utils/PositionMath.cpp
        ChartPreview/Source/Visual/Utils/PositionMath.h
        ChartPreview/Source/Visual/Utils/RenderTiming.h
        ChartPreview/Source/Visual/Renderers/TrackFade.h
        # Visual / Managers
        ChartPreview/Source/Visual/Managers/AnimationManager.h
        ChartPreview/Source/Visual/Managers/AssetManager.cpp
        ChartPreview/Source/Visual/Managers/AssetManager.h
        ChartPreview/Source/Visual/Managers/GridlineGenerator.h
        # UI - Controls
        ChartPreview/Source/UI/Controls/PanelSectionHeader.h
        ChartPreview/Source/UI/Controls/PillToggle.h
        ChartPreview/Source/UI/Controls/PopupMenuButton.h
        ChartPreview/Source/UI/Controls/CircleIconSelector.h
        ChartPreview/Source/UI/Controls/SegmentedButtons.h
        ChartPreview/Source/UI/Controls/ValueStepper.h
        # UI - LookAndFeel
        ChartPreview/Source/UI/LookAndFeel/ChartPreviewLookAndFeel.cpp
        ChartPreview/Source/UI/LookAndFeel/ChartPreviewLookAndFeel.h
        # UI - Core
        ChartPreview/Source/UI/ChartchoticLogo.h
        ChartPreview/Source/UI/Theme.h
        ChartPreview/Source/UI/SectionHeader.h
        ChartPreview/Source/UI/ToolbarComponent.cpp
        ChartPreview/Source/UI/ToolbarComponent.h
        # Utils
        ChartPreview/Source/Visual/Utils/LatencySmoother.h
        ChartPreview/Source/Utils/PPQ.h
        ChartPreview/Source/Utils/TempoTimeSignatureEventHelper.h
        ChartPreview/Source/Utils/TimeConverter.h
        ChartPreview/Source/Utils/Utils.h
)

# Binary data (images + SVG)
# Generated assets come from assets/generated/<category>/ (via convert_assets.sh)
# Static assets (no PDF source) come from assets/static/
# MIDI test files come from assets/midi/
juce_add_binary_data(ChartPreviewData
    NAMESPACE BinaryData
    SOURCES
        # Fonts
        assets/static/fonts/TurretRoad-Bold.ttf
        assets/static/fonts/TurretRoad-Medium.ttf
        assets/static/fonts/RussoOne-Regular.ttf
        assets/static/fonts/ChakraPetch-Medium.ttf
        # UI / branding
        assets/static/backgrounds/background.png
        assets/static/backgrounds/background_red.jpg
        assets/static/icons/icon_guitar.png
        assets/static/icons/icon_drums.png
        assets/static/logo-reaper.svg
        # Notes (generated from PDFs)
        assets/generated/notes/note_blue.png
        assets/generated/notes/note_green.png
        assets/generated/notes/note_orange.png
        assets/generated/notes/note_red.png
        assets/generated/notes/note_white.png
        assets/generated/notes/note_yellow.png
        assets/generated/notes/hopo_blue.png
        assets/generated/notes/hopo_green.png
        assets/generated/notes/hopo_orange.png
        assets/generated/notes/hopo_red.png
        assets/generated/notes/hopo_white.png
        assets/generated/notes/hopo_yellow.png
        assets/generated/notes/cym_blue.png
        assets/generated/notes/cym_green.png
        assets/generated/notes/cym_red.png
        assets/generated/notes/cym_white.png
        assets/generated/notes/cym_yellow.png
        # Bars (generated from PDFs)
        assets/generated/bars/bar_kick.png
        assets/generated/bars/bar_kick_2x.png
        assets/generated/bars/bar_open.png
        assets/generated/bars/bar_white.png
        # Overlays (generated from PDFs)
        assets/generated/overlays/overlay_note_ghost.png
        assets/generated/overlays/overlay_cym_ghost.png
        assets/generated/overlays/overlay_note_tap.png
        assets/generated/overlays/overlay_cym_accent.png
        assets/generated/overlays/overlay_note_accent.png
        # Sustains (generated from PDFs)
        assets/generated/sustains/sustain_open.png
        assets/generated/sustains/sustain_open_white.png
        # Markers (generated from PDFs)
        assets/generated/markers/marker_beat.png
        assets/generated/markers/marker_half_beat.png
        assets/generated/markers/marker_measure.png
        # Lanes (generated from PDFs)
        assets/generated/lanes/lane_end.png
        assets/generated/lanes/lane_mid.png
        assets/generated/lanes/lane_start.png
        # Track layers (generated from PDFs)
        assets/generated/ui/sidebars.png
        assets/generated/ui/lane_lines_guitar.png
        assets/generated/ui/lane_lines_drums.png
        assets/generated/ui/strikeline_guitar.png
        assets/generated/ui/strikeline_drums.png
        assets/generated/ui/strikeline_connectors.png
        assets/generated/ui/kick_smashers.png
        # Hit animations — flares (generated from PDFs)
        assets/generated/hits/hit_flare_blue.png
        assets/generated/hits/hit_flare_green.png
        assets/generated/hits/hit_flare_orange.png
        assets/generated/hits/hit_flare_red.png
        assets/generated/hits/hit_flare_yellow.png
        assets/generated/hits/hit_flare_white.png
        # Hit animations — flash frames (generated from PDFs)
        assets/generated/hits/hit_flash_1.png
        assets/generated/hits/hit_flash_2.png
        assets/generated/hits/hit_flash_3.png
        assets/generated/hits/hit_flash_4.png
        assets/generated/hits/hit_flash_5.png
        # Hit animations — kick frames (static)
        assets/static/hits/hit_kick_1.png
        assets/static/hits/hit_kick_2.png
        assets/static/hits/hit_kick_3.png
        assets/static/hits/hit_kick_4.png
        assets/static/hits/hit_kick_5.png
        assets/static/hits/hit_kick_6.png
        assets/static/hits/hit_kick_7.png
        # Hit animations — open frames (static)
        assets/static/hits/hit_open_1.png
        assets/static/hits/hit_open_2.png
        assets/static/hits/hit_open_3.png
        assets/static/hits/hit_open_4.png
        assets/static/hits/hit_open_5.png
        assets/static/hits/hit_open_6.png
        assets/static/hits/hit_open_7.png
        # MIDI test files loaded from disk at runtime (see CHARTPREVIEW_MIDI_ASSET_DIR)
)

# REAPER SDK header path
target_include_directories(ChartPreview
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/reaper-sdk/sdk
)

# Compile definitions
target_compile_definitions(ChartPreview
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
    PRIVATE
        CHARTPREVIEW_BUILD_CHANNEL="${BUILD_CHANNEL}"
        CHARTPREVIEW_VERSION_STRING="${VERSION_STRING}"
        CHARTPREVIEW_MIDI_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/midi"
)

# Link libraries
target_link_libraries(ChartPreview
    PRIVATE
        ChartPreviewData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Platform-specific flags
if(APPLE)
    target_compile_options(ChartPreview PRIVATE
        -Wno-deprecated-declarations
    )
endif()
