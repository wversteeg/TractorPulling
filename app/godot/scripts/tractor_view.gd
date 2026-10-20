# The tractor in graybox: a body that pitches about the rear axle, rear
# wheels that turn at their sides' speeds, front wheels that steer. Placed
# from the snapshot each frame; the wheels' angles are only for drawing.
class_name TractorView
extends Node3D

const BODY_COLOUR := Color(0.75, 0.12, 0.1)
const TIRE_COLOUR := Color(0.12, 0.12, 0.12)
# The body's look: as wide as the front track, this high above the axle.
const BODY_HEIGHT_M := 0.9
const BODY_LIFT_M := 0.2

var _body := Node3D.new()        # pivots about the rear axle
var _hitch := Marker3D.new()
var _rear_left := Node3D.new()
var _rear_right := Node3D.new()
var _front_left_steer := Node3D.new()
var _front_right_steer := Node3D.new()
var _front_left := Node3D.new()
var _front_right := Node3D.new()
var _rear_radius_m := 1.0
var _front_radius_m := 0.5
var _rear_left_angle := 0.0
var _rear_right_angle := 0.0
var _front_angle := 0.0


func build(d: Dictionary) -> void:
	var wheelbase_m: float = d["wheelbase_m"]
	var axle_height_m: float = d["rear_axle_height_m"]
	var rear_track_m: float = d["rear_track_m"]
	var front_track_m: float = d["front_track_m"]
	_rear_radius_m = d["rear_tire_radius_m"]
	_front_radius_m = d["front_tire_radius_m"]

	_body.position = TrackSpace.point(0.0, 0.0, axle_height_m)
	add_child(_body)
	var hull := Graybox.box(Vector3(front_track_m * 0.6, BODY_HEIGHT_M, wheelbase_m), BODY_COLOUR)
	hull.position = TrackSpace.point(wheelbase_m / 2.0, 0.0, BODY_LIFT_M + BODY_HEIGHT_M / 2.0)
	_body.add_child(hull)
	_hitch.position = TrackSpace.point(-float(d["hitch_behind_rear_axle_m"]), 0.0,
			float(d["hitch_height_m"]) - axle_height_m)
	_body.add_child(_hitch)

	var rear_width_m: float = d["rear_tire_width_m"]
	_add_wheel(_body, _rear_left, Vector3.ZERO, rear_track_m / 2.0, _rear_radius_m, rear_width_m)
	_add_wheel(_body, _rear_right, Vector3.ZERO, -rear_track_m / 2.0, _rear_radius_m, rear_width_m)
	# The front axle sits at the front tires' radius above the ground.
	var front_axle := TrackSpace.point(wheelbase_m, 0.0, _front_radius_m - axle_height_m)
	var front_width_m: float = d["front_tire_width_m"]
	for pair: Array in [[_front_left_steer, _front_left, 1.0], [_front_right_steer, _front_right, -1.0]]:
		var steer: Node3D = pair[0]
		steer.position = front_axle + TrackSpace.point(0.0, float(pair[2]) * front_track_m / 2.0, 0.0)
		_body.add_child(steer)
		_add_wheel(steer, pair[1], Vector3.ZERO, 0.0, _front_radius_m, front_width_m)


# Where the chain pulls, in the world.
func hitch_position() -> Vector3:
	return _hitch.global_position


func show_snapshot(s: Dictionary, delta: float) -> void:
	position = TrackSpace.point(s["tractor_x_m"], s["tractor_y_m"], 0.0)
	rotation.y = s["tractor_heading_rad"]
	_body.rotation.x = s["tractor_pitch_rad"]
	var steering: float = s["steering_rad"]
	_front_left_steer.rotation.y = steering
	_front_right_steer.rotation.y = steering
	# Rolling forward turns a wheel about -x (TrackSpace): the angle grows
	# with the wheel's speed, and the front wheels roll with the ground.
	_rear_left_angle -= float(s["rear_left_omega_rad_s"]) * delta
	_rear_right_angle -= float(s["rear_right_omega_rad_s"]) * delta
	_front_angle -= float(s["tractor_speed_m_s"]) / _front_radius_m * delta
	_rear_left.rotation.x = wrapf(_rear_left_angle, -PI, PI)
	_rear_right.rotation.x = wrapf(_rear_right_angle, -PI, PI)
	_front_left.rotation.x = wrapf(_front_angle, -PI, PI)
	_front_right.rotation.x = wrapf(_front_angle, -PI, PI)


func _add_wheel(parent: Node3D, spin: Node3D, at: Vector3, side_m: float, radius_m: float,
		width_m: float) -> void:
	spin.position = at + TrackSpace.point(0.0, side_m, 0.0)
	parent.add_child(spin)
	spin.add_child(Graybox.wheel(radius_m, width_m, TIRE_COLOUR))
	# A light stripe on the tire, so its turning shows.
	var stripe := Graybox.box(Vector3(width_m * 1.02, radius_m * 0.15, radius_m * 2.02),
			Color(0.7, 0.7, 0.7))
	spin.add_child(stripe)
