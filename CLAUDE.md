# Chart Preview

VST/AU plugin visualizing MIDI as rhythm game charts (Clone Hero/YARG style).

## JUCE Source

DO NOT reference `/Applications/JUCE`. This project patches vanilla JUCE with REAPER
modifications. Always use `third_party/JUCE/` — check `docs/REAPER_INTEGRATION.md` for
REAPER-specific details.

## Building

Local build (macOS): `./build.sh` (builds VST3+AU, installs, opens REAPER)
Options: `release`, `clean`, `--no-reaper`, `--vst3-only`, `--au-only`

Release builds are handled by CI. Push to `dev` → `dev-latest` prerelease. Merge to `main`
with VERSION change → tagged release. Version source of truth: `VERSION` file (injected into
.jucer at build time). macOS builds are signed + notarized. Windows builds include Inno Setup installer.

## Versioning

`VERSION` file at repo root is the single source of truth. CI injects it into `.jucer` before
Projucer runs. The `JucePlugin_VersionString` define is used in code (not hardcoded strings).
Build channel (`DEV`/`RELEASE`) is injected via preprocessor define `CHARTPREVIEW_BUILD_CHANNEL`.

## Watch Out

**Threading**: Audio thread processes MIDI, GUI thread renders. When updating `noteStateMapArray`,
hold `noteStateMapLock` for the ENTIRE clear+write operation. Never split it — the renderer reads
between calls and you'll get flicker/race conditions. (Burned us in v0.8.6.)

**Perspective math duplication**: `GlyphRenderer::createPerspectiveGlyphRect()` and
`PositionMath::createPerspectiveGlyphRect()` contain duplicated perspective calculations.
Any change to perspective must be applied to both. The 6 functions that call into them
(getGuitar/getDrum variants for glyphs, gridlines, lanes) share coordinates but the core
math lives in two places.

**Roadmap & Bugs**: `BACKLOG.md` — check before starting work to avoid duplicating known issues.
