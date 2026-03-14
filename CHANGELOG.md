# Changelog

## 1.1.0

### Window Resizing
- Highway length now adjusts window height automatically
- Boosted highway length to 500%, slider moved next to note speed in top bar
- Free resize mode (settings toggle) lets you stretch the window to any shape you want

### Visual
- Higher resolution strikeline and note assets
- Guitar and drums now scale consistently at the same highway length setting
- Extended highways no longer clip at the top of the viewport
- Subtle perspective improvements for more consistent note placement
- Difficulty selector reversed - Expert on top

### Disco Flips
- Pro Drums: automatic red<->yellow lane swap during disco flip sections
- Reads `[mix 3 drums*d]` text events from MIDI
- Visible start/end markers on the highway so you can see exactly where flips happen
- Toggle in View menu when pro drums cymbals are enabled

### Other
- Proper Linux builds - VST3 and LV2 format!
- Skin override directory for building plugin with custom assets - see `docs/SKINNING.md`
- Fix texture tile loop causing infinite spin at narrow widths

## 1.0.0
This is the official 1.0 release of Chartchotic. This ground-up overhaul consists of a rewritten rendering engine, brand new UI, and comprehensive improvements to every visual asset.

### Rendering
- New chart rendering engine runs significantly leaner with smoother frametimes, capped at 15/30/60fps or your native display refresh rate
- Improved 3D perspective with curved fretboard placements for notes and other glyphs

### Assets
- All assets have been rerendered at a higher resolution from vector sources
- New ghost note, tap note, and accent overlays for drums and guitar
- New SP white variants for all assets and hit indicators

### Highway
- Highways have textures now - use one of the bundled textures (courtesy of kanaizo), or add your own
- Adjustable highway length up to 300% longer than standard

### Notes & Animations
- Adjustable sizes for note gems and kick/open bars
- Hit animations follow the highway curvature now
- Star Power kicks and opens flash white on hit

### UI
- Renamed from Chart Preview to Chartchotic with a new logo
- Major UI overhaul - redesigned toolbar, cleaner layout, consistent scaling
- Revamped chart toggles - organized controls for hiding/showing modifiers, chart elements, and scene layers
- Double-click most number values to type directly
- Calibration now supports negative latency up to -200ms
- Note speed reworked to integers 2–20, matching CH/YARG conventions
- Update notifications when a new version has launched

### Infrastructure
- CMake build system with automated CI/CD on all platforms
- macOS code signing and notarization with .pkg installer (no more Gatekeeper warnings)
- Windows installer via Inno Setup
- Pluginval validation on Windows builds

### Bug Fixes
- Fixed Star Power animation color when starting playback mid-sustain
- Fixed modifier notes rendering invisible over orange lane
- Fixed playhead crash in standalone mode

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
