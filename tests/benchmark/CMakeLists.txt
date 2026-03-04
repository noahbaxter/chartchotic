cmake_minimum_required(VERSION 3.22)
project(ChartPreviewBenchmark VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE (submodule in repo)
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/JUCE ${CMAKE_BINARY_DIR}/JUCE)
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

# Source root for project headers
set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../ChartPreview/Source)
set(ASSET_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../ChartPreview/Assets)

juce_add_console_app(bench_rendering PRODUCT_NAME "Chart Preview Benchmark")

target_sources(bench_rendering PRIVATE
    bench_rendering.cpp
    ${SOURCE_ROOT}/Visual/Renderers/HighwayRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/HighwayTextureRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GlyphRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/ColumnRenderer.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/Utils/PositionMath.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Utils/ChordAnalyzer.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Utils/LaneDetector.cpp
)

target_include_directories(bench_rendering PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(bench_rendering)

# Binary data (same assets as main plugin — needed for AssetManager)
juce_add_binary_data(BenchmarkData
    NAMESPACE BinaryData
    SOURCES
        # Raster assets (PNG/JPG)
        ${ASSET_ROOT}/raster/background.png
        ${ASSET_ROOT}/raster/background_red.jpg
        ${ASSET_ROOT}/raster/bar_kick.png
        ${ASSET_ROOT}/raster/bar_kick_2x.png
        ${ASSET_ROOT}/raster/bar_open.png
        ${ASSET_ROOT}/raster/bar_white.png
        ${ASSET_ROOT}/raster/cym_blue.png
        ${ASSET_ROOT}/raster/cym_green.png
        ${ASSET_ROOT}/raster/cym_red.png
        ${ASSET_ROOT}/raster/cym_white.png
        ${ASSET_ROOT}/raster/cym_yellow.png
        ${ASSET_ROOT}/raster/hopo_blue.png
        ${ASSET_ROOT}/raster/hopo_green.png
        ${ASSET_ROOT}/raster/hopo_orange.png
        ${ASSET_ROOT}/raster/hopo_red.png
        ${ASSET_ROOT}/raster/hopo_white.png
        ${ASSET_ROOT}/raster/hopo_yellow.png
        ${ASSET_ROOT}/raster/lane_end.png
        ${ASSET_ROOT}/raster/lane_mid.png
        ${ASSET_ROOT}/raster/lane_start.png
        ${ASSET_ROOT}/raster/marker_beat.png
        ${ASSET_ROOT}/raster/marker_half_beat.png
        ${ASSET_ROOT}/raster/marker_measure.png
        ${ASSET_ROOT}/raster/note_blue.png
        ${ASSET_ROOT}/raster/note_green.png
        ${ASSET_ROOT}/raster/note_orange.png
        ${ASSET_ROOT}/raster/note_red.png
        ${ASSET_ROOT}/raster/note_white.png
        ${ASSET_ROOT}/raster/note_yellow.png
        ${ASSET_ROOT}/raster/overlay_cym_accent.png
        ${ASSET_ROOT}/raster/overlay_cym_ghost.png
        ${ASSET_ROOT}/raster/overlay_cym_ghost_80scale.png
        ${ASSET_ROOT}/raster/overlay_note_accent.png
        ${ASSET_ROOT}/raster/overlay_note_ghost.png
        ${ASSET_ROOT}/raster/overlay_note_tap.png
        ${ASSET_ROOT}/raster/sustain_blue.png
        ${ASSET_ROOT}/raster/sustain_green.png
        ${ASSET_ROOT}/raster/sustain_open.png
        ${ASSET_ROOT}/raster/sustain_open_white.png
        ${ASSET_ROOT}/raster/sustain_orange.png
        ${ASSET_ROOT}/raster/sustain_red.png
        ${ASSET_ROOT}/raster/sustain_white.png
        ${ASSET_ROOT}/raster/sustain_yellow.png
        ${ASSET_ROOT}/raster/track_drum.png
        ${ASSET_ROOT}/raster/track_guitar.png
        ${ASSET_ROOT}/raster/hit_1.png
        ${ASSET_ROOT}/raster/hit_2.png
        ${ASSET_ROOT}/raster/hit_3.png
        ${ASSET_ROOT}/raster/hit_4.png
        ${ASSET_ROOT}/raster/hit_5.png
        ${ASSET_ROOT}/raster/hit_flare_blue.png
        ${ASSET_ROOT}/raster/hit_flare_green.png
        ${ASSET_ROOT}/raster/hit_flare_orange.png
        ${ASSET_ROOT}/raster/hit_flare_red.png
        ${ASSET_ROOT}/raster/hit_flare_yellow.png
        ${ASSET_ROOT}/raster/hit_kick_1.png
        ${ASSET_ROOT}/raster/hit_kick_2.png
        ${ASSET_ROOT}/raster/hit_kick_3.png
        ${ASSET_ROOT}/raster/hit_kick_4.png
        ${ASSET_ROOT}/raster/hit_kick_5.png
        ${ASSET_ROOT}/raster/hit_kick_6.png
        ${ASSET_ROOT}/raster/hit_kick_7.png
        ${ASSET_ROOT}/raster/hit_open_1.png
        ${ASSET_ROOT}/raster/hit_open_2.png
        ${ASSET_ROOT}/raster/hit_open_3.png
        ${ASSET_ROOT}/raster/hit_open_4.png
        ${ASSET_ROOT}/raster/hit_open_5.png
        ${ASSET_ROOT}/raster/hit_open_6.png
        ${ASSET_ROOT}/raster/hit_open_7.png
        # Vector assets (SVG) — existing
        ${ASSET_ROOT}/vector/gridline_beat.svg
        ${ASSET_ROOT}/vector/gridline_half_beat.svg
        ${ASSET_ROOT}/vector/gridline_measure.svg
        ${ASSET_ROOT}/vector/track_drum.svg
        ${ASSET_ROOT}/vector/track_guitar.svg
        ${ASSET_ROOT}/vector/logo-reaper.svg
        # Vector assets (SVG) — converted from source PDFs
        ${ASSET_ROOT}/vector/note_blue.svg
        ${ASSET_ROOT}/vector/note_green.svg
        ${ASSET_ROOT}/vector/note_orange.svg
        ${ASSET_ROOT}/vector/note_red.svg
        ${ASSET_ROOT}/vector/note_white.svg
        ${ASSET_ROOT}/vector/note_yellow.svg
        ${ASSET_ROOT}/vector/hopo_blue.svg
        ${ASSET_ROOT}/vector/hopo_green.svg
        ${ASSET_ROOT}/vector/hopo_orange.svg
        ${ASSET_ROOT}/vector/hopo_red.svg
        ${ASSET_ROOT}/vector/hopo_white.svg
        ${ASSET_ROOT}/vector/hopo_yellow.svg
        ${ASSET_ROOT}/vector/cym_blue.svg
        ${ASSET_ROOT}/vector/cym_green.svg
        ${ASSET_ROOT}/vector/cym_red.svg
        ${ASSET_ROOT}/vector/cym_white.svg
        ${ASSET_ROOT}/vector/cym_yellow.svg
        ${ASSET_ROOT}/vector/bar_kick.svg
        ${ASSET_ROOT}/vector/bar_kick_2x.svg
        ${ASSET_ROOT}/vector/bar_open.svg
        ${ASSET_ROOT}/vector/bar_white.svg
        ${ASSET_ROOT}/vector/overlay_note_ghost.svg
        ${ASSET_ROOT}/vector/overlay_cym_ghost.svg
        ${ASSET_ROOT}/vector/overlay_note_tap.svg
        ${ASSET_ROOT}/vector/sustain_open.svg
        ${ASSET_ROOT}/vector/sustain_open_white.svg
        ${ASSET_ROOT}/vector/hit_flare_green.svg
        ${ASSET_ROOT}/vector/hit_flare_red.svg
        ${ASSET_ROOT}/vector/hit_flare_yellow.svg
        ${ASSET_ROOT}/vector/hit_flare_blue.svg
        ${ASSET_ROOT}/vector/hit_flare_orange.svg
        ${ASSET_ROOT}/vector/hit_1.svg
        ${ASSET_ROOT}/vector/hit_2.svg
        ${ASSET_ROOT}/vector/hit_3.svg
        ${ASSET_ROOT}/vector/hit_4.svg
        ${ASSET_ROOT}/vector/hit_5.svg
)

target_compile_definitions(bench_rendering PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chart Preview"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
    CHARTPREVIEW_BUILD_CHANNEL="BENCH"
)

target_link_libraries(bench_rendering PRIVATE
    BenchmarkData
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

if(APPLE)
    target_link_libraries(bench_rendering PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()
