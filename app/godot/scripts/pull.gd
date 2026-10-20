# One pull, drawn: the run (TractorPullRun, the C++ extension) stepped by
# each frame's time, and the track, tractor, sled and chain placed from its
# blended snapshot. The game rules all live in the run; this only shows it
# and hands it the controls.
#
# Development controls (the touch controls come next): W throttle, A and D
# steer, 0 to 8 select the gear, C switches the camera, R starts again.
# With `autoplay` the driver is scripted instead: in gear, full throttle.
extends Node3D

@export var tractor_path := "res://data/tractors/farm_stock_light.toml"
@export var sled_path := "res://data/sleds/light_class.toml"
@export var track_path := "res://data/tracks/ideal_clay_sand.toml"
@export var easy_controls_path := "res://data/controls/easy.toml"
@export var autoplay := false
@export var autoplay_gear := 6

# The development keys' steering: half the travel, the wheels at half lock.
const KEY_STEERING := 0.3
const SELECT_GEAR_S := 0.5
const NS_PER_S := 1_000_000_000.0
const RPM_PER_RAD_S := 60.0 / TAU
const KMH_PER_M_S := 3.6

var run := TractorPullRun.new()
var track_view := TrackView.new()
var tractor_view := TractorView.new()
var sled_view := SledView.new()
var chain_view := ChainView.new()
var cameras := CameraRig.new()
var _hud := Label.new()
var _gear := 0
var _time_s := 0.0


func _ready() -> void:
	_add_world()
	add_child(track_view)
	add_child(tractor_view)
	add_child(sled_view)
	add_child(chain_view)
	add_child(cameras)
	var hud_layer := CanvasLayer.new()
	_hud.position = Vector2(16, 16)
	hud_layer.add_child(_hud)
	add_child(hud_layer)
	start()


# A new run from the start line.
func start() -> void:
	var error := run.setup(tractor_path, sled_path, track_path, easy_controls_path)
	if not error.is_empty():
		push_error("cannot start the run: " + error)
		_hud.text = error
		return
	var dimensions := run.get_dimensions()
	for child in track_view.get_children():
		child.queue_free()
	track_view.build(dimensions)
	if tractor_view.get_child_count() == 0:
		tractor_view.build(dimensions)
		sled_view.build(dimensions)
	_gear = 0
	_time_s = 0.0
	_show(0.0)
	cameras.follow(tractor_view.global_position, 0.0, false)


func _process(delta: float) -> void:
	_time_s += delta
	_read_controls()
	# The run alone turns the frame's time into its 1 ms steps.
	run.advance(int(delta * NS_PER_S))
	_show(delta)
	cameras.follow(tractor_view.global_position, delta)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	if key.keycode == KEY_C:
		cameras.next()
	elif key.keycode == KEY_R:
		start()
	elif key.keycode >= KEY_0 and key.keycode <= KEY_8:
		_gear = key.keycode - KEY_0


func _read_controls() -> void:
	if autoplay:
		var go := _time_s >= SELECT_GEAR_S
		run.set_controls(1.0 if go else 0.0, 1.0, autoplay_gear if go else 0, 0.0, 0.0, 0.0)
		return
	var throttle := 1.0 if Input.is_physical_key_pressed(KEY_W) else 0.0
	var steering := 0.0
	if Input.is_physical_key_pressed(KEY_A):
		steering += KEY_STEERING
	if Input.is_physical_key_pressed(KEY_D):
		steering -= KEY_STEERING
	run.set_controls(throttle, 1.0, _gear, steering, 0.0, 0.0)


func _show(delta: float) -> void:
	var s := run.get_snapshot()
	if s.is_empty():
		return
	tractor_view.show_snapshot(s, delta)
	sled_view.show_snapshot(s)
	chain_view.show_between(tractor_view.hitch_position(), sled_view.hook_position(),
			s["chain_tension_n"])
	_hud.text = _hud_text(s)


func _hud_text(s: Dictionary) -> String:
	var text := "gear %d   %4.0f rpm   %4.1f km/h   %6.2f m" % [
			s["gear"], float(s["engine_omega_rad_s"]) * RPM_PER_RAD_S,
			float(s["tractor_speed_m_s"]) * KMH_PER_M_S, s["sled_x_m"]]
	if run.is_finished():
		var result := run.get_result()
		text += "\n%s at %.2f m (R to pull again)" % [result["ending"], result["distance_m"]]
	return text


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
