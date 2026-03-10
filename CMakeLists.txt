cmake_minimum_required(VERSION 3.22)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" CHARTCHOTIC_VERSION)
string(STRIP "${CHARTCHOTIC_VERSION}" CHARTCHOTIC_VERSION)

project(Chartchotic VERSION ${CHARTCHOTIC_VERSION} LANGUAGES C CXX)

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
        set(VERSION_STRING "${CHARTCHOTIC_VERSION}-dev.${_timestamp}.${_short_sha}")
    else()
        set(VERSION_STRING "${CHARTCHOTIC_VERSION}")
    endif()
endif()

# Suppress warnings from JUCE (third-party code)
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
add_subdirectory(third_party/JUCE)
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

# Plugin definition
juce_add_plugin(Chartchotic
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Dichotic Studios"
    BUNDLE_ID "com.dichoticstudios.chartchotic"
    PLUGIN_MANUFACTURER_CODE Dich
    PLUGIN_CODE ChPv
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Chartchotic"
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

juce_generate_juce_header(Chartchotic)

# Source files
target_sources(Chartchotic
    PRIVATE
        Chartchotic/Source/PluginProcessor.cpp
        Chartchotic/Source/PluginProcessor.h
        Chartchotic/Source/PluginEditor.cpp
        Chartchotic/Source/PluginEditor.h
        Chartchotic/Source/ReaperVST3.h
        Chartchotic/Source/UpdateChecker.cpp
        Chartchotic/Source/UpdateChecker.h
        # DebugTools
        Chartchotic/Source/DebugTools/DebugEditorController.cpp
        Chartchotic/Source/DebugTools/DebugEditorController.h
        Chartchotic/Source/DebugTools/DebugMidiFilePlayer.h
        Chartchotic/Source/DebugTools/DebugPlaybackController.h
        Chartchotic/Source/DebugTools/DebugToolbarPanel.cpp
        Chartchotic/Source/DebugTools/DebugToolbarPanel.h
        Chartchotic/Source/DebugTools/DebugTuningPanel.cpp
        Chartchotic/Source/DebugTools/DebugTuningPanel.h
        Chartchotic/Source/DebugTools/Logger.h
        Chartchotic/Source/DebugTools/DataGenerators.h
        # Midi / Processing
        Chartchotic/Source/Midi/Processing/MidiInterpreter.cpp
        Chartchotic/Source/Midi/Processing/MidiInterpreter.h
        Chartchotic/Source/Midi/Processing/MidiProcessor.cpp
        Chartchotic/Source/Midi/Processing/MidiProcessor.h
        Chartchotic/Source/Midi/Processing/NoteProcessor.cpp
        Chartchotic/Source/Midi/Processing/NoteProcessor.h
        # Midi / Providers / REAPER
        Chartchotic/Source/Midi/Providers/REAPER/MidiCache.cpp
        Chartchotic/Source/Midi/Providers/REAPER/MidiCache.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperApiHelpers.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiProvider.cpp
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiProvider.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperNoteFetcher.cpp
        Chartchotic/Source/Midi/Providers/REAPER/ReaperNoteFetcher.h
        # Midi / Pipelines
        Chartchotic/Source/Midi/Pipelines/MidiPipeline.h
        Chartchotic/Source/Midi/Pipelines/MidiPipelineFactory.cpp
        Chartchotic/Source/Midi/Pipelines/MidiPipelineFactory.h
        Chartchotic/Source/Midi/Pipelines/ReaperMidiPipeline.cpp
        Chartchotic/Source/Midi/Pipelines/ReaperMidiPipeline.h
        Chartchotic/Source/Midi/Pipelines/StandardMidiPipeline.cpp
        Chartchotic/Source/Midi/Pipelines/StandardMidiPipeline.h
        # Midi / Utils
        Chartchotic/Source/Midi/Utils/ChordAnalyzer.cpp
        Chartchotic/Source/Midi/Utils/ChordAnalyzer.h
        Chartchotic/Source/Midi/Utils/GemCalculator.cpp
        Chartchotic/Source/Midi/Utils/GemCalculator.h
        Chartchotic/Source/Midi/Utils/InstrumentMapper.h
        Chartchotic/Source/Midi/Utils/LaneDetector.cpp
        Chartchotic/Source/Midi/Utils/LaneDetector.h
        Chartchotic/Source/Midi/Utils/MidiConstants.h
        Chartchotic/Source/Midi/Utils/MidiTypes.h
        # REAPER
        Chartchotic/Source/REAPER/ReaperIntegration.cpp
        Chartchotic/Source/REAPER/ReaperIntegration.h
        Chartchotic/Source/REAPER/ReaperTrackDetector.cpp
        Chartchotic/Source/REAPER/ReaperTrackDetector.h
        Chartchotic/Source/REAPER/ReaperVST2Extensions.cpp
        Chartchotic/Source/REAPER/ReaperVST2Extensions.h
        Chartchotic/Source/REAPER/ReaperVST3Extensions.cpp
        Chartchotic/Source/REAPER/ReaperVST3Extensions.h
        Chartchotic/Source/REAPER/TrackInfoListener.cpp
        Chartchotic/Source/REAPER/TrackInfoListener.h
        # Visual / Renderers
        Chartchotic/Source/Visual/Renderers/AnimationRenderer.cpp
        Chartchotic/Source/Visual/Renderers/AnimationRenderer.h
        Chartchotic/Source/Visual/Renderers/SceneRenderer.cpp
        Chartchotic/Source/Visual/Renderers/SceneRenderer.h
        Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp
        Chartchotic/Source/Visual/Renderers/GridlineRenderer.h
        Chartchotic/Source/Visual/Renderers/TrackRenderer.cpp
        Chartchotic/Source/Visual/Renderers/TrackRenderer.h
        Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp
        Chartchotic/Source/Visual/Renderers/NoteRenderer.h
        Chartchotic/Source/Visual/Renderers/SustainRenderer.cpp
        Chartchotic/Source/Visual/Renderers/SustainRenderer.h
        # Visual / Utils
        Chartchotic/Source/Visual/Utils/DrawingConstants.h
        Chartchotic/Source/Visual/Utils/PositionConstants.h
        Chartchotic/Source/Visual/Utils/PositionMath.cpp
        Chartchotic/Source/Visual/Utils/PositionMath.h
        Chartchotic/Source/Visual/Utils/RenderTiming.h
        Chartchotic/Source/Visual/Renderers/TrackFade.h
        # Visual / Managers
        Chartchotic/Source/Visual/Managers/AnimationManager.h
        Chartchotic/Source/Visual/Managers/AssetManager.cpp
        Chartchotic/Source/Visual/Managers/AssetManager.h
        Chartchotic/Source/Visual/Managers/GridlineGenerator.h
        # UI - Controls
        Chartchotic/Source/UI/Controls/CheckboxToggle.h
        Chartchotic/Source/UI/Controls/PanelSectionHeader.h
        Chartchotic/Source/UI/Controls/PillToggle.h
        Chartchotic/Source/UI/Controls/PopupMenuButton.h
        Chartchotic/Source/UI/Controls/CircleIconSelector.h
        Chartchotic/Source/UI/Controls/SegmentedButtons.h
        Chartchotic/Source/UI/Controls/ValueStepper.h
        # UI - LookAndFeel
        Chartchotic/Source/UI/LookAndFeel/ChartchoticLookAndFeel.cpp
        Chartchotic/Source/UI/LookAndFeel/ChartchoticLookAndFeel.h
        # UI - Core
        Chartchotic/Source/UI/ChartchoticLogo.h
        Chartchotic/Source/UI/Theme.h
        Chartchotic/Source/UI/MenuGroup.h
        Chartchotic/Source/UI/SectionHeader.h
        Chartchotic/Source/UI/ControlConstants.h
        Chartchotic/Source/UI/ToolbarComponent.cpp
        Chartchotic/Source/UI/ToolbarComponent.h
        # Utils
        Chartchotic/Source/Visual/Utils/LatencySmoother.h
        Chartchotic/Source/Utils/PPQ.h
        Chartchotic/Source/Utils/TempoTimeSignatureEventHelper.h
        Chartchotic/Source/Utils/TimeConverter.h
        Chartchotic/Source/Utils/Utils.h
)

# Skin override directory: flat folder of replacement assets.
# Usage: cmake -DSKIN_DIR=/path/to/my-skin ...
# See docs/SKINNING.md for details.
set(SKIN_DIR "" CACHE PATH "Optional skin override directory")
set(_SKIN_OVERRIDE_COUNT 0)

# Resolve an asset path: checks SKIN_DIR for a flat override first, falls back to default.
# All asset filenames are unique across categories so no subdirectories needed in the skin dir.
# The category arg is unused but kept for readability in the call sites below.
function(resolve_asset OUT_VAR default_path category filename)
    if(SKIN_DIR AND EXISTS "${SKIN_DIR}/${filename}")
        set(${OUT_VAR} "${SKIN_DIR}/${filename}" PARENT_SCOPE)
        math(EXPR _new_count "${_SKIN_OVERRIDE_COUNT} + 1")
        set(_SKIN_OVERRIDE_COUNT ${_new_count} PARENT_SCOPE)
        message(STATUS "  Skin override: ${filename}")
    else()
        set(${OUT_VAR} "${default_path}" PARENT_SCOPE)
    endif()
endfunction()

# Resolve all assets (skin overrides take priority)
# Fonts
resolve_asset(ASSET_FONT_TURRET_BOLD       "assets/static/fonts/TurretRoad-Bold.ttf"       fonts TurretRoad-Bold.ttf)
resolve_asset(ASSET_FONT_TURRET_MEDIUM     "assets/static/fonts/TurretRoad-Medium.ttf"     fonts TurretRoad-Medium.ttf)
resolve_asset(ASSET_FONT_RUSSO            "assets/static/fonts/RussoOne-Regular.ttf"       fonts RussoOne-Regular.ttf)
resolve_asset(ASSET_FONT_CHAKRA           "assets/static/fonts/ChakraPetch-Medium.ttf"     fonts ChakraPetch-Medium.ttf)
# Backgrounds
resolve_asset(ASSET_BG                     "assets/static/backgrounds/background.png"       backgrounds background.png)
resolve_asset(ASSET_BG_RED                 "assets/static/backgrounds/background_red.jpg"   backgrounds background_red.jpg)
# Icons
resolve_asset(ASSET_ICON_GUITAR            "assets/static/icons/icon_guitar.png"            icons icon_guitar.png)
resolve_asset(ASSET_ICON_DRUMS             "assets/static/icons/icon_drums.png"             icons icon_drums.png)
# Logo
resolve_asset(ASSET_LOGO_REAPER            "assets/static/logo-reaper.svg"                  "" logo-reaper.svg)
# Highways
resolve_asset(ASSET_HW_AMBER              "assets/static/highways/kanaizo_amber.png"        highways kanaizo_amber.png)
resolve_asset(ASSET_HW_DARKWOOD           "assets/static/highways/kanaizo_darkwood.png"     highways kanaizo_darkwood.png)
resolve_asset(ASSET_HW_GOTHIC             "assets/static/highways/kanaizo_gothic.png"       highways kanaizo_gothic.png)
resolve_asset(ASSET_HW_ORNATE             "assets/static/highways/kanaizo_ornate.png"       highways kanaizo_ornate.png)
resolve_asset(ASSET_HW_BLUECAUSTIC        "assets/static/highways/kanaizo_BlueCaustic.png"  highways kanaizo_BlueCaustic.png)
resolve_asset(ASSET_HW_FRETWOOD           "assets/static/highways/kanaizo_FretWood.png"     highways kanaizo_FretWood.png)
resolve_asset(ASSET_HW_BLUEMAGENTAHEX     "assets/static/highways/kanaizo_BlueMagentaHex.png" highways kanaizo_BlueMagentaHex.png)
resolve_asset(ASSET_HW_ICYMETAL           "assets/static/highways/kanaizo_IcyMetal.png"     highways kanaizo_IcyMetal.png)
resolve_asset(ASSET_HW_BRIGHTRADIATED     "assets/static/highways/kanaizo_BrightRadiated.png" highways kanaizo_BrightRadiated.png)
resolve_asset(ASSET_HW_MODERNHUEISTIC     "assets/static/highways/kanaizo_ModernHueistic.png" highways kanaizo_ModernHueistic.png)
resolve_asset(ASSET_HW_REDCAUSTIC         "assets/static/highways/kanaizo_RedCaustic.png"   highways kanaizo_RedCaustic.png)
resolve_asset(ASSET_HW_SCRATCHEDREGULAR   "assets/static/highways/kanaizo_ScratchedRegular.png" highways kanaizo_ScratchedRegular.png)
# Notes
resolve_asset(ASSET_NOTE_BLUE              "assets/generated/notes/note_blue.png"            notes note_blue.png)
resolve_asset(ASSET_NOTE_GREEN             "assets/generated/notes/note_green.png"           notes note_green.png)
resolve_asset(ASSET_NOTE_ORANGE            "assets/generated/notes/note_orange.png"          notes note_orange.png)
resolve_asset(ASSET_NOTE_RED               "assets/generated/notes/note_red.png"             notes note_red.png)
resolve_asset(ASSET_NOTE_WHITE             "assets/generated/notes/note_white.png"           notes note_white.png)
resolve_asset(ASSET_NOTE_YELLOW            "assets/generated/notes/note_yellow.png"          notes note_yellow.png)
resolve_asset(ASSET_HOPO_BLUE              "assets/generated/notes/hopo_blue.png"            notes hopo_blue.png)
resolve_asset(ASSET_HOPO_GREEN             "assets/generated/notes/hopo_green.png"           notes hopo_green.png)
resolve_asset(ASSET_HOPO_ORANGE            "assets/generated/notes/hopo_orange.png"          notes hopo_orange.png)
resolve_asset(ASSET_HOPO_RED               "assets/generated/notes/hopo_red.png"             notes hopo_red.png)
resolve_asset(ASSET_HOPO_WHITE             "assets/generated/notes/hopo_white.png"           notes hopo_white.png)
resolve_asset(ASSET_HOPO_YELLOW            "assets/generated/notes/hopo_yellow.png"          notes hopo_yellow.png)
resolve_asset(ASSET_CYM_BLUE              "assets/generated/notes/cym_blue.png"              notes cym_blue.png)
resolve_asset(ASSET_CYM_GREEN             "assets/generated/notes/cym_green.png"             notes cym_green.png)
resolve_asset(ASSET_CYM_RED               "assets/generated/notes/cym_red.png"               notes cym_red.png)
resolve_asset(ASSET_CYM_WHITE             "assets/generated/notes/cym_white.png"             notes cym_white.png)
resolve_asset(ASSET_CYM_YELLOW            "assets/generated/notes/cym_yellow.png"            notes cym_yellow.png)
# Bars
resolve_asset(ASSET_BAR_KICK               "assets/generated/bars/bar_kick.png"              bars bar_kick.png)
resolve_asset(ASSET_BAR_KICK_2X            "assets/generated/bars/bar_kick_2x.png"           bars bar_kick_2x.png)
resolve_asset(ASSET_BAR_OPEN               "assets/generated/bars/bar_open.png"              bars bar_open.png)
resolve_asset(ASSET_BAR_WHITE              "assets/generated/bars/bar_white.png"             bars bar_white.png)
# Overlays
resolve_asset(ASSET_OVR_NOTE_GHOST         "assets/generated/overlays/overlay_note_ghost.png"  overlays overlay_note_ghost.png)
resolve_asset(ASSET_OVR_CYM_GHOST          "assets/generated/overlays/overlay_cym_ghost.png"   overlays overlay_cym_ghost.png)
resolve_asset(ASSET_OVR_NOTE_TAP           "assets/generated/overlays/overlay_note_tap.png"    overlays overlay_note_tap.png)
resolve_asset(ASSET_OVR_CYM_ACCENT         "assets/generated/overlays/overlay_cym_accent.png"  overlays overlay_cym_accent.png)
resolve_asset(ASSET_OVR_NOTE_ACCENT         "assets/generated/overlays/overlay_note_accent.png" overlays overlay_note_accent.png)
# Sustains
resolve_asset(ASSET_SUSTAIN_OPEN           "assets/generated/sustains/sustain_open.png"       sustains sustain_open.png)
resolve_asset(ASSET_SUSTAIN_OPEN_WHITE     "assets/generated/sustains/sustain_open_white.png" sustains sustain_open_white.png)
# Markers
resolve_asset(ASSET_MARKER_BEAT            "assets/generated/markers/marker_beat.png"         markers marker_beat.png)
resolve_asset(ASSET_MARKER_HALF_BEAT       "assets/generated/markers/marker_half_beat.png"    markers marker_half_beat.png)
resolve_asset(ASSET_MARKER_MEASURE         "assets/generated/markers/marker_measure.png"      markers marker_measure.png)
# Lanes
resolve_asset(ASSET_LANE_END               "assets/generated/lanes/lane_end.png"              lanes lane_end.png)
resolve_asset(ASSET_LANE_MID               "assets/generated/lanes/lane_mid.png"              lanes lane_mid.png)
resolve_asset(ASSET_LANE_START             "assets/generated/lanes/lane_start.png"            lanes lane_start.png)
# UI elements
resolve_asset(ASSET_UI_SIDEBARS            "assets/generated/ui/sidebars.png"                 ui sidebars.png)
resolve_asset(ASSET_UI_LANELINES_GUITAR    "assets/generated/ui/lane_lines_guitar.png"        ui lane_lines_guitar.png)
resolve_asset(ASSET_UI_LANELINES_DRUMS     "assets/generated/ui/lane_lines_drums.png"         ui lane_lines_drums.png)
resolve_asset(ASSET_UI_STRIKELINE_GUITAR   "assets/generated/ui/strikeline_guitar.png"        ui strikeline_guitar.png)
resolve_asset(ASSET_UI_STRIKELINE_DRUMS    "assets/generated/ui/strikeline_drums.png"         ui strikeline_drums.png)
resolve_asset(ASSET_UI_STRIKELINE_CONN     "assets/generated/ui/strikeline_connectors.png"    ui strikeline_connectors.png)
resolve_asset(ASSET_UI_KICK_SMASHERS       "assets/generated/ui/kick_smashers.png"            ui kick_smashers.png)
# Hit animations — flares
resolve_asset(ASSET_HIT_FLARE_BLUE         "assets/generated/hits/hit_flare_blue.png"         hits hit_flare_blue.png)
resolve_asset(ASSET_HIT_FLARE_GREEN        "assets/generated/hits/hit_flare_green.png"        hits hit_flare_green.png)
resolve_asset(ASSET_HIT_FLARE_ORANGE       "assets/generated/hits/hit_flare_orange.png"       hits hit_flare_orange.png)
resolve_asset(ASSET_HIT_FLARE_RED          "assets/generated/hits/hit_flare_red.png"          hits hit_flare_red.png)
resolve_asset(ASSET_HIT_FLARE_YELLOW       "assets/generated/hits/hit_flare_yellow.png"       hits hit_flare_yellow.png)
resolve_asset(ASSET_HIT_FLARE_WHITE        "assets/generated/hits/hit_flare_white.png"        hits hit_flare_white.png)
# Hit animations — flash frames
resolve_asset(ASSET_HIT_FLASH_1            "assets/generated/hits/hit_flash_1.png"            hits hit_flash_1.png)
resolve_asset(ASSET_HIT_FLASH_2            "assets/generated/hits/hit_flash_2.png"            hits hit_flash_2.png)
resolve_asset(ASSET_HIT_FLASH_3            "assets/generated/hits/hit_flash_3.png"            hits hit_flash_3.png)
resolve_asset(ASSET_HIT_FLASH_4            "assets/generated/hits/hit_flash_4.png"            hits hit_flash_4.png)
resolve_asset(ASSET_HIT_FLASH_5            "assets/generated/hits/hit_flash_5.png"            hits hit_flash_5.png)
# Hit animations — kick frames
resolve_asset(ASSET_HIT_KICK_1             "assets/static/hits/hit_kick_1.png"                hits hit_kick_1.png)
resolve_asset(ASSET_HIT_KICK_2             "assets/static/hits/hit_kick_2.png"                hits hit_kick_2.png)
resolve_asset(ASSET_HIT_KICK_3             "assets/static/hits/hit_kick_3.png"                hits hit_kick_3.png)
resolve_asset(ASSET_HIT_KICK_4             "assets/static/hits/hit_kick_4.png"                hits hit_kick_4.png)
resolve_asset(ASSET_HIT_KICK_5             "assets/static/hits/hit_kick_5.png"                hits hit_kick_5.png)
resolve_asset(ASSET_HIT_KICK_6             "assets/static/hits/hit_kick_6.png"                hits hit_kick_6.png)
resolve_asset(ASSET_HIT_KICK_7             "assets/static/hits/hit_kick_7.png"                hits hit_kick_7.png)
# Hit animations — open frames
resolve_asset(ASSET_HIT_OPEN_1             "assets/static/hits/hit_open_1.png"                hits hit_open_1.png)
resolve_asset(ASSET_HIT_OPEN_2             "assets/static/hits/hit_open_2.png"                hits hit_open_2.png)
resolve_asset(ASSET_HIT_OPEN_3             "assets/static/hits/hit_open_3.png"                hits hit_open_3.png)
resolve_asset(ASSET_HIT_OPEN_4             "assets/static/hits/hit_open_4.png"                hits hit_open_4.png)
resolve_asset(ASSET_HIT_OPEN_5             "assets/static/hits/hit_open_5.png"                hits hit_open_5.png)
resolve_asset(ASSET_HIT_OPEN_6             "assets/static/hits/hit_open_6.png"                hits hit_open_6.png)
resolve_asset(ASSET_HIT_OPEN_7             "assets/static/hits/hit_open_7.png"                hits hit_open_7.png)

if(SKIN_DIR AND _SKIN_OVERRIDE_COUNT EQUAL 0)
    message(FATAL_ERROR
        "SKIN_DIR is set to '${SKIN_DIR}' but no matching assets were found.\n"
        "Place asset PNGs with matching filenames directly in the skin directory. "
        "See docs/SKINNING.md for the full list of replaceable filenames.")
endif()
if(SKIN_DIR AND _SKIN_OVERRIDE_COUNT GREATER 0)
    message(STATUS "Skin: ${_SKIN_OVERRIDE_COUNT} asset(s) overridden from ${SKIN_DIR}")
endif()

# Binary data (images + SVG)
# Assets are resolved via resolve_asset() above — skin overrides take priority
# over default paths in assets/generated/ and assets/static/.
# MIDI test files come from assets/midi/
juce_add_binary_data(ChartchoticData
    NAMESPACE BinaryData
    SOURCES
        # Fonts
        ${ASSET_FONT_TURRET_BOLD}
        ${ASSET_FONT_TURRET_MEDIUM}
        ${ASSET_FONT_RUSSO}
        ${ASSET_FONT_CHAKRA}
        # UI / branding
        ${ASSET_BG}
        ${ASSET_BG_RED}
        ${ASSET_ICON_GUITAR}
        ${ASSET_ICON_DRUMS}
        ${ASSET_LOGO_REAPER}
        # Default highway textures
        ${ASSET_HW_AMBER}
        ${ASSET_HW_DARKWOOD}
        ${ASSET_HW_GOTHIC}
        ${ASSET_HW_ORNATE}
        ${ASSET_HW_BLUECAUSTIC}
        ${ASSET_HW_FRETWOOD}
        ${ASSET_HW_BLUEMAGENTAHEX}
        ${ASSET_HW_ICYMETAL}
        ${ASSET_HW_BRIGHTRADIATED}
        ${ASSET_HW_MODERNHUEISTIC}
        ${ASSET_HW_REDCAUSTIC}
        ${ASSET_HW_SCRATCHEDREGULAR}
        # Notes (generated from PDFs)
        ${ASSET_NOTE_BLUE}
        ${ASSET_NOTE_GREEN}
        ${ASSET_NOTE_ORANGE}
        ${ASSET_NOTE_RED}
        ${ASSET_NOTE_WHITE}
        ${ASSET_NOTE_YELLOW}
        ${ASSET_HOPO_BLUE}
        ${ASSET_HOPO_GREEN}
        ${ASSET_HOPO_ORANGE}
        ${ASSET_HOPO_RED}
        ${ASSET_HOPO_WHITE}
        ${ASSET_HOPO_YELLOW}
        ${ASSET_CYM_BLUE}
        ${ASSET_CYM_GREEN}
        ${ASSET_CYM_RED}
        ${ASSET_CYM_WHITE}
        ${ASSET_CYM_YELLOW}
        # Bars (generated from PDFs)
        ${ASSET_BAR_KICK}
        ${ASSET_BAR_KICK_2X}
        ${ASSET_BAR_OPEN}
        ${ASSET_BAR_WHITE}
        # Overlays (generated from PDFs)
        ${ASSET_OVR_NOTE_GHOST}
        ${ASSET_OVR_CYM_GHOST}
        ${ASSET_OVR_NOTE_TAP}
        ${ASSET_OVR_CYM_ACCENT}
        ${ASSET_OVR_NOTE_ACCENT}
        # Sustains (generated from PDFs)
        ${ASSET_SUSTAIN_OPEN}
        ${ASSET_SUSTAIN_OPEN_WHITE}
        # Markers (generated from PDFs)
        ${ASSET_MARKER_BEAT}
        ${ASSET_MARKER_HALF_BEAT}
        ${ASSET_MARKER_MEASURE}
        # Lanes (generated from PDFs)
        ${ASSET_LANE_END}
        ${ASSET_LANE_MID}
        ${ASSET_LANE_START}
        # Track layers (generated from PDFs)
        ${ASSET_UI_SIDEBARS}
        ${ASSET_UI_LANELINES_GUITAR}
        ${ASSET_UI_LANELINES_DRUMS}
        ${ASSET_UI_STRIKELINE_GUITAR}
        ${ASSET_UI_STRIKELINE_DRUMS}
        ${ASSET_UI_STRIKELINE_CONN}
        ${ASSET_UI_KICK_SMASHERS}
        # Hit animations — flares (generated from PDFs)
        ${ASSET_HIT_FLARE_BLUE}
        ${ASSET_HIT_FLARE_GREEN}
        ${ASSET_HIT_FLARE_ORANGE}
        ${ASSET_HIT_FLARE_RED}
        ${ASSET_HIT_FLARE_YELLOW}
        ${ASSET_HIT_FLARE_WHITE}
        # Hit animations — flash frames (generated from PDFs)
        ${ASSET_HIT_FLASH_1}
        ${ASSET_HIT_FLASH_2}
        ${ASSET_HIT_FLASH_3}
        ${ASSET_HIT_FLASH_4}
        ${ASSET_HIT_FLASH_5}
        # Hit animations — kick frames (static)
        ${ASSET_HIT_KICK_1}
        ${ASSET_HIT_KICK_2}
        ${ASSET_HIT_KICK_3}
        ${ASSET_HIT_KICK_4}
        ${ASSET_HIT_KICK_5}
        ${ASSET_HIT_KICK_6}
        ${ASSET_HIT_KICK_7}
        # Hit animations — open frames (static)
        ${ASSET_HIT_OPEN_1}
        ${ASSET_HIT_OPEN_2}
        ${ASSET_HIT_OPEN_3}
        ${ASSET_HIT_OPEN_4}
        ${ASSET_HIT_OPEN_5}
        ${ASSET_HIT_OPEN_6}
        ${ASSET_HIT_OPEN_7}
        # MIDI test files loaded from disk at runtime (see CHARTCHOTIC_MIDI_ASSET_DIR)
)

# REAPER SDK header path
target_include_directories(Chartchotic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/reaper-sdk/sdk
)

# Compile definitions
target_compile_definitions(Chartchotic
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
    PRIVATE
        CHARTCHOTIC_BUILD_CHANNEL="${BUILD_CHANNEL}"
        CHARTCHOTIC_VERSION_STRING="${VERSION_STRING}"
        CHARTCHOTIC_MIDI_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/midi"
)

# Link libraries
target_link_libraries(Chartchotic
    PRIVATE
        ChartchoticData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Platform-specific flags
if(APPLE)
    target_compile_options(Chartchotic PRIVATE
        -Wno-deprecated-declarations
    )
endif()
