# The pull scene: the main scene with its scripted driver for
# 10 s at a fixed 60 fps, then checks that what is drawn is the run. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/scene_test.gd
extends SceneTree

const FRAMES := 600
const TOLERANCE_M := 1e-4

var _pull: Node
var _frames := 0
var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("autoplay", true)
	root.add_child(_pull)


func _process(_delta: float) -> bool:
	_frames += 1
	if _frames < FRAMES:
		return false
	_check()
	print("scene test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
	return true


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _check() -> void:
	var run: TractorPullRun = _pull.get("run")
	var s := run.get_snapshot()
	var dimensions := run.get_dimensions()
	_expect(not s.is_empty(), "the run is set up")
	if s.is_empty():
		return
	_expect(float(s["time_s"]) > 9.0, "10 s of frames ran about 10 s of the run: %s" % s["time_s"])
	_expect(float(s["sled_x_m"]) > 1.0, "the sled has moved: %s m" % s["sled_x_m"])

	var tractor: Node3D = _pull.get("tractor_view")
	var sled: Node3D = _pull.get("sled_view")
	var at_tractor := TrackSpace.point(s["tractor_x_m"], s["tractor_y_m"], 0.0)
	var at_sled := TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	_expect(tractor.position.distance_to(at_tractor) < TOLERANCE_M,
			"the tractor is drawn where the run has it")
	_expect(sled.position.distance_to(at_sled) < TOLERANCE_M,
			"the sled is drawn where the run has it")
	_expect(tractor.position.z < sled.position.z, "the tractor is ahead of the sled (-z)")
	_expect(is_equal_approx(tractor.rotation.y, float(s["tractor_heading_rad"])),
			"the tractor's heading")

	# The chain spans the hitch and the hook: about its length while it pulls.
	var hitch: Vector3 = tractor.call("hitch_position")
	var hook: Vector3 = sled.call("hook_position")
	var chain_m: float = dimensions["chain_length_m"]
	_expect(absf(hitch.distance_to(hook) - chain_m) < 0.1 * chain_m,
			"hitch to hook is the chain's length: %.2f m" % hitch.distance_to(hook))

	var cameras: Node = _pull.get("cameras")
	var camera: Camera3D = cameras.call("current_camera")
	_expect(camera != null and camera.is_current(), "a camera is current")
	_expect(camera.global_position.distance_to(tractor.global_position) < 50.0,
			"the camera follows the tractor")
