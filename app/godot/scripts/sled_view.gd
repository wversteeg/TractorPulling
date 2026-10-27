# The sled, built in code from the run's sizes: two frame rails from the
# rear wheels to the hook, the pan sliding on the track up front, the weight
# box (an open tray stacked with weights) riding forward along the rails as
# the sled advances, and the operator's platform at the back with a lamp on
# a mast. The push-down shows: as it comes on, the rear wheels are lifted
# off the track (drawn a little higher than the few centimetres a real sled
# lifts them, to be seen from the tractor's seat) and the lamp turns from
# green to red.
class_name SledView
extends Node3D

const PAINT := Color(0.95, 0.62, 0.08)
const PAINT_DARK := Color(0.6, 0.38, 0.05)
const STEEL := Color(0.27, 0.25, 0.23)
const DARK := Color(0.1, 0.1, 0.11)
const TIRE := Color(0.08, 0.08, 0.08)
const WEIGHT := Color(0.28, 0.29, 0.31)
const LAMP_OFF := Color(0.2, 0.8, 0.2)
const LAMP_ON := Color(1.0, 0.15, 0.1)
# The sled's look: its width, its rails' height above the wheels' axle and
# the weight box's size [m].
const WIDTH_M := 2.4
const CANOPY_M := 1.9             # the canopy above the platform
const BERM := Color(0.44, 0.31, 0.19)
const OPERATOR := Color(0.78, 0.88, 0.15)
const BERM_FLATNESS := 0.6        # its height, a share of its depth
const RAIL_HEIGHT_M := 0.25
const RAIL_GAP_M := 1.7
const BOX_SIZE_M := Vector3(2.0, 1.0, 1.6)
const PAN_DEPTH_M := 1.2
# How far the wheels are drawn lifted with the push-down fully set [m].
const WHEEL_LIFT_M := 0.15
const LUGS := 18

var _box := Node3D.new()
var _hook := Marker3D.new()
var _wheels := Node3D.new()
var _lamp := MeshInstance3D.new()
var _lamp_material := StandardMaterial3D.new()
var _wheel_spin: Array[Node3D] = []
var _wheel_height_m := 0.5
var _wheel_angle := 0.0
# The berm: loose soil heaped in front of the pan's lip.
var _berm := MeshInstance3D.new()
var _berm_ahead_m := 1.0


# As TractorView's: parts never added to the tree are freed here.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		for part: Node in [_box, _hook, _wheels, _lamp, _berm]:
			if part.get_parent() == null:
				part.free()


func build(d: Dictionary) -> void:
	var hook_ahead_m: float = d["hook_ahead_of_wheels_m"]
	var hook_height_m: float = d["hook_height_m"]
	var pan_ahead_m: float = d["pan_ahead_of_wheels_m"]
	_wheel_height_m = d["sled_wheel_radius_m"]
	var wheel_width_m: float = d["sled_wheel_width_m"]
	var rail_z_m := _wheel_height_m + RAIL_HEIGHT_M

	# Two rails, cross members, and the gooseneck down to the hook.
	var rail_from_m := -0.7
	var rail_to_m := pan_ahead_m + 0.2
	for side: float in [-1.0, 1.0]:
		var rail := _painted(Vector3(0.16, 0.24, rail_to_m - rail_from_m), PAINT)
		rail.position = TrackSpace.point((rail_from_m + rail_to_m) / 2.0, side * RAIL_GAP_M / 2.0,
				rail_z_m)
		add_child(rail)
		var neck_from := TrackSpace.point(rail_to_m, side * RAIL_GAP_M / 2.0, rail_z_m)
		var neck_to := TrackSpace.point(hook_ahead_m - 0.1, side * 0.15, hook_height_m)
		var neck := _painted(Vector3(0.14, 0.18, neck_from.distance_to(neck_to)), PAINT)
		neck.position = (neck_from + neck_to) / 2.0
		neck.basis = Basis.looking_at(neck_to - neck_from, Vector3.UP)
		add_child(neck)
	var x_m := rail_from_m + 0.1
	while x_m <= rail_to_m:
		var cross := _painted(Vector3(RAIL_GAP_M, 0.12, 0.12), PAINT_DARK)
		cross.position = TrackSpace.point(x_m, 0.0, rail_z_m - 0.04)
		add_child(cross)
		x_m += 0.9
	var hook_plate := Art.box(Vector3(0.4, 0.2, 0.2), DARK, 0.4, 0.6)
	hook_plate.position = TrackSpace.point(hook_ahead_m - 0.1, 0.0, hook_height_m)
	add_child(hook_plate)
	var ring := Art.instance(_ring(0.09, 0.025), Art.material(DARK, 0.3, 0.8))
	ring.position = TrackSpace.point(hook_ahead_m, 0.0, hook_height_m)
	add_child(ring)
	_hook.position = TrackSpace.point(hook_ahead_m, 0.0, hook_height_m)
	add_child(_hook)

	# The pan: a wide steel plate on the track, its leading edge turned up,
	# hung from the rails.
	var pan := Art.box(Vector3(WIDTH_M, 0.06, PAN_DEPTH_M), STEEL, 0.7)
	pan.position = TrackSpace.point(pan_ahead_m, 0.0, 0.03)
	add_child(pan)
	var lip := Art.box(Vector3(WIDTH_M, 0.05, 0.35), STEEL, 0.7)
	lip.position = TrackSpace.point(pan_ahead_m + PAN_DEPTH_M / 2.0 + 0.12, 0.0, 0.12)
	lip.rotation.x = 0.7
	add_child(lip)
	for side: float in [-1.0, 1.0]:
		var hanger := _painted(Vector3(0.1, rail_z_m - 0.05, 0.1), PAINT_DARK)
		hanger.position = TrackSpace.point(pan_ahead_m, side * RAIL_GAP_M / 2.0, rail_z_m / 2.0)
		add_child(hanger)

	# The rear axle and its two wheels.
	add_child(_wheels)
	var axle := Art.axle_cylinder(0.08, WIDTH_M - 0.1, DARK, 12)
	axle.position = TrackSpace.point(0.0, 0.0, _wheel_height_m)
	_wheels.add_child(axle)
	for side: float in [-1.0, 1.0]:
		_add_wheel(side * (WIDTH_M / 2.0 - wheel_width_m / 2.0), wheel_width_m)

	# The berm, built at a unit size; show_snapshot scales it to the soil.
	_berm_ahead_m = pan_ahead_m + PAN_DEPTH_M / 2.0 + 0.25
	var berm_material := StandardMaterial3D.new()
	berm_material.albedo_color = BERM
	berm_material.normal_enabled = true
	berm_material.normal_texture = Art.ground_normal(0.2, 5, 8.0)
	berm_material.roughness = 1.0
	_berm.mesh = Art.cylinder_mesh(1.0, 1.0, WIDTH_M + 0.2, 20)
	_berm.material_override = berm_material
	_berm.rotation.z = PI / 2.0
	add_child(_berm)
	_build_box()
	_build_platform(rail_z_m)
	_show_pushdown(0.0)


func _add_wheel(side_m: float, width_m: float) -> void:
	var spin := Node3D.new()
	spin.position = TrackSpace.point(0.0, side_m, _wheel_height_m)
	_wheels.add_child(spin)
	_wheel_spin.append(spin)
	Art.tire(spin, _wheel_height_m - 0.03, width_m, TIRE)
	Art.rim(spin, _wheel_height_m * 0.58, width_m, signf(side_m), 8, PAINT)
	var lugs: Array[Transform3D] = []
	var lug := BoxMesh.new()
	lug.size = Vector3(width_m * 0.9, 0.06, 0.06)
	for i in LUGS:
		var basis := Basis(Vector3.RIGHT, -TAU * i / LUGS)
		lugs.append(Transform3D(basis, basis * Vector3(0.0, _wheel_height_m - 0.03, 0.0)))
	spin.add_child(Art.many(lug, lugs, Art.material(TIRE, 0.95)))


# The weight box: an open tray on runners, stacked with weights.
func _build_box() -> void:
	var size := BOX_SIZE_M
	var tray := _painted(Vector3(size.x, 0.08, size.z), PAINT_DARK)
	tray.position = Vector3(0.0, -size.y / 2.0 + 0.04, 0.0)
	_box.add_child(tray)
	for side: float in [-1.0, 1.0]:
		var wall := _painted(Vector3(0.06, size.y * 0.6, size.z), PAINT)
		wall.position = Vector3(side * size.x / 2.0, -size.y * 0.2, 0.0)
		_box.add_child(wall)
		var end := _painted(Vector3(size.x, size.y * 0.6, 0.06), PAINT)
		end.position = Vector3(0.0, -size.y * 0.2, side * size.z / 2.0)
		_box.add_child(end)
	var blocks: Array[Transform3D] = []
	var block := BoxMesh.new()
	block.size = Vector3(0.6, 0.28, 0.45)
	for layer in 3:
		for column in 3:
			for row in 3:
				if layer == 2 and (column + row) % 2 == 1:
					continue
				blocks.append(Transform3D(Basis(), Vector3((column - 1) * 0.62,
						-size.y / 2.0 + 0.22 + layer * 0.29, (row - 1) * 0.48)))
	_box.add_child(Art.many(block, blocks, Art.material(WEIGHT, 0.6, 0.4)))
	# The name on both sides, where the cameras see it past the operator.
	for side: float in [-1.0, 1.0]:
		var text := Art.label("TRANSFER SLED", 36, Color.WHITE, PAINT_DARK)
		text.position = Vector3(side * (size.x / 2.0 + 0.04), -size.y * 0.2, 0.0)
		text.rotation.y = side * PI / 2.0
		_box.add_child(text)
	add_child(_box)


# The operator's platform behind the wheels: a seat, the operator, the
# controls, and the lamp on its mast.
func _build_platform(rail_z_m: float) -> void:
	var deck := Art.box(Vector3(1.4, 0.06, 0.9), STEEL, 0.5, 0.5)
	deck.position = TrackSpace.point(-0.95, 0.0, rail_z_m)
	add_child(deck)
	var seat := Art.box(Vector3(0.45, 0.1, 0.45), DARK)
	seat.position = TrackSpace.point(-1.05, 0.0, rail_z_m + 0.45)
	add_child(seat)
	var stand := Art.box(Vector3(0.1, 0.4, 0.1), DARK)
	stand.position = TrackSpace.point(-1.05, 0.0, rail_z_m + 0.2)
	add_child(stand)
	var controls := _painted(Vector3(0.5, 0.5, 0.25), PAINT_DARK)
	controls.position = TrackSpace.point(-0.65, 0.35, rail_z_m + 0.35)
	add_child(controls)
	# In a high-visibility shirt, seated facing the tractor (TrackSpace's x
	# is the world's -z; People face +z), their hips on the seat.
	var operator := People.one(true, OPERATOR)
	operator.rotation.y = PI
	operator.position = TrackSpace.point(-0.63, 0.0, rail_z_m + 0.05)
	add_child(operator)
	# A canopy over the operator, on four posts; the lamp's mast rises
	# through it, so the puller sees the lamp from the seat.
	for x_m: float in [-1.35, -0.55]:
		for side: float in [-1.0, 1.0]:
			var post := Art.box(Vector3(0.05, CANOPY_M, 0.05), DARK, 0.5, 0.4)
			post.position = TrackSpace.point(x_m, side * 0.62, rail_z_m + CANOPY_M / 2.0)
			add_child(post)
	var canopy := _painted(Vector3(1.5, 0.05, 1.1), PAINT)
	canopy.position = TrackSpace.point(-0.95, 0.0, rail_z_m + CANOPY_M)
	add_child(canopy)
	var mast := Art.cylinder(0.04, CANOPY_M + 0.5, STEEL, 8, 0.4, 0.6)
	mast.position = TrackSpace.point(-1.4, -0.5, rail_z_m + (CANOPY_M + 0.5) / 2.0)
	add_child(mast)
	var lamp_mesh := SphereMesh.new()
	lamp_mesh.radius = 0.14
	lamp_mesh.height = 0.28
	_lamp.mesh = lamp_mesh
	_lamp_material.emission_enabled = true
	_lamp_material.emission_energy_multiplier = 2.0
	_lamp.material_override = _lamp_material
	_lamp.position = TrackSpace.point(-1.4, -0.5, rail_z_m + CANOPY_M + 0.6)
	add_child(_lamp)


# A part in the sled's glossy paint.
static func _painted(size: Vector3, colour: Color) -> MeshInstance3D:
	var part := Art.box(size, colour)
	part.material_override = Art.paint(colour, 0.45)
	return part


func hook_position() -> Vector3:
	return _hook.global_position


func show_snapshot(s: Dictionary, delta: float = 0.0) -> void:
	position = TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	rotation.y = s["sled_heading_rad"]
	_box.position = TrackSpace.point(s["box_ahead_of_wheels_m"], 0.0,
			_wheel_height_m + RAIL_HEIGHT_M + 0.12 + BOX_SIZE_M.y / 2.0)
	_wheel_angle -= float(s["sled_speed_m_s"]) / _wheel_height_m * delta
	for spin in _wheel_spin:
		spin.rotation.x = wrapf(_wheel_angle, -PI, PI)
	_show_pushdown(s["pushdown"])
	_show_berm(s["berm_m3"])


# The berm as a low mound across the pan, half a cylinder flattened to
# BERM_FLATNESS of its depth, holding the simulation's volume.
func _show_berm(volume_m3: float) -> void:
	var depth_m := sqrt(2.0 * maxf(volume_m3, 0.0) / (PI * BERM_FLATNESS * WIDTH_M))
	_berm.visible = depth_m > 0.01
	# Turned a quarter about z, the mesh's own x is up and its y across.
	_berm.scale = Vector3(BERM_FLATNESS * depth_m, 1.0, depth_m)
	_berm.position = TrackSpace.point(_berm_ahead_m + depth_m * 0.6, 0.0, 0.0)


func _show_pushdown(share: float) -> void:
	_wheels.position = TrackSpace.point(0.0, 0.0, share * WHEEL_LIFT_M)
	var colour := LAMP_OFF.lerp(LAMP_ON, share)
	_lamp_material.albedo_color = colour
	_lamp_material.emission = colour


static func _ring(radius_m: float, thickness_m: float) -> TorusMesh:
	var mesh := TorusMesh.new()
	mesh.inner_radius = radius_m - thickness_m
	mesh.outer_radius = radius_m + thickness_m
	mesh.rings = 12
	mesh.ring_segments = 6
	return mesh
