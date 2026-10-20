#!/usr/bin/env bash
# Plays the same scripted run through tp_pull and through the Godot
# extension, headless, and checks they end in the same state hash: the
# binding passes the controls and the time to tp::session unchanged.
#
#   scripts/godot_test.sh <godot binary> [build directory, default build/godot]
#
# Build first: cmake --preset godot && cmake --build --preset godot
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <godot binary> [build directory]" >&2
    exit 2
fi

readonly godot="$1"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
readonly build="${2:-${root}/build/godot}"
readonly project="${root}/app/godot"
cd "${root}"

expected="$("${build}/tools/tp_pull" run data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/ideal_clay_sand.toml --gear 6 \
    --easy data/controls/easy.toml --stick 0.002 --hash -o /dev/null |
    grep -m1 'state hash' | sed 's/^ *//')"
echo "tp_pull: ${expected}"

# No editor import: the build lists the extension in .godot/extension_list.cfg,
# so Godot loads it at startup (see app/godot/extension/CMakeLists.txt).
if [[ ! -f "${project}/.godot/extension_list.cfg" ]]; then
    echo "${project}/.godot/extension_list.cfg is missing: build the godot preset first" >&2
    exit 1
fi
# A script error leaves Godot running instead of quitting: the timeout fails
# the test then (its exit status is non-zero).
readonly timeout_s=300
output="$(timeout "${timeout_s}" "${godot}" --headless --path "${project}" \
    --script res://tests/run_test.gd 2>&1)" || {
    echo "${output:-}"
    echo "Godot failed or did not quit within ${timeout_s} s" >&2
    exit 1
}
echo "${output}"
# A script error does not stop the script: GDScript abandons the function
# and carries on, so the error has to be looked for.
if grep -q 'SCRIPT ERROR' <<<"${output}"; then
    echo "the Godot test script hit an error" >&2
    exit 1
fi
actual="$(grep -m1 'state hash' <<<"${output}" || true)"

if [[ -z "${actual}" ]]; then
    echo "the Godot run printed no state hash" >&2
    exit 1
fi
if [[ "${actual}" != "${expected}" ]]; then
    echo "the Godot run ended in a different state than tp_pull's" >&2
    exit 1
fi
echo "the same state hash through the Godot extension"
