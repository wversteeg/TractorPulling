# The cameras on the pull: far behind the tractor and close, about where
# the sled's canopy is, both turning with the tractor; in the cab, beside the tractor,
# and above the track. next() switches between them. In reverse the cab camera turns to
# look back over the hitch. While the hitch meets the hook (backing up to
# the sled, and backing off to release the chain), the rig shows the hitch
# from the side instead, unless the player is in the cab or taps the
# camera button; then it goes back to the view picked.
class_name CameraRig
extends Node3D

enum View { CHASE, CLOSE, CAB, SIDE, OVERHEAD }

# Where the chase camera sits from what it follows, along the tractor's
# heading (x ahead, y left, z up) [m]; and the fixed-direction cameras, in
# the track's axes (x along, y left, z up) [m].
const CHASE_OFFSET := Vector3(-20.0, 3.0, 9.0)
# On the right, inside the straw bales: the grandstand is on the left.
const SIDE_OFFSET := Vector3(-2.0, -10.0, 3.5)
const OVERHEAD_OFFSET := Vector3(-8.0, 0.0, 40.0)
# The hitch camera: square to the sled's line, on its right (where the
# parking is, away from the grandstand), low, so the hitch and the hook
# show apart; farther out the farther apart they are, so both stay in view
# [m, m per m apart].
const HITCH_OUT_M := 6.0
const HITCH_UP_M := 2.2
const HITCH_BACK_PER_M := 0.9
# How fast the cab camera turns to look back in reverse, and forward again
# [1/s].
const CAB_TURN_RATE_PER_S := 6.0
# Looking back the driver turns round and raises up over the seat back,
# over the right shoulder, and looks down past the hitch: from where the
# eyes look ahead, along, left and up on the tractor [m]; and how far down
# [rad]. So the back of the fenders, a little of the rear wheels and the
# chain leaving the hitch stay in the bottom of the view, as the tractor's
# own reference.
const CAB_BACK_SHIFT := Vector3(-0.3, -0.12, 0.22)
# The tilt comes on top of the eyes' own look down ahead
# (TractorView.EYES_DOWN_RAD): together 0.62 rad down.
const CAB_BACK_PITCH_RAD := -0.62 + TractorView.EYES_DOWN_RAD
# The close camera: on the chain it stands behind the tractor along its
# heading as far as the top of the sled's canopy (SledView.camera_mount())
# is, and this much farther back and higher, as a TV camera on the sled
# would see it but not shaken by the sled; looking at a point this far
# ahead of the tractor [m]. Off the chain, the sled no guide, it stands
# this far behind the tractor and this high instead [m].
const CLOSE_PAST_SLED_M := 0.5
const CLOSE_OVER_SLED_M := 1.3
const CLOSE_LOOK_AHEAD_M := 1.0
const CLOSE_UNHOOKED_BACK_M := 11.0
const CLOSE_UNHOOKED_UP_M := 5.0
# How fast the chase and close cameras turn to the tractor's heading [1/s].
const HEADING_TURN_RATE_PER_S := 3.0
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
var cab: Camera3D = null:
	set(value):
		if value == cab:
			return
		cab = value
		if cab != null:
			_cab_ahead = cab.position
			_cab_ahead_rotation = cab.rotation
var _time_s := 0.0
# Reversing: the cab camera looks back (set_reversing).
var _reversing := false
var _cab_turn := 0.0
# Where the cab camera looks ahead from, on the tractor.
var _cab_ahead := Vector3.ZERO
var _cab_ahead_rotation := Vector3.ZERO
# The hitch camera is wanted now (show_hitch), and the player has not
# switched it away since.
var _hitch_wanted := false
var _hitch_dismissed := false
var _hitch_camera := Camera3D.new()
var _hitch_apart_m := 0.0
var _hitch_heading_rad := 0.0
var _heading_rad := 0.0
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
	_hitch_camera.far = FAR_M
	add_child(_hitch_camera)
	_cameras[view].make_current()


func next() -> void:
	if showing_hitch():
		# The tap dismisses the hitch camera: back to the view picked.
		_hitch_dismissed = true
		current_camera().make_current()
		return
	# From the cab, while the hitch is wanted, the next view is the hitch's.
	view = ((view + 1) % View.size()) as View
	if view == View.CAB and cab == null:
		view = ((view + 1) % View.size()) as View
	current_camera().make_current()


func current_camera() -> Camera3D:
	if showing_hitch():
		return _hitch_camera
	return cab if view == View.CAB and cab != null else _cameras[view]


# The hitch from the side, while `wanted` (the hitch meeting the hook,
# `apart_m` from it, square to the sled's heading `heading_rad`); not from
# the cab, nor once the player has switched it away. Off again, the view
# picked comes back.
func show_hitch(wanted: bool, apart_m: float = 0.0, heading_rad: float = 0.0) -> void:
	_hitch_apart_m = apart_m
	_hitch_heading_rad = heading_rad
	if not wanted:
		_hitch_dismissed = false
	if wanted == _hitch_wanted:
		return
	_hitch_wanted = wanted
	current_camera().make_current()


func showing_hitch() -> bool:
	return _hitch_wanted and not _hitch_dismissed and view != View.CAB


# In reverse the cab camera looks back over the hitch.
func set_reversing(reversing: bool) -> void:
	_reversing = reversing


# Back to the view picked, the cab looking ahead: for a new pull.
func reset_framing() -> void:
	_hitch_wanted = false
	_hitch_dismissed = false
	_reversing = false
	_cab_turn = 0.0
	_turn_cab()
	current_camera().make_current()


# Follows a point on the track (the tractor, or between it and the sled),
# the chase and close cameras along `heading_rad` (the tractor's,
# TrackSpace), the close one as far back as `sled_mount`
# (SledView.camera_mount(), INF off the chain); `smooth` false to jump
# there.
func follow(target: Vector3, delta: float, smooth: bool = true, heading_rad: float = 0.0,
		sled_mount: Vector3 = Vector3.INF) -> void:
	var weight := 1.0 - exp(-FOLLOW_RATE_PER_S * delta) if smooth else 1.0
	_heading_rad = lerp_angle(_heading_rad, heading_rad,
			1.0 - exp(-HEADING_TURN_RATE_PER_S * delta) if smooth else 1.0)
	_time_s += delta
	# A small, quick wobble, out of step on each axis; not from above.
	var t := _time_s * SHAKE_RATE
	var shake := Vector3(sin(t), sin(1.3 * t + 1.0), sin(0.7 * t + 2.0)) * SHAKE_M * rumble
	var along := cos(_heading_rad)
	var across := sin(_heading_rad)
	var ahead := TrackSpace.point(along, across, 0.0)
	_places[View.CHASE] = target + TrackSpace.point(CHASE_OFFSET.x * along - CHASE_OFFSET.y * across,
			CHASE_OFFSET.x * across + CHASE_OFFSET.y * along, CHASE_OFFSET.z)
	var back_m := CLOSE_UNHOOKED_BACK_M
	var up_m := CLOSE_UNHOOKED_UP_M
	if sled_mount != Vector3.INF:
		var to_mount := sled_mount - target
		back_m = -to_mount.dot(ahead) + CLOSE_PAST_SLED_M
		up_m = to_mount.dot(Vector3.UP) + CLOSE_OVER_SLED_M
	_places[View.CLOSE] = target - ahead * back_m + Vector3.UP * up_m
	_places[View.SIDE] = target + TrackSpace.point(SIDE_OFFSET.x, SIDE_OFFSET.y, SIDE_OFFSET.z)
	_places[View.OVERHEAD] = target + TrackSpace.point(OVERHEAD_OFFSET.x, OVERHEAD_OFFSET.y,
			OVERHEAD_OFFSET.z)
	var out_m := HITCH_OUT_M + HITCH_BACK_PER_M * _hitch_apart_m
	var hitch_place := target + TrackSpace.point(0.0, -out_m, HITCH_UP_M) \
			.rotated(Vector3.UP, _hitch_heading_rad)
	_hitch_camera.global_position = _hitch_camera.global_position.lerp(hitch_place, weight)
	_hitch_camera.look_at(target, Vector3.UP)
	_cab_turn = lerpf(_cab_turn, 1.0 if _reversing else 0.0,
			1.0 - exp(-CAB_TURN_RATE_PER_S * delta) if smooth else 1.0)
	_turn_cab()
	for i in View.size():
		if i == View.CAB:
			continue
		var camera := _cameras[i]
		camera.global_position = camera.global_position.lerp(_places[i], weight)
		var looking_at := target + ahead * CLOSE_LOOK_AHEAD_M if i == View.CLOSE else target
		var wobble := shake if i != View.OVERHEAD else Vector3.ZERO
		camera.look_at(looking_at + wobble, Vector3.UP)


# The cab camera turned `_cab_turn` of the way to looking back.
func _turn_cab() -> void:
	if cab == null:
		return
	cab.position = _cab_ahead + TrackSpace.point(CAB_BACK_SHIFT.x, CAB_BACK_SHIFT.y,
			CAB_BACK_SHIFT.z) * _cab_turn
	cab.rotation = _cab_ahead_rotation + Vector3(CAB_BACK_PITCH_RAD * _cab_turn, PI * _cab_turn,
			0.0)
