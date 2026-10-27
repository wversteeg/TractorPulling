# The cameras on the pull: behind the tractor, close behind the sled
# turning with the tractor, in the cab, beside the tractor, and above the
# track. next() switches between them.
class_name CameraRig
extends Node3D

enum View { CHASE, CLOSE, CAB, SIDE, OVERHEAD }

# Where the fixed-direction cameras sit from what they follow, in the
# track's axes (x along, y left, z up) [m].
const CHASE_OFFSET := Vector3(-20.0, 3.0, 9.0)
# On the right, inside the straw bales: the grandstand is on the left.
const SIDE_OFFSET := Vector3(-2.0, -10.0, 3.5)
const OVERHEAD_OFFSET := Vector3(-8.0, 0.0, 40.0)
# The close camera: this far behind the sled's rear and this high, along
# the tractor's heading, looking at a point this far ahead of the tractor,
# so the whole sled is just in view [m].
const CLOSE_BEHIND_SLED_M := 3.5
const CLOSE_HEIGHT_M := 5.0
const CLOSE_LOOK_AHEAD_M := 3.0
# How fast the close camera turns to the tractor's heading, per second.
const CLOSE_TURN_RATE := 3.0
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
# The tractor's own camera at the driver's eyes (TractorView.cab_camera):
# it moves with the tractor, so follow() leaves it be. Without one, next()
# skips the cab.
var cab: Camera3D = null
# From what the cameras follow (the tractor's rear axle) back to the sled's
# rear [m]: the close camera stands behind it.
var sled_rear_behind_m := 8.0
var _time_s := 0.0
var _close_heading_rad := 0.0
# The rig's own cameras, by View; the cab's slot is the tractor's camera.
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
	if view == View.CAB and cab == null:
		view = ((view + 1) % View.size()) as View
	current_camera().make_current()


func current_camera() -> Camera3D:
	return cab if view == View.CAB and cab != null else _cameras[view]


# Follows a point on the track (the tractor, or between it and the sled),
# the close camera along `heading_rad` (the tractor's, TrackSpace);
# `smooth` false to jump there.
func follow(target: Vector3, delta: float, smooth: bool = true, heading_rad: float = 0.0) -> void:
	var weight := 1.0 - exp(-FOLLOW_RATE * delta) if smooth else 1.0
	_close_heading_rad = lerp_angle(_close_heading_rad, heading_rad,
			1.0 - exp(-CLOSE_TURN_RATE * delta) if smooth else 1.0)
	_time_s += delta
	# A small, quick wobble, out of step on each axis; not from above.
	var t := _time_s * SHAKE_RATE
	var shake := Vector3(sin(t), sin(1.3 * t + 1.0), sin(0.7 * t + 2.0)) * SHAKE_M * rumble
	var ahead := TrackSpace.point(cos(_close_heading_rad), sin(_close_heading_rad), 0.0)
	var back_m := sled_rear_behind_m + CLOSE_BEHIND_SLED_M
	var places := {
		View.CHASE: target + TrackSpace.point(CHASE_OFFSET.x, CHASE_OFFSET.y, CHASE_OFFSET.z),
		View.CLOSE: target - ahead * back_m + TrackSpace.point(0.0, 0.0, CLOSE_HEIGHT_M),
		View.SIDE: target + TrackSpace.point(SIDE_OFFSET.x, SIDE_OFFSET.y, SIDE_OFFSET.z),
		View.OVERHEAD: target + TrackSpace.point(OVERHEAD_OFFSET.x, OVERHEAD_OFFSET.y,
				OVERHEAD_OFFSET.z),
	}
	for i: View in places:
		var camera := _cameras[i]
		camera.global_position = camera.global_position.lerp(places[i], weight)
		var looking_at := target + ahead * CLOSE_LOOK_AHEAD_M if i == View.CLOSE else target
		var wobble := shake if i != View.OVERHEAD else Vector3.ZERO
		camera.look_at(looking_at + wobble, Vector3.UP)
