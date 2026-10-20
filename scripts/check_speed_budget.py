#!/usr/bin/env python3
"""Checks a benchmark run against the speed budget (bench/speed_budget.toml).

    core_bench --benchmark_filter='^(BM_GameStep|BM_PullStep|BM_VehicleStep)$' \\
        --benchmark_repetitions=5 --benchmark_report_aggregates_only=true \\
        --benchmark_out=bench.json --benchmark_out_format=json
    scripts/check_speed_budget.py bench.json ci_arm64     # or: phone

Prints each budgeted benchmark's median time against its limit in that
column and exits non-zero if one is over, or missing from the run.
"""

import json
import sys
import tomllib
from pathlib import Path

BUDGET = Path(__file__).resolve().parent.parent / "bench" / "speed_budget.toml"
UNIT_NS = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}


def medians_ns(results_path):
    with open(results_path, encoding="utf-8") as results:
        benchmarks = json.load(results)["benchmarks"]
    return {
        b["run_name"]: b["real_time"] * UNIT_NS[b["time_unit"]]
        for b in benchmarks
        if b.get("aggregate_name") == "median"
    }


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    results_path, column = argv[1], argv[2]
    key = f"{column}_ns"
    with open(BUDGET, "rb") as budget_file:
        budget = tomllib.load(budget_file)
    limits = {name: entry[key] for name, entry in budget.items() if key in entry}
    if not limits:
        print(f"no '{key}' limits in {BUDGET}", file=sys.stderr)
        return 2

    measured = medians_ns(results_path)
    failed = False
    print(f"{'benchmark':<20} {'median':>12} {'limit':>12}  ({column})")
    for name, limit_ns in limits.items():
        if name not in measured:
            print(f"{name:<20} {'missing':>12} {limit_ns:>9,} ns  FAIL")
            failed = True
            continue
        over = measured[name] > limit_ns
        failed = failed or over
        verdict = f"FAIL, {measured[name] / limit_ns:.1f}x the limit" if over else (
            f"ok, {limit_ns / measured[name]:.1f}x headroom")
        print(f"{name:<20} {measured[name]:>9,.0f} ns {limit_ns:>9,} ns  {verdict}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
