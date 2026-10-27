# The track official who starts each pull, standing on the left of the
# lane a little past the start line, facing the tractor coming to the sled.
# A red flag held low while the tractor comes to the sled; raised as the
# slack is taken up (higher with the run's flag_progress); waved green when
# the pull is on; lowered again after it. The run decides when: this only
# draws its snapshot.
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
# Waving the green flag: how far either way, and how fast.
const WAVE_RAD := deg_to_rad(25.0)
const WAVE_PER_S := 7.0

var _arm := Node3D.new()
var _cloth := MeshInstance3D.new()
var _material := StandardMaterial3D.new()
var _time_s := 0.0
var _stage := ""


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
	show_stage("to_the_sled", 0.0, 0.0)


# Stands at `at` (on the ground), facing `facing` (a point to look at).
func stand(at: Vector3, facing: Vector3) -> void:
	position = at
	look_at(Vector3(facing.x, at.y, facing.z), Vector3.UP, true)


# The run's stage and flag_progress (0 to 1) from its snapshot.
func show_stage(stage: String, flag_progress: float, delta: float) -> void:
	_time_s += delta
	_stage = stage
	var green := stage == "pulling"
	_material.albedo_color = GREEN if green else RED
	var raise := 0.0
	match stage:
		"taking_up_slack":
			raise = clampf(flag_progress, 0.0, 1.0)
		"pulling":
			raise = 1.0
	_arm.position = Vector3(0.25, lerpf(HAND_LOW_M, HAND_HIGH_M, raise), 0.0)
	var wave := sin(_time_s * WAVE_PER_S) * WAVE_RAD if green else 0.0
	_arm.rotation = Vector3(0.0, 0.0, lerpf(LOW_RAD, HIGH_RAD, raise) + wave)


# The flag's colour now, for tests: "red" or "green".
func colour_name() -> String:
	return "green" if _material.albedo_color == GREEN else "red"
