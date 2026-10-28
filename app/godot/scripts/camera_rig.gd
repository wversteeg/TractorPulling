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
# The close camera: this far behind the sled's rear (where it really is,
# the dog-tail included) and this high, along the tractor's heading,
# looking at a point this far ahead of the tractor, so the whole sled is
# just in view [m]. Off the chain, the sled no guide, it stands this far
# behind the tractor instead [m].
const CLOSE_BEHIND_SLED_M := 3.5
const CLOSE_HEIGHT_M := 5.0
const CLOSE_LOOK_AHEAD_M := 3.0
const CLOSE_UNHOOKED_BACK_M := 11.0
# How fast the close camera turns to the tractor's heading [1/s].
const CLOSE_TURN_RATE_PER_S := 3.0
# How fast a camera catches up with its place [1/s].
const FOLLOW_RATE_PER_S := 6.0
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
var _time_s := 0.0
var _close_heading_rad := 0.0
# The rig's own cameras, by View. The cab's slot holds one never placed or
# shown: the cab view is the tractor's camera (`cab`).
var _cameras: Array[Camera3D] = []
# Where each of the rig's cameras is going, by View: filled in place every
# frame, not made anew.
var _places: Array[Vector3] = []


func _ready() -> void:
	for i in View.size():
		var camera := Camera3D.new()
		camera.far = FAR_M
		add_child(camera)
		_cameras.append(camera)
		_places.append(Vector3.ZERO)
	_cameras[view].make_current()


func next() -> void:
	view = ((view + 1) % View.size()) as View
	if view == View.CAB and cab == null:
		view = ((view + 1) % View.size()) as View
	current_camera().make_current()


func current_camera() -> Camera3D:
	return cab if view == View.CAB and cab != null else _cameras[view]


# Follows a point on the track (the tractor, or between it and the sled),
# the close camera along `heading_rad` (the tractor's, TrackSpace) behind
# `sled_rear` (SledView.rear_position(), INF off the chain); `smooth` false
# to jump there.
func follow(target: Vector3, delta: float, smooth: bool = true, heading_rad: float = 0.0,
		sled_rear: Vector3 = Vector3.INF) -> void:
	var weight := 1.0 - exp(-FOLLOW_RATE_PER_S * delta) if smooth else 1.0
	_close_heading_rad = lerp_angle(_close_heading_rad, heading_rad,
			1.0 - exp(-CLOSE_TURN_RATE_PER_S * delta) if smooth else 1.0)
	_time_s += delta
	# A small, quick wobble, out of step on each axis; not from above.
	var t := _time_s * SHAKE_RATE
	var shake := Vector3(sin(t), sin(1.3 * t + 1.0), sin(0.7 * t + 2.0)) * SHAKE_M * rumble
	var ahead := TrackSpace.point(cos(_close_heading_rad), sin(_close_heading_rad), 0.0)
	var up := TrackSpace.point(0.0, 0.0, CLOSE_HEIGHT_M)
	var close_from := target - ahead * CLOSE_UNHOOKED_BACK_M if sled_rear == Vector3.INF \
			else Vector3(sled_rear.x, target.y, sled_rear.z) - ahead * CLOSE_BEHIND_SLED_M
	_places[View.CHASE] = target + TrackSpace.point(CHASE_OFFSET.x, CHASE_OFFSET.y, CHASE_OFFSET.z)
	_places[View.CLOSE] = close_from + up
	_places[View.SIDE] = target + TrackSpace.point(SIDE_OFFSET.x, SIDE_OFFSET.y, SIDE_OFFSET.z)
	_places[View.OVERHEAD] = target + TrackSpace.point(OVERHEAD_OFFSET.x, OVERHEAD_OFFSET.y,
			OVERHEAD_OFFSET.z)
	for i in View.size():
		if i == View.CAB:
			continue
		var camera := _cameras[i]
		camera.global_position = camera.global_position.lerp(_places[i], weight)
		var looking_at := target + ahead * CLOSE_LOOK_AHEAD_M if i == View.CLOSE else target
		var wobble := shake if i != View.OVERHEAD else Vector3.ZERO
		camera.look_at(looking_at + wobble, Vector3.UP)
