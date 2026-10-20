#!/usr/bin/env bash
# Runs an Android build's tests on a connected device or emulator, over adb.
#
#   scripts/android_test.sh build/android-x86_64    (emulator)
#   scripts/android_test.sh build/android-arm64     (phone)
#
# Copies data/ and the test binaries to /data/local/tmp/tp (the directory the
# Android presets compile in as TP_DATA_DIR), runs both test suites, then a
# whole event with the tool as a smoke test. Exits non-zero if any fails.
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <android build directory>" >&2
    exit 2
fi

readonly build="$1"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
readonly device_dir=/data/local/tmp/tp

# adb shell's own exit status is unreliable on older adb and devices, so each
# command prints its status and the script reads it back.
run_on_device() {
    local output
    output="$(adb shell "cd ${device_dir} && $1; echo \"tp-exit:\$?\"")"
    printf '%s\n' "${output}" | grep -v '^tp-exit:'
    [[ "$(printf '%s\n' "${output}" | grep '^tp-exit:' | tail -1 | tr -d '\r')" == "tp-exit:0" ]]
}

adb wait-for-device
adb shell "rm -rf ${device_dir} && mkdir -p ${device_dir}"
adb push "${root}/data" "${device_dir}/" >/dev/null
for binary in tests/tp_core_tests tests/tp_data_tests tools/tp_pull; do
    adb push "${build}/${binary}" "${device_dir}/" >/dev/null
done

status=0
for suite in tp_core_tests tp_data_tests; do
    echo "== ${suite}"
    run_on_device "./${suite} --gtest_brief=1" || status=1
done
echo "== tp_pull event"
run_on_device "./tp_pull event data/events/club_pull.toml > event.txt && tail -5 event.txt" || status=1

exit "${status}"
