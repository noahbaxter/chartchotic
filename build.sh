#!/bin/bash

# Chart Preview - Local Build Script
# Builds VST3 + AU for local testing and installs to plugin directories
#
# Usage:
#   ./build.sh                  Build Debug VST3+AU, install, open REAPER
#   ./build.sh release          Build Release VST3+AU, install
#   ./build.sh clean            Remove all build artifacts
#   ./build.sh --no-reaper      Build without opening REAPER
#   ./build.sh --vst3-only      Skip AU and Standalone builds
#   ./build.sh --au-only        Skip VST3 and Standalone builds
#   ./build.sh --standalone     Also build Standalone app (for testing without DAW)
#
# Debug builds check the DEV update channel, Release builds check RELEASE channel.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/ChartPreview"
JUCER_FILE="$PROJECT_DIR/ChartPreview.jucer"
XCODE_DIR="$PROJECT_DIR/Builds/MacOSX"
XCODE_PROJECT="$XCODE_DIR/ChartPreview.xcodeproj"
REAPER_TEST_PROJECT="$SCRIPT_DIR/examples/reaper/reaper-test.RPP"

BUILD_CONFIG="Debug"
OPEN_REAPER=true
BUILD_VST3=true
BUILD_AU=true
BUILD_STANDALONE=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        release)        BUILD_CONFIG="Release" ;;
        clean)
            echo "Cleaning build artifacts..."
            rm -rf "$XCODE_DIR/build"
            echo "Done."
            exit 0
            ;;
        --no-reaper)    OPEN_REAPER=false ;;
        --vst3-only)    BUILD_AU=false; BUILD_STANDALONE=false ;;
        --au-only)      BUILD_VST3=false; BUILD_STANDALONE=false ;;
        --standalone)   BUILD_STANDALONE=true ;;
        -h|--help)
            echo "Usage: ./build.sh [release|clean] [--no-reaper] [--vst3-only] [--au-only] [--standalone]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg"
            exit 1
            ;;
    esac
done

# Debug -> DEV channel, Release -> RELEASE channel
if [ "$BUILD_CONFIG" = "Debug" ]; then
    BUILD_CHANNEL="DEV"
else
    BUILD_CHANNEL="RELEASE"
fi

echo "========================================"
echo "Chart Preview — $BUILD_CONFIG Build (update channel: $BUILD_CHANNEL)"
echo "========================================"

# Verify project exists
if [ ! -f "$JUCER_FILE" ]; then
    echo "Error: ChartPreview.jucer not found at $JUCER_FILE"
    exit 1
fi

# Kill REAPER before build to avoid locked files
if [ "$OPEN_REAPER" = true ]; then
    killall -9 REAPER 2>/dev/null || true
    sleep 0.5
fi

# Regenerate Xcode project if .jucer is newer
if [ "$JUCER_FILE" -nt "$XCODE_PROJECT" ] || [ ! -d "$XCODE_PROJECT" ]; then
    echo "Regenerating Xcode project..."
    "/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "$JUCER_FILE"
fi

# Clean previous build
rm -rf "$XCODE_DIR/build"

CHANNEL_DEFINE="GCC_PREPROCESSOR_DEFINITIONS=\$(GCC_PREPROCESSOR_DEFINITIONS) CHARTPREVIEW_BUILD_CHANNEL=\\\"${BUILD_CHANNEL}\\\""

# Build VST3
if [ "$BUILD_VST3" = true ]; then
    echo ""
    echo "Building VST3..."
    xcodebuild -quiet \
        -project "$XCODE_PROJECT" \
        -target "ChartPreview - VST3" \
        -configuration "$BUILD_CONFIG" \
        "$CHANNEL_DEFINE"

    VST3_PATH="$XCODE_DIR/build/$BUILD_CONFIG/ChartPreview.vst3"
    if [ -d "$VST3_PATH" ]; then
        mkdir -p ~/Library/Audio/Plug-Ins/VST3/
        rm -rf ~/Library/Audio/Plug-Ins/VST3/ChartPreview.vst3
        cp -R "$VST3_PATH" ~/Library/Audio/Plug-Ins/VST3/
        echo "  VST3 installed"
    else
        echo "  VST3 build output not found"
        exit 1
    fi
fi

# Build AU
if [ "$BUILD_AU" = true ]; then
    echo ""
    echo "Building AU..."
    xcodebuild -quiet \
        -project "$XCODE_PROJECT" \
        -target "ChartPreview - AU" \
        -configuration "$BUILD_CONFIG" \
        "$CHANNEL_DEFINE"

    AU_PATH="$XCODE_DIR/build/$BUILD_CONFIG/ChartPreview.component"
    if [ -d "$AU_PATH" ]; then
        mkdir -p ~/Library/Audio/Plug-Ins/Components/
        rm -rf ~/Library/Audio/Plug-Ins/Components/ChartPreview.component
        cp -R "$AU_PATH" ~/Library/Audio/Plug-Ins/Components/
        echo "  AU installed"
    else
        echo "  AU build output not found"
        exit 1
    fi
fi

# Build Standalone
if [ "$BUILD_STANDALONE" = true ]; then
    echo ""
    echo "Building Standalone..."
    xcodebuild -quiet \
        -project "$XCODE_PROJECT" \
        -target "ChartPreview - Standalone Plugin" \
        -configuration "$BUILD_CONFIG" \
        "$CHANNEL_DEFINE"

    APP_PATH="$XCODE_DIR/build/$BUILD_CONFIG/ChartPreview.app"
    if [ -d "$APP_PATH" ]; then
        echo "  Standalone built: $APP_PATH"
        echo "  Run with: open \"$APP_PATH\""
    else
        echo "  Standalone build output not found"
        exit 1
    fi
fi

# Summary
echo ""
echo "========================================"
echo "Build complete! (channel: $BUILD_CHANNEL)"
echo "========================================"
[ "$BUILD_VST3" = true ]      && echo "  VST3:       ~/Library/Audio/Plug-Ins/VST3/ChartPreview.vst3"
[ "$BUILD_AU" = true ]         && echo "  AU:         ~/Library/Audio/Plug-Ins/Components/ChartPreview.component"
[ "$BUILD_STANDALONE" = true ] && echo "  Standalone: $XCODE_DIR/build/$BUILD_CONFIG/ChartPreview.app"
echo ""

# Open REAPER (skip if standalone-only)
if [ "$OPEN_REAPER" = true ] && [ "$BUILD_STANDALONE" != true -o "$BUILD_VST3" = true -o "$BUILD_AU" = true ]; then
    if [ -f "$REAPER_TEST_PROJECT" ]; then
        echo "Opening REAPER with test project..."
        open -a "REAPER" "$REAPER_TEST_PROJECT"
    else
        echo "Opening REAPER..."
        open -a "REAPER"
    fi
fi

# Auto-open standalone if it was built
if [ "$BUILD_STANDALONE" = true ]; then
    echo "Opening Standalone..."
    open "$XCODE_DIR/build/$BUILD_CONFIG/ChartPreview.app"
fi
