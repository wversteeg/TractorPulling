# The game's flow, played with simulated fingers on a practice season
# (tests/data: a light sled, so the reference tractor makes full pulls):
# the start screen with the first event, a qualifying pull, its result with
# the pull-off the full pull earns, the pull-off on the heavier sled, its
# result with no further pull, and the start screen with the next event.
# Exits non-zero if a check fails (scripts/godot_test.sh runs it).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/flow_test.gd
extends SceneTree

const SEASON := "res://tests/data/practice_season.toml"
const TAP_FINGER := 1
const THROTTLE_FINGER := 0
# The pulls run at this many times real time, to keep the test short; the
# session still steps at 1 ms (up to 100 steps a frame).
const TIME_SCALE := 4.0
# Long enough for a pull and the pause before the result, at TIME_SCALE.
const MAX_PULL_FRAMES := 1200

var _pull: Node
var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("season_path", SEASON)
	root.add_child(_pull)
	_play.call_deferred()


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _touch(finger: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = finger
	event.position = at
	event.pressed = pressed
	root.push_input(event, true)


func _drag(finger: int, at: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = finger
	event.position = at
	root.push_input(event, true)


func _tap(control: Control) -> void:
	var at := control.get_global_rect().get_center()
	_touch(TAP_FINGER, at, true)
	_touch(TAP_FINGER, at, false)


func _frames(count: int) -> void:
	for i in count:
		await process_frame


# Sixth gear and the hand throttle slid open, then the frames until the
# result screen shows; false if it never does.
func _drive_to_result(controls: DriveControls, result: ResultScreen) -> bool:
	for i in 6:
		_tap(controls.gear_up)
	var throttle := controls.throttle_slider.get_global_rect()
	var x := throttle.get_center().x
	_touch(THROTTLE_FINGER, Vector2(x, throttle.end.y - 1.0), true)
	_drag(THROTTLE_FINGER, Vector2(x, throttle.position.y - 10.0))
	_touch(THROTTLE_FINGER, Vector2(x, throttle.position.y - 10.0), false)
	for i in MAX_PULL_FRAMES:
		await process_frame
		if result.visible:
			return true
	return false


func _play() -> void:
	Engine.time_scale = TIME_SCALE
	await _frames(2)
	var run: TractorPullRun = _pull.get("run")
	var controls: DriveControls = _pull.get("controls")
	var start: StartScreen = _pull.get("start_screen")
	var result: ResultScreen = _pull.get("result_screen")
	var hud: Hud = _pull.get("hud")

	# The start screen: the season's first event, its run set up behind it.
	var event := run.get_event()
	_expect(start.visible and not hud.visible and not controls.visible, "the start screen")
	_expect(event.get("index", -1) == 0 and event.get("count", 0) == 2
			and event.get("round", -1) == 0, "the first event's qualifying: %s" % event)
	_expect(event.get("name", "") == "Practice, watered start", "its name: %s" % event)
	# The development keys belong to a pull: R (pull again) does nothing here.
	var r := InputEventKey.new()
	r.keycode = KEY_R
	r.pressed = true
	root.push_input(r, true)
	await _frames(2)
	_expect(start.visible and not controls.visible, "R on the start screen does nothing")

	# Qualifying: a full pull, then the result screen offers the pull-off.
	_tap(start.next_button)
	await _frames(2)
	_expect(controls.visible and hud.visible and not start.visible, "pulling")
	_expect(await _drive_to_result(controls, result), "the qualifying pull ends in a result")
	_expect(run.get_result().get("ending", "") == "full_pull", "a full pull: %s" % run.get_result())
	_expect(result.pull_off_button.visible and not result.next_button.visible,
			"the full pull earns the pull-off")
	_expect(not controls.visible, "no controls on the result screen")

	# The pull-off: round 1 on the heavier sled; after it the rules give no
	# more pulls, full pull or not.
	_tap(result.pull_off_button)
	await _frames(2)
	_expect(run.get_event().get("round", -1) == 1 and controls.visible, "the pull-off")
	_expect(controls.gear == 0 and controls.throttle() == 0.0, "the controls reset for it")
	_expect(await _drive_to_result(controls, result), "the pull-off ends in a result")
	_expect(result.next_button.visible and not result.pull_off_button.visible,
			"one pull-off, then on to the next event")

	# Next: the start screen with the season's second event.
	_tap(result.next_button)
	await _frames(2)
	event = run.get_event()
	_expect(start.visible and not result.visible, "back to the start screen")
	_expect(event.get("index", -1) == 1 and event.get("round", -1) == 0
			and event.get("name", "") == "Practice, firm loam", "the next event: %s" % event)

	print("flow test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
