# Changelog

## 0.9.6

- Rework speed slider to integer note speed (2–20), matching YARG/CH conventions
- CI: unit tests run on Linux build
- Fix playhead null dereference in standalone mode
- Fix compiler warnings in AssetManager gem/overlay switches
- CMake migration from Projucer
- CI/CD pipeline: automated dev + release builds on all platforms
- macOS code signing + notarization, .pkg installer
- Windows installer (Inno Setup)
- In-plugin update checker (dev + release channels)
- VERSION file as single source of truth for versioning
- Consolidated local build into single `build.sh`

## 0.9.5

- Fix modifier notes rendering as GEM::NONE over orange lane, blocking orange notes from rendering with modifiers

## 0.9.4

- Unique hit animations for open guitar notes
- REAPER: mouse scroll to move timeline forward/backward, shift for precision
- REAPER: automatic MIDI track detection, removed manual track selector UI
- REAPER: more efficient bulk-fetched MIDI data
- Fix green guitar notes positioned incorrectly
- Fix 0-tick and 1-tick notes failing to render (Moonscraper exports these)
- Fix hit animations not triggering when starting playback mid-sustain
- Fix Z-ordering for hit animations being cut off by notes
- Major refactor to MIDI processing, rendering pipeline, and REAPER API handling

## 0.9.3

- Major performance fixes for live updating while paused
- Fix star power not displaying at all (regression)

## 0.9.2

- Manual latency offset textbox (-2s to +2s)
- Throttle REAPER MIDI polling while paused (max 20Hz)

## 0.9.1

- Fix forced HOPOs not working on chords
- Fix gridlines breaking halfway through measures during playback
- Fix highway not updating when paused until cursor moves
- Replace track number buttons with text box (type any number or UP/DOWN to scroll)

## 0.9.0

- REAPER integration: reads MIDI directly from timeline instead of realtime buffers
  - Zero latency between cursor movement and note display
  - Timeline scrubbing while playback is stopped
  - Track selector from within the plugin
- Hit animations during playback (toggleable)
- Highway rendering switched to seconds-based (fixes time sig/BPM change glitches)
- Version number display in bottom left corner
- Fix HOPOs not working for chords

## 0.8.5

- Lanes enabled
- Adjusted glyph positioning

## 0.8.0

- Cross-platform CI builds (macOS universal binary, Windows, Linux)
- Installation improvements across all platforms

## 0.7.0

- Linux support
- Note sustains implemented
- General performance/stability improvements
- Fix certain notes never displaying
- Fix settings not persisting across session reloads
- Optimized build system (Windows bundles reduced from 32MB to ~2MB)

## 0.6.0

- User-selectable latency (250ms–1s)
- 60fps rendering (much smoother, more CPU efficient)
- Fix note render layering (kicks above hits, double notes on note-offs)
- Proper 3D perspective replacing previous fake-out scaling
- Meter grid lines with proper measure starts and time sig change support
- Tick-based positioning refactor

## 0.5.0

- Initial feature-complete drum charting experience
- Guitar and drum note rendering
- Basic settings (skill level, instrument, framerate)
