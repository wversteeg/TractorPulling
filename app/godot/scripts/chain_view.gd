# The chain between the tractor's hitch and the sled's hook: a thin bar
# stretched between them, darker the harder it pulls.
class_name ChainView
extends Node3D

const THICKNESS_M := 0.06
const SLACK_COLOUR := Color(0.55, 0.55, 0.55)
const TAUT_COLOUR := Color(0.1, 0.1, 0.1)
# The tension at which it is drawn fully taut [N].
const TAUT_N := 40_000.0

var _bar := Graybox.box(Vector3(THICKNESS_M, THICKNESS_M, 1.0), SLACK_COLOUR)


func _ready() -> void:
	add_child(_bar)


func show_between(hitch: Vector3, hook: Vector3, tension_n: float) -> void:
	var length_m := hitch.distance_to(hook)
	if length_m < 1e-3:
		return
	global_position = (hitch + hook) / 2.0
	look_at(hitch, Vector3.UP)
	_bar.scale = Vector3(1.0, 1.0, length_m)
	var material := _bar.material_override as StandardMaterial3D
	material.albedo_color = SLACK_COLOUR.lerp(TAUT_COLOUR, clampf(tension_n / TAUT_N, 0.0, 1.0))
