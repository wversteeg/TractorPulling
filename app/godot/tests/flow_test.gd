# The game's flow, played with simulated fingers on a practice season
# (tests/data: a light sled, so the reference tractor makes full pulls, and
# three rivals): the start screen with the first event, the settings from
# its gear and back, the setup screen
# (the weights and the tire pressure slid by a finger, kept for the season),
# a practice pull and back, then the event's board filling in with the
# rivals' pulls, the player's pulls when they are due, each after the setup
# screen (no "again" there), and their decisions,
# the event's top ten, the season's, and the start screen with the next
# event. Exits non-zero if a check fails (scripts/godot_test.sh runs it).
# With a directory after `--` it also saves a screenshot of each screen
# there (that needs a display, or Xvfb, and a renderer).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/flow_test.gd [-- /tmp/shots]
extends SceneTree

const SEASON := "res://tests/data/practice_season.toml"
const SEED := 2026
const PULLERS := 4
const TAP_FINGER := 1
const THROTTLE_FINGER := 0
# The pulls run at this many times real time, to keep the test short; the
# session still steps at 1 ms (up to 100 steps a frame).
const TIME_SCALE := 4.0
# Long enough for a pull past the line and the pause before the result, at
# TIME_SCALE; and for a whole event.
const MAX_PULL_FRAMES := 2400
const MAX_EVENT_FRAMES := 20_000
# None there: the default settings, whatever this machine keeps.
const SETTINGS_PATH := "user://flow_test_settings.cfg"

var _pull: Node
var _failures := 0
var _shots := ""


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("season_path", SEASON)
	_pull.set("season_seed", SEED)
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SETTINGS_PATH))
	_pull.set("settings_path", SETTINGS_PATH)
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_shots = args[0]
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


# A screenshot named `name`, when asked for.
func _shot(name: String) -> void:
	if _shots.is_empty():
		return
	await _frames(3)
	var path := "%s/%s.png" % [_shots, name]
	root.get_texture().get_image().save_png(path)
	print("saved ", path)


# A finger slides `slider` from its middle to past its start end.
func _slide_to_start(slider: TouchSlider) -> void:
	var rect := slider.get_global_rect()
	_touch(TAP_FINGER, rect.get_center(), true)
	_drag(TAP_FINGER, Vector2(rect.position.x - 20.0, rect.get_center().y))
	_touch(TAP_FINGER, Vector2(rect.position.x - 20.0, rect.get_center().y), false)


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
	var event: TractorPullEvent = _pull.get("event")
	var controls: DriveControls = _pull.get("controls")
	var start: StartScreen = _pull.get("start_screen")
	var board: BoardScreen = _pull.get("board")
	var result: ResultScreen = _pull.get("result_screen")
	var hud: Hud = _pull.get("hud")
	var setup: SetupScreen = _pull.get("setup_screen")

	# How a pull reads: against the line before it, not the one after.
	_expect(EventTexts.reach({"disqualified": false, "full_pull": true, "reached_m": 104.0,
			"full_pull_m": 110.0}) == "FULL PULL", "a full pull that moved the line")
	_expect(EventTexts.reach({"disqualified": false, "full_pull": false, "reached_m": 95.5,
			"full_pull_m": 100.0}) == "95.50 m", "a pull short of the line")

	# The start screen: the season's first event, started behind it.
	var info := event.get_event()
	_expect(start.visible and not hud.visible and not controls.visible, "the start screen")
	_expect(info.get("index", -1) == 0 and info.get("count", 0) == 2
			and info.get("pullers", 0) == PULLERS, "the first event: %s" % info)
	_expect(info.get("name", "") == "Practice, watered start", "its name: %s" % info)
	await _shot("1_start")
	# The development keys belong to a pull: R (pull again) does nothing here.
	var r := InputEventKey.new()
	r.keycode = KEY_R
	r.pressed = true
	root.push_input(r, true)
	await _frames(2)
	_expect(start.visible and not controls.visible, "R on the start screen does nothing")

	# The settings, from the gear, and back.
	var settings: SettingsScreen = _pull.get("settings_screen")
	_tap(start.settings_button)
	await _frames(2)
	_expect(settings.visible and settings.easy_button.selected and settings.slider_button.selected,
			"the settings, as default")
	await _shot("1a_settings")
	_tap(settings.done_button)
	await _frames(2)
	_expect(start.visible and not settings.visible, "back from the settings")

	# The setup screen first: the tractor as built, then the weights slid to
	# the rear and the tires softened, which the season keeps. "Back"
	# returns to the start screen.
	_tap(start.practice_button)
	await _frames(2)
	var built := event.get_tuning()
	_expect(setup.visible and setup.ballast_slider.visible and setup.pressure_slider.visible
			and is_equal_approx(float(built["ballast_position"]), 0.5)
			and is_equal_approx(float(built["pressure_bar"]), 1.0), "set up as built: %s" % built)
	_slide_to_start(setup.ballast_slider)
	_slide_to_start(setup.pressure_slider)
	await _frames(2)
	var tuned := event.get_tuning()
	_expect(float(tuned["ballast_position"]) == 0.0
			and float(tuned["rear_share"]) > float(built["rear_share"])
			and is_equal_approx(float(tuned["pressure_bar"]), float(tuned["min_bar"])),
			"the weights to the rear, the tires soft: %s" % tuned)
	await _shot("1b_setup")
	_tap(setup.back_button)
	await _frames(2)
	_expect(start.visible and not setup.visible, "back to the start screen")

	# A practice pull, with "again" and "menu", and back to the start.
	_tap(start.practice_button)
	await _frames(2)
	_expect(setup.visible and setup.ballast_slider.value == 0.0, "the setting kept")
	_tap(setup.pull_button)
	await _frames(2)
	_expect(controls.visible and controls.restart_button.visible and controls.menu_button.visible,
			"practice, with again and menu")
	_tap(controls.menu_button)
	await _frames(2)
	_expect(start.visible and event.get_event().get("index", -1) == 0,
			"back to the start, the same event")

	# The event: the board fills in, the player pulls and decides when due.
	_tap(start.event_button)
	await _frames(2)
	_expect(board.visible and not start.visible, "the event's board")
	var my_pulls := 0
	var frames := 0
	while frames < MAX_EVENT_FRAMES:
		var turn: String = event.get_event()["turn"]
		if turn == "finished" or turn == "failed":
			break
		if turn == "player_pull":
			_expect(board.first_button.visible and board.first_button.text == "Pull",
					"the player's turn")
			await _shot("2_board_%d" % my_pulls)
			_tap(board.first_button)
			await _frames(2)
			_expect(setup.visible and setup.pull_button.visible, "the setup before the pull")
			_tap(setup.pull_button)
			await _frames(2)
			_expect(controls.visible and not controls.restart_button.visible
					and not controls.menu_button.visible, "an event's pull: no again or menu")
			_expect(await _drive_to_result(controls, result), "the pull ends in a result")
			_expect(result.continue_button.visible and not result.again_button.visible,
					"after an event's pull: continue")
			await _shot("3_result_%d" % my_pulls)
			my_pulls += 1
			_tap(result.continue_button)
			await _frames(2)
			_expect(board.visible, "back to the board")
		elif turn == "player_decision":
			_expect(board.second_button.visible, "keep or pull again")
			await _shot("4_decision")
			_tap(board.first_button)
			await _frames(2)
		else:
			await process_frame
			frames += 1
	_expect(event.get_event()["turn"] == "finished", "the event finished: %s" % event.get_event())
	_expect(my_pulls >= 1, "the player pulled: %d" % my_pulls)
	for entry: Dictionary in event.get_log(0):
		print("  ", " | ".join(EventTexts.log_row(entry)))
	var pullers := {}
	for entry: Dictionary in event.get_log(0):
		pullers[entry.get("puller", entry.get("deciding", -1))] = true
		# A test pull that moves the line to 110 m passed the line before it.
		if entry.get("change", "") == "line_extended":
			_expect(entry["full_pull"] and EventTexts.reach(entry) == "FULL PULL",
					"a test pull past 100 m is a full pull: %s" % entry)
	_expect(pullers.size() == PULLERS, "every puller pulled: %s" % pullers)
	await _frames(2)
	_expect(board.first_button.text == "Results", "the event is over")

	# The event's top ten (here four), then the season's.
	_tap(board.first_button)
	await _frames(2)
	_expect(board.visible and board.row_count() == PULLERS, "the event's standings")
	await _shot("5_event_top")
	_expect(board.first_button.text == "Season", "on to the season")
	_tap(board.first_button)
	await _frames(2)
	var season := event.get_season()
	_expect(season.get("scored", 0) == 1 and not season.get("over", true),
			"the event is scored: %s" % season)
	var points := 0
	for line: Dictionary in event.get_season_standings():
		points += int(line["points"])
	_expect(points > 0 and board.row_count() == PULLERS, "the season's standings")
	_expect(board.first_button.text == "Next event", "on to the next event")
	await _shot("6_season_top")

	# Next: the start screen with the season's second event.
	_tap(board.first_button)
	await _frames(2)
	info = event.get_event()
	_expect(start.visible and not board.visible, "back to the start screen")
	_expect(info.get("index", -1) == 1 and info.get("name", "") == "Practice, firm loam",
			"the next event: %s" % info)

	print("flow test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
