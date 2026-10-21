# The game: a season's events in turn, one pull at a time. The start screen
# shows the next event (each on its own track) and the control layout; a
# pull runs until the run ends; the result screen shows how far the sled
# went, and offers the pull-off the rules give after a full pull, or the
# next event. The run (TractorPullRun, the C++ extension) holds the rules
# and the physics, and is stepped by each frame's time; this scene draws
# its blended snapshot and hands it the controls.
#
# The player drives with the touch controls (DriveControls; the keyboard
# and a gamepad for development), in the easy or the pro layout. With
# `autoplay` there are no screens: one pull of the tractor, sled and track
# below, the driver scripted, in gear at full throttle, then the clutch
# dropped.
extends Node3D

enum Screen { START, PULLING, RESULT }

@export var season_path := "res://data/seasons/club_season.toml"
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

var run := TractorPullRun.new()
var track_view := TrackView.new()
var tractor_view := TractorView.new()
var sled_view := SledView.new()
var chain_view := ChainView.new()
var cameras := CameraRig.new()
var controls := DriveControls.new()
var hud := Hud.new()
var start_screen := StartScreen.new()
var result_screen := ResultScreen.new()
var screen := Screen.START
# The season's event to pull, and the round there (0 qualifying, then the
# pull-offs).
var event_index := 0
var pull_off_round := 0
var _time_s := 0.0
# Since the run ended, while the pull's last moments stay in view.
var _ended_s := 0.0


func _ready() -> void:
	_add_world()
	for node: Node in [track_view, tractor_view, sled_view, chain_view, cameras]:
		add_child(node)
	var layer := CanvasLayer.new()
	for node: Node in [hud, controls, start_screen, result_screen]:
		layer.add_child(node)
	add_child(layer)
	controls.camera_requested.connect(cameras.next)
	controls.restart_requested.connect(_pull_again)
	controls.menu_requested.connect(func() -> void:
		pull_off_round = 0
		show_start())
	start_screen.layout_picked.connect(func(is_pro: bool) -> void:
		pro = is_pro
		show_start())
	start_screen.start_requested.connect(func() -> void: _set_screen(Screen.PULLING))
	result_screen.pull_off_requested.connect(func() -> void:
		pull_off_round = run.get_next_round()
		_pull_again())
	result_screen.next_requested.connect(func() -> void:
		event_index = (event_index + 1) % maxi(int(run.get_event().get("count", 1)), 1)
		pull_off_round = 0
		show_start())
	if autoplay:
		_prepare()
		_set_screen(Screen.PULLING)
	else:
		show_start()


# The start screen, with the run for the next pull set up behind it.
func show_start() -> void:
	var error := _prepare()
	start_screen.show_event(run.get_event(), pro, error)
	_set_screen(Screen.START)


# The pull, from the start line again: the same round, or the pull-off.
func _pull_again() -> void:
	if _prepare().is_empty():
		_set_screen(Screen.PULLING)


# Sets up the run for the pull to come and builds the views from it; ""
# or what went wrong.
func _prepare() -> String:
	var controls_path := pro_controls_path if pro else easy_controls_path
	var error: String
	if autoplay:
		error = run.setup(tractor_path, sled_path, track_path, controls_path)
	else:
		error = run.setup_event(season_path, event_index, controls_path, pull_off_round)
	if not error.is_empty():
		push_error("cannot set up the pull: " + error)
		return error
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
	return ""


func _set_screen(next: Screen) -> void:
	screen = next
	start_screen.visible = screen == Screen.START
	result_screen.visible = screen == Screen.RESULT
	controls.visible = screen == Screen.PULLING
	hud.visible = screen != Screen.START
	start_screen.release_buttons()
	result_screen.release_buttons()
	controls.reset_fingers()
	_ended_s = 0.0


func _process(delta: float) -> void:
	if screen == Screen.PULLING:
		_time_s += delta
		_read_controls()
		# The run alone turns the frame's time into its 1 ms steps.
		run.advance(int(delta * NS_PER_S))
		if run.is_finished() and not autoplay:
			_ended_s += delta
			if _ended_s >= RESULT_DELAY_S:
				result_screen.show_result(run.get_event(), run.get_result(), run.get_next_round())
				_set_screen(Screen.RESULT)
	_show(delta)
	cameras.follow(tractor_view.global_position, delta)


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
	hud.show_snapshot(s, run.get_event(), controls.pro)


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
