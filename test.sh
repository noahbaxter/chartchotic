#!/bin/bash

# Chart Preview - Test Runner
#
# Usage:
#   ./test.sh test [unit|integration|compliance] [-v]   Run tests (all by default)
#   ./test.sh benchmark [options]                        Run rendering benchmark
#   ./test.sh bench                                      Alias for benchmark
#   ./test.sh perf                                       Alias for benchmark
#
# Benchmark options:
#   --save-baseline   Save results as the new baseline
#   --compare         Compare against saved baseline (20% threshold)
#   --json            Output raw JSON instead of table
#   --iterations N    Override iteration count (default: 100)
#   --no-build        Skip build step (use existing binary)

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
    echo "  test         Run tests (delegates to scripts/test.sh)"
    echo "  benchmark    Build and run the rendering benchmark"
    echo "  bench        Alias for benchmark"
    echo "  perf         Alias for benchmark"
    echo ""
    echo "Test options:"
    echo "  unit|integration|compliance   Run specific test type"
    echo "  -v, --verbose                 Verbose output"
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
    test)                                       shift; "$SCRIPT_DIR/scripts/test.sh" "$@" ;;
    bench|benchmark|perf|performance)           shift; cmd_benchmark "$@" ;;
    -h|--help)                                  usage ;;
    "")                                         usage ;;
    *)                                          echo "Unknown command: $1"; usage ;;
esac
