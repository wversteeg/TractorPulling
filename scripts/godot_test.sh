#!/usr/bin/env bash
# Plays the same two scripted runs (the easy layout, and the pro one with a
# dumped clutch and a brake) through tp_pull and through the Godot
# extension, and checks they end in the same state hashes: the
# binding passes the controls and the time to tp::session unchanged. The
# GDScript also checks the binding's edges (run_test.gd), and the pull
# scene draws the run where it is (scene_test.gd), takes the touch controls
# (touch_test.gd) and plays an event's flow (flow_test.gd).
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
readonly stick=0.032        # both runs' steering control, just past the layouts' dead zone
# The pro run's right brake, with a dumped clutch. It pulls the tractor
# onto the chalk line at 25.9 s on purpose, so the run also ends in a
# disqualification: "pro result: out_of_bounds 0.0 m" is the expected end.
readonly brake_right=0.05
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
    --gear "${gear}" --pro data/controls/pro.toml --stick "${stick}" --dump \
    --brake-right "${brake_right}" \
    --hash -o /dev/null | first_hash)"
echo "tp_pull, easy: ${expected_easy}"
echo "tp_pull, pro:  ${expected_pro}"

# The build lists the extension in .godot/extension_list.cfg, so Godot
# loads it at startup (see app/godot/extension/CMakeLists.txt): the editor
# import, which builds the cache of the scripts' class names, is then safe.
if [[ ! -f "${project}/.godot/extension_list.cfg" ]]; then
    echo "${project}/.godot/extension_list.cfg is missing: build the godot preset first" >&2
    exit 1
fi
readonly timeout_s=300
timeout "${timeout_s}" "${godot}" --headless --path "${project}" --import >/dev/null 2>&1 || {
    echo "the Godot import failed" >&2
    exit 1
}

# Godot runs with a renderer where it can, as on a desktop: a GPU driver
# loads libraries of its own (Mesa's brings the system's libstdc++), which a
# headless run never sees. Under Xvfb when there is no display; headless,
# with a warning, when neither is there.
if [[ -n "${DISPLAY:-}" ]]; then
    runner=("${godot}" --rendering-method gl_compatibility)
elif command -v xvfb-run >/dev/null; then
    runner=(xvfb-run -a -s "-screen 0 640x480x24" "${godot}" --rendering-method gl_compatibility)
else
    echo "warning: no display and no xvfb-run: running headless, without a renderer" >&2
    runner=("${godot}" --headless)
fi

# Runs a test script in Godot and sets `output`. A script error leaves Godot
# running instead of quitting, so a timeout fails it; and GDScript abandons
# a function that hits an error and carries on, so the error is looked for.
play() {
    play_with "${runner[@]}" -- "$@"
}

# As play(), headless: for a test that checks what the scene shows, not how
# it draws it.
play_headless() {
    play_with "${godot}" --headless -- "$@"
}

# play()'s work with the Godot command line before "--", the test's after.
play_with() {
    local command=()
    while [[ "$1" != "--" ]]; do
        command+=("$1")
        shift
    done
    shift
    output="$(timeout "${timeout_s}" "${command[@]}" --path "${project}" "$@" 2>&1)" || {
        echo "${output:-}"
        echo "Godot failed or did not quit within ${timeout_s} s: $*" >&2
        exit 1
    }
    echo "${output}"
    if grep -q 'SCRIPT ERROR' <<<"${output}"; then
        echo "the Godot test script hit an error: $*" >&2
        exit 1
    fi
}

# The pull scene: drawn where the run has it, at a fixed 60 fps.
play --fixed-fps 60 --script res://tests/scene_test.gd
# The touch controls: simulated fingers drive the scene.
play --fixed-fps 60 --script res://tests/touch_test.gd
# The game's flow: start screen, qualifying, pull-off, the next event.
play --fixed-fps 60 --script res://tests/flow_test.gd
# A whole turn, a scripted driver from the parking to the sled and back.
# Headless: drawn by a software renderer its thousands of frames would take
# far too long.
play_headless --fixed-fps 15 --script res://tests/turn_test.gd

play --script res://tests/run_test.gd -- --gear="${gear}" --stick="${stick}" \
    --brake-right="${brake_right}"
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
