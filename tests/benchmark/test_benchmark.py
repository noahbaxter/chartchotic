"""
Rendering pipeline profiler.

Invokes the C++ benchmark binary in --profile mode, parses JSON output,
prints per-phase breakdown tables, and optionally compares against a
baseline for regression detection.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

BENCHMARK_DIR = Path(__file__).parent
BASELINE_PATH = BENCHMARK_DIR / "baseline.json"


def run_benchmark(benchmark_binary, extra_args=None):
    cmd = [benchmark_binary]
    if extra_args:
        cmd.extend(extra_args)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    assert result.returncode == 0, f"Benchmark binary failed:\n{result.stderr}"
    return json.loads(result.stdout)


def print_profile_table(data):
    results = data["results"]

    scenarios = sorted(set(r["scenario"] for r in results))
    resolutions = sorted(
        set(r["resolution"] for r in results),
        key=lambda r: next(x["width"] for x in results if x["resolution"] == r))

    phase_names = ["texture", "build_notes", "build_sustains", "build_gridlines", "execute_draws"]
    phase_labels = {
        "texture": "Texture",
        "build_notes": "Build Notes",
        "build_sustains": "Build Sustains",
        "build_gridlines": "Build Gridlines",
        "execute_draws": "Execute Draws",
    }

    # Collect all layer names across results
    all_layers = set()
    for r in results:
        if "layers" in r:
            all_layers.update(r["layers"].keys())
    layer_order = ["grid", "lane", "bar", "sustain", "note", "overlay",
                   "bar_animation", "note_animation", "background", "track", "highway_overlay"]
    sorted_layers = [l for l in layer_order if l in all_layers]

    print(f"\nProfile: {data['machine']} | {data['timestamp']} | {data['iterations']} iterations")
    print("=" * 110)

    for scenario in scenarios:
        print(f"\n  Scenario: {scenario}")
        header = f"  {'Phase':<20}"
        for res in resolutions:
            header += f"  {res:>12}"
        header += f"  {'% of 1080p':>10}"
        print(header)
        print(f"  {'-'*20}" + f"  {'-'*12}" * len(resolutions) + f"  {'-'*10}")

        # Total
        print(f"  {'TOTAL':<20}", end="")
        total_1080p = 0
        for res in resolutions:
            match = [r for r in results if r["scenario"] == scenario and r["resolution"] == res]
            if match:
                mean = match[0]["total"]["mean_us"]
                if res == "1080p":
                    total_1080p = mean
                print(f"  {mean:>9.0f} us", end="")
            else:
                print(f"  {'--':>12}", end="")
        print()

        # Phases
        for phase in phase_names:
            label = phase_labels.get(phase, phase)
            print(f"  {'  ' + label:<20}", end="")
            phase_1080p = 0
            for res in resolutions:
                match = [r for r in results if r["scenario"] == scenario and r["resolution"] == res]
                if match and phase in match[0].get("phases", {}):
                    mean = match[0]["phases"][phase]["mean_us"]
                    if res == "1080p":
                        phase_1080p = mean
                    print(f"  {mean:>9.0f} us", end="")
                else:
                    print(f"  {'--':>12}", end="")
            pct = (phase_1080p / total_1080p * 100) if total_1080p > 0 else 0
            print(f"  {pct:>8.1f}%")

        # Draw layer breakdown (if any layers present)
        layer_results = [r for r in results if r["scenario"] == scenario and r.get("layers")]
        if layer_results and any(layer_results[0]["layers"].values()):
            print(f"  {'  --- Draw Layers ---':<20}")
            for layer in sorted_layers:
                print(f"  {'    ' + layer:<20}", end="")
                layer_1080p = 0
                for res in resolutions:
                    match = [r for r in results if r["scenario"] == scenario and r["resolution"] == res]
                    if match and layer in match[0].get("layers", {}):
                        mean = match[0]["layers"][layer]["mean_us"]
                        if res == "1080p":
                            layer_1080p = mean
                        print(f"  {mean:>9.0f} us", end="")
                    else:
                        print(f"  {'--':>12}", end="")
                pct = (layer_1080p / total_1080p * 100) if total_1080p > 0 else 0
                print(f"  {pct:>8.1f}%")

    print()


def compare_baseline(data, threshold=0.20):
    if not BASELINE_PATH.exists():
        print(f"No baseline found at {BASELINE_PATH} — skipping regression check.")
        return []

    baseline = json.loads(BASELINE_PATH.read_text())
    machine = data["machine"]

    if baseline.get("machine") != machine:
        print(f"Baseline machine ({baseline.get('machine')}) != current ({machine}) — skipping.")
        return []

    regressions = []
    baseline_lookup = {}
    for r in baseline["results"]:
        key = (r["scenario"], r["resolution"])
        baseline_lookup[key] = r

    for r in data["results"]:
        key = (r["scenario"], r["resolution"])
        base = baseline_lookup.get(key)
        if not base:
            continue
        base_total = base.get("total", {}).get("mean_us", 0)
        if base_total == 0:
            continue
        current_total = r.get("total", {}).get("mean_us", 0)
        change = (current_total - base_total) / base_total
        if change > threshold:
            regressions.append({
                "scenario": r["scenario"],
                "resolution": r["resolution"],
                "baseline_us": base_total,
                "current_us": current_total,
                "change_pct": change * 100,
            })

    return regressions


def test_rendering_profile(benchmark_binary):
    """Per-phase profiler: detailed breakdown of where time is spent."""
    iters = os.environ.get("BENCH_ITERATIONS", "30")
    data = run_benchmark(benchmark_binary, ["--iterations", iters])

    assert "results" in data
    assert len(data["results"]) > 0

    print_profile_table(data)

    # Sanity: phases should sum to roughly total (within 20% for overhead)
    for r in data["results"]:
        if r.get("phases") and r["total"]["mean_us"] > 100:
            phase_sum = sum(p["mean_us"] for p in r["phases"].values())
            total = r["total"]["mean_us"]
            ratio = phase_sum / total
            assert 0.8 < ratio < 1.2, (
                f"Phase sum ({phase_sum:.0f}) doesn't match total ({total:.0f}) "
                f"for {r['scenario']}@{r['resolution']}: ratio={ratio:.2f}")

    # Compare against baseline if present
    regressions = compare_baseline(data)
    if regressions:
        print("\nREGRESSIONS DETECTED:")
        for reg in regressions:
            print(f"  {reg['scenario']}@{reg['resolution']}: "
                  f"{reg['baseline_us']:.0f} -> {reg['current_us']:.0f} us "
                  f"(+{reg['change_pct']:.1f}%)")
        pytest.fail(f"{len(regressions)} regression(s) exceeded 20% threshold")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <benchmark_binary> [--update-baseline]")
        sys.exit(1)

    binary = sys.argv[1]
    update_baseline = "--update-baseline" in sys.argv

    data = run_benchmark(binary)
    print_profile_table(data)

    if update_baseline:
        BASELINE_PATH.write_text(json.dumps(data, indent=2) + "\n")
        print(f"Baseline updated at {BASELINE_PATH}")
    else:
        regressions = compare_baseline(data)
        if regressions:
            print(f"\n{len(regressions)} regression(s) detected.")
            sys.exit(1)
        else:
            print("No regressions.")
