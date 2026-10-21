#!/usr/bin/env bash
# Exports the game: a debug APK for phones and the emulator (arm64-v8a and
# x86_64) and the Windows desktop game, from app/godot/export_presets.cfg,
# into the output directory (default build/export): tractor_pulling.apk and
# windows/ (TractorPulling.exe, its console wrapper and the extension's DLL).
#
#   scripts/godot_export.sh <godot binary> [output directory]
#
# Needs:
# - the extension built for each platform: the godot preset (the editor
#   loads it to export), godot-android-arm64, godot-android-x86_64 and
#   godot-windows (MinGW-w64, g++-mingw-w64-x86-64-posix);
# - Godot's export templates for its version, at least android_debug.apk and
#   windows_debug_x86_64.exe (and its _console), in Godot's
#   export_templates directory (~/.local/share/godot/export_templates/<version>);
# - ANDROID_HOME, an Android SDK with platform-tools and build-tools
#   (apksigner), and JAVA_HOME, a JDK 17 or later: the script writes them
#   into Godot's editor settings.
#
# The APK is signed with app/android/debug.keystore: a debug key, like the
# one Android's own debug builds use (password "android"), committed so that
# every APK installs over the last. It is not a secret; a release build
# gets its own key.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <godot binary> [output directory]" >&2
    exit 2
fi

godot="$(realpath "$1")"
readonly godot
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
output="$(realpath -m "${2:-${root}/build/export}")"
readonly output
readonly project="${root}/app/godot"

for library in android.template_debug.arm64.so android.template_debug.x86_64.so \
    windows.template_debug.x86_64.dll linux.template_debug.x86_64.so; do
    if [[ ! -f "${project}/bin/libtp_godot.${library}" ]]; then
        echo "app/godot/bin/libtp_godot.${library} is missing: build its preset first" >&2
        exit 1
    fi
done
if [[ -z "${ANDROID_HOME:-}" || -z "${JAVA_HOME:-}" ]]; then
    echo "set ANDROID_HOME (the Android SDK) and JAVA_HOME (a JDK)" >&2
    exit 1
fi

export GODOT_ANDROID_KEYSTORE_DEBUG_PATH="${root}/app/android/debug.keystore"
export GODOT_ANDROID_KEYSTORE_DEBUG_USER=androiddebugkey
export GODOT_ANDROID_KEYSTORE_DEBUG_PASSWORD=android

readonly timeout_s=600
# The import builds the cache of the scripts' class names first, and makes
# the editor settings if there are none yet.
timeout "${timeout_s}" "${godot}" --headless --path "${project}" --import >/dev/null 2>&1 || {
    echo "the Godot import failed" >&2
    exit 1
}

# The SDKs, in the editor settings of this Godot's version (e.g. 4.7): Godot
# reads them from there only.
version="$("${godot}" --headless --version | grep -oE '^[0-9]+\.[0-9]+')"
settings="${XDG_CONFIG_HOME:-${HOME}/.config}/godot/editor_settings-${version}.tres"
if [[ ! -f "${settings}" ]]; then
    echo "no Godot editor settings at ${settings}" >&2
    exit 1
fi
set_editor_setting() {
    local key="$1" value="$2"
    sed -i "\|^${key} = |d" "${settings}"
    echo "${key} = \"${value}\"" >>"${settings}"
}
set_editor_setting export/android/android_sdk_path "${ANDROID_HOME}"
set_editor_setting export/android/java_sdk_path "${JAVA_HOME}"

# Exports one preset to `path`; Godot may exit 0 after an error, so the
# file is checked too.
export_preset() {
    local preset="$1" path="$2"
    mkdir -p "$(dirname "${path}")"
    rm -f "${path}"
    timeout "${timeout_s}" "${godot}" --headless --path "${project}" \
        --export-debug "${preset}" "${path}" 2>&1 | grep -v 'savepack | Storing File' || true
    if [[ ! -s "${path}" ]]; then
        echo "the ${preset} export made no ${path}" >&2
        exit 1
    fi
    echo "exported ${preset}: ${path}"
}

export_preset Android "${output}/tractor_pulling.apk"
# A half-made export can still be a file: the APK must carry the extension
# for both ABIs and every data file.
apk_files="$(unzip -l "${output}/tractor_pulling.apk")"
for library in lib/arm64-v8a/libtp_godot.android.template_debug.arm64.so \
    lib/x86_64/libtp_godot.android.template_debug.x86_64.so; do
    if ! grep -q " ${library}\$" <<<"${apk_files}"; then
        echo "the APK has no ${library}" >&2
        exit 1
    fi
done
data_files="$(cd "${project}" && find data -name '*.toml' | wc -l)"
apk_data_files="$(grep -cE ' assets/data/.*\.toml$' <<<"${apk_files}" || true)"
if [[ "${apk_data_files}" -ne "${data_files}" ]]; then
    echo "the APK has ${apk_data_files} of the ${data_files} data files" >&2
    exit 1
fi
export_preset "Windows Desktop" "${output}/windows/TractorPulling.exe"
if [[ ! -f "${output}/windows/libtp_godot.windows.template_debug.x86_64.dll" ]]; then
    echo "the Windows export has no extension DLL next to it" >&2
    exit 1
fi
