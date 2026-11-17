# A track official with a flag, standing beside the lane facing the
# tractor. Two of them work each pull (pull.gd): one past the start line,
# who holds the red flag while the tractor comes to the sled and raises it
# while the tractor stands ready, and one further down the track, who walks
# ahead of the tractor towards the full-pull line. Both wave green while
# the pull is good, and red the moment it is over (a full pull, the sled
# stopped, a disqualification): stop. The run decides when; this only
# draws the signal it is given.
#
# The flag is in the official's left hand, at the end of an arm that turns
# at the shoulder: held out low, raised overhead, waved from the shoulder.
# Its cloth faces the tractor and flies out away from the body, so the
# driver sees all of it. In the evening (use_lamp()) the official signals
# with a glowing light wand instead, as officials do at night pulls: a
# cloth is hard to see under floodlights, a lit wand is not.
class_name Flagman
extends Node3D

const RED := Color(0.9, 0.12, 0.1)
const GREEN := Color(0.15, 0.75, 0.25)
const SHIRT := Color(0.95, 0.95, 0.92)
const POLE_M := 1.0
const POLE_RADIUS_M := 0.012
const CLOTH_M := Vector2(0.8, 0.6)
# The cloth glows a little of its own colour, so it reads in the
# official's shadow and against the dark crowd behind.
const CLOTH_GLOW := 0.25
# The arm and the flag, from straight down, positive out to the side
# (away from the body) [rad]: the arm hangs a little out with the flag held
# out and up beside the official, or is raised overhead with the flag
# straight up. The wrist turns the flag from the arm's line.
const ARM_LOW_RAD := deg_to_rad(20.0)
const ARM_HIGH_RAD := deg_to_rad(165.0)
const FLAG_LOW_RAD := deg_to_rad(120.0)
const FLAG_HIGH_RAD := deg_to_rad(175.0)
# Waving: how far either way the arm swings, and how fast.
const WAVE_RAD := deg_to_rad(22.0)
const WAVE_PER_S := 7.0
# The light wand: a dark grip, then the lit tube; how bright it glows (above
# the environment's glow threshold, so it blooms), and the halo around it
# that carries it to the cab from far down the track [m].
const GRIP_M := 0.12
const WAND_M := 0.5
const WAND_RADIUS_M := 0.028
const WAND_GLOW := 4.0
const HALO_M := 1.1
const LAMP_RED := Color(1.0, 0.12, 0.08)
const LAMP_GREEN := Color(0.15, 1.0, 0.3)

var _shoulder := Node3D.new()
var _hand := Node3D.new()
var _flag := Node3D.new()
var _lamp := Node3D.new()
var _cloth: MeshInstance3D
var _cloth_material := ShaderMaterial.new()
var _wand_material := StandardMaterial3D.new()
var _halo_material := StandardMaterial3D.new()
var _green := false
var _raise := 0.0
var _use_lamp := false
var _time_s := 0.0


func _ready() -> void:
	var shirt := People.material(SHIRT)
	var person := MeshInstance3D.new()
	person.mesh = People.standing_without_arm()
	person.material_override = shirt
	add_child(person)
	_shoulder.position = People.SHOULDER_M
	add_child(_shoulder)
	var arm := MeshInstance3D.new()
	arm.mesh = People.arm()
	arm.material_override = shirt
	_shoulder.add_child(arm)
	_hand.position = Vector3(0.0, -People.ARM_M, 0.0)
	_shoulder.add_child(_hand)
	_build_flag()
	_build_lamp()
	_hand.add_child(_flag)
	_hand.add_child(_lamp)
	use_lamp(_use_lamp)
	show_signal(false, 0.0, false, 0.0)


# In the hand's frame the flag runs along -y, out of the fist; the cloth
# hangs from its tip and flies to -x, which is away from the body whether
# the flag is held out low or overhead.
func _build_flag() -> void:
	var pole := Art.cylinder(POLE_RADIUS_M, POLE_M, Color(0.85, 0.85, 0.85), 8)
	pole.position = Vector3(0.0, -POLE_M / 2.0, 0.0)
	_flag.add_child(pole)
	var cloth := PlaneMesh.new()
	cloth.size = CLOTH_M
	cloth.subdivide_width = 8
	cloth.orientation = PlaneMesh.FACE_Z
	_cloth_material.shader = VenueView.FLAG_SHADER
	_cloth_material.set_shader_parameter("glow", CLOTH_GLOW)
	_cloth = Art.instance(cloth, _cloth_material)
	# Facing the tractor (+z); turned about y so the flag shader's pole
	# edge (UV.x 0, the plane's -x) is at the pole and the fly end out at -x.
	_cloth.rotation.y = PI
	_cloth.position = Vector3(-CLOTH_M.x / 2.0, -POLE_M + CLOTH_M.y / 2.0, 0.0)
	_flag.add_child(_cloth)


# The wand runs along -y out of the fist as the flag does; its halo, a
# glowing disc always turned to the camera, sits on the lit tube.
func _build_lamp() -> void:
	var grip := Art.cylinder(WAND_RADIUS_M, GRIP_M, Color(0.08, 0.08, 0.08), 8)
	grip.position = Vector3(0.0, -GRIP_M / 2.0 + 0.04, 0.0)
	_lamp.add_child(grip)
	var tube := CylinderMesh.new()
	tube.top_radius = WAND_RADIUS_M
	tube.bottom_radius = WAND_RADIUS_M
	tube.height = WAND_M
	tube.radial_segments = 8
	tube.rings = 1
	_wand_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_wand_material.emission_enabled = true
	_wand_material.emission_energy_multiplier = WAND_GLOW
	var wand := Art.instance(tube, _wand_material)
	wand.position = Vector3(0.0, -GRIP_M + 0.04 - WAND_M / 2.0, 0.0)
	_lamp.add_child(wand)
	var halo := QuadMesh.new()
	halo.size = Vector2(HALO_M, HALO_M)
	var fade := Gradient.new()
	fade.set_color(0, Color(1.0, 1.0, 1.0, 0.8))
	fade.add_point(0.3, Color(1.0, 1.0, 1.0, 0.35))
	fade.set_color(fade.get_point_count() - 1, Color(1.0, 1.0, 1.0, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = fade
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	texture.width = 64
	texture.height = 64
	_halo_material.albedo_texture = texture
	_halo_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_halo_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_halo_material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	_halo_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_halo_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var glow := Art.instance(halo, _halo_material)
	glow.position = wand.position
	glow.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_lamp.add_child(glow)


# Stands at `at` (on the ground), facing `facing` (a point to look at).
func stand(at: Vector3, facing: Vector3) -> void:
	position = at
	var level := Vector3(facing.x, at.y, facing.z)
	if not level.is_equal_approx(at):
		look_at(level, Vector3.UP, true)


# A light wand instead of the flag (the evening), or the flag.
func use_lamp(lamp: bool) -> void:
	_use_lamp = lamp
	_flag.visible = not lamp
	_lamp.visible = lamp


# The signal: green or red, raised from 0 (held out low) to 1 (overhead),
# waved or held still.
func show_signal(green: bool, raise: float, wave: bool, delta: float) -> void:
	_time_s += delta
	_green = green
	_raise = clampf(raise, 0.0, 1.0)
	_cloth_material.set_shader_parameter("colour", GREEN if green else RED)
	var lamp := LAMP_GREEN if green else LAMP_RED
	_wand_material.albedo_color = lamp
	_wand_material.emission = lamp
	_halo_material.albedo_color = lamp
	var swing := sin(_time_s * WAVE_PER_S) * WAVE_RAD if wave else 0.0
	var arm_rad := lerpf(ARM_LOW_RAD, ARM_HIGH_RAD, _raise) + swing
	_shoulder.rotation = Vector3(0.0, 0.0, arm_rad)
	_hand.rotation = Vector3(0.0, 0.0, lerpf(FLAG_LOW_RAD, FLAG_HIGH_RAD, _raise) - arm_rad + swing)


# The signal's colour now, for tests: "red" or "green".
func colour_name() -> String:
	return "green" if _green else "red"


# Whether the flag is up (raised past halfway), for tests.
func raised() -> bool:
	return _raise > 0.5


# Whether the official signals with the light wand, for tests.
func holds_lamp() -> bool:
	return _use_lamp


# Where the hand holding the flag or the wand is, in the world, for tests.
func hand_position() -> Vector3:
	return _hand.global_position


# The way the flag's cloth faces in the world (either side shows), for
# tests.
func cloth_normal() -> Vector3:
	return _cloth.global_basis.z.normalized()


# Whether the flag or the wand is shown, for tests: "flag" or "lamp".
func shown_signal() -> String:
	return "lamp" if _lamp.visible and not _flag.visible else "flag"


# Where the signal is: the flag's tip, or the wand's end, in the world,
# for tests.
func signal_position() -> Vector3:
	var reach_m := GRIP_M - 0.04 + WAND_M if _use_lamp else POLE_M
	return _hand.global_transform * Vector3(0.0, -reach_m, 0.0)
