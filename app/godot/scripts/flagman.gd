# A track official with a flag, standing beside the lane facing the
# tractor. Two of them work each pull (pull.gd): one past the start line,
# who holds the red flag while the tractor comes to the sled and raises it
# while the tractor stands ready, and one further down the track, who walks
# ahead of the tractor towards the full-pull line. Both wave green while
# the pull is good, and red the moment it is over (a full pull, the sled
# stopped, a disqualification): stop. The run decides when; this only
# draws the signal it is given.
class_name Flagman
extends Node3D

const RED := Color(0.9, 0.12, 0.1)
const GREEN := Color(0.15, 0.75, 0.25)
const SHIRT := Color(0.95, 0.95, 0.92)
const POLE_M := 1.2
const CLOTH_M := Vector2(0.7, 0.5)
# Where the flag's hand is: held low, and raised overhead [m above the
# ground]. The arm swings the pole about the hand.
const HAND_LOW_M := 1.0
const HAND_HIGH_M := 1.9
const LOW_RAD := deg_to_rad(-60.0)
const HIGH_RAD := deg_to_rad(35.0)
# Waving a flag: how far either way, and how fast.
const WAVE_RAD := deg_to_rad(25.0)
const WAVE_PER_S := 7.0

var _arm := Node3D.new()
var _cloth := MeshInstance3D.new()
var _material := StandardMaterial3D.new()
var _time_s := 0.0


func _ready() -> void:
	var person := People.one(false, SHIRT)
	add_child(person)
	add_child(_arm)
	var pole := Art.cylinder(0.015, POLE_M, Color(0.85, 0.85, 0.85), 8)
	pole.position = Vector3(0.0, POLE_M / 2.0, 0.0)
	_arm.add_child(pole)
	var cloth := QuadMesh.new()
	cloth.size = CLOTH_M
	_cloth.mesh = cloth
	_material.albedo_color = RED
	_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	_material.roughness = 0.9
	_cloth.material_override = _material
	_cloth.position = Vector3(0.0, POLE_M - CLOTH_M.y / 2.0, CLOTH_M.x / 2.0)
	_cloth.rotation.y = PI / 2.0
	_arm.add_child(_cloth)
	show_signal(false, 0.0, false, 0.0)


# Stands at `at` (on the ground), facing `facing` (a point to look at).
func stand(at: Vector3, facing: Vector3) -> void:
	position = at
	var level := Vector3(facing.x, at.y, facing.z)
	if not level.is_equal_approx(at):
		look_at(level, Vector3.UP, true)


# The flag: green or red, raised from 0 (held low) to 1 (overhead), waved
# or held still.
func show_signal(green: bool, raise: float, wave: bool, delta: float) -> void:
	_time_s += delta
	_material.albedo_color = GREEN if green else RED
	var up := clampf(raise, 0.0, 1.0)
	_arm.position = Vector3(0.25, lerpf(HAND_LOW_M, HAND_HIGH_M, up), 0.0)
	var swing := sin(_time_s * WAVE_PER_S) * WAVE_RAD if wave else 0.0
	_arm.rotation = Vector3(0.0, 0.0, lerpf(LOW_RAD, HIGH_RAD, up) + swing)


# The flag's colour now, for tests: "red" or "green".
func colour_name() -> String:
	return "green" if _material.albedo_color == GREEN else "red"


# Whether the flag is up (raised past halfway), for tests.
func raised() -> bool:
	return _arm.position.y > (HAND_LOW_M + HAND_HIGH_M) / 2.0
