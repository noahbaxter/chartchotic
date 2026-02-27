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
        ChartPreview/Source/DebugTools/Logger.h
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
        ChartPreview/Source/Visual/Renderers/ColumnRenderer.cpp
        ChartPreview/Source/Visual/Renderers/ColumnRenderer.h
        ChartPreview/Source/Visual/Renderers/GlyphRenderer.cpp
        ChartPreview/Source/Visual/Renderers/GlyphRenderer.h
        ChartPreview/Source/Visual/Renderers/HighwayRenderer.cpp
        ChartPreview/Source/Visual/Renderers/HighwayRenderer.h
        # Visual / Utils
        ChartPreview/Source/Visual/Utils/DrawingConstants.h
        ChartPreview/Source/Visual/Utils/PositionConstants.h
        ChartPreview/Source/Visual/Utils/PositionMath.cpp
        ChartPreview/Source/Visual/Utils/PositionMath.h
        # Visual / Managers
        ChartPreview/Source/Visual/Managers/AnimationManager.h
        ChartPreview/Source/Visual/Managers/AssetManager.cpp
        ChartPreview/Source/Visual/Managers/AssetManager.h
        ChartPreview/Source/Visual/Managers/GridlineGenerator.h
        # Utils
        ChartPreview/Source/Utils/PPQ.h
        ChartPreview/Source/Utils/TempoTimeSignatureEventHelper.h
        ChartPreview/Source/Utils/TimeConverter.h
        ChartPreview/Source/Utils/Utils.h
)

# Binary data (images + SVG)
juce_add_binary_data(ChartPreviewData
    NAMESPACE BinaryData
    SOURCES
        ChartPreview/Assets/background.png
        ChartPreview/Assets/bar_kick.png
        ChartPreview/Assets/bar_kick_2x.png
        ChartPreview/Assets/bar_open.png
        ChartPreview/Assets/bar_white.png
        ChartPreview/Assets/cym_blue.png
        ChartPreview/Assets/cym_green.png
        ChartPreview/Assets/cym_red.png
        ChartPreview/Assets/cym_white.png
        ChartPreview/Assets/cym_yellow.png
        ChartPreview/Assets/hopo_blue.png
        ChartPreview/Assets/hopo_green.png
        ChartPreview/Assets/hopo_orange.png
        ChartPreview/Assets/hopo_red.png
        ChartPreview/Assets/hopo_white.png
        ChartPreview/Assets/hopo_yellow.png
        ChartPreview/Assets/lane_end.png
        ChartPreview/Assets/lane_mid.png
        ChartPreview/Assets/lane_start.png
        ChartPreview/Assets/marker_beat.png
        ChartPreview/Assets/marker_half_beat.png
        ChartPreview/Assets/marker_measure.png
        ChartPreview/Assets/note_blue.png
        ChartPreview/Assets/note_green.png
        ChartPreview/Assets/note_orange.png
        ChartPreview/Assets/note_red.png
        ChartPreview/Assets/note_white.png
        ChartPreview/Assets/note_yellow.png
        ChartPreview/Assets/overlay_cym_accent.png
        ChartPreview/Assets/overlay_cym_ghost.png
        ChartPreview/Assets/overlay_cym_ghost_80scale.png
        ChartPreview/Assets/overlay_note_accent.png
        ChartPreview/Assets/overlay_note_ghost.png
        ChartPreview/Assets/overlay_note_tap.png
        ChartPreview/Assets/sustain_blue.png
        ChartPreview/Assets/sustain_green.png
        ChartPreview/Assets/sustain_open.png
        ChartPreview/Assets/sustain_open_white.png
        ChartPreview/Assets/sustain_orange.png
        ChartPreview/Assets/sustain_red.png
        ChartPreview/Assets/sustain_white.png
        ChartPreview/Assets/sustain_yellow.png
        ChartPreview/Assets/track_drum.png
        ChartPreview/Assets/track_guitar.png
        ChartPreview/Assets/logo-reaper.svg
        ChartPreview/Assets/hit_1.png
        ChartPreview/Assets/hit_2.png
        ChartPreview/Assets/hit_3.png
        ChartPreview/Assets/hit_4.png
        ChartPreview/Assets/hit_5.png
        ChartPreview/Assets/hit_flare_blue.png
        ChartPreview/Assets/hit_flare_green.png
        ChartPreview/Assets/hit_flare_orange.png
        ChartPreview/Assets/hit_flare_red.png
        ChartPreview/Assets/hit_flare_yellow.png
        ChartPreview/Assets/hit_kick_1.png
        ChartPreview/Assets/hit_kick_2.png
        ChartPreview/Assets/hit_kick_3.png
        ChartPreview/Assets/hit_kick_4.png
        ChartPreview/Assets/hit_kick_5.png
        ChartPreview/Assets/hit_kick_6.png
        ChartPreview/Assets/hit_kick_7.png
        ChartPreview/Assets/hit_open_1.png
        ChartPreview/Assets/hit_open_2.png
        ChartPreview/Assets/hit_open_3.png
        ChartPreview/Assets/hit_open_4.png
        ChartPreview/Assets/hit_open_5.png
        ChartPreview/Assets/hit_open_6.png
        ChartPreview/Assets/hit_open_7.png
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
