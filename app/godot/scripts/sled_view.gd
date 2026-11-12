# The sled, built in code from the run's sizes, after a modern transfer
# sled: two frame rails from the rear wheels to the hook, the pan sliding on
# the track up front, the weight box (a closed carrier with a flat deck, the
# weights inside) riding forward along the rails as the sled advances, the
# hook on a drawbar ahead of the pan behind a wide black rubber stop plate
# (the tractor's rear tires stop against it: the sled's fenders,
# sled::Fenders), an orange flag post up front, a tandem axle at the back
# and over it the operator's glass cab beside the engine's housing, with a
# lamp on a mast. The push-down shows: as it comes on, the rear wheels are
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

const PAINT := Color(0.97, 0.45, 0.07)
const PAINT_DARK := Color(0.66, 0.29, 0.05)
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
# The rubber stop plate across the front, over the fenders: this tall over
# the hook's height and this much wider than the fenders reach [m].
const STOP_PLATE_HEIGHT_M := 1.0
const STOP_PLATE_PAST_M := 0.15
# The flag post up front on the left: its height over the rails, how far
# out, and its flag [m].
const FLAG_POST_M := 2.6
const FLAG_POST_OUT_M := 1.05
const FLAG_SIZE_M := Vector2(0.45, 0.3)
const FLAG := Color(0.9, 0.08, 0.06)
# The tandem axle: each axle this far ahead and behind of the wheels' point
# of contact the simulation has [m].
const TANDEM_HALF_M := 0.55
# The cab: glass over a steel frame, beside the engine's housing.
const GLASS := Color(0.55, 0.7, 0.8, 0.35)
const ENGINE := Color(0.13, 0.2, 0.32)
const CAB_ACROSS_M := 1.0
const CAB_OFFSET_M := 0.32
const CAB_LENGTH_M := 1.0
const ENGINE_SIZE_M := Vector3(0.85, 0.75, 0.9)
const EXHAUST_M := 0.9
# The lamp's mast: at the cab's back corner on the left, clear of the
# engine (along, across) [m].
const LAMP_AT_M := Vector2(-1.5, CAB_OFFSET_M + CAB_ACROSS_M / 2.0)
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
# The operator's platform: its front edge behind the wheels [m]. Where the
# weight box starts behind the wheels, under it, the platform stands raised
# on stilts with this much room over the box [m], as real sleds' operators
# sit high at the back with the box running beneath them.
const PLATFORM_FRONT_M := -0.5
const PLATFORM_GAP_M := 0.1
# Where the close camera sees from: over the canopy's front edge, behind
# the wheels, this high over the canopy, as a TV camera on the sled [m].
const CAMERA_MOUNT_X_M := -0.45
const CAMERA_MOUNT_UP_M := 0.35
const STILT_M := 0.08
# The box rides this far above the rails [m].
const BOX_LIFT_M := 0.12
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
# How high the platform stands above the rails to clear the box where it
# starts beneath it [m], and where the rails end behind the wheels, under
# the box at its start [m, negative: behind].
var _platform_up_m := 0.0
var _rear_m := -REAR_BEHIND_WHEELS_M
var _box_weights: MultiMeshInstance3D = null
# The pan and its lip, lifted on the transport wheels; the wheels on their
# arms, down when `transport` is 1.
var _pan := Node3D.new()
var _transport_wheels: Array[Node3D] = []
var _transport_spin: Array[Node3D] = []
# 0 the transport wheels up (the pan on the track) to 1 down (the pan
# lifted, the sled on them).
var transport := 0.0
var _transport_angle := 0.0
var _hook := Marker3D.new()
# Where the close camera sees from (CameraRig), on the sled: over the
# canopy's front edge, set as the platform is built.
var _camera_mount_m := TrackSpace.point(CAMERA_MOUNT_X_M, 0.0, CANOPY_M + CAMERA_MOUNT_UP_M)
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
	# The box's rear at its start, behind the wheels on a sled whose box
	# starts there: under the platform, which is raised over it, and the
	# rails reach back beneath it.
	var box_rear_m := float(d.get("box_start_ahead_of_wheels_m", 0.0)) - BOX_SIZE_M.z / 2.0
	var under_platform := box_rear_m < PLATFORM_FRONT_M + PLATFORM_GAP_M
	_platform_up_m = BOX_LIFT_M + BOX_SIZE_M.y + PLATFORM_GAP_M if under_platform else 0.0
	_rear_m = minf(-REAR_BEHIND_WHEELS_M, box_rear_m - PLATFORM_GAP_M)

	# Two rails, cross members, and the gooseneck down to the hook.
	var rail_from_m := _rear_m
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

	# The tandem axle at the back, two wheels on each.
	add_child(_wheels)
	for axle_m: float in [-TANDEM_HALF_M, TANDEM_HALF_M]:
		var axle := Art.axle_cylinder(0.08, WIDTH_M - 0.1, DARK, 12)
		axle.position = TrackSpace.point(axle_m, 0.0, _wheel_height_m)
		_wheels.add_child(axle)
		for side: float in [-1.0, 1.0]:
			_add_wheel(axle_m, side * (WIDTH_M / 2.0 - wheel_width_m / 2.0), wheel_width_m)
	_add_flag_post(pan_ahead_m, rail_z_m)

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
# the moment the chain is off and the box runs back. Up (0) otherwise: the
# operator lifts them before the next pull. A pull from the line ends
# parked with the chain still on and the sled where it stopped: up.
static func transport_wanted(s: Dictionary) -> float:
	var going_back: bool = s.get("stage", "pulling") in ["to_the_parking", "parked"] \
			and not bool(s.get("chain_hooked", true))
	return 1.0 if going_back else 0.0


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
	# The wide rubber plate behind the fenders: what the tractor's tires see
	# coming back to the sled.
	var plate := Art.box(Vector3(2.0 * (FENDER_SIDE_M + FENDER_SIZE_M.x / 2.0 + STOP_PLATE_PAST_M),
			STOP_PLATE_HEIGHT_M, 0.08), RUBBER, 0.95)
	plate.position = TrackSpace.point(beam_m - 0.1, 0.0,
			hook_height_m + 0.1 + STOP_PLATE_HEIGHT_M / 2.0 - FENDER_SIZE_M.y / 2.0)
	add_child(plate)


# The flag post up front on the left: an orange post from the rail with a
# red flag at its top, as real sleds carry.
func _add_flag_post(pan_ahead_m: float, rail_z_m: float) -> void:
	var at_m := pan_ahead_m + PAN_DEPTH_M / 2.0
	var post := _painted(Vector3(0.09, FLAG_POST_M, 0.09), PAINT)
	post.position = TrackSpace.point(at_m, FLAG_POST_OUT_M, rail_z_m + FLAG_POST_M / 2.0)
	add_child(post)
	var arm := _painted(Vector3(0.09, 0.09, 0.4), PAINT)
	arm.position = TrackSpace.point(at_m + 0.2, FLAG_POST_OUT_M, rail_z_m + FLAG_POST_M)
	add_child(arm)
	var cloth := QuadMesh.new()
	cloth.size = FLAG_SIZE_M
	var flag_material := Art.material(FLAG, 0.8)
	var flag := Art.instance(cloth, flag_material)
	flag.material_override = flag_material.duplicate()
	(flag.material_override as StandardMaterial3D).cull_mode = BaseMaterial3D.CULL_DISABLED
	flag.rotation.y = PI / 2.0
	flag.position = TrackSpace.point(at_m + 0.2, FLAG_POST_OUT_M + FLAG_SIZE_M.x / 2.0,
			rail_z_m + FLAG_POST_M - FLAG_SIZE_M.y / 2.0)
	add_child(flag)


func _add_wheel(x_m: float, side_m: float, width_m: float) -> void:
	var spin := Node3D.new()
	spin.position = TrackSpace.point(x_m, side_m, _wheel_height_m)
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


# The weight box: a closed carrier on runners with a flat deck, the weights
# inside.
func _build_box() -> void:
	var size := BOX_SIZE_M
	var tray := _painted(Vector3(size.x, 0.08, size.z), PAINT_DARK)
	tray.position = Vector3(0.0, -size.y / 2.0 + 0.04, 0.0)
	_box.add_child(tray)
	for side: float in [-1.0, 1.0]:
		var wall := _painted(Vector3(0.06, size.y, size.z), PAINT)
		wall.position = Vector3(side * size.x / 2.0, 0.0, 0.0)
		_box.add_child(wall)
		var end := _painted(Vector3(size.x, size.y, 0.06), PAINT)
		end.position = Vector3(0.0, 0.0, side * size.z / 2.0)
		_box.add_child(end)
		# A dark band along each side, low down, as the sled's frame shows.
		var band := Art.box(Vector3(0.02, 0.12, size.z - 0.1), DARK, 0.6)
		band.position = Vector3(side * (size.x / 2.0 + 0.035), -size.y / 2.0 + 0.2, 0.0)
		_box.add_child(band)
	var deck := _painted(Vector3(size.x + 0.12, 0.06, size.z + 0.12), PAINT)
	deck.position = Vector3(0.0, size.y / 2.0, 0.0)
	_box.add_child(deck)
	# Every place a weight can go, bottom layer first: show_box_mass()
	# loads as many as the box's mass makes.
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
	add_child(_box)


# The operator's platform behind the wheels: a seat, the operator, the
# controls, and the lamp on its mast.
func _build_platform(rail_z_m: float) -> void:
	# Raised over the box where it starts beneath: four stilts from the
	# rails up to the deck.
	if _platform_up_m > 0.0:
		for x_m: float in [-1.35, -0.55]:
			for side: float in [-1.0, 1.0]:
				var stilt := _painted(Vector3(STILT_M, _platform_up_m, STILT_M), PAINT_DARK)
				stilt.position = TrackSpace.point(x_m, side * (BOX_SIZE_M.x / 2.0 + STILT_M),
						rail_z_m + _platform_up_m / 2.0)
				add_child(stilt)
	var deck_z_m := rail_z_m + _platform_up_m
	# Wide enough for the cab and the engine beside it.
	var deck_width_m := maxf(BOX_SIZE_M.x + 3.0 * STILT_M, WIDTH_M - 0.2)
	var deck := Art.box(Vector3(deck_width_m, 0.06, CAB_LENGTH_M + 0.1), STEEL, 0.5, 0.5)
	deck.position = TrackSpace.point(-0.95, 0.0, deck_z_m)
	add_child(deck)
	var seat := Art.box(Vector3(0.45, 0.1, 0.45), DARK)
	seat.position = TrackSpace.point(-1.05, CAB_OFFSET_M, deck_z_m + 0.45)
	add_child(seat)
	var stand := Art.box(Vector3(0.1, 0.4, 0.1), DARK)
	stand.position = TrackSpace.point(-1.05, CAB_OFFSET_M, deck_z_m + 0.2)
	add_child(stand)
	var controls := _painted(Vector3(0.5, 0.5, 0.25), PAINT_DARK)
	controls.position = TrackSpace.point(-0.65, CAB_OFFSET_M + 0.3, deck_z_m + 0.35)
	add_child(controls)
	# In a high-visibility shirt, seated facing the tractor (TrackSpace's x
	# is the world's -z; People face +z), their hips on the seat.
	var operator := People.one(true, OPERATOR)
	operator.rotation.y = PI
	operator.position = TrackSpace.point(-0.63, CAB_OFFSET_M, deck_z_m + 0.05)
	add_child(operator)
	# The cab over the operator: glass on a steel frame, the roof in the
	# sled's paint; the engine's housing beside it, with its exhaust.
	var cab_x_m := -0.95
	var cab_y_m := CAB_OFFSET_M
	var glass := Art.box(Vector3(CAB_ACROSS_M, CANOPY_M, CAB_LENGTH_M), GLASS, 0.1)
	var glass_material := StandardMaterial3D.new()
	glass_material.albedo_color = GLASS
	glass_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	glass_material.roughness = 0.1
	glass.material_override = glass_material
	glass.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	glass.position = TrackSpace.point(cab_x_m, cab_y_m, deck_z_m + CANOPY_M / 2.0)
	add_child(glass)
	for x_m: float in [cab_x_m - CAB_LENGTH_M / 2.0, cab_x_m + CAB_LENGTH_M / 2.0]:
		for side: float in [-1.0, 1.0]:
			var post := Art.box(Vector3(0.06, CANOPY_M, 0.06), DARK, 0.5, 0.4)
			post.position = TrackSpace.point(x_m, cab_y_m + side * CAB_ACROSS_M / 2.0,
					deck_z_m + CANOPY_M / 2.0)
			add_child(post)
	var canopy := _painted(Vector3(CAB_ACROSS_M + 0.15, 0.08, CAB_LENGTH_M + 0.15), PAINT)
	canopy.position = TrackSpace.point(cab_x_m, cab_y_m, deck_z_m + CANOPY_M)
	add_child(canopy)
	var engine := Art.box(ENGINE_SIZE_M, ENGINE, 0.5, 0.3)
	engine.position = TrackSpace.point(cab_x_m, cab_y_m - CAB_ACROSS_M / 2.0
			- ENGINE_SIZE_M.x / 2.0 - 0.05, deck_z_m + ENGINE_SIZE_M.y / 2.0)
	add_child(engine)
	var exhaust := Art.cylinder(0.05, EXHAUST_M, DARK, 8, 0.4, 0.6)
	exhaust.position = engine.position + Vector3(0.0, ENGINE_SIZE_M.y / 2.0 + EXHAUST_M / 2.0,
			0.25)
	add_child(exhaust)
	_camera_mount_m = TrackSpace.point(CAMERA_MOUNT_X_M, 0.0,
			deck_z_m + CANOPY_M + CAMERA_MOUNT_UP_M)
	var mast := Art.cylinder(0.04, CANOPY_M + 0.5, STEEL, 8, 0.4, 0.6)
	mast.position = TrackSpace.point(LAMP_AT_M.x, LAMP_AT_M.y,
			deck_z_m + (CANOPY_M + 0.5) / 2.0)
	add_child(mast)
	var lamp_mesh := SphereMesh.new()
	lamp_mesh.radius = 0.14
	lamp_mesh.height = 0.28
	_lamp.mesh = lamp_mesh
	_lamp_material.emission_enabled = true
	_lamp_material.emission_energy_multiplier = 2.0
	_lamp.material_override = _lamp_material
	_lamp.position = TrackSpace.point(LAMP_AT_M.x, LAMP_AT_M.y, deck_z_m + CANOPY_M + 0.6)
	add_child(_lamp)


# A part in the sled's glossy paint.
static func _painted(size: Vector3, colour: Color) -> MeshInstance3D:
	var part := Art.box(size, colour)
	part.material_override = Art.paint(colour, 0.45)
	return part


func hook_position() -> Vector3:
	return _hook.global_position


# Over the sled's canopy, in the world: the close camera stands as far
# behind the tractor (CameraRig.follow).
func camera_mount() -> Vector3:
	return transform * _camera_mount_m if not is_inside_tree() \
			else global_transform * _camera_mount_m


func show_snapshot(s: Dictionary, delta: float = 0.0) -> void:
	position = TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	rotation.y = s["sled_heading_rad"]
	_box.position = TrackSpace.point(s["box_ahead_of_wheels_m"], 0.0,
			_wheel_height_m + RAIL_HEIGHT_M + BOX_LIFT_M + BOX_SIZE_M.y / 2.0)
	_wheel_angle -= float(s["sled_speed_m_s"]) / _wheel_height_m * delta
	for spin in _wheel_spin:
		spin.rotation.x = wrapf(_wheel_angle, -PI, PI)
	_show_pushdown(s["pushdown"])
	transport = move_toward(transport, transport_wanted(s), TRANSPORT_RATE_PER_S * delta)
	_show_transport()
	# Down on the track the transport wheels roll with the sled; lifted
	# clear, they stand still.
	if transport > 0.0:
		_transport_angle -= float(s["sled_speed_m_s"]) / TRANSPORT_RADIUS_M * delta
		for spin in _transport_spin:
			spin.rotation.x = wrapf(_transport_angle, -PI, PI)
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
