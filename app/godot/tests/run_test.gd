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
# The garage's starting tractor's rear axle share at rest, weights halfway.
const REAR_SHARE := 0.616

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


# A season's event (TractorPullEvent): the player and the named rivals,
# the rivals' work in slices until the player is due, the player's pull on
# the event's sled, recorded once, and practice pulls at any time.
func _check_event() -> void:
	var event := TractorPullEvent.new()
	_expect(event.get_season().is_empty() and event.get_event().is_empty(), "no season yet")
	_expect(event.start_event() == "no season", "no event without a season")
	var error := event.start_season(SEASON, 7)
	_expect(error.is_empty(), "the season: " + error)
	var season := event.get_season()
	var names: PackedStringArray = season.get("names", PackedStringArray())
	_expect(season.get("events", 0) == 10 and season.get("index", -1) == 0 and names.size() == 10
			and names[0] == "You" and names[1] == "Henk", "the season: %s" % season)
	error = event.start_event()
	_expect(error.is_empty(), "the first event: " + error)
	var info := event.get_event()
	_expect(info.get("name", "") == "Spring opener, watered start" and info.get("index", -1) == 0
			and info.get("pullers", 0) == 10 and info.get("full_pull_m", 0.0) == 100.0,
			"the event: %s" % info)

	_check_tuning(event)
	var run := TractorPullRun.new()
	_expect(run.setup_practice(event, EASY).is_empty(), "a practice pull at the event")
	_expect(event.record_pull(run).is_empty(), "a practice pull is not the event's")
	var dimensions := run.get_dimensions()
	_expect(dimensions.get("stall_omega_rad_s", 0.0) > 0.0
			and dimensions.get("rated_omega_rad_s", 0.0) < dimensions.get("high_idle_omega_rad_s", 0.0),
			"the engine's speeds: %s" % dimensions)
	_expect(dimensions.get("track_length_m", 0.0) == 130.0
			and dimensions.get("full_pull_line_m", 0.0) == 100.0,
			"a run past the line until the sled stops: %s" % dimensions)

	var slices := 0
	while event.get_event()["turn"] == "rivals" and slices < 10_000:
		var steps := event.advance(5000)
		_expect(steps <= 5000, "a slice runs at most its steps")
		slices += 1
	info = event.get_event()
	_expect(info["turn"] == "player_pull", "the player's turn: %s" % info)
	_expect(event.advance(5000) == 0, "the rivals wait for the player")
	_expect(not run.setup_event_pull(null, EASY).is_empty(), "no event")
	error = run.setup_event_pull(event, EASY)
	_expect(error.is_empty(), "the player's pull: " + error)
	_expect(event.record_pull(run).is_empty(), "not before the run ends")
	run.set_controls(1.0, 1.0, 6, 0.0, 0.0, 0.0)
	var steps := 0
	while not run.is_finished() and steps < MAX_STEPS:
		run.advance(STEP_NS)
		steps += 1
	var size := event.get_log_size()
	var entry := event.record_pull(run)
	_expect(entry.get("puller", -1) == 0 and entry.get("name", "") == "You"
			and entry.get("kind", "") == info["pull_kind"]
			and entry.get("reached_m", -1.0) == run.get_result()["distance_m"],
			"the player's pull: %s" % entry)
	_expect(event.get_log_size() == size + 1, "in the log")
	_expect(event.record_pull(run).is_empty(), "recorded once")
	if event.get_event()["turn"] != "player_pull":
		_expect(run.setup_event_pull(event, EASY).contains("no pull"),
				"no pull of the player's due")
	_expect(not event.score_event(), "an unfinished event is not scored")


# The player's tuning: as built at first, each setting held to its range,
# and a run set up after it is on the tuned tractor. The player starts on
# the garage's first levels: 100 kg of weights halfway put a little more of
# the lighter tractor on its rear axle than the reference's 60 %.
func _check_tuning(event: TractorPullEvent) -> void:
	var tuning := event.get_tuning()
	_expect(tuning.get("ballast", false) and tuning.get("pressure", false)
			and is_equal_approx(float(tuning["ballast_position"]), 0.5)
			and is_equal_approx(float(tuning["pressure_bar"]), 1.0)
			and is_equal_approx(float(tuning["min_bar"]), 0.5)
			and is_equal_approx(float(tuning["max_bar"]), 1.6)
			and absf(float(tuning["rear_share"]) - REAR_SHARE) < 0.005, "as built: %s" % tuning)
	var built_hash := _practice_hash(event)
	event.set_tuning(-1.0, 9.0)
	tuning = event.get_tuning()
	_expect(float(tuning["ballast_position"]) == 0.0 and float(tuning["pressure_bar"]) == 1.6
			and float(tuning["rear_share"]) > 0.6, "held to the ranges: %s" % tuning)
	_expect(_practice_hash(event) != built_hash, "the run is on the tuned tractor")
	event.set_tuning(0.5, 1.0)
	_expect(_practice_hash(event) == built_hash, "as built again: the same run")


# A practice pull's state hash after a few seconds at full throttle in 6th.
func _practice_hash(event: TractorPullEvent) -> String:
	var run := TractorPullRun.new()
	run.setup_practice(event, PRO)
	for step in 3000:
		run.set_controls(1.0, 0.0 if step > 500 else 1.0, 6, 0.0, 0.0, 0.0)
		run.advance(STEP_NS)
	return run.get_state_hash()


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
