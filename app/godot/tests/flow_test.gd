# The game's flow, played with simulated fingers on a practice season
# (tests/data: a light sled, so the reference tractor makes full pulls, and
# three rivals): the start screen with the first event, the settings from
# its gear and back, the garage (the start money spent, the prize money
# paid after the event, nothing to buy during it), the setup screen
# (the weights and the tire pressure slid by a finger, kept for the season),
# a practice pull and back, then the event's board filling in with the
# rivals' pulls, the player's pulls when they are due, each after the setup
# screen (no "again" there), and their decisions,
# the event's results, the season's standings, and the start screen with the next
# event; then the game launched again: the saved season continued, at the
# next event with the same standings, tuning and career, and a new season
# with the career carried over. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it).
# With a directory after `--` it also saves a screenshot of each screen
# there (that needs a display, or Xvfb, and a renderer).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/flow_test.gd [-- /tmp/shots]
extends SceneTree

const CAREER := "res://tests/data/practice_career.toml"
const LADDER := "res://tests/data/practice_ladder.toml"
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
	_pull.set("career_path", CAREER)
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


# A new career: the garage's start money and nothing bought, in the game
# and in the save, at the start of a season.
func _check_new_career(event: TractorPullEvent, start: StartScreen, what: String) -> void:
	var fresh := event.get_garage()
	var nothing_bought := (fresh["upgrades"] as Array).all(
			func(u: Dictionary) -> bool: return int(u["level"]) == 0)
	_expect(start.visible and int(fresh["money_eur"]) == START_MONEY_EUR and nothing_bought
			and int(event.get_season().get("scored", -1)) == 0, "%s: %s" % [what, fresh])
	var saved := FileAccess.get_file_as_string(SAVE_PATH)
	var levels := RegEx.create_from_string("level = (\\d+)").search_all(saved)
	_expect(saved.contains("money_eur = %d\n" % START_MONEY_EUR) and not levels.is_empty()
			and levels.all(func(m: RegExMatch) -> bool: return m.get_string(1) == "0"),
			"%s, saved: %s" % [what, saved])


# More rows than fit: all of them in the list, which shows the newest; a
# table opens at its top.
func _check_board_scrolls(board: BoardScreen) -> void:
	var rows: Array[PackedStringArray] = []
	for i in 30:
		rows.append(PackedStringArray(["Puller %d" % i, "pull", "%d m" % i, ""]))
	board.visible = true
	board.set_rows(rows)
	await _frames(3)
	_expect(board.row_count() == 30 and board.at_end(), "every row, the newest in view")
	board.set_rows(rows, PackedInt32Array(), false)
	await _frames(3)
	_expect(not board.at_end(), "a table opens at its top")
	board.visible = false


# The TEST flag for a test pull; in a pull-off the chequered flag at the
# round's best so far (not a disqualified pull, not another round's).
func _check_officials_flags(venue: VenueView) -> void:
	var log: Array = [
		{"kind": "pull_off", "pull_off_round": 1, "reached_m": 96.5, "disqualified": false},
		{"kind": "pull_off", "pull_off_round": 1, "reached_m": 99.0, "disqualified": true},
		{"kind": "pull_off", "pull_off_round": 2, "reached_m": 98.0, "disqualified": false},
		{"kind": "competition", "pull_off_round": 0, "reached_m": 100.0, "disqualified": false},
	]
	_expect(is_equal_approx(_pull.best_in_pull_off(log, 1), 96.5)
			and _pull.best_in_pull_off(log, 0) < 0.0, "the pull-off round's best")
	venue.show_flags(true, 96.5)
	_expect(venue.test_flag_shown() and is_equal_approx(venue.best_flag_at_m(), 96.5),
			"the TEST flag and the chequered flag at 96.5 m")
	venue.show_flags(false, -1.0)
	_expect(not venue.test_flag_shown() and venue.best_flag_at_m() < 0.0, "no flags")


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


# Drags a vertical slider past its bottom end.
func _slide_to_bottom(slider: TouchSlider) -> void:
	var rect := slider.get_global_rect()
	var below := Vector2(rect.get_center().x, rect.end.y + 20.0)
	_touch(TAP_FINGER, rect.get_center(), true)
	_drag(TAP_FINGER, below)
	_touch(TAP_FINGER, below, false)


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


# The replay keeps the last 2 s of the wait for the flag, the pull and 3 s
# after it, and cuts between its trackside shots down the track.
func _check_replay_recording() -> void:
	var replay := PullReplay.new()
	const DT := 0.1
	for stage: String in ["to_the_sled", "waiting_for_the_flag", "pulling", "parked"]:
		for i in 50 if stage != "pulling" else 100:
			replay.record({"stage": stage, "tractor_x_m": 0.0}, DT)
	_expect(replay.has_pull() and absf(replay.length_s()
			- (PullReplay.BEFORE_FLAG_S + 10.0 + PullReplay.AFTER_END_S)) <= DT + 0.001,
			"the replay from just before the flag to just after the pull: %.1f s"
			% replay.length_s())
	var backed_off := PullReplay.new()
	for stage: String in ["waiting_for_the_flag", "taking_up_slack"]:
		backed_off.record({"stage": stage}, DT)
	_expect(not backed_off.has_pull() and backed_off.length_s() == 0.0,
			"no replay before the flag")
	# On a pull past the line the head-on camera stays ahead of the tractor.
	var far: Vector3 = PullReplay.shot({"stage": "pulling", "tractor_x_m": 118.0}, 100.0,
			9.0)["from"]
	_expect(far.x >= 118.0 + PullReplay.HEAD_ON_AHEAD_M, "head-on ahead of the tractor: %s" % far)
	var names: Array[String] = []
	for x_m: float in [5.0, 40.0, 90.0]:
		names.append(PullReplay.shot({"stage": "pulling", "tractor_x_m": x_m}, 100.0, 9.0)["name"])
	names.append(PullReplay.shot({"stage": "parked", "sled_x_m": 60.0}, 100.0, 9.0)["name"])
	_expect(names == ["launch", "alongside", "head_on", "end"], "the replay's shots: %s" % [names])


# The result's Replay: the pull again from beside the track, over by
# itself or by "Back", then the result again.
func _check_replay(result: ResultScreen) -> void:
	var overlay: ReplayOverlay = _pull.get("replay_overlay")
	var tractor: Node3D = _pull.get("tractor_view")
	var parked_at := tractor.global_position
	_expect(result.replay_button.visible, "a replay of the pull offered")
	var marks: TrackMarks = _pull.get("track_marks")
	var pulled_marks := marks.point_counts()
	_tap(result.replay_button)
	await _frames(2)
	_expect(marks.point_counts().x < pulled_marks.x / 4,
			"the replay lays the track's marks as it plays: %s of %s"
			% [marks.point_counts(), pulled_marks])
	_expect(_pull.get("screen") == _pull.Screen.REPLAY and overlay.visible
			and not result.visible, "the replay over the result")
	var replay: PullReplay = _pull.get("replay")
	var moved := false
	var shots := {}
	for i in MAX_PULL_FRAMES:
		await process_frame
		if result.visible:
			break
		moved = moved or tractor.global_position.distance_to(parked_at) > 5.0
		var snapshot: Dictionary = _pull.get("_snapshot")
		shots[PullReplay.shot(snapshot, 100.0, 9.0)["name"]] = true
	_expect(result.visible and _pull.get("screen") == _pull.Screen.RESULT,
			"the result again once the replay is over (%.1f s)" % replay.length_s())
	_expect(moved, "the replay shows the tractor pulling, away from its bay")
	_expect(marks.point_counts().x > 0, "the marks laid again: %s" % marks.point_counts())
	_expect(shots.has("launch") and shots.has("alongside"),
			"the replay's trackside shots: %s" % [shots.keys()])
	_expect(tractor.global_position.distance_to(parked_at) < 0.5,
			"back in its bay after the replay")
	var replayed_marks := marks.point_counts()
	_tap(result.replay_button)
	await _frames(5)
	_tap(overlay.back_button)
	await _frames(2)
	_expect(result.visible and not overlay.visible, "Back: the result again")
	_expect(marks.point_counts() == replayed_marks,
			"Back mid-replay: the pull's marks all laid: %s of %s"
			% [marks.point_counts(), replayed_marks])


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

	_check_replay_recording()
	# How a pull reads: a full pull as the event judges it; a test pull
	# that moved the line to 110 m counts its distance.
	_expect(EventTexts.reach({"disqualified": false, "full_pull": true, "reached_m": 101.0,
			"full_pull_m": 100.0}) == "FULL PULL", "a full pull")
	_expect(EventTexts.reach({"disqualified": false, "full_pull": false, "reached_m": 104.0,
			"full_pull_m": 110.0}) == "104.00 m", "a test pull that moved the line")
	_expect(EventTexts.reach({"disqualified": false, "full_pull": false, "reached_m": 95.5,
			"full_pull_m": 100.0}) == "95.50 m", "a pull short of the line")
	# The standings show each pull-off round, and say "full pull" only for
	# one.
	var two_rounds := EventTexts.standing_row({"place": 1, "name": "Henk", "disqualified": false,
			"full_pull": false, "distance_m": 96.0, "pull_off_rounds": 2,
			"pull_off_distances_m": PackedFloat64Array([120.0, 98.5]), "points": 20})
	_expect(two_rounds[2] == "96.00 m, pull-off 1: 120.00 m, pull-off 2: 98.50 m",
			"both pull-offs in the standings: %s" % two_rounds[2])
	_expect(two_rounds[3] == "20 points", "the place's points in the results: %s" % two_rounds[3])
	_expect(EventTexts.ordinal(1) == "1st" and EventTexts.ordinal(2) == "2nd"
			and EventTexts.ordinal(12) == "12th" and EventTexts.ordinal(23) == "23rd",
			"places in words")
	# The board's bold rows: the main round's best four as they count (a
	# re-pull replaces the test pull) and the best of the pull-off under way.
	var log := [
		{"puller": 1, "kind": "test", "reached_m": 99.0, "disqualified": false},
		{"puller": 2, "kind": "competition", "reached_m": 85.0, "disqualified": false},
		{"deciding": 1, "keep": false},
		{"puller": 3, "kind": "competition", "reached_m": 92.0, "disqualified": false},
		{"puller": 4, "kind": "competition", "reached_m": 70.0, "disqualified": false},
		{"puller": 5, "kind": "competition", "reached_m": 88.0, "disqualified": true},
		{"puller": 6, "kind": "competition", "reached_m": 81.0, "disqualified": false},
		{"puller": 1, "kind": "repull", "reached_m": 90.0, "disqualified": false},
		{"puller": 3, "kind": "pull_off", "pull_off_round": 1, "reached_m": 91.0,
				"disqualified": false},
		{"puller": 1, "kind": "pull_off", "pull_off_round": 1, "reached_m": 93.5,
				"disqualified": false},
	]
	# The boards list every puller, however many: fifteen, the player 12th.
	var field: Array = []
	for i in 15:
		var puller := 0 if i == 11 else i + 1
		field.append({"place": i + 1, "puller": puller, "name": "P%d" % puller,
				"disqualified": false, "full_pull": false, "distance_m": 99.0 - i,
				"pull_off_rounds": 0, "points": 15 - i, "wins": 0})
	_expect(EventTexts.standing_rows(field).size() == 15
			and EventTexts.season_rows(field).size() == 15
			and EventTexts.player_rows(field) == PackedInt32Array([11]),
			"every puller on the boards, the player's row marked")
	var bold := EventTexts.leading_rows(log, 4)
	bold.sort()
	_expect(bold == PackedInt32Array([1, 3, 6, 7, 9]), "the bold rows: %s" % bold)
	# A guest: marked by name, no points, and in bold only past the
	# competitors' last pull-off place.
	var guest := EventTexts.standing_row({"place": 1, "name": "Rik", "guest": true,
			"disqualified": false, "full_pull": false, "distance_m": 96.0, "points": 0})
	_expect(guest[1] == "Rik (guest)" and guest[3] == "guest", "a guest's row: %s" % guest)
	var with_guests := [
		{"puller": 0, "kind": "test", "reached_m": 95.0, "disqualified": false},
		{"puller": 1, "kind": "competition", "reached_m": 94.0, "disqualified": false},
		{"puller": 5, "kind": "competition", "reached_m": 99.0, "disqualified": false,
				"guest": true},
		{"puller": 2, "kind": "competition", "reached_m": 93.0, "disqualified": false},
		{"puller": 6, "kind": "competition", "reached_m": 91.0, "disqualified": false,
				"guest": true},
		{"puller": 3, "kind": "competition", "reached_m": 92.0, "disqualified": false},
		{"puller": 4, "kind": "competition", "reached_m": 80.0, "disqualified": false},
	]
	var bold_guests := EventTexts.leading_rows(with_guests, 4)
	bold_guests.sort()
	_expect(bold_guests == PackedInt32Array([0, 1, 2, 3, 5]),
			"the competitors' four and the guest past them: %s" % bold_guests)

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

	# The garage: the workshop shows a pin for the powershift and one for
	# the pump choice. The powershift's screen buys its second level once
	# with the start money; then it is at its top. Then the spare pump,
	# which is fitted, taken off and fitted again for nothing.
	var garage: GarageScreen = _pull.get("garage_screen")
	_tap(start.garage_button)
	await _frames(2)
	var stock := event.get_garage()
	_expect(garage.visible and garage.view == GarageScreen.View.WORKSHOP
			and int(stock["money_eur"]) == START_MONEY_EUR and stock["can_buy"]
			and garage.upgrade_pins.size() == 1 and garage.choice_pins.size() == 1
			and garage.upgrade_pins[0].affordable and garage.done_button.visible,
			"the workshop: %s" % stock)
	await _shot("1c_garage")
	_tap(garage.upgrade_pins[0])
	await _frames(2)
	_expect(garage.view == GarageScreen.View.UPGRADE and garage.buy_button.visible
			and not garage.buy_button.disabled and garage.back_button.visible
			and not garage.done_button.visible, "the powershift's screen")
	await _shot("1c_garage_part")
	_tap(garage.buy_button)
	await _frames(2)
	var bought := event.get_garage()
	var powershift: Dictionary = bought["upgrades"][0]
	_expect(int(bought["money_eur"]) == 400 and int(powershift["level"]) == 1
			and not bought["fully_upgraded"] and not garage.buy_button.visible
			and garage.view == GarageScreen.View.UPGRADE,
			"bought the powershift: %s" % bought)
	_expect(event.buy_upgrade(0) == "top level", "nothing more to buy")
	_tap(garage.back_button)
	await _frames(2)
	_expect(garage.view == GarageScreen.View.WORKSHOP and garage.upgrade_pins[0].complete,
			"back in the workshop, the powershift complete")
	await _check_garage_choice(event, garage)
	await _check_board_scrolls(_pull.get("board"))
	_check_officials_flags(_pull.get("venue_view"))
	garage.show_garage(event.get_garage())
	_tap(garage.back_button)
	await _frames(2)
	_tap(garage.done_button)
	await _frames(2)
	_expect(start.visible and not garage.visible, "back from the garage")

	# The setup screen first: the tractor as built (its hitch set per pull
	# from its lowest hole, 0.40 m, to its own drawbar's 0.50 m, in the top
	# hole), then the weights slid to the rear and the tires softened, which
	# the season keeps. "Back" returns to the start screen.
	_tap(start.practice_button)
	await _frames(2)
	var built := event.get_tuning()
	_expect(setup.visible and setup.ballast_slider.visible and setup.pressure_slider.visible
			and setup.hitch_slider.visible and is_equal_approx(float(built["hitch_m"]), 0.5)
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
	# The lane as the crew walked it, and the start: the slider's left end
	# puts the sled at full left, as far as the lane allows; then back to
	# the middle.
	var crew := event.get_crew_report()
	var max_y := float(crew.get("max_y_m", 0.0))
	var seen := int(crew.get("pulls_seen", -1))
	_expect(setup.start_slider.visible and max_y > 2.0 and crew.has("spots") and seen >= 0
			and is_equal_approx(float(crew.get("uncertainty", 0.0)), 1.0 / (1.0 + 0.25 * seen)),
			"the crew's report: %s" % crew)
	_slide_to_start(setup.start_slider)
	await _frames(2)
	var left := float(event.get_crew_report().get("start_y_m", 0.0))
	_expect(is_equal_approx(left, clampf(snappedf(max_y, SetupScreen.START_STEP_M), -max_y, max_y)),
			"the sled at full left: %f of %f" % [left, max_y])
	await _shot("1b_setup")
	await _check_track_screen(setup, event)
	setup.start_slider.value = 0.0
	await _frames(2)
	_expect(float(event.get_crew_report().get("start_y_m", 1.0)) == 0.0, "the sled back in the middle")
	_check_hitch_slider(setup)
	_check_crew_tip(setup, event)
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
			var sound: EngineSound = _pull.get("engine_sound")
			_expect(sound.target_volume() == 0.0,
					"the engine silent behind the result: %f" % sound.target_volume())
			await _shot("3_result_%d" % my_pulls)
			if my_pulls == 0:
				await _check_replay(result)
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
		# A test pull that moves the line to 110 m counts its distance.
		if entry.get("change", "") == "line_extended":
			_expect(not entry["full_pull"] and EventTexts.reach(entry).ends_with(" m"),
					"a test pull past 100 m counts its distance: %s" % entry)
	_expect(pullers.size() == PULLERS, "every puller pulled: %s" % pullers)
	await _frames(2)
	_expect(board.first_button.text == "Results", "the event is over")

	# The event's results, every puller, then the season's standings.
	_tap(board.first_button)
	await _frames(2)
	_expect(board.visible and board.row_count() == PULLERS, "the event's standings")
	# Every place's points, as the championship then counts them.
	var event_points := 0
	for line: Dictionary in event.get_standings():
		event_points += int(line["points"])
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
	_expect(points == event_points,
			"the results' points are the championship's: %d, %d" % [event_points, points])
	_expect(board.first_button.text == "Next event", "on to the next event")
	_expect(int(event.get_garage()["money_eur"]) == 100 + prize and event.get_garage()["can_buy"],
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
	await _check_move_up()

	print("flow test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)


# Tapping the pit board's lane opens the track screen over it, the pit
# board's buttons hidden under it: the lane lengthwise with the crew's
# report, the ground along the track from the start to the full-pull line
# (as prepared: the same grip as the track card's at the start), the
# spots listed, and the start slider where the pit board's is, which moves
# the sled's start (its bottom end full right); "Done" goes back.
func _check_track_screen(setup: SetupScreen, event: TractorPullEvent) -> void:
	var track := setup.track_screen
	var before := float(event.get_crew_report().get("start_y_m", 0.0))
	_tap(setup.lane_button)
	await _frames(2)
	_expect(track.visible and not setup.pull_button.is_visible_in_tree()
			and not setup.lane_button.is_visible_in_tree(), "the track screen over the pit board")
	var ground: Dictionary = event.get_event().get("track", {})
	var profile: Array = ground.get("profile", [])
	var full_m := float(event.get_crew_report().get("full_pull_m", 0.0))
	_expect(profile.size() >= 2 and float(profile[0]["at_m"]) == 0.0
			and float(profile[profile.size() - 1]["at_m"]) == full_m
			and float(profile[0]["grip"]) == float(ground.get("grip", -1.0)),
			"the ground along the track: %d points to %f" % [profile.size(), full_m])
	_expect(track.lane.lengthwise and track.lane.marks
			and track.lane.report.get("spots", []).size()
			== event.get_crew_report().get("spots", []).size(), "the crew's lane, lengthwise")
	_expect(is_equal_approx(track.start_slider.value, setup.start_slider.value)
			and is_equal_approx(setup.start_y_m(), before)
			and track.start_slider.end_value > track.start_slider.start_value,
			"the start where the pit board has it, full left at the top")
	# The ground in words: each stretch apart, by its most different point.
	var ground_along := []
	for i in 41:
		var at_m := 2.5 * i
		var soft := at_m >= 20.0 and at_m <= 30.0
		var firm := at_m >= 80.0
		ground_along.append({"at_m": at_m, "grip": 1.2,
				"cone_index_kpa": 1200.0 if soft else (4000.0 if firm else 2500.0)})
	var words: String = TrackScreen._along_words(ground_along)
	_expect(words == "A patch from about 20 to 30 m: softer; from about 80 m on: firmer.",
			"the ground's stretches in words: %s" % words)
	await _shot("1c_track")
	_slide_to_bottom(track.start_slider)
	await _frames(2)
	var right := float(event.get_crew_report().get("start_y_m", 0.0))
	_expect(right < 0.0 and is_equal_approx(setup.start_y_m(), right),
			"the track screen's start moves the sled: %f" % right)
	_tap(track.done_button)
	await _frames(2)
	_expect(not track.visible and setup.visible and setup.pull_button.is_visible_in_tree(),
			"back to the pit board")


# The hitch's slider sends 0 for the top hole, so that a drawbar bought
# later raises it, and a lower hole as its height (2 cm holes). Checked
# within one frame, then put back: the practice tractor's hitch is fixed.
func _check_hitch_slider(setup: SetupScreen) -> void:
	var slider := setup.hitch_slider
	var was_visible := slider.visible
	var was := [slider.start_value, slider.end_value, slider.value]
	slider.visible = true
	slider.start_value = 0.40
	slider.end_value = 0.50
	slider.value = 0.50
	var top: float = setup.call("_hitch_m")
	slider.value = 0.443
	var lower: float = setup.call("_hitch_m")
	slider.start_value = was[0]
	slider.end_value = was[1]
	slider.value = was[2]
	slider.visible = was_visible
	_expect(top == 0.0 and is_equal_approx(lower, 0.44),
			"the hitch's top hole is 0, a lower hole its height: %f, %f" % [top, lower])


# The crew chief names the gear the crew's trials liked and, with the hitch
# in its lowest hole and the weights halfway, says to raise it; one hole
# up, the weights all back, or a hitch with one hole (nothing bought to
# raise it), the warning goes. Shown for a tractor with a hitch to set,
# then the pit board put back as it was.
# The event's own report names a gear of the class's, once the trials are
# done.
func _check_crew_tip(setup: SetupScreen, event: TractorPullEvent) -> void:
	var tuning := {"hitch": true, "hitch_m": 0.40, "min_hitch_m": 0.40, "max_hitch_m": 0.50,
			"ballast": true, "ballast_position": 0.5}
	setup.show_setup("test", tuning, true, false, {"tip": "A fair track."}, "Test",
			{"gear": 5})
	var low := setup.tip_text()
	tuning["hitch_m"] = 0.42
	setup.show_values(tuning)
	var up := setup.tip_text()
	tuning["hitch_m"] = 0.40
	tuning["ballast_position"] = 0.0
	setup.show_values(tuning)
	var back := setup.tip_text()
	tuning["ballast_position"] = 0.5
	tuning["hitch"] = false
	tuning["max_hitch_m"] = 0.40
	setup.show_values(tuning)
	var one_hole := setup.tip_text()
	var crew := event.get_crew_report()
	setup.show_setup("put back", event.get_tuning(), false, false, {}, "", crew)
	_expect(low.contains("5th gear") and low.contains("Raise the hitch")
			and low.contains("fair track") and up.contains("5th gear")
			and not up.contains("Raise the hitch") and not back.contains("Raise the hitch")
			and not one_hole.contains("Raise the hitch"),
			"the crew chief's tip: %s | %s | %s | %s" % [low, up, back, one_hole])
	var gear := int(crew.get("gear", -1))
	_expect(gear == 0 or (gear >= 1 and gear <= 8), "the crew's gear: %d" % gear)


# The pump choice, from its pin: the spare pump bought (and fitted) with
# the money left, the stock one fitted again for nothing, and the spare one
# again; then everything is bought. The fitted alternative is picked out.
func _check_garage_choice(event: TractorPullEvent, garage: GarageScreen) -> void:
	_tap(garage.choice_pins[0])
	await _frames(2)
	var buttons := garage.alternative_buttons
	_expect(garage.view == GarageScreen.View.CHOICE and buttons.size() == 2
			and buttons[0].selected and not buttons[1].selected and not buttons[1].disabled,
			"the pumps on offer")
	_tap(buttons[1])
	await _frames(2)
	buttons = garage.alternative_buttons
	var spare := event.get_garage()
	var pump: Dictionary = spare["choices"][0]
	_expect(int(spare["money_eur"]) == 100 and int(pump["fitted"]) == 1
			and pump["alternatives"][1]["owned"] and spare["fully_upgraded"]
			and buttons[1].selected, "bought the spare pump: %s" % spare)
	await _shot("1d_garage_choice")
	_tap(buttons[0])
	await _frames(2)
	buttons = garage.alternative_buttons
	var stock := event.get_garage()
	_expect(int(stock["money_eur"]) == 100 and int(stock["choices"][0]["fitted"]) == 0
			and buttons[0].selected and buttons[1].text == "Fit · free",
			"the stock pump fitted again")
	_tap(buttons[1])
	await _frames(2)
	_expect(int(event.get_garage()["money_eur"]) == 100
			and int(event.get_garage()["choices"][0]["fitted"]) == 1, "and the spare one")
	_expect(event.buy_alternative(0, 1) == "owned already", "nothing more to buy")
	# The crew chief's advice: none where no alternative suits another
	# (the practice garage); for a final drive that suits the fitted tires,
	# its line, and to buy it first when it is not owned.
	_expect(event.get_garage().get("advice", {}).is_empty()
			and GarageScreen.crew_advice(event.get_garage()) == "", "no advice for the pumps")
	var advice := {"choice": 2, "alternative": 1, "label": "Tall final drive",
			"for": "Puller tires", "owned": false}
	_expect(GarageScreen.crew_advice({"advice": advice})
			== "Crew chief: with the puller tires, fit the tall final drive (buy it first).",
			"the crew chief's advice: %s" % GarageScreen.crew_advice({"advice": advice}))
	advice["owned"] = true
	_expect(GarageScreen.crew_advice({"advice": advice})
			== "Crew chief: with the puller tires, fit the tall final drive.", "advice, owned")


# A title opened the level above (a save that says so: a season won, and
# the app closed on the season's board): launched again, the career screen
# offers every class and level, the level above picked (the highest open)
# and the class above locked, with no going back (no season to go back
# to); starting there gives the level above's season with the money and
# upgrades carried up. Then the settings' "Start" (for testing a class)
# starts a new career a class up, and the career screen takes the player
# down a class and back up again: each class keeps its own tractor, the
# money one wallet.
func _check_move_up() -> void:
	var saved := FileAccess.get_file_as_string(SAVE_PATH)
	_expect(saved.contains("class = \"Practice\"") and saved.contains("unlocked_level = \"Belofte\""),
			"the save names the class and level: %s" % saved)
	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	file.store_string(saved.replace("unlocked_level = \"Belofte\"", "unlocked_level = \"Promo\""))
	file.close()
	var pull := await _launch(LADDER)
	var start: StartScreen = pull.get("start_screen")
	var event: TractorPullEvent = pull.get("event")
	# No season yet behind the career screen: the career's money.
	var money := int(event.get_career().get("money_eur", -1))
	var careers: CareerScreen = pull.get("career_screen")
	_expect(careers.visible and not start.visible and careers.picked_rung() == 1
			and not careers.back_button.visible and careers.level_buttons.size() == 3
			and not careers.level_buttons[1].disabled and careers.level_buttons[2].disabled,
			"the career screen, the level above picked, the class above locked")
	# What opens a level, from the career's promotion place.
	_expect(CareerScreen.places_words(1) == "1st" and CareerScreen.places_words(2) == "1st or 2nd"
			and CareerScreen.places_words(3) == "1st to 3rd"
			and CareerScreen.places_words(12) == "1st to 12th", "the places that open a level")
	await _shot("8_career")
	_tap(careers.start_button)
	await _frames(2)
	var career := event.get_career()
	_expect(career.get("rung", -1) == 1 and career.get("level_name", "") == "Promo"
			and not career.get("can_move_up", true), "moved up: %s" % career)
	_expect(start.visible and not start.move_up_button.visible
			and int(event.get_season().get("scored", -1)) == 0
			and int(event.get_event().get("index", -1)) == 0, "the level above's first event")
	_expect(money > 0 and int(event.get_garage().get("money_eur", -1)) == money,
			"the career carried up: %s" % event.get_garage())
	_expect(FileAccess.get_file_as_string(SAVE_PATH).contains("level = \"Promo\""),
			"saved on the level above")

	var settings: SettingsScreen = pull.get("settings_screen")
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.start_button)
	await _frames(1)
	_tap(settings.start_button)
	await _frames(1)
	_expect(settings.start_rung() == 2 and settings.start_button.text == "Start: 3",
			"the settings pick the third rung: %s" % settings.start_button.text)
	await _shot("9_start_at")
	_tap(settings.new_career_button)
	await _frames(1)
	_tap(settings.new_career_button)
	await _frames(2)
	career = event.get_career()
	_expect(career.get("rung", -1) == 2 and career.get("class_name", "") == "Practice, a class up"
			and int(event.get_garage().get("money_eur", -1)) == START_MONEY_EUR,
			"a new career a class up: %s" % career)
	await _check_keep_money(pull)
	await _check_class_round_trip(pull)


# For testing, a new career may keep the money there is: € 1000 added in
# the settings, "Money: keep", and a new career at the same rung starts
# with it, not the garage's start money; "Money: start" again after.
func _check_keep_money(pull: Node) -> void:
	var event: TractorPullEvent = pull.get("event")
	var start: StartScreen = pull.get("start_screen")
	var settings: SettingsScreen = pull.get("settings_screen")
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.money_button)
	await _frames(1)
	var money := int(event.get_garage().get("money_eur", -1))
	_expect(money == START_MONEY_EUR + SettingsScreen.TEST_MONEY_EUR, "money added: %d" % money)
	_expect(not settings.keeps_money() and settings.keep_money_button.text == "Money: start",
			"a new career starts with the start money by default")
	_tap(settings.keep_money_button)
	await _frames(1)
	_expect(settings.keeps_money() and settings.keep_money_button.text == "Money: keep",
			"the money kept: %s" % settings.keep_money_button.text)
	_tap(settings.new_career_button)
	await _frames(1)
	_tap(settings.new_career_button)
	await _frames(2)
	var career := event.get_career()
	_expect(career.get("rung", -1) == 2 and int(event.get_garage().get("money_eur", -1)) == money,
			"a new career with the money kept: %s" % event.get_garage().get("money_eur", -1))
	var bought := 0
	for part: Dictionary in event.get_garage()["upgrades"]:
		bought += int(part["level"])
	_expect(bought == 0, "and nothing bought")
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.keep_money_button)
	await _frames(1)
	_expect(not settings.keeps_money(), "back to the start money")
	_tap(settings.done_button)
	await _frames(2)


# A part bought a class up; the career screen (from a new season) to the
# class below, which has a new tractor of its own, the same money; a part
# bought there; then back up, where the tractor is as it was left, and the
# save keeps the other class's tractor.
func _check_class_round_trip(pull: Node) -> void:
	var event: TractorPullEvent = pull.get("event")
	var careers: CareerScreen = pull.get("career_screen")
	_expect(event.buy_upgrade(0).is_empty(), "a part bought a class up")
	var up := event.get_garage()
	pull.call("_choose_season")
	await _frames(2)
	_expect(careers.visible and careers.picked_rung() == 2 and careers.back_button.visible,
			"the career screen, the highest open picked")
	_tap(careers.level_buttons[0])
	await _frames(1)
	_expect(careers.picked_rung() == 0, "the class below picked")
	_tap(careers.start_button)
	await _frames(2)
	var career := event.get_career()
	var down := event.get_garage()
	_expect(career.get("class_name", "") == "Practice" and int(career.get("rung", -1)) == 0
			and int(career.get("unlocked", -1)) == 2, "down a class, the class above open: %s"
			% career)
	_expect(int(down["money_eur"]) == int(up["money_eur"]), "one wallet: %s" % down)
	var bought := 0
	for part: Dictionary in down["upgrades"]:
		bought += int(part["level"])
	_expect(bought == 0, "the class below's own tractor, new: %s" % down)
	var choices: Array = career.get("choices", [])
	_expect(choices.size() == 3 and String(choices[2]["tractor"]) == "yours"
			and int(choices[2]["stages_bought"]) == 1, "the class above's tractor kept: %s"
			% [choices])
	_expect(FileAccess.get_file_as_string(SAVE_PATH).contains("[[career.tractors]]"),
			"the save keeps the other class's tractor")
	pull.call("_choose_season")
	await _frames(2)
	_tap(careers.start_button)
	await _frames(2)
	_expect(event.get_career().get("rung", -1) == 2 and event.get_garage()["upgrades"]
			== up["upgrades"], "back up, the tractor as it was left: %s" % event.get_garage())


# The pull scene again, as at the next launch, on the same save; with
# another career file if `career` is given.
func _launch(career: String = CAREER) -> Node:
	_pull.queue_free()
	await _frames(2)
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("career_path", career)
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
	# New career on the launch screen asks first; New season then still
	# carries the career over.
	_tap(start.new_career_button)
	await _frames(2)
	_expect(start.continue_button.visible and start.new_career_button.text.begins_with("Sure")
			and event.get_garage()["upgrades"] == garage["upgrades"],
			"a new career at launch asks first")
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

	# The launch choice again: New career on the second tap starts over.
	start.show_choice(event.get_season())
	_tap(start.new_career_button)
	await _frames(1)
	_tap(start.new_career_button)
	await _frames(2)
	_check_new_career(event, start, "a new career at launch")

	# And from the settings, on the second tap, after buying a part again.
	_expect(event.buy_upgrade(0).is_empty(), "a part bought again")
	var settings: SettingsScreen = pull.get("settings_screen")
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.new_career_button)
	await _frames(2)
	_expect(settings.visible and int(event.get_garage()["upgrades"][0]["level"]) == 1,
			"the first tap only asks")
	_tap(settings.new_career_button)
	await _frames(2)
	_check_new_career(event, start, "a new career from the settings")
