# A whole turn through the pull scene: a practice pull in the pro layout,
# driven by a scripted driver (the scene's `driver`) that follows paths by
# pure pursuit, as tests/turn/turn_test.cpp's does. Out of the bay, past
# the sled into the lane, back up to it in reverse, the slack taken up,
# the pull at the green flag, the chain released, and back to the bay
# round the end of the pull and along the return lane. Checks what the
# scene shows at each stage: the rivals in their bays, the officials' flags
# and the signal lights (green while the pull is good, red to stop),
# the chain on or off, the stopping marker, the guidance line, and the
# result screen only once the tractor is back in its bay.
# Run: godot --fixed-fps 15 --path app/godot --script res://tests/turn_test.gd
extends SceneTree

const SETTINGS_PATH := "user://turn_test_settings.cfg"
# The whole turn is about 150 s of the run's time; at 15 fps that is under
# 2500 frames.
const MAX_FRAMES := 6000
const LOOK_AHEAD_M := 4.0
const ARRIVED_M := 1.0
const WHEELBASE_M := 2.4
const MAX_STEER_RAD := deg_to_rad(40.0)
# The pro layout's steering dead zone (data/controls/pro.toml): the
# control is set past it so the wheels take the angle asked.
const DEAD_ZONE := 0.03
const REVERSE := -1
const PARKED_WITHIN_M := 2.0

var _pull: Node
var _failures := 0
var _path: Array[Vector2] = []
var _next := 0
var _ramp := 0.0
var _clutch := 1.0
var _home: Array[Vector2] = []
var _bay := Vector2.ZERO
var _chain_m := 1.15


func _initialize() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SETTINGS_PATH))
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("settings_path", SETTINGS_PATH)
	_pull.set("season_path", "res://tests/data/practice_season.toml")
	_pull.set("save_path", "")
	root.add_child(_pull)
	_play.call_deferred()


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _frames(count: int) -> void:
	for i in count:
		await process_frame


func _play() -> void:
	await _frames(2)
	_pull.set("pro", true)
	_pull.set("driver", _drive)
	_pull.call("_start_practice")
	await _frames(1)
	var run: TractorPullRun = _pull.get("run")
	var dimensions := run.get_dimensions()
	_chain_m = dimensions["chain_length_m"]
	var lane_m: float = dimensions["lane_width_m"]
	_bay = Vector2(VenueView.PLAYER_BAY_X_M, -(lane_m / 2.0 + VenueView.PARKING_RIGHT_OF_LANE_M))
	var s := run.get_snapshot()
	var parked: Array[TractorView] = _pull.get("parked")
	var flagman: Flagman = _pull.get("flagman")
	var end_flagman: Flagman = _pull.get("end_flagman")
	var venue: VenueView = _pull.get("venue_view")
	var marker: MeshInstance3D = _pull.get("hitch_marker")
	var hud: Hud = _pull.get("hud")
	var result: ResultScreen = _pull.get("result_screen")

	# In the bay, the rivals in theirs behind it, the flag red and low.
	_expect(s["stage"] == "to_the_sled"
			and Vector2(s["tractor_x_m"], s["tractor_y_m"]).distance_to(_bay) < 1e-6,
			"starts in its bay: %s" % s)
	_expect(parked.size() == VenueView.RIVAL_BAYS_X_M.size()
			and parked.all(func(t: TractorView) -> bool: return t.visible),
			"the rivals wait in their bays")
	_expect(flagman.visible and flagman.colour_name() == "red", "the official holds the red flag")
	_expect(marker.visible, "the stopping mark in front of the sled")
	_expect(not bool(s["chain_hooked"]), "the chain off")
	_expect(hud.turn_text(s).begins_with("Drive past the sled"), "told where to go")
	_expect(venue.signal_shown() == "off" and hud.signal_shown() == "off",
			"no signal before the chain is on")
	_check_steer_hint(hud)
	_check_reverse_key(_pull.get("controls"))

	var stages: Array[String] = [s["stage"]]
	var backing_up_told := false
	var told_to_stand := false
	var result_too_soon := false
	var flag_green_pulling := true
	var flag_red_before := true
	var chain_on_while_hooked := true
	var green_while_pulling := true
	var red_to_stop := true
	var told_to_stop := false
	var ahead_of_the_tractor := true
	var full_pull_m: float = dimensions["full_pull_line_m"]
	for frame in MAX_FRAMES:
		await process_frame
		s = run.get_snapshot()
		var stage: String = s["stage"]
		if stage != stages.back():
			stages.append(stage)
		backing_up_told = backing_up_told or hud.turn_text(s).begins_with("Back up to the sled")
		told_to_stand = told_to_stand or hud.turn_text(s).begins_with("Chain tight")
		if stage in ["to_the_sled", "taking_up_slack", "waiting_for_the_flag"]:
			flag_red_before = flag_red_before and flagman.colour_name() == "red"
		if stage == "pulling":
			flag_green_pulling = flag_green_pulling and flagman.colour_name() == "green"
		if stage in ["taking_up_slack", "waiting_for_the_flag", "pulling", "releasing"]:
			chain_on_while_hooked = chain_on_while_hooked and bool(s["chain_hooked"])
		if stage == "pulling":
			green_while_pulling = green_while_pulling and end_flagman.colour_name() == "green" \
					and end_flagman.raised() and venue.signal_shown() == "green" \
					and hud.signal_shown() == "green"
			# Down the track, from 80 m to the full-pull line, in front of
			# the tractor.
			var official_m := -end_flagman.position.z
			ahead_of_the_tractor = ahead_of_the_tractor and official_m >= 80.0 - 1e-3 \
					and official_m <= full_pull_m + 1e-3 \
					and (official_m > float(s["tractor_x_m"]) or official_m >= full_pull_m - 1e-3)
		if stage == "releasing":
			red_to_stop = red_to_stop and flagman.colour_name() == "red" and flagman.raised() \
					and end_flagman.colour_name() == "red" and end_flagman.raised() \
					and venue.signal_shown() == "red" and hud.signal_shown() == "red"
			told_to_stop = told_to_stop or hud.turn_text(s).begins_with("Red flag: stop!")
		if stage != "parked" and result.visible:
			result_too_soon = true
		if stage == "parked" and result.visible:
			break

	_expect(stages == ["to_the_sled", "taking_up_slack", "waiting_for_the_flag", "pulling",
			"releasing", "to_the_parking", "parked"], "every stage in order: %s" % [stages])
	_expect(told_to_stand, "told to stand ready for the flag")
	_expect(backing_up_told, "told how far to back up")
	_expect(flag_red_before and flag_green_pulling, "red before the pull, green in it")
	_expect(chain_on_while_hooked, "the chain on from hooking to releasing")
	_expect(green_while_pulling, "both officials green, the lights green, in the pull")
	_expect(ahead_of_the_tractor, "the official down the track walks ahead of the tractor")
	_expect(red_to_stop, "both officials red, the lights red, once the pull is over")
	_expect(told_to_stop, "told to stop after the pull")
	_expect(venue.signal_shown() == "off", "the light off once parked")
	_expect(not result_too_soon, "no result before the tractor is back in its bay")
	_expect(result.visible, "the result once parked")
	_expect(run.get_result().get("ending", "") != "out_of_bounds"
			and float(run.get_result().get("distance_m", 0.0)) > 20.0,
			"a pull, the lines not counted on the way: %s" % run.get_result())
	_expect(Vector2(s["tractor_x_m"], s["tractor_y_m"]).distance_to(_bay) < PARKED_WITHIN_M,
			"back in its bay: %s" % s)
	_expect(not bool(s["chain_hooked"]) and not marker.visible, "the chain off again")
	print("turn test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)


# Backing up, the tail goes the way the wheel is turned: a hitch left of
# the sled's line (across > 0) wants the wheel to the right.
func _check_steer_hint(hud: Hud) -> void:
	var backing := {"stage": "to_the_sled", "hitch_along_m": 2.0,
			"hitch_to_hook_m": _chain_m + 1.0, "hitch_across_m": 0.5}
	_expect(hud.turn_text(backing).ends_with("steer right"), "left of the line: steer right")
	backing["hitch_across_m"] = -0.5
	_expect(hud.turn_text(backing).ends_with("steer left"), "right of the line: steer left")
	backing["hitch_across_m"] = 0.05
	_expect(not hud.turn_text(backing).contains("steer"), "on the line: no steering hint")


# The keyboard's minus key: reverse from neutral only, as the touch "-".
func _check_reverse_key(controls: DriveControls) -> void:
	var minus := InputEventKey.new()
	minus.keycode = KEY_MINUS
	minus.pressed = true
	controls.set_gear(3)
	controls._unhandled_input(minus)
	_expect(controls.gear == 3, "minus does not jump from a gear into R: %d" % controls.gear)
	controls.set_gear(0)
	controls._unhandled_input(minus)
	_expect(controls.gear == REVERSE, "minus from N selects R: %d" % controls.gear)
	controls.set_gear(0)


# The scripted driver: [throttle, clutch_pedal, gear, steering, brake_left,
# brake_right] for the snapshot `s`.
func _drive(s: Dictionary) -> Array:
	var stage: String = s["stage"]
	var at := Vector2(s["tractor_x_m"], s["tractor_y_m"])
	var speed: float = s["tractor_speed_m_s"]
	var gear: int = s["gear"]
	match stage:
		"to_the_sled":
			if _path.is_empty() and gear != REVERSE:
				# Out of the bay along the return lane, into the lane past the sled.
				_follow_path([Vector2(0.0, _bay.y), Vector2(8.0, _bay.y + 3.0),
						Vector2(14.0, -3.0), Vector2(19.0, 0.0), Vector2(24.0, 0.0)])
			if gear != REVERSE:
				if at.x < 23.0:
					return _follow(s, 3, false, 0.6)
				return _stop(REVERSE if absf(speed) < 0.01 else 0)
			if float(s["hitch_along_m"]) < 0.75 * _chain_m:
				return _stop(REVERSE)
			_follow_path([Vector2(at.x - 30.0, 0.0)])
			return _follow(s, REVERSE, true, 0.6)
		"taking_up_slack":
			# Creep in 1st until the chain is tight.
			_clutch = maxf(_clutch - 0.4 / 15.0, 0.55)
			return [0.3, _clutch, 1, 0.0, 0.0, 0.0]
		"waiting_for_the_flag":
			# Stand ready in 6th, the clutch pressed.
			_clutch = 1.0
			return [0.0, 1.0, 6, 0.0, 0.0, 0.0]
		"pulling":
			# Full throttle, the clutch let in, along the lane's middle.
			_clutch = maxf(_clutch - 0.5 / 15.0, 0.0)
			_follow_path([Vector2(at.x + 15.0, 0.0)])
			return [1.0, _clutch, 6, _steering(s, false), 0.0, 0.0]
		"releasing":
			if gear != REVERSE:
				return _stop(REVERSE if absf(speed) < 0.01 else 0)
			if float(s["hitch_to_hook_m"]) < _chain_m - 0.6:
				return _stop(REVERSE)
			_follow_path([Vector2(at.x - 10.0, at.y)])
			return _follow(s, REVERSE, true, 0.6)
		"to_the_parking":
			if _home.is_empty():
				# Round to the right at the end of the pull, back along the
				# return lane to the bay.
				var x0 := at.x
				for i in 9:
					var a := PI * i / 8.0
					_home.append(Vector2(x0 + 3.0 + 5.0 * sin(a), -5.0 + 5.0 * cos(a)))
				_home.append(Vector2(x0, _bay.y))
				_home.append(Vector2(0.0, _bay.y))
				_home.append(_bay)
				_follow_path(_home)
			if gear != 3 and absf(speed) > 0.01:
				return _stop(0)
			if at.distance_to(_bay) < 0.5 * PARKED_WITHIN_M:
				return _stop(0)
			return _follow(s, 3, false, 0.6)
	return _stop(0)


# A new path to follow, from its start.
func _follow_path(path: Array[Vector2]) -> void:
	_path = path
	_next = 0


# Pure pursuit: the wheels towards a point LOOK_AHEAD_M along the path,
# the clutch let in over a second, at `throttle`.
func _follow(s: Dictionary, gear: int, reverse: bool, throttle: float) -> Array:
	_ramp = minf(_ramp + 1.0 / 15.0, 1.0)
	return [throttle, 1.0 - _ramp, gear, _steering(s, reverse), 0.0, 0.0]


# The steering control towards the path, forwards or in reverse.
func _steering(s: Dictionary, reverse: bool) -> float:
	var at := Vector2(s["tractor_x_m"], s["tractor_y_m"])
	var heading := float(s["tractor_heading_rad"]) + (PI if reverse else 0.0)
	while _next + 1 < _path.size() and at.distance_to(_path[_next]) < LOOK_AHEAD_M:
		_next += 1
	var target := _path[_next]
	var alpha := wrapf(atan2(target.y - at.y, target.x - at.x) - heading, -PI, PI)
	var wheels := atan(2.0 * WHEELBASE_M * sin(alpha) / LOOK_AHEAD_M)
	if reverse:
		wheels = -wheels
	var share := clampf(wheels / MAX_STEER_RAD, -1.0, 1.0)
	return signf(share) * (DEAD_ZONE + absf(share) * (1.0 - DEAD_ZONE)) if share != 0.0 else 0.0


# Clutch pressed, both brakes on, in `gear`.
func _stop(gear: int) -> Array:
	_ramp = 0.0
	_clutch = 1.0
	return [0.0, 1.0, gear, 0.0, 1.0, 1.0]
