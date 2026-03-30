#!/bin/bash

# Quick build + REAPER restart for iterative testing.
# Runs build.sh, then quits REAPER cleanly and reopens it with the test project.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REAPER_TEST_PROJECT="$SCRIPT_DIR/examples/reaper/reaper-test.RPP"

# Build (pass through any args like "release", "--vst3-only", etc.)
"$SCRIPT_DIR/build.sh" "$@"

# Quit REAPER cleanly via AppleScript (handles save prompts)
if pgrep -x REAPER > /dev/null; then
    echo "Quitting REAPER..."
    osascript -e 'tell application "REAPER" to quit' 2>/dev/null || true
    for i in {1..20}; do
        pgrep -x REAPER > /dev/null || break
        sleep 0.5
    done
    # If still alive after 10s, force kill
    if pgrep -x REAPER > /dev/null; then
        echo "REAPER didn't quit cleanly, force killing..."
        pkill -9 -x REAPER
        sleep 1
    fi
fi

# Reopen
if [ -f "$REAPER_TEST_PROJECT" ]; then
    echo "Opening REAPER with test project..."
    open -a "REAPER" "$REAPER_TEST_PROJECT"
else
    echo "Warning: test project not found at $REAPER_TEST_PROJECT"
    echo "Opening REAPER without project..."
    open -a "REAPER"
fi
