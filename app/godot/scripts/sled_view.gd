# The sled in graybox: its frame from the rear wheels to the hook, the
# pan, and the weight box riding forward as the sled advances.
class_name SledView
extends Node3D

const FRAME_COLOUR := Color(0.2, 0.35, 0.6)
const BOX_COLOUR := Color(0.85, 0.55, 0.1)
const TIRE_COLOUR := Color(0.12, 0.12, 0.12)
# The sled's look: its width and the weight box's size [m].
const WIDTH_M := 2.4
const FRAME_HEIGHT_M := 0.3
const BOX_SIZE_M := Vector3(2.0, 1.2, 1.6)

var _box := Node3D.new()
var _hook := Marker3D.new()
var _wheel_height_m := 0.5


func build(d: Dictionary) -> void:
	var hook_ahead_m: float = d["hook_ahead_of_wheels_m"]
	_wheel_height_m = d["sled_wheel_radius_m"]
	var frame := Graybox.box(Vector3(WIDTH_M, FRAME_HEIGHT_M, hook_ahead_m), FRAME_COLOUR)
	frame.position = TrackSpace.point(hook_ahead_m / 2.0, 0.0, _wheel_height_m)
	add_child(frame)
	for side: float in [-1.0, 1.0]:
		var wheel := Graybox.wheel(_wheel_height_m, 0.3, TIRE_COLOUR)
		wheel.position = TrackSpace.point(0.0, side * WIDTH_M / 2.0, _wheel_height_m)
		add_child(wheel)
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
