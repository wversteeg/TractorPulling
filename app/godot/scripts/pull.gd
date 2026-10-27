# The game: a season of events, each on its own track, against nine
# scripted rivals (TractorPullEvent, the C++ extension, holds them and the
# rules). The start screen shows the event to come; "Practice" is a free
# pull on its track, "Event" goes to the event's board, where the rivals'
# pulls and the sled operator's verdicts fill in as they are simulated, a
# few each frame (from the start screen on). Before every pull the setup
# screen sets the tractor (the weights and the rear tires' pressure, kept
# for the season). "Garage" spends the prize money each event pays on the
# tractor (the career: money and upgrades carry from season to season).
# The gear on the start screen opens the settings: the control layout and
# the steering (slider or tilt), kept on the device. When the player is due
# they pull (a run, TractorPullRun, stepped by each frame's time and drawn
# from its blended snapshot) or, after a test pull, keep it or pull again. After the
# event its top ten, then the season's; then the next event, or a new
# season after the last. The season is saved after every event (and the
# tuning on every pull): at the next launch the start screen offers to
# continue it, at the next event (an event left halfway starts again), or
# to start a new one.
#
# A pull is the player's whole turn (`whole_turn`): from their bay in the
# parking beside the track, past the sled and back up to it in reverse,
# the chain hooked on, the slack taken up while the official holds the red
# flag, the green flag and the pull (a second official down the track,
# walking ahead of the tractor, waves green while it is good and both wave
# red the moment it is over: stop; the light on the full-pull gantry and
# the HUD's show the same), the chain released by backing off,
# and back to the bay along the return lane while the sled drives itself
# back; the result screen comes once the tractor stands in its bay again.
# The rivals wait in the bays behind. Without it (and with `autoplay`), a
# pull from the start line, hooked on.
#
# The player drives with the touch controls (DriveControls; the keyboard
# and a gamepad for development), in the easy or the pro layout, steering
# with the slider or by tilting the phone. With
# `autoplay` there are no screens: one pull of the tractor, sled and track
# below, the driver scripted, in gear at full throttle, then the clutch
# dropped.
extends Node3D

enum Screen { START, SETTINGS, GARAGE, EVENT, SETUP, PULLING, RESULT, EVENT_END, SEASON }

@export var season_path := "res://data/seasons/club_season.toml"
# The season's draws: the rivals, the running orders and their pulls; -1
# for a new one at every launch.
@export var season_seed := -1
@export var easy_controls_path := "res://data/controls/easy.toml"
@export var pro_controls_path := "res://data/controls/pro.toml"
# Where the settings are kept; they set `pro` and `tilt` at the start
# (not with autoplay).
@export var settings_path := "user://settings.cfg"
# Where the season is saved (TractorPullEvent.get_save()); "" saves nothing.
@export var save_path := "user://season.toml"
@export var pro := false
@export var tilt := false
@export var autoplay := false
@export var whole_turn := true
@export var autoplay_gear := 6
@export var tractor_path := "res://data/tractors/farm_stock_light.toml"
@export var sled_path := "res://data/sleds/light_class.toml"
@export var track_path := "res://data/tracks/ideal_clay_sand.toml"

# How hard the cameras shake (CameraRig.rumble): for full throttle, and for
# each unit of the rear tires' slip ratio.
const RUMBLE_PER_THROTTLE := 0.4
const RUMBLE_PER_SLIP := 1.5
const SELECT_GEAR_S := 0.5
# The scripted driver drops the clutch a moment after selecting the gear:
# a gear goes in only with the clutch pressed.
const DROP_CLUTCH_S := 1.0
const NS_PER_S := 1_000_000_000.0
# How long the stopped sled stays in view before the result screen.
const RESULT_DELAY_S := 2.0
# The rivals' work each frame: slices of steps until this much time is
# spent, so the board keeps the frame rate on a phone. A slice is the least
# a frame runs: 500 steps are 25 ms even at the phone's speed budget (50 µs
# a step, docs/performance.md).
const RIVALS_SLICE_STEPS := 500
const RIVALS_BUDGET_US := 6000
const TOP := 10
# The rivals' tractors in their bays, painted their own colours (none like
# the player's red), and their drivers' shirts.
const RIVAL_PAINTS: Array[Color] = [Color(0.16, 0.42, 0.2), Color(0.16, 0.3, 0.62),
		Color(0.9, 0.45, 0.1)]
const RIVAL_SHIRTS: Array[Color] = [Color(0.85, 0.8, 0.2), Color(0.8, 0.8, 0.82),
		Color(0.2, 0.2, 0.22)]
# The officials stand on the left of the lane, in from the left chalk line
# [m]: one this far past the start line; the other down the track, from
# here up to the full-pull line, this far ahead of the tractor's rear axle.
const FLAGMAN_X_M := 9.0
const FLAGMAN_IN_M := 1.5
const END_FLAGMAN_FROM_M := 80.0
const END_FLAGMAN_AHEAD_M := 12.0
# Backing up to the sled, the cameras frame the hitch and the hook from
# this close [m], and how fast they turn to them and back [1/s].
const HITCH_FRAMING_M := 12.0
const FRAMING_RATE_PER_S := 2.0
# Off the hitch, the chain lies on the track this share of its length.
const CHAIN_LYING_SHARE := 0.9
# Where the stopping marker lies in front of the hook: a share of the
# chain's length, where the hitch hooks on.
const MARKER_SHARE := 0.6

var run := TractorPullRun.new()
var event := TractorPullEvent.new()
var track_view := TrackView.new()
var venue_view := VenueView.new()
var track_marks := TrackMarks.new()
var tractor_view := TractorView.new()
var sled_view := SledView.new()
var chain_view := ChainView.new()
var cameras := CameraRig.new()
var controls := DriveControls.new()
var hud := Hud.new()
var start_screen := StartScreen.new()
var board := BoardScreen.new()
var setup_screen := SetupScreen.new()
var settings_screen := SettingsScreen.new()
var garage_screen := GarageScreen.new()
var result_screen := ResultScreen.new()
var flagman := Flagman.new()
var end_flagman := Flagman.new()
var parked: Array[TractorView] = []
# The white mark on the track in front of the sled's hook: stop the hitch
# there and the chain goes on.
var hitch_marker := MeshInstance3D.new()
# For tests: a scripted driver, called each frame with the run's snapshot,
# returning [throttle, clutch_pedal, gear, steering, brake_left,
# brake_right] for the run in place of the touch controls.
var driver := Callable()
var screen := Screen.START
# The pull being pulled is a practice pull (or autoplay's), not the event's.
var practice := false
# The season's next event is started and waits behind the start screen.
var _event_ready := false
var _title := ""
var _shown_log := -1
var _shown_turn := ""
var _time_s := 0.0
# Where the officials stand: the full-pull line, and in from the left
# chalk line [m].
var _full_pull_m := 100.0
var _left_m := 4.5
# Set for each run in _build_turn: whether it is the whole turn, and the
# chain's length [m].
var _whole_turn := false
var _chain_m := 1.0
# The latest snapshot (_show), and the cameras' eased offset from the
# tractor (_camera_target).
var _snapshot := {}
var _framing := Vector3.ZERO
# Since the run ended, while the pull's last moments stay in view.
var _ended_s := 0.0


func _ready() -> void:
	_add_world()
	for node: Node in [venue_view, track_view, track_marks, tractor_view, sled_view, chain_view,
			flagman, end_flagman, hitch_marker, cameras]:
		add_child(node)
	var marker := BoxMesh.new()
	marker.size = Vector3(1.2, 0.02, 0.15)
	hitch_marker.mesh = marker
	hitch_marker.material_override = Art.material(Color(0.95, 0.95, 0.9), 0.9)
	var layer := CanvasLayer.new()
	for node: Node in [hud, controls, start_screen, settings_screen, garage_screen, board,
			setup_screen, result_screen]:
		layer.add_child(node)
	add_child(layer)
	controls.camera_requested.connect(cameras.next)
	controls.restart_requested.connect(func() -> void:
		if autoplay:
			_start_autoplay()
		elif practice:
			_start_practice())
	controls.menu_requested.connect(func() -> void:
		if practice and not autoplay:
			show_start())
	start_screen.practice_requested.connect(func() -> void: _show_setup(true))
	start_screen.settings_requested.connect(func() -> void:
		settings_screen.show_settings(pro, tilt, controls.tilt_steering.available())
		_set_screen(Screen.SETTINGS))
	settings_screen.changed.connect(_change_settings)
	settings_screen.done.connect(show_start)
	start_screen.garage_requested.connect(func() -> void:
		garage_screen.show_garage(event.get_garage())
		_set_screen(Screen.GARAGE))
	garage_screen.buy_requested.connect(func(index: int) -> void:
		var error := event.buy_upgrade(index)
		if error.is_empty():
			_save_season()
		garage_screen.show_garage(event.get_garage(),
				"" if error.is_empty() else "Cannot buy it: %s." % error))
	garage_screen.done.connect(show_start)
	setup_screen.tuning_changed.connect(func(ballast_position: float, bar: float) -> void:
		event.set_tuning(ballast_position, bar)
		setup_screen.show_values(event.get_tuning()))
	setup_screen.pull_requested.connect(func() -> void:
		_save_season()
		if practice:
			_start_practice()
		else:
			_start_event_pull())
	setup_screen.back_requested.connect(func() -> void:
		if practice:
			show_start()
		else:
			_set_screen(Screen.EVENT))
	start_screen.event_requested.connect(func() -> void: _set_screen(Screen.EVENT))
	start_screen.continue_requested.connect(show_start)
	start_screen.new_season_requested.connect(_new_season)
	board.first_tapped.connect(_on_board_first)
	board.second_tapped.connect(_on_board_second)
	result_screen.again_requested.connect(_start_practice)
	result_screen.menu_requested.connect(show_start)
	result_screen.continue_requested.connect(func() -> void: _set_screen(Screen.EVENT))
	if autoplay:
		_start_autoplay()
	else:
		var settings := GameSettings.read(settings_path)
		pro = settings.pro
		tilt = settings.tilt
		if not _resume_season():
			_new_season()


func _start_autoplay() -> void:
	practice = true
	run.clear_parking()
	if _build(run.setup(tractor_path, sled_path, track_path, _controls_path())):
		_set_screen(Screen.PULLING)


# The settings picked, kept on the device for the next launch.
func _change_settings(is_pro: bool, by_tilt: bool) -> void:
	pro = is_pro
	tilt = by_tilt
	var settings := GameSettings.new()
	settings.pro = pro
	settings.tilt = tilt
	var error := settings.write(settings_path)
	if error != OK:
		push_warning("cannot keep the settings: %s" % error_string(error))


func _controls_path() -> String:
	return pro_controls_path if pro else easy_controls_path


func _new_season() -> void:
	var seed := season_seed if season_seed >= 0 else randi()
	var error := event.start_season(season_path, seed)
	_event_ready = false
	if not error.is_empty():
		push_error("cannot start the season: " + error)
	else:
		# One line on the console, which CI's Windows check looks for: the
		# extension and the data loaded.
		var season := event.get_season()
		print("A season of %d events, %d pullers, seed %d" % [season["events"],
				PackedStringArray(season["names"]).size(), seed])
		_save_season()
	show_start(error)


# The season saved at `save_path`, picked up again and offered on the start
# screen; false if there is none (or it cannot be read, or it is over, or
# it has no event pulled yet: nothing to lose then). Its career carries
# into a new season either way.
func _resume_season() -> bool:
	if save_path.is_empty() or not FileAccess.file_exists(save_path):
		return false
	var error := event.resume_season(season_path, FileAccess.get_file_as_string(save_path))
	if not error.is_empty():
		push_warning("cannot continue the saved season: " + error)
		return false
	var season := event.get_season()
	if season.get("over", true) or int(season.get("scored", 0)) == 0:
		return false
	_event_ready = false
	start_screen.show_choice(season)
	_set_screen(Screen.START)
	return true


# Saves the season so far, with the career, for the next launch. A season
# that is over is kept for its career: the next launch starts a new season
# with it.
func _save_season() -> void:
	if save_path.is_empty() or autoplay:
		return
	# Written beside the save, then renamed over it: an app killed mid-write
	# (Android does that to apps in the background) leaves the old save whole.
	var written := save_path + ".tmp"
	var file := FileAccess.open(written, FileAccess.WRITE)
	if file == null:
		push_warning("cannot save the season: %s" % error_string(FileAccess.get_open_error()))
		return
	file.store_string(event.get_save())
	file.close()
	var error := DirAccess.rename_absolute(ProjectSettings.globalize_path(written),
			ProjectSettings.globalize_path(save_path))
	if error != OK:
		push_warning("cannot save the season: %s" % error_string(error))


# The start screen, with the season's next event started behind it.
func show_start(error: String = "") -> void:
	if error.is_empty() and not _event_ready:
		error = event.start_event()
		_event_ready = error.is_empty()
	if not error.is_empty():
		push_error("cannot start the event: " + error)
	start_screen.show_event(event.get_event(), error)
	_set_screen(Screen.START)


# The setup screen before a pull: the tractor's tuning. For
# a practice pull (`for_practice`) or the event's pull that is due.
func _show_setup(for_practice: bool) -> void:
	practice = for_practice
	var info := event.get_event()
	var which := "Practice" if practice else EventTexts.pull_name(info.get("pull_kind", ""),
			info.get("pull_off_round", 0))
	setup_screen.show_setup("%s  ·  %s" % [which, info.get("name", "")], event.get_tuning(), pro,
			tilt and controls.tilt_steering.available())
	_set_screen(Screen.SETUP)


# The next run is the whole turn from the player's bay, or the pull alone.
func _place_parking() -> void:
	if whole_turn and not autoplay:
		run.set_parking(VenueView.PLAYER_BAY_X_M, VenueView.PARKING_RIGHT_OF_LANE_M)
	else:
		run.clear_parking()


func _start_practice() -> void:
	practice = true
	_title = "Practice  ·  %s" % event.get_event().get("name", "")
	_place_parking()
	if _build(run.setup_practice(event, _controls_path())):
		_set_screen(Screen.PULLING)


func _start_event_pull() -> void:
	var info := event.get_event()
	practice = false
	_title = "%s  ·  %s" % [EventTexts.pull_name(info.get("pull_kind", ""),
			info.get("pull_off_round", 0)), info.get("name", "")]
	_place_parking()
	if _build(run.setup_event_pull(event, _controls_path())):
		_set_screen(Screen.PULLING)


# Builds the views for the run just set up, if `error` is ""; false if not.
func _build(error: String) -> bool:
	if not error.is_empty():
		push_error("cannot set up the pull: " + error)
		return false
	var dimensions := run.get_dimensions()
	for child in track_view.get_children():
		child.queue_free()
	track_view.build(dimensions)
	venue_view.build(dimensions)
	# A new pull on a prepared track: the last pull's marks are gone.
	track_marks.build(dimensions)
	# The tractor moves its weights to the tuning on every pull; the rest of
	# it, and the sled, are built once.
	tractor_view.build(dimensions)
	if sled_view.get_child_count() == 0:
		sled_view.build(dimensions)
	hud.build(dimensions)
	_build_turn(dimensions)
	controls.gear_count = dimensions["gear_count"]
	controls.has_reverse = dimensions.get("has_reverse", false)
	controls.set_layout(run.get_layout() == "pro")
	controls.set_steering(tilt)
	controls.set_clutch_range(dimensions["clutch_full_grip_pedal"],
			dimensions["clutch_bite_pedal"])
	controls.reset()
	_time_s = 0.0
	_show(0.0)
	_framing = Vector3.ZERO
	cameras.follow(tractor_view.global_position, 0.0, false)
	return true


# The parking, its rivals and the official, for a whole turn.
func _build_turn(dimensions: Dictionary) -> void:
	var lane_m: float = dimensions["lane_width_m"]
	var turn: bool = run.get_snapshot().get("stage", "pulling") != "pulling"
	_whole_turn = turn
	_chain_m = dimensions["chain_length_m"]
	var bay_y_m := -(lane_m / 2.0 + VenueView.PARKING_RIGHT_OF_LANE_M)
	# Built for the first whole turn only: a pull from the line has none.
	if parked.is_empty() and turn:
		for i in VenueView.RIVAL_BAYS_X_M.size():
			var rival := TractorView.new()
			rival.paint = RIVAL_PAINTS[i % RIVAL_PAINTS.size()]
			rival.paint_dark = rival.paint.darkened(0.4)
			rival.driver_shirt = RIVAL_SHIRTS[i % RIVAL_SHIRTS.size()]
			add_child(rival)
			rival.build(dimensions)
			parked.append(rival)
	for i in parked.size():
		parked[i].position = TrackSpace.point(VenueView.RIVAL_BAYS_X_M[i], bay_y_m, 0.0)
		parked[i].visible = turn
	flagman.stand(TrackSpace.point(FLAGMAN_X_M, lane_m / 2.0 - FLAGMAN_IN_M, 0.0),
			TrackSpace.point(0.0, 0.0, 0.0))
	_full_pull_m = dimensions["full_pull_line_m"]
	_left_m = lane_m / 2.0 - FLAGMAN_IN_M
	hud.set_parking(Vector2(VenueView.PLAYER_BAY_X_M, bay_y_m) if turn else Vector2.INF)


func _set_screen(next: Screen) -> void:
	screen = next
	start_screen.visible = screen == Screen.START
	settings_screen.visible = screen == Screen.SETTINGS
	garage_screen.visible = screen == Screen.GARAGE
	setup_screen.visible = screen == Screen.SETUP
	board.visible = screen in [Screen.EVENT, Screen.EVENT_END, Screen.SEASON]
	result_screen.visible = screen == Screen.RESULT
	controls.visible = screen == Screen.PULLING
	hud.visible = screen in [Screen.PULLING, Screen.RESULT]
	# Again and menu only for practice: an event's pull counts.
	controls.restart_button.visible = practice
	controls.menu_button.visible = practice and not autoplay
	start_screen.release_buttons()
	settings_screen.release_buttons()
	garage_screen.release_buttons()
	setup_screen.release_buttons()
	setup_screen.release_sliders()
	board.release_buttons()
	result_screen.release_buttons()
	controls.reset_fingers()
	_ended_s = 0.0
	_shown_log = -1
	_shown_turn = ""
	match screen:
		Screen.EVENT:
			_show_event()
		Screen.EVENT_END:
			_show_event_end()
		Screen.SEASON:
			_show_season()


func _process(delta: float) -> void:
	if screen == Screen.PULLING:
		_time_s += delta
		_read_controls()
		# The run alone turns the frame's time into its 1 ms steps.
		run.advance(int(delta * NS_PER_S))
		# Over once the pull has ended at the line, or the tractor is back
		# in its bay after a whole turn.
		if run.is_over() and not autoplay:
			_ended_s += delta
			if _ended_s >= RESULT_DELAY_S:
				_show_result()
	elif screen in [Screen.START, Screen.SETTINGS, Screen.GARAGE, Screen.EVENT, Screen.SETUP]:
		# The rivals get on with their work (their gears first) while the
		# player reads the start screen too.
		_advance_rivals()
		if screen == Screen.EVENT:
			_show_event()
	_show(delta)
	cameras.follow(_camera_target(delta), delta)


# What the cameras follow: the tractor, or backing up to the sled, the
# middle between its hitch and the sled's hook, so both are in view. The
# cameras turn from one to the other over a moment, not in one frame: the
# offset from the tractor is eased, so the tractor itself is not lagged.
func _camera_target(delta: float) -> Vector3:
	var wanted := Vector3.ZERO
	if _snapshot.get("stage", "") == "to_the_sled" \
			and float(_snapshot["hitch_to_hook_m"]) < HITCH_FRAMING_M:
		var middle := (tractor_view.hitch_position() + sled_view.hook_position()) / 2.0
		wanted = middle - tractor_view.global_position
	_framing = _framing.lerp(wanted, 1.0 - exp(-FRAMING_RATE_PER_S * delta))
	return tractor_view.global_position + _framing


# The rivals' work, a slice at a time until the frame's budget is spent.
func _advance_rivals() -> void:
	var begun := Time.get_ticks_usec()
	# advance() runs a whole slice unless the player is due or the event
	# is over.
	while event.advance(RIVALS_SLICE_STEPS) == RIVALS_SLICE_STEPS:
		if Time.get_ticks_usec() - begun >= RIVALS_BUDGET_US:
			return


func _show_result() -> void:
	var entry := {}
	if not practice:
		entry = event.record_pull(run)
	var full_pull_m: float = run.get_dimensions()["full_pull_line_m"]
	result_screen.show_result(_title, run.get_result(), full_pull_m, entry)
	_set_screen(Screen.RESULT)


# The event's board: the pulls so far, and what is up next.
func _show_event() -> void:
	var info := event.get_event()
	var turn: String = info.get("turn", "")
	var size := event.get_log_size()
	if size == _shown_log and turn == _shown_turn:
		return
	_shown_log = size
	_shown_turn = turn
	board.set_title("%s  ·  event %d of %d" % [info.get("name", ""),
			int(info.get("index", 0)) + 1, info.get("count", 0)])
	var rows: Array[PackedStringArray] = []
	var mine := PackedInt32Array()
	for entry: Dictionary in event.get_log(0):
		if int(entry.get("puller", entry.get("deciding", -1))) == 0:
			mine.append(rows.size())
		rows.append(EventTexts.log_row(entry))
	board.set_rows(rows, mine)
	match turn:
		"rivals":
			board.set_status("The rivals choose their gears..." if size == 0
					else "The rivals pull...")
			board.set_buttons("")
		"player_pull":
			board.set_status("Your turn: %s" % EventTexts.pull_name(info.get("pull_kind", ""),
					info.get("pull_off_round", 0)).to_lower())
			board.set_buttons("Pull")
		"player_decision":
			board.set_status("The sled has changed. Keep your test pull, or pull again? "
					+ "(Most would %s.)" % ("keep it" if event.get_suggested_keep()
					else "pull again"))
			board.set_buttons("Keep", "Pull again")
		"finished":
			board.set_status("The event is over.")
			board.set_buttons("Results")
		_:
			board.set_status("The event cannot go on.")
			board.set_buttons("Menu")


func _show_event_end() -> void:
	var info := event.get_event()
	board.set_title("%s  ·  top %d" % [info.get("name", ""), TOP])
	var rows: Array[PackedStringArray] = []
	var mine := PackedInt32Array()
	var place := 0
	for line: Dictionary in event.get_standings():
		if int(line["puller"]) == 0:
			place = line["place"]
			if rows.size() < TOP:
				mine.append(rows.size())
		if rows.size() < TOP:
			rows.append(EventTexts.standing_row(line))
	board.set_rows(rows, mine)
	var prize := event.get_event_prize()
	board.set_status("You placed %d of %d%s." % [place, info.get("pullers", 0),
			" and won %s" % GarageScreen.euros(prize) if prize > 0 else ""])
	board.set_buttons("Season")


func _show_season() -> void:
	event.score_event()
	_save_season()
	_event_ready = false
	var season := event.get_season()
	var over: bool = season.get("over", false)
	board.set_title("The season%s  ·  top %d" % [" is over" if over
			else " after event %d of %d" % [season.get("scored", 0), season.get("events", 0)],
			TOP])
	var rows: Array[PackedStringArray] = []
	var mine := PackedInt32Array()
	var standings := event.get_season_standings()
	for line: Dictionary in standings.slice(0, TOP):
		if int(line["puller"]) == 0:
			mine.append(rows.size())
		rows.append(EventTexts.season_row(line))
	board.set_rows(rows, mine)
	var status := ""
	if over and not standings.is_empty():
		var champion: Dictionary = standings[0]
		status = "You are the champion!" if int(champion["puller"]) == 0 \
				else "%s is the champion." % champion["name"]
	board.set_status(status)
	board.set_buttons("New season" if over else "Next event")


func _on_board_first() -> void:
	match screen:
		Screen.EVENT:
			match event.get_event().get("turn", ""):
				"player_pull":
					_show_setup(false)
				"player_decision":
					event.decide(true)
				"finished":
					_set_screen(Screen.EVENT_END)
				"failed":
					_new_season()
		Screen.EVENT_END:
			_set_screen(Screen.SEASON)
		Screen.SEASON:
			if event.get_season().get("over", false):
				_new_season()
			else:
				show_start()


func _on_board_second() -> void:
	if screen == Screen.EVENT and event.get_event().get("turn", "") == "player_decision":
		event.decide(false)


func _read_controls() -> void:
	if driver.is_valid():
		var c: Array = driver.call(run.get_snapshot())
		run.set_controls(c[0], c[1], c[2], c[3], c[4], c[5])
		return
	if autoplay:
		var go := _time_s >= SELECT_GEAR_S
		# The easy layout's assist ignores the clutch.
		var clutch := 0.0 if _time_s >= DROP_CLUTCH_S else 1.0
		run.set_controls(1.0 if go else 0.0, clutch, autoplay_gear if go else 0, 0.0, 0.0, 0.0)
		return
	# The easy layout's run ignores the clutch and the brakes: its assist
	# works them.
	run.set_controls(controls.throttle(), controls.clutch(), controls.gear, controls.steering(),
			controls.brake_left(), controls.brake_right())


func _show(delta: float) -> void:
	var s := run.get_snapshot()
	_snapshot = s
	if s.is_empty():
		return
	controls.lowest_gear = s["lowest_gear"]
	tractor_view.show_snapshot(s, delta)
	sled_view.show_snapshot(s, delta)
	venue_view.show_distance(s["sled_x_m"])
	track_marks.show_snapshot(s)
	# The cameras shake with the work: the throttle, more as the tires slip.
	cameras.rumble = clampf(RUMBLE_PER_THROTTLE * float(s["throttle"])
			+ RUMBLE_PER_SLIP * absf(float(s["slip_ratio"])), 0.0, 1.0) \
			if bool(s["engine_running"]) else 0.0
	var hook := sled_view.hook_position()
	if s.get("chain_hooked", true):
		chain_view.show_between(tractor_view.hitch_position(), hook, s["chain_tension_n"])
	else:
		# Off the hitch, it lies on the track in front of the hook.
		var ahead := TrackSpace.point(cos(float(s["sled_heading_rad"])),
				sin(float(s["sled_heading_rad"])), 0.0)
		chain_view.show_between(Vector3(hook.x, 0.03, hook.z) + ahead * _chain_m * CHAIN_LYING_SHARE,
				hook, 0.0)
	var stage: String = s.get("stage", "pulling")
	_show_officials(s, delta)
	hitch_marker.visible = stage == "to_the_sled"
	if hitch_marker.visible:
		var forward := TrackSpace.point(cos(float(s["sled_heading_rad"])),
				sin(float(s["sled_heading_rad"])), 0.0)
		hitch_marker.position = Vector3(hook.x, 0.01, hook.z) + forward * _chain_m * MARKER_SHARE
		hitch_marker.rotation.y = float(s["sled_heading_rad"])
	hud.show_snapshot(s, _title, controls.pro)


# The officials' signal from the run's stage: "green" while the pull is
# on and good, "red" while the tractor must wait (taking up the slack,
# standing ready for the flag) or stop (the pull is over: until the chain is off), "off" otherwise.
# `turn` says the run is the whole turn; at the line it ends with the pull.
static func signal_for(s: Dictionary, turn: bool) -> String:
	match s.get("stage", "pulling"):
		"pulling":
			return "green"
		"taking_up_slack", "waiting_for_the_flag", "releasing":
			return "red"
		"parked":
			return "off" if turn else "red"
	return "off"


# Both officials, the gantry's light and the HUD's, from the snapshot.
func _show_officials(s: Dictionary, delta: float) -> void:
	var stage: String = s.get("stage", "pulling")
	var shown := signal_for(s, _whole_turn)
	var stop := shown == "red" and stage not in ["taking_up_slack", "waiting_for_the_flag"]
	# The start official: the red flag low, raised while the tractor stands
	# ready, green waved for the pull, red waved to stop.
	var raise := 0.0
	match stage:
		"waiting_for_the_flag":
			raise = float(s.get("flag_progress", 0.0))
		"pulling":
			raise = 1.0
	if stop:
		raise = 1.0
	flagman.show_signal(shown == "green", raise, stage == "pulling" or stop, delta)
	# The official down the track walks ahead of the tractor, from 80 m up
	# to the full-pull line, the green flag up while the pull is good.
	var at_m := clampf(float(s["tractor_x_m"]) + END_FLAGMAN_AHEAD_M,
			minf(END_FLAGMAN_FROM_M, _full_pull_m), _full_pull_m)
	end_flagman.stand(TrackSpace.point(at_m, _left_m, 0.0),
			TrackSpace.point(float(s["tractor_x_m"]), 0.0, 0.0))
	end_flagman.show_signal(shown == "green", 1.0 if shown != "off" else 0.0, stop, delta)
	venue_view.show_signal(shown)
	hud.show_signal(shown)


func _add_world() -> void:
	add_child(VenueView.environment())
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-48.0), deg_to_rad(35.0), 0.0)
	sun.light_energy = 1.2
	sun.shadow_enabled = true
	# Shadows only near the camera, where the tractor and the sled are.
	sun.directional_shadow_max_distance = 60.0
	add_child(sun)
