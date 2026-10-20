# The extension, headless (scripts/godot_test.sh runs it in CI).
#
# First the edges: before setup, a missing file, the pro layout, frames of
# 60 fps and a hitch. Then a scripted run whose state hash it prints: the
# same driver as `tp_pull run ... --easy data/controls/easy.toml
# --stick 0.002 --hash`, a step at a time, so the two must print the same
# hash. Exits non-zero if any check fails.
#
#   godot --headless --path app/godot --script res://tests/run_test.gd
extends SceneTree

const TRACTOR := "res://data/tractors/farm_stock_light.toml"
const SLED := "res://data/sleds/light_class.toml"
const TRACK := "res://data/tracks/ideal_clay_sand.toml"
const EASY := "res://data/controls/easy.toml"

const STEP_NS := 1_000_000
const STEP_S := 0.001
const FRAME_60_FPS_NS := 16_666_667
const HITCH_NS := 10_000_000_000
const MAX_STEPS_PER_FRAME := 100
const SELECT_GEAR_S := 0.5
const LAUNCH_S := 3.0
const GEAR := 6
const STICK := 0.002
const MAX_STEPS := 200_000

var _failures := 0


func _init() -> void:
	_check_before_setup()
	_check_a_missing_file()
	_check_frames()
	if not _play_the_scripted_run():
		_failures += 1
	quit(1 if _failures > 0 else 0)


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _check_before_setup() -> void:
	var run := TractorPullRun.new()
	_expect(run.advance(STEP_NS) == 0, "no steps before setup")
	_expect(run.get_snapshot().is_empty(), "no snapshot before setup")
	_expect(run.get_state_hash().is_empty(), "no hash before setup")
	_expect(not run.is_finished(), "not finished before setup")
	_expect(run.get_result().is_empty(), "no result before setup")


func _check_a_missing_file() -> void:
	var run := TractorPullRun.new()
	var error := run.setup("res://data/tractors/nope.toml", SLED, TRACK, "")
	_expect(error == "res://data/tractors/nope.toml: cannot open file",
			"a missing file is named: " + error)
	_expect(run.advance(STEP_NS) == 0, "no run after a failed setup")


# The pro layout (no easy controls file): 60 fps frames run 16 or 17 steps,
# and a 10 s hitch runs at most 100 and reports the time it dropped.
func _check_frames() -> void:
	var run := TractorPullRun.new()
	var error := run.setup(TRACTOR, SLED, TRACK, "")
	_expect(error.is_empty(), "pro layout setup: " + error)
	run.set_controls(0.0, 1.0, 0, 0.0, 0.0, 0.0)
	_expect(run.advance(FRAME_60_FPS_NS) == 16, "a 60 fps frame runs 16 steps")
	_expect(run.advance(FRAME_60_FPS_NS) == 17, "the next runs 17")
	_expect(run.get_dropped_ns() == 0, "a 60 fps frame drops nothing")
	_expect(run.advance(HITCH_NS) == MAX_STEPS_PER_FRAME, "a hitch runs the most steps a frame may")
	_expect(run.get_dropped_ns() > HITCH_NS - (MAX_STEPS_PER_FRAME + 1) * STEP_NS,
			"a hitch reports the time it dropped")
	var snapshot := run.get_snapshot()
	_expect(snapshot.has("tractor_x_m") and snapshot.has("chain_tension_n"), "the snapshot's keys")
	_expect(snapshot.gear == 0 and snapshot.engine_running, "idling in neutral")


func _play_the_scripted_run() -> bool:
	var run := TractorPullRun.new()
	var error := run.setup(TRACTOR, SLED, TRACK, EASY)
	if not error.is_empty():
		printerr("setup: ", error)
		return false
	var steps := 0
	while not run.is_finished() and steps < MAX_STEPS:
		var t := steps * STEP_S
		if t < SELECT_GEAR_S:
			run.set_controls(0.0, 1.0, 0, 0.0, 0.0, 0.0)
		else:
			run.set_controls(1.0, 1.0, GEAR, STICK if t >= LAUNCH_S else 0.0, 0.0, 0.0)
		run.advance(STEP_NS)
		steps += 1
	if not run.is_finished():
		printerr("the run did not end")
		return false
	var result := run.get_result()
	print("result: ", result.ending, " ", result.distance_m, " m")
	print("state hash ", run.get_state_hash())
	return true
