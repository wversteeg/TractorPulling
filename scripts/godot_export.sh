#!/usr/bin/env bash
# Exports the game: a debug APK for phones and the emulator (arm64-v8a and
# x86_64), the Windows desktop game and the web game, from
# app/godot/export_presets.cfg, into the output directory (default
# build/export): tractor_pulling.apk, windows/ (TractorPulling.exe, its
# console wrapper and the extension's DLL) and web/ (index.html and the
# files it loads, the extension's .wasm among them; serve the directory
# over HTTP, and see scripts/web_game_test.sh).
#
#   scripts/godot_export.sh <godot binary> [output directory]
#
# Needs:
# - the extension built for each platform: the godot preset (the editor
#   loads it to export), godot-android-arm64, godot-android-x86_64,
#   godot-windows (MinGW-w64, g++-mingw-w64-x86-64-posix) and godot-web
#   (Emscripten);
# - Godot's export templates for its version, at least android_debug.apk and
#   windows_debug_x86_64.exe (and its _console) and
#   web_dlink_nothreads_debug.zip, in Godot's
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
    windows.template_debug.x86_64.dll linux.template_debug.x86_64.so \
    web.template_debug.wasm32.nothreads.wasm; do
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

# The build: its number counts the commits along the first parents, so each
# merge commit on develop adds one, and its commit names the source. The
# game reads them from build_info.cfg (BuildInfo, shown in the settings);
# the file is removed after the export, so the project itself stays a
# development build. A shallow clone would count wrong: CI checks out the
# whole history.
#
# A pull request's build (TP_PR_NUMBER and TP_PR_HEAD set, as CI does on a
# pull request) is checked out as GitHub's merge commit, which no branch
# keeps: it is marked with the PR instead, numbered as the develop it
# builds on (the merge's first parent), and names the PR's own head commit.
if [[ "$(git -C "${root}" rev-parse --is-shallow-repository)" == true ]]; then
    echo "a shallow clone: the build number needs the whole history (git fetch --unshallow)" >&2
    exit 1
fi
build_pr="${TP_PR_NUMBER:-0}"
if [[ "${build_pr}" -gt 0 ]]; then
    build_number="$(git -C "${root}" rev-list --count --first-parent HEAD^1)"
    build_commit="$(git -C "${root}" rev-parse --short "${TP_PR_HEAD:?TP_PR_HEAD: the PR head commit}")"
else
    build_number="$(git -C "${root}" rev-list --count --first-parent HEAD)"
    build_commit="$(git -C "${root}" rev-parse --short HEAD)"
fi
readonly build_pr build_number build_commit
printf '[build]\nnumber=%d\ncommit="%s"\npr=%d\n' "${build_number}" "${build_commit}" \
    "${build_pr}" >"${project}/build_info.cfg"
trap 'rm -f "${project}/build_info.cfg"' EXIT
echo "build ${build_number} (${build_commit}), PR ${build_pr}"

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
export_preset Web "${output}/web/index.html"
if [[ ! -f "${output}/web/libtp_godot.web.template_debug.wasm32.nothreads.wasm" ]]; then
    echo "the web export has no extension next to it" >&2
    exit 1
fi
# The web game itself reports the data files it carries:
# scripts/web_game_test.sh checks them.
