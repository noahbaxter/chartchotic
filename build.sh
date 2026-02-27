#!/bin/bash

# Chart Preview - Local Build Script
# Builds VST3 + AU for local testing and installs to plugin directories
#
# Usage:
#   ./build.sh                  Build Debug VST3+AU, install
#   ./build.sh release          Build Release VST3+AU, install
#   ./build.sh clean            Remove all build artifacts
#   ./build.sh --reaper         Build and open REAPER with test project
#   ./build.sh --vst3-only      Skip AU and Standalone builds
#   ./build.sh --au-only        Skip VST3 and Standalone builds
#   ./build.sh --standalone     Build Standalone app (skips VST3+AU unless specified)
#
# Debug builds check the DEV update channel, Release builds check RELEASE channel.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
REAPER_TEST_PROJECT="$SCRIPT_DIR/examples/reaper/reaper-test.RPP"

BUILD_CONFIG="Debug"
OPEN_REAPER=false
BUILD_VST3=true
BUILD_AU=true
BUILD_STANDALONE=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        release)        BUILD_CONFIG="Release" ;;
        clean)
            echo "Cleaning build artifacts..."
            rm -rf "$BUILD_DIR"
            echo "Done."
            exit 0
            ;;
        --reaper)       OPEN_REAPER=true ;;
        --vst3-only)    BUILD_AU=false; BUILD_STANDALONE=false ;;
        --au-only)      BUILD_VST3=false; BUILD_STANDALONE=false ;;
        --standalone)   BUILD_STANDALONE=true; BUILD_VST3=false; BUILD_AU=false ;;
        -h|--help)
            echo "Usage: ./build.sh [release|clean] [--reaper] [--vst3-only] [--au-only] [--standalone]"
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

# Setup REAPER SDK header if missing
REAPER_HEADER="$SCRIPT_DIR/third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h"
if [ ! -f "$REAPER_HEADER" ]; then
    echo "Setting up REAPER SDK header..."
    mkdir -p "$(dirname "$REAPER_HEADER")"
    cp "$SCRIPT_DIR/.ci/reaper-headers/reaper_vst3_interfaces.h" "$REAPER_HEADER"
fi

# Kill running instances before build to avoid locked files
if [ "$OPEN_REAPER" = true ]; then
    killall -9 REAPER 2>/dev/null || true
fi
if [ "$BUILD_STANDALONE" = true ]; then
    killall -9 "Chart Preview" 2>/dev/null || true
fi

# Configure CMake (Xcode generator for macOS)
echo ""
echo "Configuring CMake..."
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
    -DBUILD_CHANNEL="$BUILD_CHANNEL" \
    -S "$SCRIPT_DIR"

# Build targets
ARTIFACT_DIR="$BUILD_DIR/ChartPreview_artefacts/$BUILD_CONFIG"

if [ "$BUILD_VST3" = true ]; then
    echo ""
    echo "Building VST3..."
    cmake --build "$BUILD_DIR" --config "$BUILD_CONFIG" --target ChartPreview_VST3 -- -quiet

    VST3_PATH="$ARTIFACT_DIR/VST3/Chart Preview.vst3"
    if [ -d "$VST3_PATH" ]; then
        mkdir -p ~/Library/Audio/Plug-Ins/VST3/
        rm -rf ~/Library/Audio/Plug-Ins/VST3/Chart\ Preview.vst3
        cp -R "$VST3_PATH" ~/Library/Audio/Plug-Ins/VST3/
        echo "  VST3 installed"
    else
        echo "  VST3 build output not found at: $VST3_PATH"
        exit 1
    fi
fi

if [ "$BUILD_AU" = true ]; then
    echo ""
    echo "Building AU..."
    cmake --build "$BUILD_DIR" --config "$BUILD_CONFIG" --target ChartPreview_AU -- -quiet

    AU_PATH="$ARTIFACT_DIR/AU/Chart Preview.component"
    if [ -d "$AU_PATH" ]; then
        mkdir -p ~/Library/Audio/Plug-Ins/Components/
        rm -rf ~/Library/Audio/Plug-Ins/Components/Chart\ Preview.component
        cp -R "$AU_PATH" ~/Library/Audio/Plug-Ins/Components/
        echo "  AU installed"
    else
        echo "  AU build output not found at: $AU_PATH"
        exit 1
    fi
fi

if [ "$BUILD_STANDALONE" = true ]; then
    echo ""
    echo "Building Standalone..."
    cmake --build "$BUILD_DIR" --config "$BUILD_CONFIG" --target ChartPreview_Standalone -- -quiet

    APP_PATH="$ARTIFACT_DIR/Standalone/Chart Preview.app"
    if [ -d "$APP_PATH" ]; then
        echo "  Standalone built: $APP_PATH"
    else
        echo "  Standalone build output not found at: $APP_PATH"
        exit 1
    fi
fi

# Summary
echo ""
echo "========================================"
echo "Build complete! (channel: $BUILD_CHANNEL)"
echo "========================================"
[ "$BUILD_VST3" = true ]      && echo "  VST3:       ~/Library/Audio/Plug-Ins/VST3/Chart Preview.vst3"
[ "$BUILD_AU" = true ]         && echo "  AU:         ~/Library/Audio/Plug-Ins/Components/Chart Preview.component"
[ "$BUILD_STANDALONE" = true ] && echo "  Standalone: $APP_PATH"
echo ""

# Open REAPER if requested
if [ "$OPEN_REAPER" = true ]; then
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
    open "$APP_PATH"
fi
