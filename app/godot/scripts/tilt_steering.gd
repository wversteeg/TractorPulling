# Steering by tilting the phone, held in landscape like a steering wheel:
# rolling it about the screen's normal, its left side down, steers left.
# Whatever angle the phone is held at when the pull begins (centre()) is
# straight ahead, so it works sitting, standing or lying down; FULL_TILT_DEG
# either way is the steering control's end (-1 full right, 1 full left,
# the run's convention). The core's dead zone and steering rate then shape
# it, as they do the slider (data/controls/easy.toml, pro.toml).
#
# The angle is read from gravity in the screen's plane, so it does not
# matter which in-plane axes the platform reports it in. `gravity` gives
# it, Input.get_gravity() by default (m/s², zero without a sensor); a test
# hands in its own.
class_name TiltSteering
extends RefCounted

# The roll either way that steers fully [degrees].
const FULL_TILT_DEG := 25.0
# Gravity in the screen's plane below this share of its whole: the phone
# lies (nearly) flat, the roll is not defined, so the last reading holds.
const MIN_IN_PLANE_SHARE := 0.3

var gravity: Callable = Input.get_gravity
var _centre_rad := 0.0
var _steering := 0.0


# Whether the device has a motion sensor to steer with.
func available() -> bool:
	return gravity.call() != Vector3.ZERO


# The phone's angle now is straight ahead.
func centre() -> void:
	var roll: Variant = _roll()
	_centre_rad = roll if roll != null else 0.0
	_steering = 0.0


# -1 (full right) to 1 (full left).
func steering() -> float:
	var roll: Variant = _roll()
	if roll != null:
		var tilt_rad := wrapf(float(roll) - _centre_rad, -PI, PI)
		_steering = clampf(tilt_rad / deg_to_rad(FULL_TILT_DEG), -1.0, 1.0)
	return _steering


# The phone's roll about the screen's normal [rad], or null lying flat.
# Rolled by θ with its left side down, gravity in the screen's plane turns
# the other way: its x grows as sin θ.
func _roll() -> Variant:
	var g: Vector3 = gravity.call()
	var in_plane := Vector2(g.x, g.y)
	if g == Vector3.ZERO or in_plane.length() < MIN_IN_PLANE_SHARE * g.length():
		return null
	return atan2(in_plane.x, in_plane.y)
