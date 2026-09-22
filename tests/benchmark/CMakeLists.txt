cmake_minimum_required(VERSION 3.22)
project(ChartchoticBenchmark VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE (submodule in repo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/JUCE ${CMAKE_BINARY_DIR}/JUCE)

# Source root for project headers
set(REPO_ROOT   ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SOURCE_ROOT ${REPO_ROOT}/Chartchotic/Source)

juce_add_console_app(bench_rendering PRODUCT_NAME "Chartchotic Benchmark")

target_sources(bench_rendering PRIVATE
    bench_main.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SceneRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/NoteRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SustainRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GridlineRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/TextEventRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/WaveformRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/TrackRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/ProceduralTrackArt.cpp
    ${SOURCE_ROOT}/Visual/Art/BarGemArt.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/Geometry/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Geometry/RenderTypeConfig.cpp
    ${SOURCE_ROOT}/Visual/Geometry/HitTestMapper.cpp
    ${SOURCE_ROOT}/Visual/Utils/FrameRenderer.cpp
)

target_include_directories(bench_rendering PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(bench_rendering)

# Binary data — globbed rather than listed. A hand-copied list silently rotted here once
# already: it still pointed at the pre-rename Chartchotic/Assets long after that directory
# was emptied. The benchmark only needs every BinaryData symbol AssetManager and
# TrackRenderer reference to resolve, so a superset is fine and cannot go stale.
file(GLOB_RECURSE BENCH_ASSETS CONFIGURE_DEPENDS
    ${REPO_ROOT}/assets/generated/*.png
    ${REPO_ROOT}/assets/generated/*.jpg
    ${REPO_ROOT}/assets/generated/*.svg
    ${REPO_ROOT}/assets/static/*.png
    ${REPO_ROOT}/assets/static/*.jpg
    ${REPO_ROOT}/assets/static/*.svg
    ${REPO_ROOT}/assets/static/*.ttf
)

if(NOT BENCH_ASSETS)
    message(FATAL_ERROR
        "No benchmark assets found under ${REPO_ROOT}/assets. Is the assets submodule checked out?")
endif()

juce_add_binary_data(BenchmarkData
    NAMESPACE BinaryData
    SOURCES ${BENCH_ASSETS}
)

target_compile_definitions(bench_rendering PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
)

target_link_libraries(bench_rendering PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    BenchmarkData
)

if(APPLE)
    target_link_libraries(bench_rendering PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

#==============================================================================
# Pipeline benchmark (MIDI processing, NoteProcessor, GemCalculator, MidiInterpreter)

juce_add_console_app(bench_pipeline PRODUCT_NAME "Chartchotic Pipeline Benchmark")

target_sources(bench_pipeline PRIVATE
    bench_pipeline.cpp
    ${SOURCE_ROOT}/Midi/Processing/NoteProcessor.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiProcessor.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Processing/TrackResolver.cpp
    ${SOURCE_ROOT}/Visual/Geometry/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SceneRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/NoteRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SustainRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GridlineRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/TextEventRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/WaveformRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/TrackRenderer.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/Art/BarGemArt.cpp
    ${SOURCE_ROOT}/Visual/HighwayComponent.cpp
    ${SOURCE_ROOT}/UI/LookAndFeel/ChartchoticLookAndFeel.cpp
)

target_include_directories(bench_pipeline PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(bench_pipeline)

target_compile_definitions(bench_pipeline PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
    CHARTCHOTIC_MIDI_ASSET_DIR="${REPO_ROOT}/assets/midi"
    CHARTCHOTIC_NO_BINARY_DATA=1
)

target_link_libraries(bench_pipeline PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

if(APPLE)
    target_link_libraries(bench_pipeline PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()
