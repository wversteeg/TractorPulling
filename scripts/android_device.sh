# shellcheck shell=bash
# Shared by the android_*.sh scripts: talking to a device or emulator over
# adb. Source it; it expects `root` (the repository) to be set.

readonly device_dir=/data/local/tmp/tp

# adb shell's own exit status is unreliable on older adb and devices, so each
# command prints its status and the caller reads it back.
run_on_device() {
    local output
    output="$(adb shell "cd ${device_dir} && $1; echo \"tp-exit:\$?\"")"
    printf '%s\n' "${output}" | grep -v '^tp-exit:'
    [[ "$(printf '%s\n' "${output}" | grep '^tp-exit:' | tail -1 | tr -d '\r')" == "tp-exit:0" ]]
}

# A fresh device directory with data/ (where the Android presets point
# TP_DATA_DIR) and the given binaries from the build directory.
push_to_device() {
    local build_dir="$1"
    shift
    adb wait-for-device
    adb shell "rm -rf ${device_dir} && mkdir -p ${device_dir}"
    adb push "${root}/data" "${device_dir}/" >/dev/null
    local binary
    for binary in "$@"; do
        adb push "${build_dir}/${binary}" "${device_dir}/" >/dev/null
    done
}
