# Close-ups of the pull scene, for reviewing the look: the tractor from the
# front and from behind, the sled, the grandstand and the standing crowd,
# the venue from above, and the game's own cameras during a pull, with the
# push-down set near its end. The scripted driver pulls; each shot is a PNG
# in the directory given after `--`. Needs a display (or Xvfb) and a
# renderer:
#
#   godot --fixed-fps 60 --rendering-method gl_compatibility --resolution 1280x720 \
#       --path app/godot --script res://tests/showcase.gd -- /tmp/shots
extends SceneTree

var _pull: Node
var _directory := "user://"
var _camera := Camera3D.new()


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_directory = args[0]
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("autoplay", true)
	root.add_child(_pull)
	_camera.far = 600.0
	_camera.fov = 55.0
	_pull.add_child(_camera)
	_play.call_deferred()


func _frames(count: int) -> void:
	for i in count:
		await process_frame


# A shot from `from` looking at `at` (both in TrackSpace: x along the
# track, y left, z up), the HUD hidden.
func _close_up(name: String, from: Vector3, at: Vector3, fov: float = 55.0) -> void:
	_set_overlay(false)
	_camera.fov = fov
	_camera.global_position = TrackSpace.point(from.x, from.y, from.z)
	_camera.look_at(TrackSpace.point(at.x, at.y, at.z), Vector3.UP)
	_camera.make_current()
	await _save(name)


func _game_view(name: String) -> void:
	_set_overlay(true)
	var cameras: CameraRig = _pull.get("cameras")
	cameras.current_camera().make_current()
	await _save(name)


func _save(name: String) -> void:
	await _frames(3)
	var path := "%s/%s.png" % [_directory, name]
	root.get_texture().get_image().save_png(path)
	print("saved ", path)


func _set_overlay(on: bool) -> void:
	for name: String in ["hud", "controls"]:
		(_pull.get(name) as CanvasItem).visible = on


# Where the tractor and the sled are, in TrackSpace (x along, y left).
func _tractor_x() -> float:
	return float((_pull.get("run") as TractorPullRun).get_snapshot()["tractor_x_m"])


func _sled_x() -> float:
	return float((_pull.get("run") as TractorPullRun).get_snapshot()["sled_x_m"])


func _play() -> void:
	await _frames(20)
	var t := _tractor_x()
	var s := _sled_x()
	await _close_up("01_tractor_front", Vector3(t + 5.0, -3.8, 1.9), Vector3(t + 1.2, 0.0, 1.0))
	await _close_up("02_tractor_side", Vector3(t + 1.2, -6.5, 1.4), Vector3(t + 1.2, 0.0, 1.0))
	await _close_up("03_tractor_rear", Vector3(t - 3.6, 3.0, 1.8), Vector3(t, 0.0, 0.9))
	await _close_up("04_sled", Vector3(s - 4.5, -4.5, 2.6), Vector3(s + 1.5, 0.0, 0.8))
	await _close_up("05_sled_and_tractor", Vector3(s - 6.0, 7.0, 3.5), Vector3((s + t) / 2.0, 0.0, 0.8))
	await _close_up("06_grandstand", Vector3(45.0, -2.0, 3.0), Vector3(50.0, 18.0, 2.0))
	await _close_up("07_standing_crowd", Vector3(30.0, 2.0, 2.2), Vector3(35.0, -16.0, 1.2))
	await _close_up("08_venue", Vector3(-30.0, -40.0, 30.0), Vector3(55.0, 0.0, 0.0))
	await _close_up("09_full_pull_line", Vector3(80.0, -2.5, 2.0), Vector3(100.0, 0.0, 3.0))
	await _game_view("10_game_start")
	# The pull under way: the game's chase and side cameras.
	while _sled_x() < 40.0:
		await process_frame
	await _game_view("11_game_chase")
	var cameras: CameraRig = _pull.get("cameras")
	cameras.next()
	await _frames(40)
	await _game_view("12_game_side")
	cameras.next()
	cameras.next()
	# Near the end, the push-down coming on and the smoke black.
	var run: TractorPullRun = _pull.get("run")
	var waited := 0
	while not run.is_finished() and float(run.get_snapshot().get("pushdown", 1.0)) < 1.0 \
			and _sled_x() < 92.0 and waited < 60 * 120:
		await process_frame
		waited += 1
	t = _tractor_x()
	s = _sled_x()
	await _close_up("13_pulling_hard", Vector3(t + 4.0, -5.0, 2.0), Vector3(t - 1.0, 0.0, 1.0))
	await _close_up("14_sled_at_the_end", Vector3(s - 3.5, -4.0, 2.2), Vector3(s + 1.0, 0.0, 0.8))
	await _game_view("15_game_end")
	quit(0)
