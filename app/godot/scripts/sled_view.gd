# The sled, built in code from the run's sizes: two frame rails from the
# rear wheels to the hook, the pan sliding on the track up front, the weight
# box (an open tray stacked with weights) riding forward along the rails as
# the sled advances, the hook on a drawbar ahead of the pan with a rubber
# fender either side (the tractor's rear tires stop against them: the
# sled's fenders, sled::Fenders), and the operator's platform at the back with
# a lamp on a mast. The push-down shows: as it comes on, the rear wheels are
# lifted off the track (drawn a little higher than the few centimetres a
# real sled lifts them, to be seen from the tractor's seat). The lamp is
# green while the chain pulls on the hook, red once the sled has stopped
# after the pull (until it drives back), and off otherwise.
#
# Two transport wheels stand either side of the pan. Driving back the sled
# rides on them with the pan lifted off the track: they come down as the
# weight box runs back at the end of the pull. Before the next pull the
# operator lifts them again, setting the pan down (the sled preparation,
# while the tractor comes to the sled). The box holds as many weights as
# its mass for the pull makes (show_box_mass).
class_name SledView
extends Node3D

const PAINT := Color(0.95, 0.62, 0.08)
const PAINT_DARK := Color(0.6, 0.38, 0.05)
const STEEL := Color(0.27, 0.25, 0.23)
const DARK := Color(0.1, 0.1, 0.11)
const TIRE := Color(0.08, 0.08, 0.08)
const WEIGHT := Color(0.28, 0.29, 0.31)
const LAMP_GREEN := Color(0.2, 0.9, 0.25)
const LAMP_RED := Color(1.0, 0.15, 0.1)
const LAMP_DARK := Color(0.25, 0.25, 0.25)
# The fenders: either side of the sled's line, where a tractor's rear tires
# meet them; their size [m]. How far behind the hook their faces are comes
# with the run's sizes.
const FENDER_SIDE_M := 0.95
const FENDER_SIZE_M := Vector3(0.55, 0.6, 0.18)
const RUBBER := Color(0.06, 0.06, 0.06)
# The sled's look: its width, its rails' height above the wheels' axle and
# the weight box's size [m].
const WIDTH_M := 2.4
const CANOPY_M := 1.9             # the canopy above the platform
const BERM := Color(0.44, 0.31, 0.19)
const OPERATOR := Color(0.78, 0.88, 0.15)
const BERM_FLATNESS := 0.6        # its height, a share of its depth
const BERM_WIDTH_M := WIDTH_M + 0.2  # across the pan, spilling a little past it
const RAIL_HEIGHT_M := 0.25
# How far the rails reach behind the wheels: the sled's rear [m].
const REAR_BEHIND_WHEELS_M := 0.7
const RAIL_GAP_M := 1.7
const BOX_SIZE_M := Vector3(2.0, 1.0, 1.6)
const PAN_DEPTH_M := 1.2
# How far the wheels are drawn lifted with the push-down fully set [m].
const WHEEL_LIFT_M := 0.15
const LUGS := 18
# The transport wheels beside the pan: their size, how far they lift
# clear of the track for the pull, and how far down they lift the pan to
# drive back [m]; and how quickly they go up or down (a share a second:
# about two seconds).
const TRANSPORT_RADIUS_M := 0.3
const TRANSPORT_WIDTH_M := 0.22
const TRANSPORT_CLEAR_M := 0.2
const PAN_LIFT_M := 0.15
const TRANSPORT_RATE_PER_S := 0.5
# A weight in the box [kg], and how the box stacks them: a layer of
# BLOCK_COLUMNS × BLOCK_ROWS, at most BLOCK_LAYERS high.
const BLOCK_KG := 250.0
const BLOCK_COLUMNS := 3
const BLOCK_ROWS := 3
const BLOCK_LAYERS := 4

var _box := Node3D.new()
var _box_weights: MultiMeshInstance3D = null
# The pan and its lip, lifted on the transport wheels; the wheels on their
# arms, down when `transport` is 1.
var _pan := Node3D.new()
var _transport_wheels: Array[Node3D] = []
var _transport_spin: Array[Node3D] = []
# 0 the transport wheels up (the pan on the track) to 1 down (the pan
# lifted, the sled on them).
var transport := 0.0
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
# The chain pulls on the hook: at least the tension that takes up the slack
# (the run's take_up_tension_n) [N].
var lamp_tension_n := 1000.0


# As TractorView's: parts never added to the tree are freed here.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		for part: Node in [_box, _hook, _wheels, _lamp, _berm, _pan]:
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
	var rail_from_m := -REAR_BEHIND_WHEELS_M
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
	lamp_tension_n = d["take_up_tension_n"]
	_add_fenders(hook_ahead_m, hook_height_m, d["fender_behind_hook_m"])

	# The pan: a wide steel plate on the track, its leading edge turned up,
	# hung from the rails.
	add_child(_pan)
	var pan := Art.box(Vector3(WIDTH_M, 0.06, PAN_DEPTH_M), STEEL, 0.7)
	pan.position = TrackSpace.point(pan_ahead_m, 0.0, 0.03)
	_pan.add_child(pan)
	var lip := Art.box(Vector3(WIDTH_M, 0.05, 0.35), STEEL, 0.7)
	lip.position = TrackSpace.point(pan_ahead_m + PAN_DEPTH_M / 2.0 + 0.12, 0.0, 0.12)
	lip.rotation.x = 0.7
	_pan.add_child(lip)
	_add_transport_wheels(pan_ahead_m, rail_z_m)
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
	_berm.mesh = Art.cylinder_mesh(1.0, 1.0, BERM_WIDTH_M, 20)
	_berm.material_override = berm_material
	_berm.rotation.z = PI / 2.0
	add_child(_berm)
	_build_box()
	_build_platform(rail_z_m)
	_show_pushdown(0.0)
	_show_transport()
	_show_lamp(LAMP_DARK)


# A transport wheel either side of the pan, each on an arm down from the
# rail: the arm slides, so the wheel goes up and down with `transport`.
func _add_transport_wheels(pan_ahead_m: float, rail_z_m: float) -> void:
	for side: float in [-1.0, 1.0]:
		var across_m := side * (WIDTH_M / 2.0 + TRANSPORT_WIDTH_M / 2.0 + 0.06)
		var holder := Node3D.new()
		holder.position = TrackSpace.point(pan_ahead_m, across_m, 0.0)
		add_child(holder)
		var arm := _painted(Vector3(0.1, rail_z_m, 0.1), PAINT_DARK)
		arm.position = TrackSpace.point(0.0, 0.0, TRANSPORT_RADIUS_M + rail_z_m / 2.0)
		holder.add_child(arm)
		var spin := Node3D.new()
		spin.position = TrackSpace.point(0.0, 0.0, TRANSPORT_RADIUS_M)
		holder.add_child(spin)
		Art.tire(spin, TRANSPORT_RADIUS_M, TRANSPORT_WIDTH_M, TIRE)
		Art.rim(spin, TRANSPORT_RADIUS_M * 0.6, TRANSPORT_WIDTH_M, side, 6, PAINT)
		_transport_wheels.append(holder)
		_transport_spin.append(spin)
		# The bracket on the rail the arm slides in.
		var bracket := _painted(Vector3(0.16, 0.16, absf(across_m) - RAIL_GAP_M / 2.0 + 0.1),
				PAINT_DARK)
		bracket.position = TrackSpace.point(pan_ahead_m,
				side * (RAIL_GAP_M / 2.0 + absf(across_m)) / 2.0, rail_z_m)
		add_child(bracket)


# Where the transport wheels and the pan are drawn for `transport`.
func _show_transport() -> void:
	for holder in _transport_wheels:
		holder.position.y = (1.0 - transport) * TRANSPORT_CLEAR_M
	_pan.position = TrackSpace.point(0.0, 0.0, transport * PAN_LIFT_M)


# Down (1) while the sled goes back to the start line after the pull: from
# the moment the turn is over and the box runs back. Up (0) otherwise: the
# operator lifts them before the next pull.
static func transport_wanted(s: Dictionary) -> float:
	return 1.0 if s.get("stage", "pulling") in ["to_the_parking", "parked"] else 0.0


# At the start of a run: a whole turn starts with the sled on its transport
# wheels as it came back, and the operator lifts them while the tractor
# comes to the sled; a pull from the line starts with them up.
func start_run(s: Dictionary) -> void:
	transport = 1.0 if s.get("stage", "pulling") == "to_the_sled" else 0.0
	_show_transport()


# The weights in the box: one for each BLOCK_KG of `box_mass_kg`, rounded,
# stacked layer by layer from the bottom (at most what the box holds).
func show_box_mass(box_mass_kg: float) -> void:
	_box_weights.multimesh.visible_instance_count = weight_count(box_mass_kg)


static func weight_count(box_mass_kg: float) -> int:
	return clampi(roundi(maxf(box_mass_kg, 0.0) / BLOCK_KG), 0,
			BLOCK_COLUMNS * BLOCK_ROWS * BLOCK_LAYERS)


func weights_shown() -> int:
	return -1 if _box_weights == null else _box_weights.multimesh.visible_instance_count


# A cross beam behind the hook, and on each end a rubber fender facing the
# tractor, at its rear tires' height.
func _add_fenders(hook_ahead_m: float, hook_height_m: float, behind_hook_m: float) -> void:
	var face_m := hook_ahead_m - behind_hook_m
	var beam_m := face_m - FENDER_SIZE_M.z - 0.06
	var beam := _painted(Vector3(2.0 * FENDER_SIDE_M + FENDER_SIZE_M.x, 0.12, 0.12), PAINT_DARK)
	beam.position = TrackSpace.point(beam_m, 0.0, hook_height_m)
	add_child(beam)
	for side: float in [-1.0, 1.0]:
		var fender := Art.box(Vector3(FENDER_SIZE_M.x, FENDER_SIZE_M.y, FENDER_SIZE_M.z), RUBBER,
				0.95)
		fender.position = TrackSpace.point(face_m - FENDER_SIZE_M.z / 2.0, side * FENDER_SIDE_M,
				hook_height_m + 0.1)
		add_child(fender)
		var leg := Art.box(Vector3(0.1, hook_height_m + 0.1, 0.1), STEEL, 0.5, 0.5)
		leg.position = TrackSpace.point(beam_m, side * FENDER_SIDE_M, (hook_height_m + 0.1) / 2.0)
		add_child(leg)


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
	# Every place a weight can go, bottom layer first: show_box_mass()
	# shows as many as the box's mass makes.
	var blocks: Array[Transform3D] = []
	var block := BoxMesh.new()
	block.size = Vector3(0.6, 0.22, 0.45)
	for layer in BLOCK_LAYERS:
		for column in BLOCK_COLUMNS:
			for row in BLOCK_ROWS:
				blocks.append(Transform3D(Basis(), Vector3((column - 1) * 0.62,
						-size.y / 2.0 + 0.19 + layer * 0.23, (row - 1) * 0.48)))
	_box_weights = Art.many(block, blocks, Art.material(WEIGHT, 0.6, 0.4))
	_box.add_child(_box_weights)
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


# The middle of the sled's rear on the ground, in the world: where it sits
# and points, the dog-tail included.
func rear_position() -> Vector3:
	return global_transform * TrackSpace.point(-REAR_BEHIND_WHEELS_M, 0.0, 0.0)


func show_snapshot(s: Dictionary, delta: float = 0.0) -> void:
	position = TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	rotation.y = s["sled_heading_rad"]
	_box.position = TrackSpace.point(s["box_ahead_of_wheels_m"], 0.0,
			_wheel_height_m + RAIL_HEIGHT_M + 0.12 + BOX_SIZE_M.y / 2.0)
	_wheel_angle -= float(s["sled_speed_m_s"]) / _wheel_height_m * delta
	for spin in _wheel_spin:
		spin.rotation.x = wrapf(_wheel_angle, -PI, PI)
	_show_pushdown(s["pushdown"])
	transport = move_toward(transport, transport_wanted(s), TRANSPORT_RATE_PER_S * delta)
	_show_transport()
	# On the track the transport wheels roll with the sled.
	for spin in _transport_spin:
		spin.rotation.x = wrapf(-float(s["sled_x_m"]) / TRANSPORT_RADIUS_M, -PI, PI)
	_show_berm(s["berm_m3"])
	_show_lamp(lamp_colour(s, lamp_tension_n))


# The lamp: green once the chain pulls on the hook with at least
# `tension_n` while the slack is taken up, and from then on through waiting
# for the flag and the pull, however the chain eases; red once the pull is
# over until the sled drives back, off otherwise.
static func lamp_colour(s: Dictionary, tension_n: float) -> Color:
	match s.get("stage", "pulling"):
		"taking_up_slack":
			if float(s.get("chain_tension_n", 0.0)) >= tension_n:
				return LAMP_GREEN
		"waiting_for_the_flag", "pulling":
			return LAMP_GREEN
		"releasing", "to_the_parking", "parked":
			if float(s.get("sled_speed_m_s", 0.0)) >= 0.0 and not bool(s.get("sled_back", false)):
				return LAMP_RED
	return LAMP_DARK


func lamp_shown() -> Color:
	return _lamp_material.albedo_color


func _show_lamp(colour: Color) -> void:
	_lamp_material.albedo_color = colour
	_lamp_material.emission = colour
	_lamp_material.emission_energy_multiplier = 0.0 if colour == LAMP_DARK else 2.0


# The berm as a low mound across the pan, half a cylinder flattened to
# BERM_FLATNESS of its depth, holding the simulation's volume.
func _show_berm(volume_m3: float) -> void:
	var depth_m := sqrt(2.0 * maxf(volume_m3, 0.0) / (PI * BERM_FLATNESS * BERM_WIDTH_M))
	_berm.visible = depth_m > 0.01
	# Turned a quarter about z, the mesh's own x is up and its y across.
	_berm.scale = Vector3(BERM_FLATNESS * depth_m, 1.0, depth_m)
	_berm.position = TrackSpace.point(_berm_ahead_m + depth_m * 0.6, 0.0, 0.0)


func _show_pushdown(share: float) -> void:
	_wheels.position = TrackSpace.point(0.0, 0.0, share * WHEEL_LIFT_M)


static func _ring(radius_m: float, thickness_m: float) -> TorusMesh:
	var mesh := TorusMesh.new()
	mesh.inner_radius = radius_m - thickness_m
	mesh.outer_radius = radius_m + thickness_m
	mesh.rings = 12
	mesh.ring_segments = 6
	return mesh
