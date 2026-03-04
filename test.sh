#!/bin/bash

# Chart Preview - Test Runner
#
# Usage:
#   ./test.sh benchmark                 Build + run rendering benchmark, print table
#   ./test.sh benchmark --save-baseline Save current results as baseline
#   ./test.sh benchmark --compare       Compare against saved baseline (20% threshold)
#   ./test.sh benchmark --json          Output raw JSON instead of table
#   ./test.sh benchmark --iterations N  Override iteration count (default: 100)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BENCH_BUILD_DIR="$SCRIPT_DIR/build-benchmark"
BASELINE_PATH="$SCRIPT_DIR/tests/benchmark/baseline.json"

find_bench_bin() {
    for p in \
        "$BENCH_BUILD_DIR/bench_rendering_artefacts/bench_rendering" \
        "$BENCH_BUILD_DIR/bench_rendering_artefacts/Release/bench_rendering"; do
        if [ -f "$p" ]; then
            echo "$p"
            return
        fi
    done
}

usage() {
    echo "Usage: ./test.sh <command> [options]"
    echo ""
    echo "Commands:"
    echo "  benchmark    Build and run the rendering benchmark"
    echo ""
    echo "Benchmark options:"
    echo "  --save-baseline   Save results as the new baseline"
    echo "  --compare         Compare against saved baseline"
    echo "  --json            Output raw JSON instead of table"
    echo "  --iterations N    Override iteration count (default: 100)"
    echo "  --no-build        Skip build step (use existing binary)"
    exit 1
}

build_benchmark() {
    echo "Building benchmark (Release)..."
    cmake -B "$BENCH_BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -S "$SCRIPT_DIR/tests/benchmark" \
        > /dev/null 2>&1
    cmake --build "$BENCH_BUILD_DIR" --config Release 2>&1 | tail -1

    BENCH_BIN="$(find_bench_bin)"
    if [ -z "$BENCH_BIN" ]; then
        echo "Error: benchmark binary not found"
        exit 1
    fi
}

cmd_benchmark() {
    local SKIP_BUILD=false
    local BENCH_ARGS=()

    # Parse benchmark-specific args
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --no-build)     SKIP_BUILD=true ;;
            --save-baseline)
                BENCH_ARGS+=("--save-baseline" "--baseline" "$BASELINE_PATH")
                ;;
            --compare)
                BENCH_ARGS+=("--baseline" "$BASELINE_PATH")
                ;;
            --json)         BENCH_ARGS+=("--json") ;;
            --iterations)
                BENCH_ARGS+=("--iterations" "$2")
                shift
                ;;
            *)
                echo "Unknown option: $1"
                usage
                ;;
        esac
        shift
    done

    if [ "$SKIP_BUILD" = false ]; then
        build_benchmark
    else
        BENCH_BIN="$(find_bench_bin)"
        if [ -z "$BENCH_BIN" ]; then
            echo "Error: benchmark binary not found"
            echo "Run without --no-build first"
            exit 1
        fi
    fi

    "$BENCH_BIN" "${BENCH_ARGS[@]}"
}

# Main dispatch
case "${1:-}" in
    benchmark)  shift; cmd_benchmark "$@" ;;
    -h|--help)  usage ;;
    "")         usage ;;
    *)          echo "Unknown command: $1"; usage ;;
esac
