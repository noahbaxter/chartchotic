# Rendering Profiler Improvements

Follow-up to commit `6a85076` (rendering profiler: per-phase benchmark with headless pipeline).

## 1. Add missing phase timings

Three chunks of work in `HighwayRenderer::paint()` aren't captured by any phase:

- **Animation detection + `renderToDrawCallMap`** — between `build_gridlines` and
  `execute_draws` (~lines 79-85). Invisible when `hitIndicators` is true.
- **`onAfterLanes` callback** — fires inside `execute_draws` but isn't attributed to any
  `layer_us` entry. Inflates `execute_draws` with no way to diagnose why.
- **`animationRenderer.advanceFrames()`** — after `execute_draws` but before `total`
  (~lines 123-127). Shows in total but no phase owns it.

Add fields to `PhaseTiming` struct and instrument these three gaps. Tighten the sanity
threshold in `test_benchmark.py` once the phase sum actually accounts for everything.

## 2. Add Drums benchmark scenario

`bench_rendering.cpp` only exercises `Part::GUITAR`. Drums have different column counts,
coordinate tables, kick/cymbal rendering. Add at least one Drums scenario (medium density)
to catch drum-specific regressions. May need a drum variant in `bench_helpers.h` synthetic
data factories.

## 3. Decide on DEBUG guards

Currently the `PhaseTiming` struct, `collectPhaseTiming` bool, and all branch points compile
into release builds unconditionally. Overhead is near-zero (predicted-not-taken branch per
phase), but it's worth deciding:

- **Option A: `#if DEBUG`** — zero release overhead, but benchmark binary needs a debug build
- **Option B: Keep as-is** — trivial cost, benchmark can run against release builds

Leaning toward keeping as-is since the overhead is negligible and it lets the benchmark test
release-compiled code paths, which is the whole point.

## 4. RAII cleanup (nice-to-have)

The 7 repeated timing pairs (`auto t = collectPhaseTiming ? Clock::now() : ...` /
`if (collectPhaseTiming) field = ...`) are functional but noisy. A small `ScopedPhaseMeasure`
RAII helper would collapse each to one line and make adding new phases easier. Do this
whenever touching the timing code for items 1-2.
