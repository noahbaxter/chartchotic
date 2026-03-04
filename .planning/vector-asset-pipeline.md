# Vector Asset Pipeline — Status & Findings

## What We Built

### Conversion Pipeline
- `scripts/convert_assets.sh` — Inkscape CLI converts source PDFs → SVGs
- `assets/source/asset_mapping.json` — PDF→SVG name mapping (37 assets)
- `assets/source/` — git submodule pointing to `noahbaxter/chartchotic-assets` (private repo with 63 PDFs + .ai file)
- All 36 SVGs generated successfully in `ChartPreview/Assets/vector/`

### AssetManager Changes (AssetManager.h/cpp)
- Added `Drawable` unique_ptrs for all 36 SVG-backed assets
- `initVectorAssets()` loads SVG Drawables from BinaryData
- `renderCachedImages(width, height)` renders each Drawable to a `juce::Image` at 2x viewport size, overwriting the PNG-loaded fallback
- `renderDrawableToImage()` static helper preserves source aspect ratio
- No-ops if dimensions haven't changed (cached width/height check)
- PNG loading in `initRasterAssets()` stays as fallback — if no SVG Drawable exists, the PNG Image is used

### Cache Invalidation (HighwayRenderer.cpp)
- `assetManager.renderCachedImages(width, height)` called at top of `paint()` after setting dimensions from clipBounds
- Only re-renders when viewport size changes

### CMakeLists.txt
- Both main and benchmark CMakeLists updated with new SVG entries alongside existing PNGs
- Benchmark CMakeLists also fixed: stale flat `Assets/` paths updated to `Assets/raster/` and `Assets/vector/`

### Infrastructure
- `.gitignore` updated for `assets/source/*` (private submodule safety net)
- CRLF prevention hook installed at `~/.claude/hooks/check-crlf.sh` (PreToolUse on Write|Edit)

## What We Found

### JUCE SVG Parser Limitations
- **Cannot handle complex Inkscape-converted SVGs.** The cymbal/note PDFs with curved gradients get converted to SVGs with 300+ path segments approximating the gradients. JUCE's SVG parser renders these incorrectly — flat colored shapes with broken clipping instead of smooth 3D gems.
- JUCE's parser doesn't support `clipPath`, has known gradient issues, and struggles with complex files.
- Simple SVGs (gridlines, tracks — few paths) render perfectly and are FASTER than PNG `drawImage` resampling.

### Performance Reality
- **Simple SVGs rendered directly via `Drawable::drawWithin()` per frame = faster than `drawImage` resampling.** This is because rasterizing a few paths is cheaper than bilinear interpolation on a bitmap. This was proven with gridlines/tracks.
- **Complex SVGs (hundreds of paths) rendered per frame = dramatically slower.** JUCE walks every path segment on CPU.
- **The cached Image pipeline (SVG → render to Image → drawImage) has the same performance as PNGs.** `drawImage` cost is proportional to destination rect size, not source size. Bigger source images don't make it faster.
- **The core bottleneck is per-note `drawImage` with perspective transforms at high note density.** This is CPU bilinear interpolation, and the cost scales with number of notes × destination pixel area.

### OpenGL/GPU Acceleration
- JUCE community consensus: **avoid OpenGL in plugins.** Threading issues, DAW compatibility problems, Windows driver failures, deprecated on macOS.
- Simply attaching `OpenGLContext` rarely helps for 2D — CPU still does path work.
- `drawImage` with OpenGL can be SLOWER due to per-call texture upload/delete overhead.
- Would need custom `OpenGLTexture` management to actually benefit — essentially a custom renderer.

### Key Insight
The performance win with gridline SVGs came from **eliminating `drawImage` resampling entirely** — JUCE drew vector paths directly to the destination at the exact size needed. One rasterization pass, no resampling. This only works for simple geometry (few paths).

## Options Going Forward

### 1. Simplify Source Art for Direct SVG Rendering
If the source art could be simplified to use flat colors with simple gradients (linear/radial only, no curved gradients), Illustrator could export clean SVGs that JUCE handles natively. Then `Drawable::drawWithin()` per frame would work and be fast. **Trades visual fidelity for performance.**

### 2. resvg4JUCE (github.com/JanosGit/Resvg4JUCE)
Drop-in JUCE module wrapping the resvg library (Rust-based, full SVG spec support). Renders SVGs to `juce::Image` correctly — handles clipPaths, complex gradients, everything JUCE's parser can't. Could be used for the cached Image pipeline to get correct visual quality. **Doesn't help performance** (still drawImage resampling), but fixes the rendering quality issue.

### 3. High-Res PNG via Inkscape
Change conversion script to render PDF → large PNG (512px–1024px wide) instead of SVG. Guaranteed pixel-perfect. Same pipeline, just bigger rasters. **Quality improvement only, no performance change.**

### 4. Mip-Mapped Asset Cache
Pre-render each note at several discrete sizes (32, 64, 128, 256, 512px). At render time, pick the closest size to the destination. Minimizes resampling cost (never scaling more than 2x). **Moderate performance improvement + quality improvement.**

### 5. Hybrid: Simple SVG for Simple Assets, Raster for Complex
Use direct `Drawable::drawWithin()` for assets with few paths (gridlines, overlays, simple shapes). Use high-res cached rasters for complex gradient-heavy assets (note gems, cymbals). **Best of both worlds but more code complexity.**

## Files Modified (This Session)
- `ChartPreview/Source/Visual/Managers/AssetManager.h` — full rewrite with Drawable support
- `ChartPreview/Source/Visual/Managers/AssetManager.cpp` — full rewrite with SVG loading + cache rendering
- `ChartPreview/Source/Visual/Renderers/HighwayRenderer.cpp` — added renderCachedImages call
- `CMakeLists.txt` — added 36 SVG entries
- `tests/benchmark/CMakeLists.txt` — fixed stale asset paths, added SVG entries
- `.gitignore` — added assets/source exclusion
- `scripts/convert_assets.sh` — new (conversion script)
- `assets/source/asset_mapping.json` — new (name mapping)
- `~/.claude/settings.json` — added CRLF prevention hook
- `~/.claude/hooks/check-crlf.sh` — new (CRLF hook)

## Uncommitted State
All changes are on `dev` branch, uncommitted. The SVG rendering looks wrong (JUCE parser issue), so this shouldn't be committed as-is. The infrastructure (conversion script, submodule, AssetManager Drawable support) is solid and reusable regardless of which rendering approach is chosen.
