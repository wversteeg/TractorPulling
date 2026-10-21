# The game: a season of events, each on its own track, against nine
# scripted rivals (TractorPullEvent, the C++ extension, holds them and the
# rules). The start screen shows the event to come and the control layout;
# "Practice" is a free pull on its track, "Event" goes to the event's
# board, where the rivals' pulls and the sled operator's verdicts fill in
# as they are simulated, a few each frame (from the start screen on). When the player is due they
# pull (a run, TractorPullRun, stepped by each frame's time and drawn from
# its blended snapshot) or, after a test pull, keep it or pull again. After
# the event its top ten, then the season's; then the next event, or a new
# season after the last. Nothing is saved: every launch is a new season.
#
# The player drives with the touch controls (DriveControls; the keyboard
# and a gamepad for development), in the easy or the pro layout. With
# `autoplay` there are no screens: one pull of the tractor, sled and track
# below, the driver scripted, in gear at full throttle, then the clutch
# dropped.
extends Node3D

enum Screen { START, EVENT, PULLING, RESULT, EVENT_END, SEASON }

@export var season_path := "res://data/seasons/club_season.toml"
# The season's draws: the rivals, the running orders and their pulls; -1
# for a new one at every launch.
@export var season_seed := -1
@export var easy_controls_path := "res://data/controls/easy.toml"
@export var pro_controls_path := "res://data/controls/pro.toml"
@export var pro := false
@export var autoplay := false
@export var autoplay_gear := 6
@export var tractor_path := "res://data/tractors/farm_stock_light.toml"
@export var sled_path := "res://data/sleds/light_class.toml"
@export var track_path := "res://data/tracks/ideal_clay_sand.toml"

const SELECT_GEAR_S := 0.5
# The scripted driver drops the clutch a moment after selecting the gear:
# a gear goes in only with the clutch pressed.
const DROP_CLUTCH_S := 1.0
const NS_PER_S := 1_000_000_000.0
# How long the stopped sled stays in view before the result screen.
const RESULT_DELAY_S := 2.0
# The rivals' work each frame: slices of steps until this much time is
# spent, so the board keeps the frame rate on a phone.
const RIVALS_SLICE_STEPS := 2000
const RIVALS_BUDGET_US := 6000
const TOP := 10

var run := TractorPullRun.new()
var event := TractorPullEvent.new()
var track_view := TrackView.new()
var tractor_view := TractorView.new()
var sled_view := SledView.new()
var chain_view := ChainView.new()
var cameras := CameraRig.new()
var controls := DriveControls.new()
var hud := Hud.new()
var start_screen := StartScreen.new()
var board := BoardScreen.new()
var result_screen := ResultScreen.new()
var screen := Screen.START
# The pull being pulled is a practice pull (or autoplay's), not the event's.
var practice := false
# The season's next event is started and waits behind the start screen.
var _event_ready := false
var _title := ""
var _shown_log := -1
var _shown_turn := ""
var _time_s := 0.0
# Since the run ended, while the pull's last moments stay in view.
var _ended_s := 0.0


func _ready() -> void:
	_add_world()
	for node: Node in [track_view, tractor_view, sled_view, chain_view, cameras]:
		add_child(node)
	var layer := CanvasLayer.new()
	for node: Node in [hud, controls, start_screen, board, result_screen]:
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
	start_screen.layout_picked.connect(func(is_pro: bool) -> void:
		pro = is_pro
		show_start())
	start_screen.practice_requested.connect(_start_practice)
	start_screen.event_requested.connect(func() -> void: _set_screen(Screen.EVENT))
	board.first_tapped.connect(_on_board_first)
	board.second_tapped.connect(_on_board_second)
	result_screen.again_requested.connect(_start_practice)
	result_screen.menu_requested.connect(show_start)
	result_screen.continue_requested.connect(func() -> void: _set_screen(Screen.EVENT))
	if autoplay:
		_start_autoplay()
	else:
		_new_season()


func _start_autoplay() -> void:
	practice = true
	if _build(run.setup(tractor_path, sled_path, track_path, _controls_path())):
		_set_screen(Screen.PULLING)


func _controls_path() -> String:
	return pro_controls_path if pro else easy_controls_path


func _new_season() -> void:
	var seed := season_seed if season_seed >= 0 else randi()
	var error := event.start_season(season_path, seed)
	_event_ready = false
	if not error.is_empty():
		push_error("cannot start the season: " + error)
	show_start(error)


# The start screen, with the season's next event started behind it.
func show_start(error: String = "") -> void:
	if error.is_empty() and not _event_ready:
		error = event.start_event()
		_event_ready = error.is_empty()
	if not error.is_empty():
		push_error("cannot start the event: " + error)
	start_screen.show_event(event.get_event(), pro, error)
	_set_screen(Screen.START)


func _start_practice() -> void:
	practice = true
	_title = "Practice  ·  %s" % event.get_event().get("name", "")
	if _build(run.setup_practice(event, _controls_path())):
		_set_screen(Screen.PULLING)


func _start_event_pull() -> void:
	var info := event.get_event()
	practice = false
	_title = "%s  ·  %s" % [EventTexts.pull_name(info.get("pull_kind", ""),
			info.get("pull_off_round", 0)), info.get("name", "")]
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
	if tractor_view.get_child_count() == 0:
		tractor_view.build(dimensions)
		sled_view.build(dimensions)
	hud.build(dimensions)
	controls.gear_count = dimensions["gear_count"]
	controls.set_layout(run.get_layout() == "pro")
	controls.reset()
	_time_s = 0.0
	_show(0.0)
	cameras.follow(tractor_view.global_position, 0.0, false)
	return true


func _set_screen(next: Screen) -> void:
	screen = next
	start_screen.visible = screen == Screen.START
	board.visible = screen in [Screen.EVENT, Screen.EVENT_END, Screen.SEASON]
	result_screen.visible = screen == Screen.RESULT
	controls.visible = screen == Screen.PULLING
	hud.visible = screen in [Screen.PULLING, Screen.RESULT]
	# Again and menu only for practice: an event's pull counts.
	controls.restart_button.visible = practice
	controls.menu_button.visible = practice and not autoplay
	start_screen.release_buttons()
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
		if run.is_finished() and not autoplay:
			_ended_s += delta
			if _ended_s >= RESULT_DELAY_S:
				_show_result()
	elif screen == Screen.START or screen == Screen.EVENT:
		# The rivals get on with their work (their gears first) while the
		# player reads the start screen too.
		_advance_rivals()
		if screen == Screen.EVENT:
			_show_event()
	_show(delta)
	cameras.follow(tractor_view.global_position, delta)


# The rivals' work, a slice at a time until the frame's budget is spent.
func _advance_rivals() -> void:
	var begun := Time.get_ticks_usec()
	while event.get_event().get("turn", "") == "rivals":
		event.advance(RIVALS_SLICE_STEPS)
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
	board.set_status("You placed %d of %d." % [place, info.get("pullers", 0)])
	board.set_buttons("Season")


func _show_season() -> void:
	event.score_event()
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
					_start_event_pull()
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
	if s.is_empty():
		return
	tractor_view.show_snapshot(s, delta)
	sled_view.show_snapshot(s)
	chain_view.show_between(tractor_view.hitch_position(), sled_view.hook_position(),
			s["chain_tension_n"])
	hud.show_snapshot(s, _title, controls.pro)


func _add_world() -> void:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color(0.55, 0.7, 0.9)
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.6, 0.6, 0.6)
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50.0), deg_to_rad(30.0), 0.0)
	sun.shadow_enabled = true
	add_child(sun)
