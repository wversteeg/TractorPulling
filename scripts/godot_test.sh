#!/usr/bin/env bash
# Plays the same two scripted runs (the easy layout, and the pro one with a
# dumped clutch and a brake) through tp_pull and through the Godot
# extension, headless, and checks they end in the same state hashes: the
# binding passes the controls and the time to tp::session unchanged. The
# GDScript also checks the binding's edges (run_test.gd).
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

# The two scripted runs, driven the same way by tp_pull and by
# app/godot/tests/run_test.gd. Their values live here only: Godot gets them
# as user arguments. The timings (neutral until 0.5 s, the launch at 3 s)
# are tp_pull's kPullSelectGear_s and kPullRevUpEnd_s.
readonly gear=6
readonly stick=0.002        # the easy run's steering control
readonly brake_right=0.05   # the pro run's right brake, with a dumped clutch
readonly tractor=data/tractors/farm_stock_light.toml
readonly sled=data/sleds/light_class.toml
readonly track=data/tracks/ideal_clay_sand.toml

first_hash() {
    grep -m1 'state hash' | sed 's/^ *//'
}
expected_easy="$("${build}/tools/tp_pull" run "${tractor}" "${sled}" "${track}" \
    --gear "${gear}" --easy data/controls/easy.toml --stick "${stick}" --hash -o /dev/null |
    first_hash)"
expected_pro="$("${build}/tools/tp_pull" run "${tractor}" "${sled}" "${track}" \
    --gear "${gear}" --dump --brake-right "${brake_right}" --hash -o /dev/null | first_hash)"
echo "tp_pull, easy: ${expected_easy}"
echo "tp_pull, pro:  ${expected_pro}"

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
    --script res://tests/run_test.gd -- --gear="${gear}" --stick="${stick}" \
    --brake-right="${brake_right}" 2>&1)" || {
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
actual_easy="$(grep -m1 'easy state hash' <<<"${output}" | sed 's/^easy //' || true)"
actual_pro="$(grep -m1 'pro state hash' <<<"${output}" | sed 's/^pro //' || true)"

status=0
for layout in easy pro; do
    expected_var="expected_${layout}"
    actual_var="actual_${layout}"
    if [[ -z "${!actual_var}" ]]; then
        echo "the Godot ${layout} run printed no state hash" >&2
        status=1
    elif [[ "${!actual_var}" != "${!expected_var}" ]]; then
        echo "the Godot ${layout} run ended in a different state than tp_pull's" >&2
        status=1
    fi
done
if [[ "${status}" -eq 0 ]]; then
    echo "the same state hashes through the Godot extension"
fi
exit "${status}"
