# The sled, built in code from the run's sizes, after a modern Dutch
# transfer sled, 12.5 m long and 2.55 m wide (the most a trailer may be on
# European roads). Up front the pan, as wide as the sled, slides on the
# track from just behind the front fenders back to the middle of the sled;
# above it stands the narrower body, clad from the pan up, in which the
# weight box rides forward on two rails as the sled advances. The box is
# open on top: steel slabs as wide as it, stood in from its back towards
# its front. At the back a tandem axle of dual wheels (eight tires, as wide
# as the pan) with a fender over each side's, and behind them the rear
# section, clad all round down to about 30 cm over the track, with the
# operator's glass cab right at the back and the engine low between the
# cab and the box, a lamp on a mast and an exhaust stack at the cab's back
# corners. The hook is on a drawbar out of the body's nose, behind a wide
# black rubber stop plate and two big rubber fenders in steel frames (the
# tractor's rear tires stop against them: sled::Fenders), with an orange
# flag post up front. The push-down shows: as it comes on, the rear wheels
# are lifted off the track (drawn a little higher than the few centimetres
# a real sled lifts them, to be seen from the tractor's seat). The lamp is
# green while the chain pulls on the hook, red once the sled has stopped
# after the pull (until it drives back), and off otherwise.
#
# Two transport wheels stand either side of the pan, near its front, on
# arms hinged off the body. Driving back the sled rides on them, swung
# down outside the pan, with the pan lifted off the track: they come down
# as the weight box runs back at the end of the pull. Before the next pull
# the operator swings them up and over onto the pan beside the body,
# within the sled's width, setting the pan down (the sled preparation,
# while the tractor comes to the sled). The box holds as many slabs as its
# mass for the pull makes (show_box_mass).
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
const FENDER_SIDE_M := 0.83
const FENDER_SIZE_M := Vector3(0.8, 0.95, 0.3)
# The steel frame round each fender: its bars' thickness [m].
const FENDER_FRAME_M := 0.06
# The rubber stop plate across the front, over the fenders: this tall over
# the hook's height and this much wider than the fenders reach, within the
# sled's width [m].
const STOP_PLATE_HEIGHT_M := 1.2
const STOP_PLATE_PAST_M := 0.0
# The flag post up front on the left: its height over the body, how far in
# from the body's side, and its flag [m].
const FLAG_POST_M := 2.2
const FLAG_POST_IN_M := 0.12
const FLAG_SIZE_M := Vector2(0.45, 0.3)
const FLAG := Color(0.9, 0.08, 0.06)
# The tandem axle: each axle this far ahead and behind of the wheels' point
# of contact the simulation has [m].
const TANDEM_HALF_M := 0.55
# The gap between the tires side by side [m].
const WHEEL_CLEAR_M := 0.05
# How round the sled's parts are: the edges' radius [m].
const ROUND_M := 0.12
const ROUND_SMALL_M := 0.05
# The cab at the very back: glass over a steel frame [m]; the engine's low
# housing ahead of it, hardly seen [m].
const GLASS := Color(0.55, 0.7, 0.8, 0.35)
const ENGINE := Color(0.13, 0.13, 0.14)
const CAB_ACROSS_M := 2.0
const CAB_LENGTH_M := 1.4
const CAB_BEHIND_M := 0.1           # the cab's back inside the sled's rear
const ENGINE_HEIGHT_M := 0.3
const ENGINE_ACROSS_M := 1.5
# The sled reaches back at least this far past a weight box starting far
# behind the wheels [m]. The rear section's covers rise round the box to
# this far over its top, where the cab stands on four posts at the edges,
# the box free to start beneath it [m].
const REAR_PAST_BOX_M := 0.1
const COVER_OVER_BOX_M := 0.05
const CAB_POST_M := 0.08
const RUBBER := Color(0.06, 0.06, 0.06)
# The sled's look: its width and length, the frame's clearance over the
# rear wheels' tops, and the weight box's size [m].
const WIDTH_M := 2.55
const LENGTH_M := 12.5
const FRAME_OVER_WHEELS_M := 0.1
const CANOPY_M := 1.5             # the cab's roof above the deck
const BERM := Color(0.44, 0.31, 0.19)
const OPERATOR := Color(0.78, 0.88, 0.15)
const BERM_FLATNESS := 0.6        # its height, a share of its depth
const BERM_WIDTH_M := WIDTH_M + 0.2  # across the pan, spilling a little past it
# The body: round the weight box, a little narrower than the pan, clad
# from just over the pan (or this high over the track behind it) up to
# this far over the box's bottom, so the box's upper part and its slabs
# show [m]. Ahead of the box's furthest reach it narrows to the nose, the
# transport wheels folding up beside it [m].
const BODY_WIDTH_M := 2.48
const NOSE_WIDTH_M := 1.7
const COVER_THICK_M := 0.04
const BODY_OVER_PAN_M := 0.1
const COVER_GROUND_M := 0.3
const BODY_OVER_BOX_M := 0.4
# The rear section's deck over the rails [m]; it is clad down to
# COVER_GROUND_M.
const DECK_OVER_RAILS_M := 0.1
# The fenders over the tandem clear the tires by this much [m].
const WHEEL_FENDER_CLEAR_M := 0.12
const WHEEL_FENDER_PANELS := 6
const COVER_STRIPE := Color(0.08, 0.08, 0.09)
# The pan: its front edge this far behind the stop plate; it reaches back
# to the middle of the sled [m].
const PAN_BEHIND_PLATE_M := 0.45
# Where the close camera sees from: over the cab's front edge, this high
# over its roof, as a TV camera on the sled [m].
const CAMERA_MOUNT_UP_M := 0.35
# The box rides this far above the rails [m].
const BOX_LIFT_M := 0.12
const RAIL_GAP_M := 1.2
const BOX_SIZE_M := Vector3(2.36, 0.8, 1.9)
const BOX_WALL_M := 0.05
# How far the wheels are drawn lifted with the push-down fully set [m].
const WHEEL_LIFT_M := 0.15
const LUGS := 18
# The transport wheels, beside the nose: their size; down, this far clear
# outside the pan; up, standing on the pan beside
# the body, their middle this high [m]. How far down they lift the pan to
# drive back [m]; and how quickly they go up or down (a share a second:
# about two seconds).
const TRANSPORT_RADIUS_M := 0.3
const TRANSPORT_WIDTH_M := 0.22
const TRANSPORT_CLEAR_OF_PAN_M := 0.03
const TRANSPORT_UP_M := 0.75
const TRANSPORT_ARM_M := 0.06
const PAN_LIFT_M := 0.15
const TRANSPORT_RATE_PER_S := 0.5
# A weight in the box [kg]: a steel slab across the box, this thick
# [m], stood in from the box's back with this gap between slabs [m]. The
# box holds as many as fill its length: 11 t, the most it takes.
const BLOCK_KG := 500.0
const SLAB_THICK_M := 0.075
const SLAB_GAP_M := 0.005
const BOX_SLABS := 22

var _box := Node3D.new()
# Where the rails end behind the wheels [m, negative: behind].
var _rear_m := -1.0
# The rails' top over the track [m].
var _rail_z_m := 1.6
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
# cab's front edge, set as the cab is built.
var _camera_mount_m := TrackSpace.point(-3.0, 0.0, 3.5)
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
	_wheel_height_m = d["sled_wheel_radius_m"]
	var wheel_width_m: float = d["sled_wheel_width_m"]
	# The frame over the wheels' tops, the box riding on it.
	var rail_z_m := 2.0 * _wheel_height_m + FRAME_OVER_WHEELS_M
	_rail_z_m = rail_z_m
	_rear_m = rear_of(d)
	lamp_tension_n = d["take_up_tension_n"]
	var plate_m := _add_fenders(hook_ahead_m, hook_height_m, d["fender_behind_hook_m"])
	var nose_m := plate_m - 0.15
	var pan_front_m := plate_m - PAN_BEHIND_PLATE_M
	var pan_back_m := pan_back_of(d)

	# Two rails inside the body from the rear to the nose, and the drawbar
	# out of the nose down to the hook.
	var rail_to_m := nose_m - 0.4
	for side: float in [-1.0, 1.0]:
		var rail := _rounded(Vector3(0.16, 0.24, rail_to_m - _rear_m), PAINT_DARK, ROUND_SMALL_M,
				Vector3.AXIS_Z)
		rail.position = TrackSpace.point((_rear_m + rail_to_m) / 2.0, side * RAIL_GAP_M / 2.0,
				rail_z_m)
		add_child(rail)
		var neck_from := TrackSpace.point(rail_to_m, side * RAIL_GAP_M / 2.0, rail_z_m)
		var neck_to := TrackSpace.point(hook_ahead_m - 0.1, side * 0.15, hook_height_m)
		var neck := _rounded(Vector3(0.14, 0.18, neck_from.distance_to(neck_to)), PAINT_DARK,
				ROUND_SMALL_M, Vector3.AXIS_Z)
		neck.position = (neck_from + neck_to) / 2.0
		neck.basis = Basis.looking_at(neck_to - neck_from, Vector3.UP)
		add_child(neck)
	var hook_plate := Art.box(Vector3(0.4, 0.2, 0.2), DARK, 0.4, 0.6)
	hook_plate.position = TrackSpace.point(hook_ahead_m - 0.1, 0.0, hook_height_m)
	add_child(hook_plate)
	var ring := Art.instance(_ring(0.09, 0.025), Art.material(DARK, 0.3, 0.8))
	ring.position = TrackSpace.point(hook_ahead_m, 0.0, hook_height_m)
	add_child(ring)
	_hook.position = TrackSpace.point(hook_ahead_m, 0.0, hook_height_m)
	add_child(_hook)

	# The pan: a long steel plate as wide as the sled on the track from
	# just behind the stop plate back to the middle of the sled, its
	# leading edge turned up. (The simulation has its contact's middle at
	# pan_ahead_of_wheels_m; the plate drawn is the real one's length.)
	add_child(_pan)
	var pan := Art.box(Vector3(WIDTH_M, 0.06, pan_front_m - pan_back_m), STEEL, 0.7)
	pan.name = "Pan"
	pan.position = TrackSpace.point((pan_front_m + pan_back_m) / 2.0, 0.0, 0.03)
	_pan.add_child(pan)
	var lip := Art.box(Vector3(WIDTH_M, 0.05, 0.35), STEEL, 0.7)
	lip.position = TrackSpace.point(pan_front_m + 0.12, 0.0, 0.12)
	lip.rotation.x = 0.7
	_pan.add_child(lip)
	# The nose: from just ahead of the box's furthest reach forward.
	var nose_from_m := minf(float(d.get("box_end_ahead_of_wheels_m", d["pan_ahead_of_wheels_m"]))
			+ BOX_SIZE_M.z / 2.0 + 0.1, nose_m - 2.0 * TRANSPORT_RADIUS_M - 0.2)
	_add_transport_wheels((nose_from_m + nose_m) / 2.0)

	# The tandem axle at the back, dual wheels at each end of each axle:
	# eight tires, together as wide as the sled, their hubs included.
	add_child(_wheels)
	var hub_m := Art.RIM_PROUD_M + Art.HUB_REACH_M
	var tire_m := minf(wheel_width_m, (WIDTH_M - 2.0 * hub_m - 3.0 * WHEEL_CLEAR_M) / 4.0)
	var inner_m := tire_m / 2.0 + WHEEL_CLEAR_M / 2.0
	var outer_m := inner_m + tire_m + WHEEL_CLEAR_M
	for axle_m: float in [-TANDEM_HALF_M, TANDEM_HALF_M]:
		var axle := Art.axle_cylinder(0.08, 2.0 * outer_m, DARK, 12)
		axle.position = TrackSpace.point(axle_m, 0.0, _wheel_height_m)
		_wheels.add_child(axle)
		for side: float in [-1.0, 1.0]:
			_add_wheel(axle_m, side * inner_m, tire_m)
			_add_wheel(axle_m, side * outer_m, tire_m)
	_add_body(rail_z_m, nose_from_m, nose_m, pan_back_m)
	_add_rear_section(rail_z_m)
	_add_flag_post(pan_front_m, rail_z_m)

	# The berm, built at a unit size; show_snapshot scales it to the soil.
	_berm_ahead_m = pan_front_m + 0.25
	var berm_material := StandardMaterial3D.new()
	berm_material.albedo_color = BERM
	berm_material.normal_enabled = true
	berm_material.normal_texture = Art.ground_normal(0.2, 5, 8.0)
	berm_material.roughness = 1.0
	_berm.mesh = Art.cylinder_mesh(1.0, 1.0, BERM_WIDTH_M, 20)
	_berm.material_override = berm_material
	_berm.rotation.z = PI / 2.0
	_berm.name = "Berm"
	add_child(_berm)
	_build_box()
	_build_cab(rail_z_m, float(d.get("box_start_ahead_of_wheels_m", 0.0)) - BOX_SIZE_M.z / 2.0)
	_show_pushdown(0.0)
	_show_transport()
	_show_lamp(LAMP_DARK)


# The transport wheels: one either side of the pan near its front, each
# on an arm hinged off a bracket from the nose. Swung down (`transport` 1)
# the wheel stands on the track outside the pan; swung up and over (0) it
# stands on the pan beside the body, within the sled's width. Its hub
# faces out when down, so in when up.
func _add_transport_wheels(at_m: float) -> void:
	var hub_m := Art.RIM_PROUD_M + Art.HUB_REACH_M
	var down := Vector2(WIDTH_M / 2.0 + TRANSPORT_CLEAR_OF_PAN_M + TRANSPORT_WIDTH_M / 2.0,
			TRANSPORT_RADIUS_M)
	var up := Vector2(NOSE_WIDTH_M / 2.0 + 0.02 + hub_m + TRANSPORT_WIDTH_M / 2.0,
			TRANSPORT_UP_M)
	# The hinge halfway: half a turn about it takes the wheel from one to
	# the other.
	var hinge := (down + up) / 2.0
	var arm := down - hinge
	for side: float in [-1.0, 1.0]:
		var bracket := _painted(Vector3(hinge.x - NOSE_WIDTH_M / 2.0, 0.12, 0.16), PAINT_DARK)
		bracket.position = TrackSpace.point(at_m, side * (hinge.x + NOSE_WIDTH_M / 2.0) / 2.0,
				hinge.y)
		add_child(bracket)
		var holder := Node3D.new()
		holder.position = TrackSpace.point(at_m, side * hinge.x, hinge.y)
		holder.set_meta("side", side)
		add_child(holder)
		var bar := _painted(Vector3(TRANSPORT_ARM_M, arm.length(), TRANSPORT_ARM_M), PAINT_DARK)
		bar.position = TrackSpace.point(0.0, side * arm.x / 2.0, arm.y / 2.0)
		bar.rotation.z = side * atan2(arm.x, -arm.y)
		holder.add_child(bar)
		var spin := Node3D.new()
		spin.position = TrackSpace.point(0.0, side * arm.x, arm.y)
		holder.add_child(spin)
		Art.tire(spin, TRANSPORT_RADIUS_M, TRANSPORT_WIDTH_M, TIRE)
		Art.rim(spin, TRANSPORT_RADIUS_M * 0.6, TRANSPORT_WIDTH_M, side, 6, PAINT)
		_transport_wheels.append(holder)
		_transport_spin.append(spin)


# Where the transport wheels and the pan are drawn for `transport`: each
# arm turned up and over, outward first, half a turn when fully up.
func _show_transport() -> void:
	for holder in _transport_wheels:
		holder.rotation.z = -float(holder.get_meta("side")) * PI * (1.0 - transport)
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


# The slabs in the box: one for each BLOCK_KG of `box_mass_kg`, rounded,
# stood in from the box's back towards its front (at most what it holds).
func show_box_mass(box_mass_kg: float) -> void:
	_box_weights.multimesh.visible_instance_count = weight_count(box_mass_kg)


static func weight_count(box_mass_kg: float) -> int:
	return clampi(roundi(maxf(box_mass_kg, 0.0) / BLOCK_KG), 0, BOX_SLABS)


func weights_shown() -> int:
	return -1 if _box_weights == null else _box_weights.multimesh.visible_instance_count


# A cross beam behind the hook, and on each end a rubber fender facing the
# tractor, at its rear tires' height.
# Where the stop plate stands along the sled [m].
func _add_fenders(hook_ahead_m: float, hook_height_m: float, behind_hook_m: float) -> float:
	var face_m := hook_ahead_m - behind_hook_m
	var beam_m := face_m - FENDER_SIZE_M.z - 0.06
	var beam := _painted(Vector3(2.0 * FENDER_SIDE_M + FENDER_SIZE_M.x, 0.12, 0.12), PAINT_DARK)
	beam.position = TrackSpace.point(beam_m, 0.0, hook_height_m)
	add_child(beam)
	for side: float in [-1.0, 1.0]:
		var fender := Art.instance(Art.rounded_box(FENDER_SIZE_M, ROUND_SMALL_M, Vector3.AXIS_Z),
				Art.material(RUBBER, 0.95))
		fender.position = TrackSpace.point(face_m - FENDER_SIZE_M.z / 2.0, side * FENDER_SIDE_M,
				hook_height_m + 0.1)
		add_child(fender)
		# Its steel frame: a bar along each side and across the top, the
		# rubber standing proud of it.
		var frame_depth_m := FENDER_SIZE_M.z * 0.6
		var frame_x_m := face_m - FENDER_SIZE_M.z + frame_depth_m / 2.0
		for edge: float in [-1.0, 1.0]:
			var bar := _painted(Vector3(FENDER_FRAME_M, FENDER_SIZE_M.y, frame_depth_m),
					PAINT_DARK)
			bar.position = TrackSpace.point(frame_x_m, side * FENDER_SIDE_M
					+ edge * (FENDER_SIZE_M.x - FENDER_FRAME_M) / 2.0, hook_height_m + 0.1)
			add_child(bar)
		var top := _painted(Vector3(FENDER_SIZE_M.x, FENDER_FRAME_M, frame_depth_m), PAINT_DARK)
		top.position = TrackSpace.point(frame_x_m, side * FENDER_SIDE_M,
				hook_height_m + 0.1 + (FENDER_SIZE_M.y + FENDER_FRAME_M) / 2.0)
		add_child(top)
		var leg_m := hook_height_m + 0.1 - FENDER_SIZE_M.y / 2.0
		var leg := Art.box(Vector3(0.1, leg_m, 0.1), STEEL, 0.5, 0.5)
		leg.position = TrackSpace.point(beam_m, side * FENDER_SIDE_M, leg_m / 2.0)
		add_child(leg)
	# The wide rubber plate behind the fenders: what the tractor's tires see
	# coming back to the sled.
	var plate := Art.instance(Art.rounded_box(Vector3(2.0 * (FENDER_SIDE_M + FENDER_SIZE_M.x / 2.0
			+ STOP_PLATE_PAST_M), STOP_PLATE_HEIGHT_M, 0.08), ROUND_M, Vector3.AXIS_Z),
			Art.material(RUBBER, 0.95))
	plate.position = TrackSpace.point(beam_m - 0.1, 0.0,
			hook_height_m + 0.1 + STOP_PLATE_HEIGHT_M / 2.0 - FENDER_SIZE_M.y / 2.0)
	add_child(plate)
	return beam_m - 0.1


# The body from the rear section to the nose, a little narrower than the
# pan, in the sled's paint with a thin dark stripe: its sides clad from
# just over the pan (behind the pan, from COVER_GROUND_M; over the tandem,
# from over the tires) up past the box's bottom, a floor inside for the box
# to run over; ahead of the box's reach it narrows to the nose, decked
# over. Over the tandem on each side a fender out to the sled's width.
func _add_body(rail_z_m: float, nose_from_m: float, nose_m: float, pan_back_m: float) -> void:
	var top_z_m := rail_z_m + BOX_LIFT_M + BODY_OVER_BOX_M
	var arch_half_m := TANDEM_HALF_M + _wheel_height_m + WHEEL_FENDER_CLEAR_M
	var arch_top_z_m := 2.0 * _wheel_height_m + WHEEL_FENDER_CLEAR_M
	for side: float in [-1.0, 1.0]:
		# Over the pan, behind it, and over the tandem; and the nose's sides
		# (from, to, bottom, width).
		for part: Vector4 in [Vector4(pan_back_m, nose_from_m, BODY_OVER_PAN_M, BODY_WIDTH_M),
				Vector4(arch_half_m, pan_back_m, COVER_GROUND_M, BODY_WIDTH_M),
				Vector4(-arch_half_m, arch_half_m, arch_top_z_m, BODY_WIDTH_M),
				Vector4(nose_from_m, nose_m, BODY_OVER_PAN_M, NOSE_WIDTH_M)]:
			if part.y - part.x <= 0.0:
				continue
			var cover := _rounded(Vector3(COVER_THICK_M, top_z_m - part.z, part.y - part.x),
					PAINT, 0.015, Vector3.AXIS_Z)
			cover.name = "SideCover%d" % get_child_count()
			cover.position = TrackSpace.point((part.x + part.y) / 2.0,
					side * (part.w - COVER_THICK_M) / 2.0, (part.z + top_z_m) / 2.0)
			add_child(cover)
		# The shoulder where the body narrows to the nose.
		var shoulder := _painted(Vector3((BODY_WIDTH_M - NOSE_WIDTH_M) / 2.0,
				top_z_m - BODY_OVER_PAN_M, COVER_THICK_M), PAINT)
		shoulder.position = TrackSpace.point(nose_from_m, side * (BODY_WIDTH_M + NOSE_WIDTH_M)
				/ 4.0, (top_z_m + BODY_OVER_PAN_M) / 2.0)
		add_child(shoulder)
		var stripe := Art.box(Vector3(0.01, 0.05, nose_from_m + arch_half_m - 0.1), COVER_STRIPE,
				0.6)
		stripe.position = TrackSpace.point((nose_from_m - arch_half_m) / 2.0,
				side * (BODY_WIDTH_M / 2.0 + 0.005), top_z_m - 0.12)
		add_child(stripe)
		_add_wheel_fender(side, arch_top_z_m)
	var nose := _rounded(Vector3(NOSE_WIDTH_M, top_z_m - BODY_OVER_PAN_M, 0.12), PAINT,
			ROUND_M, Vector3.AXIS_Y)
	nose.position = TrackSpace.point(nose_m - 0.06, 0.0, (top_z_m + BODY_OVER_PAN_M) / 2.0)
	add_child(nose)
	# The body's floor between the rails' tops, from the rear section to
	# the nose, the box running over it.
	var floor := Art.box(Vector3(BODY_WIDTH_M - 2.0 * COVER_THICK_M, 0.04,
			nose_from_m + arch_half_m), STEEL, 0.6, 0.4)
	floor.position = TrackSpace.point((nose_from_m - arch_half_m) / 2.0, 0.0, rail_z_m + 0.08)
	add_child(floor)
	# The deck over the nose and the shoulders.
	var deck := _painted(Vector3(BODY_WIDTH_M, 0.06, nose_m - nose_from_m), PAINT_DARK)
	deck.position = TrackSpace.point((nose_m + nose_from_m) / 2.0, 0.0, top_z_m - 0.03)
	add_child(deck)


# The fender over one side's tandem: a flat top over both axles and quarter
# arcs down round the front and the rear wheel, from the body's side out to
# the sled's width.
func _add_wheel_fender(side: float, arch_top_z_m: float) -> void:
	var across_m := WIDTH_M / 2.0 - BODY_WIDTH_M / 2.0 + COVER_THICK_M
	var y_m := side * (WIDTH_M / 2.0 - across_m / 2.0)
	var radius_m := _wheel_height_m + WHEEL_FENDER_CLEAR_M
	var top := _painted(Vector3(across_m, 0.06, 2.0 * TANDEM_HALF_M), PAINT_DARK)
	top.name = "WheelFender%d" % (0 if side > 0.0 else 1)
	top.position = TrackSpace.point(0.0, y_m, arch_top_z_m)
	add_child(top)
	for end: float in [-1.0, 1.0]:
		var centre_m := end * TANDEM_HALF_M
		for i in WHEEL_FENDER_PANELS:
			# From the top (a quarter turn) down to the axle's height.
			var angle := lerpf(PI / 2.0, 0.0, (float(i) + 0.5) / WHEEL_FENDER_PANELS)
			var panel := _painted(Vector3(across_m, 0.06,
					radius_m * PI / 2.0 / WHEEL_FENDER_PANELS + 0.01), PAINT_DARK)
			panel.position = TrackSpace.point(centre_m + end * radius_m * cos(angle), y_m,
					_wheel_height_m + radius_m * sin(angle))
			panel.rotation.x = -end * (PI / 2.0 - angle)
			add_child(panel)


# The rear section behind the tandem: closed all round, as wide as the
# sled, from COVER_GROUND_M over the track up to its deck, and its covers
# rising at the sides and the back round the weight box where it starts,
# open ahead for the box to run out; the engine stands on the deck within
# them, the cab on top. A dark stripe along it as along the body.
func _add_rear_section(rail_z_m: float) -> void:
	var front_m := -(TANDEM_HALF_M + _wheel_height_m + WHEEL_FENDER_CLEAR_M)
	var deck_z_m := rail_z_m + DECK_OVER_RAILS_M
	var rear := _rounded(Vector3(WIDTH_M, deck_z_m - COVER_GROUND_M, front_m - _rear_m), PAINT,
			ROUND_M, Vector3.AXIS_Y)
	rear.name = "RearSection"
	rear.position = TrackSpace.point((front_m + _rear_m) / 2.0, 0.0,
			(deck_z_m + COVER_GROUND_M) / 2.0)
	add_child(rear)
	for side: float in [-1.0, 1.0]:
		# Within the width: proud of the side by a hair.
		var stripe := Art.box(Vector3(0.006, 0.05, front_m - _rear_m - 2.0 * ROUND_M),
				COVER_STRIPE, 0.6)
		stripe.position = TrackSpace.point((front_m + _rear_m) / 2.0, side * (WIDTH_M / 2.0
				- 0.0025), deck_z_m - 0.15)
		add_child(stripe)
	var wall_top_z_m := _cab_floor_z(rail_z_m)
	for side: float in [-1.0, 1.0]:
		var wall := _rounded(Vector3(COVER_THICK_M, wall_top_z_m - deck_z_m, front_m - _rear_m),
				PAINT, 0.015, Vector3.AXIS_Z)
		wall.name = "RearCover%d" % (0 if side > 0.0 else 1)
		wall.position = TrackSpace.point((front_m + _rear_m) / 2.0,
				side * (WIDTH_M - COVER_THICK_M) / 2.0, (deck_z_m + wall_top_z_m) / 2.0)
		add_child(wall)
	var back := _rounded(Vector3(WIDTH_M, wall_top_z_m - deck_z_m, COVER_THICK_M), PAINT, 0.015,
			Vector3.AXIS_Y)
	back.position = TrackSpace.point(_rear_m + COVER_THICK_M / 2.0, 0.0,
			(deck_z_m + wall_top_z_m) / 2.0)
	add_child(back)


# The cab's floor over the track: on the rear covers' top, over the box [m].
static func _cab_floor_z(rail_z_m: float) -> float:
	return rail_z_m + BOX_LIFT_M + BOX_SIZE_M.y + COVER_OVER_BOX_M


# The flag post up front on the left: an orange post from the body with a
# red flag at its top, as real sleds carry.
func _add_flag_post(pan_ahead_m: float, rail_z_m: float) -> void:
	var at_m := pan_ahead_m
	var from_z_m := rail_z_m + BOX_LIFT_M + BODY_OVER_BOX_M
	var out_m := NOSE_WIDTH_M / 2.0 - FLAG_POST_IN_M
	var post := _painted(Vector3(0.09, FLAG_POST_M, 0.09), PAINT)
	post.position = TrackSpace.point(at_m, out_m, from_z_m + FLAG_POST_M / 2.0)
	add_child(post)
	var arm := _painted(Vector3(0.09, 0.09, 0.4), PAINT)
	arm.position = TrackSpace.point(at_m + 0.2, out_m, from_z_m + FLAG_POST_M)
	add_child(arm)
	var cloth := QuadMesh.new()
	cloth.size = FLAG_SIZE_M
	var flag_material := Art.material(FLAG, 0.8)
	var flag := Art.instance(cloth, flag_material)
	flag.material_override = flag_material.duplicate()
	(flag.material_override as StandardMaterial3D).cull_mode = BaseMaterial3D.CULL_DISABLED
	flag.rotation.y = PI / 2.0
	flag.position = TrackSpace.point(at_m + 0.2, out_m + FLAG_SIZE_M.x / 2.0,
			from_z_m + FLAG_POST_M - FLAG_SIZE_M.y / 2.0)
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


# The weight box: an open steel box on a tray, the slabs standing in it
# across its width, the first at its back.
func _build_box() -> void:
	var size := BOX_SIZE_M
	var tray := _painted(Vector3(size.x, 0.08, size.z), PAINT_DARK)
	tray.position = Vector3(0.0, -size.y / 2.0 + 0.04, 0.0)
	_box.add_child(tray)
	for side: float in [-1.0, 1.0]:
		var wall := _painted(Vector3(BOX_WALL_M, size.y, size.z), PAINT)
		wall.position = Vector3(side * (size.x - BOX_WALL_M) / 2.0, 0.0, 0.0)
		_box.add_child(wall)
		var end := _painted(Vector3(size.x, size.y, BOX_WALL_M), PAINT)
		end.position = Vector3(0.0, 0.0, side * (size.z - BOX_WALL_M) / 2.0)
		_box.add_child(end)
	# Every place a slab can go, the back one first (down the track is -z,
	# so the box's back is +z): show_box_mass() loads as many as the box's
	# mass makes.
	var slabs: Array[Transform3D] = []
	var slab := BoxMesh.new()
	var slab_height_m := size.y - 0.15
	slab.size = Vector3(size.x - 2.0 * BOX_WALL_M - 0.04, slab_height_m, SLAB_THICK_M)
	for i in BOX_SLABS:
		var z_m := size.z / 2.0 - BOX_WALL_M - SLAB_GAP_M - (i + 0.5) * (SLAB_THICK_M + SLAB_GAP_M)
		slabs.append(Transform3D(Basis(), Vector3(0.0, -size.y / 2.0 + 0.08 + slab_height_m / 2.0,
				z_m)))
	_box_weights = Art.many(slab, slabs, Art.material(WEIGHT, 0.6, 0.4))
	_box.add_child(_box_weights)
	add_child(_box)


# The cab at the very back of the rear section's deck: glass on a steel
# frame, the roof in the sled's paint, the operator seated facing the
# tractor; the engine's low dark housing between the cab and the box's
# rear at its start; the exhaust stack and the lamp's mast at the cab's
# back corners. The cab stands on four posts at the sled's edges, clear of
# the box: a class whose box starts far behind the wheels (its weight
# levers the pan up for the start) has it start beneath the cab, and the
# engine then sits out of sight.
func _build_cab(rail_z_m: float, box_rear_m: float) -> void:
	var cab_back_m := _rear_m + CAB_BEHIND_M
	var cab_front_m := cab_back_m + CAB_LENGTH_M
	var cab_x_m := (cab_back_m + cab_front_m) / 2.0
	var deck_z_m := _cab_floor_z(rail_z_m)
	var post_z_m := rail_z_m + DECK_OVER_RAILS_M
	for x_m: float in [cab_back_m + CAB_POST_M, cab_front_m - CAB_POST_M]:
		for side: float in [-1.0, 1.0]:
			var post := _painted(Vector3(CAB_POST_M, deck_z_m - post_z_m, CAB_POST_M), PAINT_DARK)
			# Half in the rear cover, clear of the box.
			post.position = TrackSpace.point(x_m, side * (WIDTH_M / 2.0 - COVER_THICK_M
					- CAB_POST_M / 2.0 + 0.03), (post_z_m + deck_z_m) / 2.0)
			add_child(post)
	var floor := Art.box(Vector3(CAB_ACROSS_M + 0.1, 0.06, CAB_LENGTH_M + 0.1), STEEL, 0.5, 0.5)
	floor.position = TrackSpace.point(cab_x_m, 0.0, deck_z_m + 0.03)
	add_child(floor)
	var seat := Art.box(Vector3(0.45, 0.1, 0.45), DARK)
	seat.position = TrackSpace.point(cab_x_m - 0.15, 0.0, deck_z_m + 0.45)
	add_child(seat)
	var stand := Art.box(Vector3(0.1, 0.4, 0.1), DARK)
	stand.position = TrackSpace.point(cab_x_m - 0.15, 0.0, deck_z_m + 0.2)
	add_child(stand)
	var controls := _painted(Vector3(0.5, 0.5, 0.25), PAINT_DARK)
	controls.position = TrackSpace.point(cab_x_m + 0.35, 0.3, deck_z_m + 0.35)
	add_child(controls)
	# In a high-visibility shirt, seated facing the tractor (TrackSpace's x
	# is the world's -z; People face +z), their hips on the seat.
	var operator := People.one(true, OPERATOR)
	operator.rotation.y = PI
	operator.position = TrackSpace.point(cab_x_m + 0.27, 0.0, deck_z_m + 0.05)
	add_child(operator)
	var glass_material := StandardMaterial3D.new()
	glass_material.albedo_color = GLASS
	glass_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	glass_material.roughness = 0.1
	var glass := Art.instance(Art.rounded_box(Vector3(CAB_ACROSS_M, CANOPY_M, CAB_LENGTH_M),
			ROUND_M, Vector3.AXIS_Y), glass_material)
	glass.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	glass.position = TrackSpace.point(cab_x_m, 0.0, deck_z_m + CANOPY_M / 2.0)
	add_child(glass)
	for x_m: float in [cab_back_m, cab_front_m]:
		for side: float in [-1.0, 1.0]:
			var post := Art.box(Vector3(0.06, CANOPY_M, 0.06), DARK, 0.5, 0.4)
			post.position = TrackSpace.point(x_m - signf(x_m - cab_x_m) * ROUND_M * 0.3,
					side * (CAB_ACROSS_M / 2.0 - ROUND_M * 0.3), deck_z_m + CANOPY_M / 2.0)
			add_child(post)
	var canopy := _rounded(Vector3(CAB_ACROSS_M + 0.15, 0.1, CAB_LENGTH_M + 0.15), PAINT,
			ROUND_M, Vector3.AXIS_Y)
	canopy.position = TrackSpace.point(cab_x_m, 0.0, deck_z_m + CANOPY_M)
	add_child(canopy)
	var engine_from_m := cab_front_m + 0.1
	var engine_to_m := box_rear_m - 0.1
	if engine_to_m - engine_from_m > 0.2:
		var engine := Art.instance(Art.rounded_box(Vector3(ENGINE_ACROSS_M, ENGINE_HEIGHT_M,
				engine_to_m - engine_from_m), ROUND_SMALL_M, Vector3.AXIS_Z),
				Art.paint(ENGINE, 0.6))
		engine.position = TrackSpace.point((engine_from_m + engine_to_m) / 2.0, 0.0,
				post_z_m + ENGINE_HEIGHT_M / 2.0)
		add_child(engine)
	var corner_y_m := CAB_ACROSS_M / 2.0 + 0.12
	var stack_from_m := deck_z_m
	var stack_m := deck_z_m + CANOPY_M + 0.4 - stack_from_m
	var exhaust := Art.cylinder(0.07, stack_m, Color(0.75, 0.75, 0.78), 10, 0.25, 0.9)
	exhaust.position = TrackSpace.point(cab_back_m + 0.2, -corner_y_m, stack_from_m + stack_m / 2.0)
	add_child(exhaust)
	_camera_mount_m = TrackSpace.point(cab_front_m, 0.0, deck_z_m + CANOPY_M + CAMERA_MOUNT_UP_M)
	var mast_m := deck_z_m + CANOPY_M + 0.5 - stack_from_m
	var mast := Art.cylinder(0.04, mast_m, STEEL, 8, 0.4, 0.6)
	mast.position = TrackSpace.point(cab_back_m + 0.2, corner_y_m, stack_from_m + mast_m / 2.0)
	add_child(mast)
	var lamp_mesh := SphereMesh.new()
	lamp_mesh.radius = 0.14
	lamp_mesh.height = 0.28
	_lamp.mesh = lamp_mesh
	_lamp_material.emission_enabled = true
	_lamp_material.emission_energy_multiplier = 2.0
	_lamp.material_override = _lamp_material
	_lamp.position = TrackSpace.point(cab_back_m + 0.2, corner_y_m, deck_z_m + CANOPY_M + 0.6)
	add_child(_lamp)


# A part in the sled's glossy paint with its edges along `along` rounded.
static func _rounded(size: Vector3, colour: Color, radius_m: float, along: int) -> MeshInstance3D:
	return Art.instance(Art.rounded_box(size, radius_m, along), Art.paint(colour, 0.45))


# A part in the sled's glossy paint.
static func _painted(size: Vector3, colour: Color) -> MeshInstance3D:
	var part := Art.box(size, colour)
	part.material_override = Art.paint(colour, 0.45)
	return part


# The sled's rear behind the wheels: LENGTH_M from its hook, or just past
# a weight box starting further back [m, negative behind].
static func rear_of(d: Dictionary) -> float:
	var box_rear_m := float(d.get("box_start_ahead_of_wheels_m", 0.0)) - BOX_SIZE_M.z / 2.0
	return minf(float(d["hook_ahead_of_wheels_m"]) - LENGTH_M, box_rear_m - REAR_PAST_BOX_M)


# The pan's back edge ahead of the wheels: the middle of the sled, never
# ahead of the simulation's contact [m].
static func pan_back_of(d: Dictionary) -> float:
	return minf((float(d["hook_ahead_of_wheels_m"]) + rear_of(d)) / 2.0,
			float(d["pan_ahead_of_wheels_m"]))


func hook_position() -> Vector3:
	return _hook.global_position


# Over the sled's cab, in the world: the close camera stands as far
# behind the tractor (CameraRig.follow).
func camera_mount() -> Vector3:
	return transform * _camera_mount_m if not is_inside_tree() \
			else global_transform * _camera_mount_m


func show_snapshot(s: Dictionary, delta: float = 0.0) -> void:
	position = TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	rotation.y = s["sled_heading_rad"]
	_box.position = TrackSpace.point(s["box_ahead_of_wheels_m"], 0.0,
			_rail_z_m + BOX_LIFT_M + BOX_SIZE_M.y / 2.0)
	_wheel_angle -= float(s["sled_speed_m_s"]) / _wheel_height_m * delta
	for spin in _wheel_spin:
		spin.rotation.x = wrapf(_wheel_angle, -PI, PI)
	_show_pushdown(s["pushdown"])
	transport = move_toward(transport, transport_wanted(s), TRANSPORT_RATE_PER_S * delta)
	_show_transport()
	# Down on the track the transport wheels roll with the sled; lifted
	# clear, they stand still.
	if transport >= 1.0:
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
