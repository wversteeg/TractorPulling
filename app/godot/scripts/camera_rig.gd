# Three cameras on the pull: behind the tractor, beside it, and above the
# track. next() switches between them.
class_name CameraRig
extends Node3D

enum View { CHASE, SIDE, OVERHEAD }

# Where each camera sits from what it follows, in the track's axes
# (x along, y left, z up) [m].
const CHASE_OFFSET := Vector3(-20.0, 3.0, 9.0)
# On the right, inside the straw bales: the grandstand is on the left.
const SIDE_OFFSET := Vector3(-2.0, -10.0, 3.5)
const OVERHEAD_OFFSET := Vector3(-8.0, 0.0, 40.0)
# How fast a camera catches up with its place, per second.
const FOLLOW_RATE := 6.0
# Far enough to see the hills round the field (VenueView) [m].
const FAR_M := 900.0
# How far a camera shakes at full rumble [m], and how fast.
const SHAKE_M := 0.06
const SHAKE_RATE := 29.0

var view := View.CHASE
# How hard the tractor works, 0 to 1: the near cameras shake with it.
var rumble := 0.0
var _time_s := 0.0
var _cameras: Array[Camera3D] = []


func _ready() -> void:
	for i in View.size():
		var camera := Camera3D.new()
		camera.far = FAR_M
		add_child(camera)
		_cameras.append(camera)
	_cameras[view].make_current()


func next() -> void:
	view = ((view + 1) % View.size()) as View
	_cameras[view].make_current()


func current_camera() -> Camera3D:
	return _cameras[view]


# Follows a point on the track (the tractor, or between it and the sled),
# `smooth` false to jump there.
func follow(target: Vector3, delta: float, smooth: bool = true) -> void:
	var offsets: Array[Vector3] = [CHASE_OFFSET, SIDE_OFFSET, OVERHEAD_OFFSET]
	var weight := 1.0 - exp(-FOLLOW_RATE * delta) if smooth else 1.0
	_time_s += delta
	# A small, quick wobble, out of step on each axis; not from above.
	var t := _time_s * SHAKE_RATE
	var shake := Vector3(sin(t), sin(1.3 * t + 1.0), sin(0.7 * t + 2.0)) * SHAKE_M * rumble
	for i in _cameras.size():
		var o := offsets[i]
		var place := target + TrackSpace.point(o.x, o.y, o.z)
		var camera := _cameras[i]
		camera.global_position = camera.global_position.lerp(place, weight)
		var wobble := shake if i != View.OVERHEAD else Vector3.ZERO
		camera.look_at(target + wobble, Vector3.UP)
