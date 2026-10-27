# The sled in graybox: its frame from the rear wheels to the hook, the
# pan, and the weight box riding forward as the sled advances. The
# push-down shows: as it comes on, the rear wheels are lifted off the track
# (drawn a little higher than the few centimetres a real sled lifts them, to
# be seen from the tractor's seat) and the lamp on the frame turns from
# green to red.
class_name SledView
extends Node3D

const FRAME_COLOUR := Color(0.2, 0.35, 0.6)
const BOX_COLOUR := Color(0.85, 0.55, 0.1)
const TIRE_COLOUR := Color(0.12, 0.12, 0.12)
# The sled's look: its width and the weight box's size [m].
const WIDTH_M := 2.4
const FRAME_HEIGHT_M := 0.3
const BOX_SIZE_M := Vector3(2.0, 1.2, 1.6)
# How far the wheels are drawn lifted with the push-down fully set [m].
const WHEEL_LIFT_M := 0.15
const LAMP_SIZE_M := 0.25
const LAMP_OFF := Color(0.2, 0.8, 0.2)
const LAMP_ON := Color(1.0, 0.15, 0.1)

var _box := Node3D.new()
var _hook := Marker3D.new()
var _wheels := Node3D.new()
var _lamp := MeshInstance3D.new()
var _lamp_material := StandardMaterial3D.new()
var _wheel_height_m := 0.5


# As TractorView's: parts never added to the tree are freed here.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		for part: Node in [_box, _hook, _wheels, _lamp]:
			if part.get_parent() == null:
				part.free()


func build(d: Dictionary) -> void:
	var hook_ahead_m: float = d["hook_ahead_of_wheels_m"]
	_wheel_height_m = d["sled_wheel_radius_m"]
	var frame := Graybox.box(Vector3(WIDTH_M, FRAME_HEIGHT_M, hook_ahead_m), FRAME_COLOUR)
	frame.position = TrackSpace.point(hook_ahead_m / 2.0, 0.0, _wheel_height_m)
	add_child(frame)
	for side: float in [-1.0, 1.0]:
		var wheel := Graybox.wheel(_wheel_height_m, 0.3, TIRE_COLOUR)
		wheel.position = TrackSpace.point(0.0, side * WIDTH_M / 2.0, _wheel_height_m)
		_wheels.add_child(wheel)
	add_child(_wheels)
	var lamp_mesh := SphereMesh.new()
	lamp_mesh.radius = LAMP_SIZE_M / 2.0
	lamp_mesh.height = LAMP_SIZE_M
	_lamp.mesh = lamp_mesh
	_lamp_material.emission_enabled = true
	_lamp.material_override = _lamp_material
	_lamp.position = TrackSpace.point(hook_ahead_m - 0.2, 0.0,
			_wheel_height_m + FRAME_HEIGHT_M + LAMP_SIZE_M)
	add_child(_lamp)
	_show_pushdown(0.0)
	_hook.position = TrackSpace.point(hook_ahead_m, 0.0, float(d["hook_height_m"]))
	add_child(_hook)
	_box.add_child(Graybox.box(BOX_SIZE_M, BOX_COLOUR))
	add_child(_box)


func hook_position() -> Vector3:
	return _hook.global_position


func show_snapshot(s: Dictionary) -> void:
	position = TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	rotation.y = s["sled_heading_rad"]
	_box.position = TrackSpace.point(s["box_ahead_of_wheels_m"], 0.0,
			_wheel_height_m + FRAME_HEIGHT_M / 2.0 + BOX_SIZE_M.y / 2.0)
	_show_pushdown(s["pushdown"])


func _show_pushdown(share: float) -> void:
	_wheels.position = TrackSpace.point(0.0, 0.0, share * WHEEL_LIFT_M)
	var colour := LAMP_OFF.lerp(LAMP_ON, share)
	_lamp_material.albedo_color = colour
	_lamp_material.emission = colour
