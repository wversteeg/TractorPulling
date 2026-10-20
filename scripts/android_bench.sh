#!/usr/bin/env bash
# Runs the benchmarks the speed budget is set on, on a connected phone or
# emulator over adb, and checks them against the phone budget.
#
#   scripts/android_bench.sh build/android-arm64    (a phone: the real check)
#   scripts/android_bench.sh build/android-x86_64   (the emulator: a smoke test)
#
# Needs python3 (3.11+) on the host. Exits non-zero if a benchmark is over
# its budget (bench/speed_budget.toml, column `phone`).
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <android build directory>" >&2
    exit 2
fi

readonly build="$1"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
# shellcheck source=scripts/android_device.sh
source "${root}/scripts/android_device.sh"

filter="$("${root}/scripts/check_speed_budget.py" --filter)"
push_to_device "${build}" bench/core_bench
adb shell getprop ro.product.model || true
run_on_device "./core_bench --benchmark_filter='${filter}' \
--benchmark_repetitions=5 --benchmark_report_aggregates_only=true \
--benchmark_out=bench.json --benchmark_out_format=json"

results="$(mktemp)"
trap 'rm -f "${results}"' EXIT
adb pull "${device_dir}/bench.json" "${results}" >/dev/null
"${root}/scripts/check_speed_budget.py" "${results}" phone
