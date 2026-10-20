# The extension, without a scene (scripts/godot_test.sh runs it in CI).
#
# First the edges: before setup, a missing file, the pro layout, frames of
# 60 fps and a hitch. Then two scripted runs whose state hashes it prints,
# driven as tp_pull drives them, a step at a time: the easy layout (`tp_pull
# run ... --easy data/controls/easy.toml --stick S --hash`) and the pro one
# with a dumped clutch and a brake (`--dump --brake-right B --hash`). The
# script passes the gear, S and B as user arguments, so the two must print
# the same hashes. Exits non-zero if any check fails.
#
#   godot --path app/godot --script res://tests/run_test.gd \
#       -- --gear=6 --stick=0.002 --brake-right=0.05
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
# tp_pull's script timings: neutral until kPullSelectGear_s, the launch
# (the clutch let in, the steering or the brake) at kPullRevUpEnd_s.
const SELECT_GEAR_S := 0.5
const LAUNCH_S := 3.0
const MAX_STEPS := 200_000

var _failures := 0


func _init() -> void:
	var args := _user_args()
	_check_before_setup()
	_check_a_missing_file()
	_check_frames()
	if not args.has_all(["gear", "stick", "brake-right"]):
		printerr("needs --gear=N --stick=S --brake-right=B after --")
		_failures += 1
	else:
		var gear := int(args["gear"])
		_play("easy", EASY, gear, float(args["stick"]), 0.0)
		_play("pro", "", gear, 0.0, float(args["brake-right"]))
	quit(1 if _failures > 0 else 0)


# The arguments after `--`, as --name=value.
func _user_args() -> Dictionary:
	var args := {}
	for arg: String in OS.get_cmdline_user_args():
		var parts := arg.trim_prefix("--").split("=", true, 1)
		if parts.size() == 2:
			args[parts[0]] = parts[1]
	return args


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _check_before_setup() -> void:
	var run := TractorPullRun.new()
	_expect(run.advance(STEP_NS) == 0, "no steps before setup")
	_expect(run.get_snapshot().is_empty(), "no snapshot before setup")
	_expect(run.get_dimensions().is_empty(), "no dimensions before setup")
	_expect(run.get_state_hash().is_empty(), "no hash before setup")
	_expect(not run.is_finished(), "not finished before setup")
	_expect(run.get_result().is_empty(), "no result before setup")


func _check_a_missing_file() -> void:
	var run := TractorPullRun.new()
	var error := run.setup("res://data/tractors/nope.toml", SLED, TRACK, "")
	_expect(error == "res://data/tractors/nope.toml: cannot open file",
			"a missing file is named: " + error)
	_expect(run.advance(STEP_NS) == 0, "no run after a failed setup")
	_expect(run.get_dimensions().is_empty(), "no dimensions after a failed setup")


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
	var dimensions := run.get_dimensions()
	_expect(dimensions.get("wheelbase_m", 0.0) > 0.0 and dimensions.get("chain_length_m", 0.0) > 0.0,
			"the tractor's and sled's dimensions")
	_expect(dimensions.get("lane_width_m", 0.0) > 0.0
			and dimensions.get("full_pull_line_m", 0.0) <= dimensions.get("track_length_m", 0.0),
			"the track's dimensions")


# A scripted run, as tp_pull drives it: in neutral until SELECT_GEAR_S,
# then full throttle in `gear` with the clutch pressed; from LAUNCH_S the
# easy layout steers at `stick` and its assist lets the clutch in, while the
# pro layout dumps the clutch and holds the right brake at `brake_right`.
func _play(layout: String, easy_controls: String, gear: int, stick: float,
		brake_right: float) -> void:
	var run := TractorPullRun.new()
	var error := run.setup(TRACTOR, SLED, TRACK, easy_controls)
	if not error.is_empty():
		printerr(layout, " setup: ", error)
		_failures += 1
		return
	var steps := 0
	while not run.is_finished() and steps < MAX_STEPS:
		var t := steps * STEP_S
		if t < SELECT_GEAR_S:
			run.set_controls(0.0, 1.0, 0, 0.0, 0.0, 0.0)
		elif t < LAUNCH_S:
			run.set_controls(1.0, 1.0, gear, 0.0, 0.0, 0.0)
		else:
			run.set_controls(1.0, 0.0, gear, stick, 0.0, brake_right)
		run.advance(STEP_NS)
		steps += 1
	if not run.is_finished():
		printerr(layout, ": the run did not end")
		_failures += 1
		return
	var result := run.get_result()
	print(layout, " result: ", result.ending, " ", result.distance_m, " m")
	print(layout, " state hash ", run.get_state_hash())
