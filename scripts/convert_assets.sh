#!/usr/bin/env bash
#
# convert_assets.sh — Convert source PDFs to SVGs and high-res PNGs via Inkscape
#
# Usage: ./scripts/convert_assets.sh [--force]
#
# Reads assets/source/asset_mapping.json for PDF→asset name mapping.
# "vector" entries → SVG to ChartPreview/Assets/vector/{name}.svg
# "raster" entries → PNG to ChartPreview/Assets/raster/{name}.png (1024px wide,
#   bars at 2048px)
#
# Skips conversion if the output is newer than the source PDF,
# unless --force is passed.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/assets/source"
VECTOR_DIR="$ROOT_DIR/ChartPreview/Assets/vector"
RASTER_DIR="$ROOT_DIR/ChartPreview/Assets/raster"
MAPPING_FILE="$SOURCE_DIR/asset_mapping.json"

FORCE=false
if [[ "${1:-}" == "--force" ]]; then
    FORCE=true
fi

# Check dependencies
if ! command -v inkscape &>/dev/null; then
    echo "ERROR: inkscape not found. Install via: brew install --cask inkscape"
    exit 1
fi

if ! command -v jq &>/dev/null; then
    echo "ERROR: jq not found. Install via: brew install jq"
    exit 1
fi

if [[ ! -f "$MAPPING_FILE" ]]; then
    echo "ERROR: Mapping file not found: $MAPPING_FILE"
    exit 1
fi

mkdir -p "$VECTOR_DIR" "$RASTER_DIR"

vec_converted=0
vec_skipped=0
vec_failed=0
ras_converted=0
ras_skipped=0
ras_failed=0

# Convert vector assets (PDF → SVG)
echo "=== Vector assets (SVG) ==="
jq -r '.vector | to_entries[] | "\(.key)\t\(.value)"' "$MAPPING_FILE" | while IFS=$'\t' read -r src_name dst_name; do
    src_path="$SOURCE_DIR/$src_name"
    dst_path="$VECTOR_DIR/${dst_name}.svg"

    if [[ ! -f "$src_path" ]]; then
        echo "SKIP: Source not found: $src_name"
        ((vec_skipped++)) || true
        continue
    fi

    if [[ "$FORCE" != true && -f "$dst_path" && "$dst_path" -nt "$src_path" ]]; then
        ((vec_skipped++)) || true
        continue
    fi

    echo "SVG: $src_name → ${dst_name}.svg"
    if inkscape --export-type=svg --export-plain-svg --export-filename="$dst_path" "$src_path" 2>/dev/null; then
        ((vec_converted++)) || true
    else
        echo "FAILED: $src_name"
        ((vec_failed++)) || true
    fi
done

# Convert raster assets (PDF → high-res PNG)
echo ""
echo "=== Raster assets (PNG) ==="
jq -r '.raster | to_entries[] | "\(.key)\t\(.value)"' "$MAPPING_FILE" | while IFS=$'\t' read -r src_name dst_name; do
    src_path="$SOURCE_DIR/$src_name"
    dst_path="$RASTER_DIR/${dst_name}.png"

    if [[ ! -f "$src_path" ]]; then
        echo "SKIP: Source not found: $src_name"
        ((ras_skipped++)) || true
        continue
    fi

    if [[ "$FORCE" != true && -f "$dst_path" && "$dst_path" -nt "$src_path" ]]; then
        ((ras_skipped++)) || true
        continue
    fi

    # Bars get 2048px width, everything else 1024px
    case "$dst_name" in
        bar_*) export_width=2048 ;;
        *)     export_width=1024 ;;
    esac

    echo "PNG: $src_name → ${dst_name}.png (${export_width}px)"
    if inkscape --export-type=png --export-width="$export_width" --export-filename="$dst_path" "$src_path" 2>/dev/null; then
        ((ras_converted++)) || true
    else
        echo "FAILED: $src_name"
        ((ras_failed++)) || true
    fi
done

echo ""
echo "Done."
echo "  Vector: Converted=$vec_converted, Skipped=$vec_skipped, Failed=$vec_failed"
echo "  Raster: Converted=$ras_converted, Skipped=$ras_skipped, Failed=$ras_failed"
