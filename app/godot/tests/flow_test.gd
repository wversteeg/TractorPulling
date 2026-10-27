# The game's flow, played with simulated fingers on a practice season
# (tests/data: a light sled, so the reference tractor makes full pulls, and
# three rivals): the start screen with the first event, the settings from
# its gear and back, the garage (the start money spent, the prize money
# paid after the event, nothing to buy during it), the setup screen
# (the weights and the tire pressure slid by a finger, kept for the season),
# a practice pull and back, then the event's board filling in with the
# rivals' pulls, the player's pulls when they are due, each after the setup
# screen (no "again" there), and their decisions,
# the event's top ten, the season's, and the start screen with the next
# event; then the game launched again: the saved season continued, at the
# next event with the same standings, tuning and career, and a new season
# with the career carried over. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it).
# With a directory after `--` it also saves a screenshot of each screen
# there (that needs a display, or Xvfb, and a renderer).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/flow_test.gd [-- /tmp/shots]
extends SceneTree

const SEASON := "res://tests/data/practice_season.toml"
const SEED := 2026
# The test garage's start money (tests/data/practice_garage.toml).
const START_MONEY_EUR := 1000
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
const SAVE_PATH := "user://flow_test_season.toml"

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
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SAVE_PATH))
	_pull.set("save_path", SAVE_PATH)
	# Pulls from the start line: the whole turn is turn_test.gd's.
	_pull.set("whole_turn", false)
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_shots = args[0]
	root.add_child(_pull)
	_play.call_deferred()


# A part at its top level keeps the garage's columns: the row after it
# starts in the first column, its Buy in the last.
func _check_garage_columns(garage: GarageScreen) -> void:
	var line := {"name": "Part", "label": "Now", "level": 0, "levels": 2,
			"next_label": "", "next_cost_eur": 0}
	var next := line.duplicate()
	next["next_label"] = "Better"
	next["next_cost_eur"] = 100
	garage.show_garage({"money_eur": 1000, "can_buy": true, "upgrades": [line, next]})
	await _frames(2)
	var table: GridContainer = garage.get("_table")
	var cells := table.get_children()
	_expect(cells.size() == 2 * table.columns, "a cell for each column: %d" % cells.size())
	if cells.size() == 2 * table.columns:
		var first := cells[0] as Control
		var below := cells[table.columns] as Control
		var buy := cells[table.columns - 1] as Control
		var buy_below := cells[2 * table.columns - 1] as Control
		_expect(is_equal_approx(first.position.x, below.position.x)
				and is_equal_approx(buy.position.x, buy_below.position.x),
				"the columns line up after a part at its top level")


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

	# The garage: the start money buys the second powershift level once;
	# then it is at its top.
	var garage: GarageScreen = _pull.get("garage_screen")
	_tap(start.garage_button)
	await _frames(2)
	var stock := event.get_garage()
	_expect(garage.visible and int(stock["money_eur"]) == START_MONEY_EUR and stock["can_buy"]
			and garage.buy_buttons.size() == 1 and garage.buy_buttons[0].visible,
			"the garage: %s" % stock)
	await _shot("1c_garage")
	_tap(garage.buy_buttons[0])
	await _frames(2)
	var bought := event.get_garage()
	var powershift: Dictionary = bought["upgrades"][0]
	_expect(int(bought["money_eur"]) == 400 and int(powershift["level"]) == 1
			and bought["fully_upgraded"] and not garage.buy_buttons[0].visible,
			"bought the powershift: %s" % bought)
	_expect(event.buy_upgrade(0) == "top level", "nothing more to buy")
	await _check_garage_columns(garage)
	garage.show_garage(event.get_garage())
	_tap(garage.done_button)
	await _frames(2)
	_expect(start.visible and not garage.visible, "back from the garage")

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
			_expect(not event.get_garage()["can_buy"], "no upgrades during the event")
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
	var prize := event.get_event_prize()
	_expect(prize >= 0 and (prize == 0 or board.status().contains("won")),
			"the prize, if any: %d" % prize)
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
	_expect(int(event.get_garage()["money_eur"]) == 400 + prize and event.get_garage()["can_buy"],
			"the prize paid: %s" % event.get_garage())
	await _shot("6_season_top")

	# Next: the start screen with the season's second event.
	_tap(board.first_button)
	await _frames(2)
	info = event.get_event()
	_expect(start.visible and not board.visible, "back to the start screen")
	_expect(info.get("index", -1) == 1 and info.get("name", "") == "Practice, firm loam",
			"the next event: %s" % info)

	await _relaunch(event.get_season_standings(), event.get_tuning(), event.get_garage())

	print("flow test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)


# The pull scene again, as at the next launch, on the same save.
func _launch() -> Node:
	_pull.queue_free()
	await _frames(2)
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("season_path", SEASON)
	_pull.set("season_seed", SEED + 1)
	_pull.set("settings_path", SETTINGS_PATH)
	_pull.set("save_path", SAVE_PATH)
	_pull.set("whole_turn", false)
	root.add_child(_pull)
	await _frames(2)
	return _pull


# Launched again after the first event: the start screen offers the saved
# season; Continue picks it up at the second event, with the same
# championship and tuning. Launched once more, New season starts over at
# the first event, and the save is the new season's.
func _relaunch(standings: Array, tuning: Dictionary, garage: Dictionary) -> void:
	_expect(FileAccess.file_exists(SAVE_PATH) and not FileAccess.file_exists(SAVE_PATH + ".tmp"),
			"the season is saved, renamed into place")
	var pull := await _launch()
	var start: StartScreen = pull.get("start_screen")
	var event: TractorPullEvent = pull.get("event")
	_expect(start.visible and start.continue_button.visible and start.new_season_button.visible
			and start.new_career_button.visible
			and not start.practice_button.visible and not start.settings_button.visible,
			"continue, a new season or a new career")
	await _shot("7_continue")
	_tap(start.continue_button)
	await _frames(2)
	var info := event.get_event()
	_expect(start.visible and start.practice_button.visible and info.get("index", -1) == 1,
			"continued at the second event: %s" % info)
	_expect(event.get_season_standings() == standings, "the same championship: %s"
			% [event.get_season_standings()])
	var resumed := event.get_tuning()
	_expect(resumed["ballast_position"] == tuning["ballast_position"]
			and resumed["pressure_bar"] == tuning["pressure_bar"], "the same tuning: %s" % resumed)
	_expect(event.get_garage() == garage, "the same career: %s" % event.get_garage())

	pull = await _launch()
	start = pull.get("start_screen")
	event = pull.get("event")
	_tap(start.new_season_button)
	await _frames(2)
	info = event.get_event()
	_expect(start.visible and info.get("index", -1) == 0
			and int(event.get_season().get("scored", -1)) == 0, "a new season: %s" % info)
	var saved := FileAccess.get_file_as_string(SAVE_PATH)
	_expect(not saved.contains("standings") and saved.contains(str(SEED + 1)),
			"the save is the new season's: %s" % saved)
	_expect(event.get_garage()["money_eur"] == garage["money_eur"]
			and event.get_garage()["upgrades"] == garage["upgrades"],
			"the career carries into the new season: %s" % event.get_garage())

	# A new career from the settings, on the second tap: the garage's start
	# money, nothing bought.
	var settings: SettingsScreen = pull.get("settings_screen")
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.new_career_button)
	await _frames(2)
	_expect(settings.visible and event.get_garage()["upgrades"] == garage["upgrades"],
			"the first tap only asks")
	_tap(settings.new_career_button)
	await _frames(2)
	var fresh := event.get_garage()
	var nothing_bought := (fresh["upgrades"] as Array).all(
			func(u: Dictionary) -> bool: return int(u["level"]) == 0)
	_expect(start.visible and int(fresh["money_eur"]) == START_MONEY_EUR and nothing_bought
			and int(event.get_season().get("scored", -1)) == 0,
			"a new career: %s" % fresh)
	var saved_career := FileAccess.get_file_as_string(SAVE_PATH)
	_expect(not saved_career.contains("level = 1"), "the save has the new career: %s"
			% saved_career)
