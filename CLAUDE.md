# CLAUDE.md

Guidance for Claude Code when working with this repository.

## IMPORTANT: Git Commit Guidelines

**NEVER add co-authoring or "Generated with Claude Code" to commit messages unless explicitly requested.**

## CRITICAL: Use Local JUCE Submodule Only

**DO NOT reference /Applications/JUCE** — This project uses a custom JUCE with REAPER modifications.

**ALWAYS use**: `third_party/JUCE/` for all JUCE source code references.

When researching JUCE implementation details:
1. Check `third_party/JUCE/modules/` first
2. Refer to `docs/REAPER_INTEGRATION.md` for REAPER-specific details
3. Only use web searches if local resources are insufficient

## Local-Only Files

These are in `.gitignore` and must never be committed:
- `sessions/` (local task management)
- `.claude/` (local Claude Code configuration)

## Project Overview

Chart Preview is a VST/AU plugin that visualizes MIDI notes as rhythm game charts (Clone Hero/YARG style). Built with JUCE for Windows, macOS, and Linux.

**Current**: v0.9.5
**Roadmap & Bugs**: See `BACKLOG.md`

## Development Commands

### Building

**CI/CD**: GitHub Actions builds all platforms (Windows VST3 ~2.5MB, macOS universal VST3+AU ~6.7MB, Linux VST3 ~4.4MB)

**Local REAPER Testing** (macOS): `cd ChartPreview && ./build-scripts/build-local-test.sh [--open-reaper]`
  - Builds VST2 + VST3 in Debug mode, installs to plugin folders
  - `--open-reaper` flag to force quit & reopen REAPER

**macOS Release**: `./build-scripts/build-macos-release.sh` (see `ChartPreview/build-scripts/BUILD-RELEASE-GUIDE.md`)
**Windows**: Use `ChartPreview/Builds/VisualStudio2022/ChartPreview.sln`
**Linux**: `./build-linux.sh` or `make -j$(nproc) CONFIG=Release` in `Builds/LinuxMakefile/`

### Local Dev Setup

1. Clone the repo
2. Download JUCE 8.0.0 from GitHub releases, extract to `third_party/JUCE/`
3. Build scripts apply REAPER patches from `.ci/juce-patches/` automatically
4. CI downloads vanilla JUCE and applies patches fresh each build

### Projucer
Main file: `ChartPreview.jucer`. Build script auto-regenerates platform projects.

### Testing
Test DAW projects are local-only (gitignored `examples/` directory).

## Architecture

### Processing Chain

1. `ChartPreviewAudioProcessor` — Main plugin processor
2. **REAPER Mode**: `ReaperMidiPipeline` — Direct timeline MIDI access via REAPER API
3. **Standard Mode**: `MidiProcessor` — Processes MIDI buffer events
4. `MidiInterpreter` — Converts MIDI data to visual chart elements
5. `HighwayRenderer` — Renders the chart visualization
6. `ChartPreviewAudioProcessorEditor` — UI and real-time display

Pipeline auto-selected in `prepareToPlay()` based on REAPER detection. See `docs/REAPER_INTEGRATION.md` for details on both pipelines.

### Key Data Structures
- `NoteStateMapArray` — Tracks held notes across all MIDI pitches (thread-safe)
- `TrackWindow` / `SustainWindow` — Frame-based chart data for rendering
- `GridlineMap` — Beat/measure grid timing (thread-safe)
- `MidiCache` — Cached REAPER timeline MIDI with smart invalidation

### Threading

**CRITICAL**: Audio thread processes MIDI, GUI thread renders. Thread-safe with `juce::CriticalSection`.

**Race Condition Fix (v0.8.6)**: `ReaperMidiPipeline::processCachedNotesIntoState` holds `noteStateMapLock` for the ENTIRE clear+write operation. Never split atomic clear+write when updating shared data read by the renderer.

### Timing

All timing uses **PPQ (Pulses Per Quarter)** for tempo-independence. Absolute positioning from project start (not relative to playback start) — this is what makes scrubbing work.

**REAPER PPQ gotcha**: REAPER internal resolution is 960 PPQ per quarter note, VST playhead uses 1.0 = 1 QN. Must divide by 960. See `docs/REAPER_INTEGRATION.md`.

### 3D Perspective Rendering

`HighwayRenderer.cpp` — `createPerspectiveGlyphRect()` and related functions.

**GOTCHA**: Perspective math is duplicated across 6 functions that must stay in sync:
- `getGuitarGlyphRect()` / `getDrumGlyphRect()`
- `getGuitarGridlineRect()` / `getDrumGridlineRect()`
- `getGuitarLaneCoordinates()` / `getDrumLaneCoordinates()`

Any change to perspective algorithm must be applied to all 6. This is a major source of bugs if they drift apart.

### Instruments
**Drums**: Normal/Pro modes, ghost notes, accents, 2x kick, cymbal/tom differentiation, lanes
**Guitar**: Open notes, tap notes, HOPOs (configurable), sustains, lanes

## Important Implementation Details

### MIDI Pitch Mappings
Defined in `Utils.h` as `MidiPitchDefinitions`:
- Separate mappings for Guitar and Drums
- Multiple difficulty levels (Easy, Medium, Hard, Expert)
- Special pitches for star power, dynamics, lanes

### Performance
- 60 FPS default (configurable: 15/30/60/120/144)
- Draw call batching into `DrawCallMap` sorted by depth/layer
- MIDI caching in REAPER mode (cache invalidation on track switch, project change, MIDI edit)
- Debug logging: conditional string concatenation only when debug console is open

### Known Difficult Issues
- **Star Power rendering**: Partially working, architectural approach unclear. Three TODOs in code about SP state passing — needs to either be completed or removed.
- **Motion sickness**: Some users report it with scroll speed. No clear solution yet. Possibly related to frame drops or scroll acceleration.

## State Management

Plugin state stored in `juce::ValueTree`:
- `part`: Guitar/Drums
- `skillLevel`: Easy/Medium/Hard/Expert
- `framerate`: 15/30/60 FPS
- `latency`: 250ms/500ms/750ms/1000ms/1500ms
- `starPower`, `kick2x`, `dynamics`: Visibility toggles

## File Structure

- `ChartPreview/Source/` — Main source
- `ChartPreview/Builds/` — Platform-specific (MacOSX, VisualStudio2022, LinuxMakefile)
- `ChartPreview/Assets/` and `Audio/` — Embedded in binaries
- `ChartPreview/build-scripts/` — Build automation
- `.github/workflows/build.yml` — CI/CD
- `.ci/juce-patches/` — REAPER-specific JUCE patches applied at build time
- `.ci/reaper-headers/` — Tracked REAPER SDK header
- `third_party/` — External deps (git-ignored except README)
- `DO NOT DISTRIBUTE/` — Additional development assets

## Build System

**CI/CD**: Triggers on all pushes/PRs. Windows (VS2022), macOS (Xcode, universal VST3+AU), Linux (Make, VST3). Artifacts cached and auto-generated.

**Dependencies**: JUCE downloaded at CI time, patches from `.ci/juce-patches/` applied. No git submodules — REAPER SDK header committed to `.ci/reaper-headers/`.
