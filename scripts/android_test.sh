#!/usr/bin/env bash
# Runs an Android build's tests on a connected device or emulator, over adb.
#
#   scripts/android_test.sh build/android-x86_64 [expected_event.txt]   (emulator)
#   scripts/android_test.sh build/android-arm64 [expected_event.txt]    (phone)
#
# Copies data/ and the test binaries to /data/local/tmp/tp (the directory the
# Android presets compile in as TP_DATA_DIR), runs both test suites, then a
# whole event with the tool. Given the same event's output from another
# platform (`tp_pull event data/events/club_pull.toml > expected_event.txt`),
# it checks the device prints exactly the same pulls and standings. Exits
# non-zero if anything fails or differs.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <android build directory> [expected event output]" >&2
    exit 2
fi

readonly build="$1"
readonly expected="${2:-}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
# shellcheck source=scripts/android_device.sh
source "${root}/scripts/android_device.sh"

push_to_device "${build}" tests/tp_core_tests tests/tp_data_tests tests/tp_alloc_tests tools/tp_pull

status=0
for suite in tp_core_tests tp_data_tests tp_alloc_tests; do
    echo "== ${suite}"
    run_on_device "./${suite} --gtest_brief=1" || status=1
done
echo "== tp_pull event"
if run_on_device "./tp_pull event data/events/club_pull.toml > event.txt"; then
    event="$(mktemp)"
    adb pull "${device_dir}/event.txt" "${event}" >/dev/null
    if [[ -z "${expected}" ]]; then
        tail -5 "${event}"
    elif diff -u "${expected}" "${event}"; then
        echo "the same pulls and standings as ${expected}"
    else
        status=1
    fi
    rm -f "${event}"
else
    status=1
fi

exit "${status}"
