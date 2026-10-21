# The extension, without a scene (scripts/godot_test.sh runs it in CI).
#
# First the edges: before setup, a missing file, the pro layout, frames of
# 60 fps and a hitch. Then two scripted runs whose state hashes it prints,
# driven as tp_pull drives them, a step at a time: the easy layout (`tp_pull
# run ... --easy data/controls/easy.toml --stick S --hash`) and the pro one
# with a dumped clutch and a brake (`--pro data/controls/pro.toml --stick S
# --dump --brake-right B --hash`): both steer at S, past the dead zone, so
# the hashes differ if a side drops the layout's steering. The
# script passes the gear, S and B as user arguments, so the two must print
# the same hashes. Exits non-zero if any check fails.
#
#   godot --path app/godot --script res://tests/run_test.gd \
#       -- --gear=6 --stick=0.032 --brake-right=0.05
extends SceneTree

const TRACTOR := "res://data/tractors/farm_stock_light.toml"
const SLED := "res://data/sleds/light_class.toml"
const TRACK := "res://data/tracks/ideal_clay_sand.toml"
const EASY := "res://data/controls/easy.toml"
const PRO := "res://data/controls/pro.toml"
const SEASON := "res://data/seasons/club_season.toml"

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
	_check_layouts()
	_check_event()
	if not args.has_all(["gear", "stick", "brake-right"]):
		printerr("needs --gear=N --stick=S --brake-right=B after --")
		_failures += 1
	else:
		var gear := int(args["gear"])
		_play("easy", EASY, gear, float(args["stick"]), 0.0)
		_play("pro", PRO, gear, float(args["stick"]), float(args["brake-right"]))
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
	_expect(run.get_layout().is_empty(), "no layout before setup")
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


# The pro layout (no controls file): 60 fps frames run 16 or 17 steps,
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
	_expect(dimensions.get("gear_count", 0) == 8, "the reference tractor's eight gears")
	_expect(dimensions.get("lane_width_m", 0.0) > 0.0
			and dimensions.get("full_pull_line_m", 0.0) <= dimensions.get("track_length_m", 0.0),
			"the track's dimensions")


# The controls file picks the layout; a file of neither is named.
func _check_layouts() -> void:
	var run := TractorPullRun.new()
	for case: Array in [[EASY, "easy"], [PRO, "pro"], ["", "pro"]]:
		var error := run.setup(TRACTOR, SLED, TRACK, case[0])
		_expect(error.is_empty() and run.get_layout() == case[1],
				"%s plays the %s layout: %s" % [case[0], case[1], error])
	var error := run.setup(TRACTOR, SLED, TRACK, TRACK)
	_expect(error.contains("easy, pro"), "a track is no controls file: " + error)


# A season's event: its name, place and round, the engine's speeds for
# the rev counter, the rounds the rules allow, and no next round before the
# pull ends or after one short of a full pull.
func _check_event() -> void:
	var run := TractorPullRun.new()
	_expect(run.get_event().is_empty() and run.get_next_round() == -1, "no event before setup")
	var error := run.setup_event(SEASON, 0, EASY, 0)
	_expect(error.is_empty(), "the season's first event: " + error)
	var event := run.get_event()
	_expect(event.get("name", "") == "Spring opener, watered start" and event.get("index", -1) == 0
			and event.get("count", 0) == 10 and event.get("round", -1) == 0
			and event.get("pull_offs", -1) == 1, "the event: %s" % event)
	var dimensions := run.get_dimensions()
	_expect(dimensions.get("stall_omega_rad_s", 0.0) > 0.0
			and dimensions.get("rated_omega_rad_s", 0.0) < dimensions.get("high_idle_omega_rad_s", 0.0),
			"the engine's speeds: %s" % dimensions)
	_expect(run.get_next_round() == -1, "no next round before the pull ends")
	# Full left from the start: out of the lane, no pull-off.
	run.set_controls(1.0, 1.0, 6, 1.0, 0.0, 0.0)
	var steps := 0
	while not run.is_finished() and steps < MAX_STEPS:
		run.advance(STEP_NS)
		steps += 1
	_expect(run.is_finished() and run.get_next_round() == -1, "no pull-off after a short pull")
	_expect(run.setup_event(SEASON, 0, PRO, 1).is_empty() and run.get_event()["round"] == 1,
			"the pull-off round")
	_expect(run.setup_event(SEASON, 10, EASY, 0).contains("no event 10"), "past the last event")
	_expect(run.setup_event(SEASON, 0, EASY, 2).contains("no round 2"), "past the last pull-off")
	_expect(run.get_event().is_empty(), "no event after a failed setup")
	run.setup(TRACTOR, SLED, TRACK, EASY)
	_expect(run.get_event().is_empty(), "a plain setup is no event's pull")


# A scripted run, as tp_pull drives it: in neutral until SELECT_GEAR_S,
# then full throttle in `gear` with the clutch pressed; from LAUNCH_S the
# steering control goes to `stick`; the easy layout's assist lets the clutch
# in, while the pro layout dumps the clutch and holds the right brake at
# `brake_right`.
func _play(layout: String, controls: String, gear: int, stick: float,
		brake_right: float) -> void:
	var run := TractorPullRun.new()
	var error := run.setup(TRACTOR, SLED, TRACK, controls)
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
