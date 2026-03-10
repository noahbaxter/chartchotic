# Skinning Guide

Chartchotic's visuals are PNG images compiled into the plugin binary via JUCE's BinaryData
system. You can reskin the plugin by replacing these PNGs and rebuilding.

## Quick Start

1. Create a folder and drop in your replacement PNGs (same filenames as the originals)
2. Only include the files you want to change — everything else falls back to defaults
3. Build with `--skin-dir`:

```bash
./build.sh --skin-dir /path/to/my-skin
```

The build checks your skin dir first for each asset and falls back to defaults for anything
missing. No need to touch the `assets/` submodule. You can also pass it directly to CMake:
`cmake -DSKIN_DIR=/path/to/my-skin ...`

**Highway textures and backgrounds don't require a rebuild** — they're loaded from disk at
runtime. See [Runtime-loaded assets](#runtime-loaded-assets-no-rebuild-needed) below.

## Skin Directory Structure

Your skin directory is completely flat — just drop PNGs in a folder:

```
my-skin/
  note_blue.png
  note_red.png
  bar_kick.png
  hit_flare_white.png
  ...
```

Only include the files you want to override. Everything else uses the defaults. Filenames
must match the originals exactly (see the [asset manifest](#asset-manifest) below for the
full list). Subdirectories are ignored.

## Dimensions

Assets in the same category should share the same **aspect ratio**. The actual pixel
dimensions can differ (the renderer downscales by width), but matching aspect ratios ensures
consistent visual alignment. The stock assets use these ratios:

| Category     | Aspect Ratio (W:H) |
|--------------|---------------------|
| `notes`      | 2:1                 |
| `bars`       | 16:1                |
| `overlays`   | 2:1                 |
| `hits`       | 1:1                 |
| `markers`    | 16:1                |
| `ui`         | *(varies)*          |

## Asset Manifest

### Notes (aspect ratio 2:1)

| Filename          | Description                               |
|-------------------|-------------------------------------------|
| `note_blue.png`   | Blue tom / guitar fret note               |
| `note_green.png`  | Green tom / guitar fret note              |
| `note_orange.png` | Orange guitar fret note                   |
| `note_red.png`    | Red tom / guitar fret note                |
| `note_white.png`  | Star Power note (used for all colors)     |
| `note_yellow.png` | Yellow tom / guitar fret note             |
| `hopo_blue.png`   | Blue HOPO (hammer-on/pull-off)            |
| `hopo_green.png`  | Green HOPO                                |
| `hopo_orange.png` | Orange HOPO                               |
| `hopo_red.png`    | Red HOPO                                  |
| `hopo_white.png`  | Star Power HOPO                           |
| `hopo_yellow.png` | Yellow HOPO                               |
| `cym_blue.png`    | Blue cymbal                               |
| `cym_green.png`   | Green cymbal                              |
| `cym_red.png`     | Red cymbal                                |
| `cym_white.png`   | Star Power cymbal                         |
| `cym_yellow.png`  | Yellow cymbal                             |

### Bars (aspect ratio 16:1)

| Filename          | Description                               |
|-------------------|-------------------------------------------|
| `bar_kick.png`    | Kick drum note                            |
| `bar_kick_2x.png` | 2x kick drum note                        |
| `bar_open.png`    | Guitar open strum                         |
| `bar_white.png`   | Star Power kick/open                      |

### Overlays (aspect ratio 2:1)

| Filename                    | Description                        |
|-----------------------------|------------------------------------|
| `overlay_note_ghost.png`    | Ghost note overlay (drums)         |
| `overlay_cym_ghost.png`     | Ghost cymbal overlay (drums)       |
| `overlay_note_tap.png`      | Tap note overlay (guitar)          |
| `overlay_note_accent.png`   | Accent tom overlay (drums)         |
| `overlay_cym_accent.png`    | Accent cymbal overlay (drums)      |

### Not Currently Skinnable

**Sustains** and **lanes** are drawn entirely programmatically as perspective-projected
trapezoids — no image assets are used. The sustain and lane PNGs exist in the asset pipeline
and are compiled into BinaryData, but the renderer does not reference them. Image-based
rendering for these may be added as a toggleable option in a future update.

### Markers (aspect ratio 16:1)

| Filename               | Description                            |
|------------------------|----------------------------------------|
| `marker_beat.png`      | Beat line                              |
| `marker_half_beat.png` | Half-beat line                         |
| `marker_measure.png`   | Measure (bar) line                     |

### Hit Animations (aspect ratio 1:1)

| Filename               | Description                            |
|------------------------|----------------------------------------|
| `hit_flare_white.png`  | White flare (also used to generate purple — see gotchas) |
| `hit_flare_orange.png` | Orange flare                           |
| `hit_flare_blue.png`   | Blue flare                             |
| `hit_flare_yellow.png` | Yellow flare                           |
| `hit_flare_red.png`    | Red flare                              |
| `hit_flare_green.png`  | Green flare                            |
| `hit_flash_1.png` … `hit_flash_5.png` | Flash animation (5 frames)  |
| `hit_kick_1.png` … `hit_kick_7.png`   | Kick hit animation (7 frames) |
| `hit_open_1.png` … `hit_open_7.png`   | Open hit animation (7 frames) |

### UI (no unified canvas)

Each UI asset has independent dimensions.

| Filename                    | Description                        |
|-----------------------------|------------------------------------|
| `sidebars.png`              | Left/right highway edge borders    |
| `lane_lines_guitar.png`     | Guitar lane divider lines          |
| `lane_lines_drums.png`      | Drum lane divider lines            |
| `strikeline_guitar.png`     | Guitar strikeline                  |
| `strikeline_drums.png`      | Drum strikeline                    |
| `strikeline_connectors.png` | Guitar strikeline sidebar connectors |
| `kick_smashers.png`         | Kick drum smasher graphic          |
| `icon_guitar.png`           | Guitar mode icon                   |
| `icon_drums.png`            | Drum mode icon                     |
| `background.png`            | Default scene background           |
| `logo-reaper.svg`           | REAPER logo (SVG)                  |

### Runtime-Loaded Assets (No Rebuild Needed)

Highway textures and backgrounds are loaded from disk at runtime — no rebuild needed.
Drop files in these folders and they appear in the in-plugin picker:

| Asset       | macOS                                                    | Windows                          |
|-------------|----------------------------------------------------------|----------------------------------|
| Highways    | `~/Library/Application Support/Chartchotic/highways/`    | `%APPDATA%/Chartchotic/highways/`    |
| Backgrounds | `~/Library/Application Support/Chartchotic/backgrounds/` | `%APPDATA%/Chartchotic/backgrounds/` |

Highway textures accept `.png`. Backgrounds accept `.png`, `.jpg`, `.jpeg`. Name them
whatever you want — the filename (without extension) becomes the display name.

## Renaming Files

**Don't rename files in your skin directory.** Filenames must match the originals exactly.
JUCE derives the BinaryData identifier from the filename (`note_blue.png` →
`BinaryData::note_blue_png`), so a renamed file won't be found by the code.

**Highway textures and backgrounds are the exception** — since they're loaded from disk,
you can name them anything.

## Gotchas

**Purple flare is runtime-generated from the white flare.** At startup, `hit_flare_white.png`
is pixel-tinted to produce a purple flare for tap notes. The tinting reads the red channel as
luminance (`R==G==B` assumed), so the white flare source must be grayscale-friendly — a
colorized white flare will produce wrong purple tones.

**BinaryData naming.** JUCE converts filenames to C++ identifiers. Underscores are fine, but
dashes and spaces become underscores too, which can cause collisions. Stick to
`snake_case` filenames with no special characters.

**All animation frames are viewport-scaled.** Kick, open, flash, and flare frames are all in
the `scalableAssets` list and get downscaled to match the viewport width at runtime via
`AssetManager::rescaleForWidth`. Source PNGs should be high-resolution — the renderer
downscales as needed but cannot upscale beyond your original resolution.

**Highway textures tile vertically.** Highway textures scroll down the fretboard, so they
should be vertically seamless if you want smooth scrolling without visible seams.