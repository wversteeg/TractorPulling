# The tractor, built in code from the run's sizes so it matches the
# simulation: a farm-stock 2WD tractor with its hood and grille, a cab
# frame with the driver, fenders over the big rear wheels and their lugs,
# the exhaust stack, the drawbar at the hitch and the wheelie bar behind.
# A pulling tractor (one with a roll bar, the Sport classes) is drawn as
# today's are: a rounded two-tone bonnet sloping to its nose, a black frame
# out to the front weights, discs over the rear rims, big rounded fenders,
# a high-backed seat and a silver roll bar.
# The weights show where the puller set them: suitcase weights on the
# front bracket, wheel weights on the rear hubs, in proportion.
#
# The body pitches about the rear axle; the rear wheels turn at their
# sides' speeds and the front wheels steer. Black smoke rises with the
# throttle, heavier as the engine is pulled down, and the rear tires throw
# dirt as they slip. Placed from the snapshot each frame; the wheels'
# angles are only for drawing.
class_name TractorView
extends Node3D

const TIRE := Color(0.08, 0.08, 0.08)
const IRON := Color(0.22, 0.22, 0.24)
const FRAME := Color(0.06, 0.06, 0.06)
const CHROME := Color(0.8, 0.8, 0.82)
const WEIGHT := Color(0.9, 0.72, 0.1)
# Nearly clear and colourless: the cab and the driver show through it.
const GLASS := Color(0.92, 0.94, 0.94, 0.15)
const LAMP := Color(1.0, 0.95, 0.75)
const SMOKE := Color(0.16, 0.16, 0.16, 0.8)
const DIRT := Color(0.45, 0.31, 0.19)
const DUST := Color(0.66, 0.54, 0.42)
# A pulling tractor's bonnet: the dark of its lower panels and grille
# (its top is the look's trim), its width and height and how round it is,
# how far its top falls to the nose [m, rad]; its lamp strips.
const BONNET_DARK := Color(0.08, 0.08, 0.09)
const BONNET_WIDTH_M := 0.72
const BONNET_TOP_M := 0.82        # its back top edge, above the rear axle
const BONNET_HEIGHT_M := 0.8
const BONNET_ROUND_M := 0.2
const BONNET_FALL_RAD := 0.09
const LAMP_STRIP_M := Vector3(0.18, 0.035, 0.02)
# Its front frame: two black rails under the bonnet out past the front
# axle to a bumper, where the weights hang [m].
const FRAME_RAIL_M := Vector2(0.1, 0.16)
const FRAME_RAILS_APART_M := 0.5
# The pulling tractor's front frame: past the bonnet's nose to the bumper,
# at this height above the front axle, and the bumper's depth [m].
const FRAME_PAST_HOOD_M := 0.15
const FRAME_ABOVE_FRONT_AXLE_M := 0.3
const BUMPER_DEPTH_M := 0.14
# A pulling tractor's bonnet, frame and stack are drawn for one of this
# wheelbase (the Sport 2.5 t's) and grow with a longer one: a heavier class
# pulls a bigger tractor, its bonnet wider and taller [m]. No smaller than
# drawn, and at most this much bigger.
const BODY_WHEELBASE_M := 2.2
const BODY_MOST_SCALE := 1.4
# A weight rack (a model's "rack" nose) reaches this far past the hood's
# nose, instead of the bumper's FRAME_PAST_HOOD_M [m].
const RACK_PAST_HOOD_M := 0.5
# A pulling turbo's big stack (a model's "fat" one): this much thicker and
# longer than the usual one.
const FAT_STACK_RADIUS := 1.5
const FAT_STACK_LENGTH := 1.15
# The front weights hang this far ahead of the pulling tractor's bumper [m].
const WEIGHTS_AHEAD_OF_BUMPER_M := 0.03
# The pulling seat's back: this tall over the seat [m].
const RACING_SEAT_BACK_M := 0.7

# Shapes the simulation does not size [m].
const HOOD_WIDTH_M := 0.62
const HOOD_TOP_M := 0.65          # above the rear axle
const HOOD_PAST_FRONT_AXLE_M := 0.3
const CAB_ROOF_M := 1.75          # above the rear axle
const CAB_HALF_DEPTH_M := 0.55
# The cab-side exhaust ends this far over the cab's roof [m].
const SIDE_STACK_OVER_ROOF_M := 0.2
const CAB_FLOOR_M := 0.35         # above the rear axle
# A farm cab: its corner posts, the roof's thickness and how far it
# overhangs the glass, the beacon on it and the work lamps at its front,
# the mirrors out on their arms past the posts [m].
const CAB_POST_M := 0.07
const CAB_ROOF_THICK_M := 0.12
const CAB_ROOF_OVERHANG_M := 0.08
# The back window's sill, above the rear axle: level with the fenders'
# tops, a dark panel below it down to the floor [m].
const CAB_BACK_SILL_M := 0.9
const BEACON := Color(1.0, 0.55, 0.05)
const BEACON_RADIUS_M := 0.07
const WORK_LAMP_M := Vector3(0.12, 0.08, 0.06)
const MIRROR_ARM_M := 0.32
const MIRROR_M := Vector3(0.16, 0.22, 0.04)
# A pulling tractor's roll bar: a hoop behind the seat, clear of the
# driver's helmet, above the rear axle and behind it [m].
const ROLL_BAR_TOP_M := 1.62
const ROLL_BAR_BEHIND_M := 0.36
const ROLL_BAR_BRACE_BEHIND_M := 0.8
const ROLL_BAR_TUBE_M := 0.035
# A Supersport driver's cell: its half width (just wider than the seat),
# the top of its arches over the helmet, above the rear axle, its hoops
# along the tractor (behind the seat to before the face), the segments of
# each arch, and the ladder's rungs up each side, as shares of the posts'
# height [m].
const CELL_HALF_WIDTH_M := 0.34
const CELL_TOP_M := 1.86
const CELL_HOOPS_M: Array[float] = [-0.42, -0.12, 0.2]
const CELL_ARCH_SEGMENTS := 8         # a multiple of 4: the bars meet its quarters
const CELL_RUNGS: Array[float] = [0.35, 0.65]
# The cab camera: at the seated driver's eyes, a little in front of the
# face so the head does not fill the view, above the rear axle [m]; and
# its near plane, close enough for the steering wheel [m].
const EYES_AHEAD_M := 0.12
const EYES_UP_M := 1.38
const CAB_NEAR_M := 0.05
# The eyes look a little down, at the road ahead over the dashboard, so the
# wheel and the gauges sit in the bottom of the view [rad].
const EYES_DOWN_RAD := 0.2
# The steering wheel: its hub ahead of and above the rear axle, tilted back
# towards the driver [m, rad]; turned this many times the front wheels'
# angle (the steering box's ratio).
const WHEEL_AT := Vector3(0.43, 0.0, 1.03)
const WHEEL_TILT_RAD := 0.85
const WHEEL_RADIUS_M := 0.18
const STEERING_RATIO := 12.0
# The dashboard behind the wheel: a panel leaning back towards the driver
# with a rev counter and a boost gauge [m, rad]. The needles sweep this far
# from rest to the top of their dial [rad].
const DASH_AT := Vector3(0.7, 0.0, 0.92)
const DASH_SIZE := Vector3(0.5, 0.2, 0.06)
const DASH_TILT_RAD := 0.6
const GAUGE_RADIUS_M := 0.06
const NEEDLE_SWEEP_RAD := 4.2
# The rev counter's top, a share past the rated speed; its dial's ticks.
const REV_COUNTER_HEADROOM := 1.25
const DIAL_TICKS := 9
const DIAL := Color(0.93, 0.93, 0.9)
const NEEDLE := Color(0.9, 0.2, 0.1)
# The rear tires' tread, as fitted (the run's rear_tire_tread): lugs per
# side of the chevron, how tall, how thick along the tread, how long (a
# share of the tire's width), turned how far off square, and how far
# from the tread's middle they start (a share of the width) [m, rad]. A
# farm tire's few tall lugs, an open centre between the halves; one cut for
# pulling, lower; a puller tire's many low, sharp bars meeting in the
# middle as a V.
const TREADS := {
	"farm": {"lugs": 20, "height_m": 0.08, "thick_m": 0.11, "length": 0.46, "turn_rad": 0.62,
			"from_middle": 0.05},
	"cut": {"lugs": 23, "height_m": 0.06, "thick_m": 0.085, "length": 0.56, "turn_rad": 0.5,
			"from_middle": 0.0},
	"puller": {"lugs": 46, "height_m": 0.035, "thick_m": 0.045, "length": 0.54,
			"turn_rad": 0.42, "from_middle": 0.0},
}
# A big painted weight box on the front (a model's "box" nose): its size
# beside the bonnet's width, its height and depth [m].
const BOX_WIDER_M := 0.16
const BOX_HEIGHT_M := 0.42
const BOX_DEPTH_M := 0.38
# How far the box's middle sits above the frame's end, a share of its height.
const BOX_RISE := 0.3
# A front weight's thickness and the step from one to the next [m].
const PLATE_THICK_M := 0.045
const PLATE_PITCH_M := 0.05
# The weights at the sizes they come in, so any class's ballast draws
# right: a suitcase weight on the front bracket, a wheel weight on a rear
# hub [kg]; and as many as the bracket and a hub take.
const PLATE_KG := 45.0
const DISC_KG := 70.0
const MAX_PLATES := 12
const MAX_DISCS_A_SIDE := 4
# Taken off before rounding up, so a share that is a whole number of weights
# in doubles a hair over does not draw one more.
const WEIGHT_COUNT_TOLERANCE := 1e-6
# A wheel weight's thickness, the step from one to the next, and the gap
# that keeps the first clear of the hub cap [m].
const DISC_THICKNESS_M := 0.06
const DISC_PITCH_M := 0.065
const DISC_CLEAR_OF_CAP_M := 0.005
# Slip above which the rear tires throw dirt, and where they throw most.
const DIRT_FROM_SLIP := 0.08
const DIRT_FULL_SLIP := 0.5
# Speed from which the rolling tires raise dust [m/s].
const DUST_FROM_SPEED_M_S := 0.3

# The tractor's colours and number (its class's, or a rival's); set
# before build(). A new look builds the tractor again.
var look := TractorLook.new()
var _body := Node3D.new()        # pivots about the rear axle
var _cab_camera: Camera3D = null
var _hitch := Marker3D.new()
var _rear_left := Node3D.new()
var _rear_right := Node3D.new()
var _front_left_steer := Node3D.new()
var _front_right_steer := Node3D.new()
var _front_left := Node3D.new()
var _front_right := Node3D.new()
var _front_weights := Node3D.new()
var _rear_weights := Node3D.new()
var _smoke := CPUParticles3D.new()
var _dirt: Array[CPUParticles3D] = []
var _dust: Array[CPUParticles3D] = []
# What the tractor was built for (its look and sizes): built again when
# that changes, as after a move up to the next class.
var _built_for := ""
# Drawn as a pulling tractor (a roll bar, the Sport classes) or a farm one.
var _pulling_look := false
# The model drawn (TractorModels.style()), and the sizes it sets: the farm
# hood's top and the cab's roof above the rear axle, the bonnet's height
# as a share [m, m, share].
var _style: Dictionary = TractorModels.STYLES["farm_classic"]
var _hood_top_m := HOOD_TOP_M
var _cab_roof_m := CAB_ROOF_M
var _bonnet_share := 1.0
# Half the cab's width, from the rear track [m].
var _cab_half_width_m := 0.5
# How much bigger than drawn the pulling tractor's body is (BODY_WHEELBASE_M).
var _body_scale := 1.0
# A rebuilt tractor's farm hood, as much wider and taller (1 on a farm or a
# bonnet tractor); how far its frame reaches past the nose [m].
var _hood_scale := 1.0
var _frame_past_hood_m := FRAME_PAST_HOOD_M
# The front of the frame: its depth (a bumper, or a weight box) [m].
var _bumper_depth_m := BUMPER_DEPTH_M
# A weight box on the front (a model's "box" nose), its plates the weights.
var _weight_box := false
# The rear tires' tread drawn (TREADS).
var _tread: Dictionary = TREADS["cut"]
var _rear_radius_m := 1.0
var _front_radius_m := 0.5
var _rated_rad_s := 230.0
# The steering wheel, turned about its own axis, and the gauges' needles.
var _steering_wheel := Node3D.new()
var _rev_needle := Node3D.new()
var _boost_needle := Node3D.new()
var _stall_rad_s := 70.0
# The governor's limit: the rev counter's red zone starts there.
var _high_idle_rad_s := 250.0
# How many of the rev counter's ticks are red (for the tests).
var rev_red_ticks := 0
var _rear_left_angle := 0.0
var _rear_right_angle := 0.0
var _front_angle := 0.0


# The parts join the tree in build(), for the first pull; the game can quit
# before one (on the start screen), so the parts never added are freed here.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		_free_loose_parts()


func _free_loose_parts() -> void:
	for part: Node in [_body, _hitch, _rear_left, _rear_right, _front_left_steer,
			_front_right_steer, _front_left, _front_right, _front_weights, _rear_weights,
			_smoke, _steering_wheel, _rev_needle, _boost_needle]:
		if part.get_parent() == null:
			part.free()


# The tractor from the run's sizes and its look, once; every pull after
# that only moves the weights to where the puller set them, until another
# tractor or look comes.
func build(d: Dictionary) -> void:
	# The engine's speeds first: the dashboard's rev counter is drawn from them.
	_rated_rad_s = d["rated_omega_rad_s"]
	_high_idle_rad_s = d.get("high_idle_omega_rad_s", _rated_rad_s)
	_stall_rad_s = d["stall_omega_rad_s"]
	var built_for := _shape_of(d)
	if built_for != _built_for:
		if not _built_for.is_empty():
			_take_down()
		_built_for = built_for
		_build_body(d)
	_show_weights(d)


# The look and the sizes the tractor is drawn from; the weights are not.
func _shape_of(d: Dictionary) -> String:
	var sizes: Array = [look.name, look.paint, look.paint_dark, look.trim, look.rim, look.disc,
			look.ring, look.number, look.driver_shirt, look.style_id]
	for key: String in ["protection", "wheelbase_m", "rear_axle_height_m", "rear_track_m",
			"front_track_m", "rear_tire_radius_m", "rear_tire_width_m", "rear_tire_tread",
			"front_tire_radius_m",
			"front_tire_width_m", "hitch_behind_rear_axle_m", "hitch_height_m",
			"wheelie_bar_behind_rear_axle_m", "wheelie_bar_tip_height_m"]:
		sizes.append(d.get(key))
	sizes.append_array([_rated_rad_s, _high_idle_rad_s])
	return var_to_str(sizes)


# Everything built goes, for a new build: fresh parts in its place.
func _take_down() -> void:
	_free_loose_parts()
	remove_child(_body)
	_body.queue_free()
	_body = Node3D.new()
	_cab_camera = null
	_hitch = Marker3D.new()
	_rear_left = Node3D.new()
	_rear_right = Node3D.new()
	_front_left_steer = Node3D.new()
	_front_right_steer = Node3D.new()
	_front_left = Node3D.new()
	_front_right = Node3D.new()
	_front_weights = Node3D.new()
	_rear_weights = Node3D.new()
	_smoke = CPUParticles3D.new()
	_dirt.clear()
	_dust.clear()
	_steering_wheel = Node3D.new()
	_rev_needle = Node3D.new()
	_boost_needle = Node3D.new()
	rev_red_ticks = 0


func _build_body(d: Dictionary) -> void:
	var wheelbase_m: float = d["wheelbase_m"]
	var axle_height_m: float = d["rear_axle_height_m"]
	var rear_track_m: float = d["rear_track_m"]
	var front_track_m: float = d["front_track_m"]
	_rear_radius_m = d["rear_tire_radius_m"]
	_front_radius_m = d["front_tire_radius_m"]
	var rear_width_m: float = d["rear_tire_width_m"]
	_tread = TREADS.get(String(d.get("rear_tire_tread", "cut")), TREADS["cut"])
	var front_width_m: float = d["front_tire_width_m"]

	# The model: without one, a tractor behind a roll bar is a pulling
	# tractor, drawn as one, and one with a cab a farm tractor.
	_style = TractorModels.style(look.style_id, d.get("protection", "cab"))
	# A pulling or a rebuilt tractor: on a pulling frame, its parts drawn.
	var pulling := TractorModels.on_pulling_frame(_style)
	var bonnet: bool = _style["body"] == TractorModels.PULLING
	_pulling_look = pulling
	_body_scale = clampf(wheelbase_m / BODY_WHEELBASE_M, 1.0, BODY_MOST_SCALE) \
			if pulling else 1.0
	# A rebuilt tractor's farm hood grows with the tractor as a bonnet does.
	_hood_scale = _body_scale if not bonnet else 1.0
	_hood_top_m = HOOD_TOP_M * float(_style["hood_height"])
	_frame_past_hood_m = RACK_PAST_HOOD_M if _style.get("nose", "bumper") == "rack" \
			else FRAME_PAST_HOOD_M
	_weight_box = _style.get("nose", "bumper") == "box"
	_bumper_depth_m = BOX_DEPTH_M if _weight_box else BUMPER_DEPTH_M
	_bonnet_share = float(_style["hood_height"])
	_cab_roof_m = CAB_FLOOR_M + (CAB_ROOF_M - CAB_FLOOR_M) * float(_style["cab_height"])
	_body.position = TrackSpace.point(0.0, 0.0, axle_height_m)
	add_child(_body)
	_add_driveline(wheelbase_m, rear_track_m, front_track_m, axle_height_m)
	if bonnet:
		_add_bonnet(wheelbase_m)
	else:
		_add_hood(wheelbase_m)
	if pulling:
		_add_front_frame(wheelbase_m, axle_height_m)
	_add_cab(rear_track_m)
	_add_fenders(rear_track_m, rear_width_m, 12 if pulling else 7)
	_add_hitch_and_bar(d, axle_height_m, rear_track_m)

	_add_rear_wheel(_rear_left, rear_track_m / 2.0, rear_width_m, pulling)
	_add_rear_wheel(_rear_right, -rear_track_m / 2.0, rear_width_m, pulling)
	# The front axle sits at the front tires' radius above the ground.
	var front_axle := TrackSpace.point(wheelbase_m, 0.0, _front_radius_m - axle_height_m)
	for pair: Array in [[_front_left_steer, _front_left, 1.0],
			[_front_right_steer, _front_right, -1.0]]:
		var steer: Node3D = pair[0]
		steer.position = front_axle + TrackSpace.point(0.0, float(pair[2]) * front_track_m / 2.0,
				0.0)
		_body.add_child(steer)
		_add_front_wheel(steer, pair[1], front_width_m, float(pair[2]))

	# On the farm hood's front; on a pulling tractor, from its bumper.
	if pulling:
		_front_weights.position = TrackSpace.point(wheelbase_m + HOOD_PAST_FRONT_AXLE_M
				+ _frame_past_hood_m + _bumper_depth_m / 2.0 + WEIGHTS_AHEAD_OF_BUMPER_M, 0.0,
				_front_radius_m - axle_height_m + FRAME_ABOVE_FRONT_AXLE_M)
	else:
		_front_weights.position = TrackSpace.point(wheelbase_m + HOOD_PAST_FRONT_AXLE_M + 0.05,
				0.0, _front_radius_m - axle_height_m + 0.35)
	_body.add_child(_front_weights)
	_body.add_child(_rear_weights)
	_add_smoke(wheelbase_m)
	for side: float in [-1.0, 1.0]:
		_add_dirt(side * rear_track_m / 2.0, axle_height_m)


# The rear axle housing and gearbox, the engine under the hood, the front
# axle beam.
func _add_driveline(wheelbase_m: float, rear_track_m: float, front_track_m: float,
		axle_height_m: float) -> void:
	var gearbox := Art.box(Vector3(0.75, 0.62, 0.9), IRON, 0.5, 0.3)
	gearbox.position = TrackSpace.point(0.2, 0.0, 0.0)
	_body.add_child(gearbox)
	var trumpets := Art.axle_cylinder(0.09, rear_track_m - 0.2, IRON, 12, 0.5, 0.3)
	_body.add_child(trumpets)
	var engine := Art.box(Vector3(0.5, 0.55, wheelbase_m - 0.4), IRON, 0.5, 0.3)
	engine.position = TrackSpace.point(0.5 + (wheelbase_m - 0.4) / 2.0, 0.0, 0.05)
	_body.add_child(engine)
	var front_z_m := _front_radius_m - axle_height_m
	var beam := Art.box(Vector3(front_track_m - 0.25, 0.14, 0.18), IRON, 0.5, 0.3)
	beam.position = TrackSpace.point(wheelbase_m, 0.0, front_z_m)
	_body.add_child(beam)
	var pivot := Art.box(Vector3(0.3, 0.3, 0.4), IRON)
	pivot.position = TrackSpace.point(wheelbase_m, 0.0, front_z_m + 0.2)
	_body.add_child(pivot)


# A farm tractor's hood over the engine, in the model's shape: square (a
# box with a rounded top), round (an older hood, rounded all over and a
# little narrower at the front) or sloped (a modern one, falling to its
# nose, lamp strips in the grille); its stripe, louvres and number on each
# side, a grille with bars, and lamps.
func _add_hood(wheelbase_m: float) -> void:
	var front_m := wheelbase_m + HOOD_PAST_FRONT_AXLE_M
	var back_m := 0.75
	var length_m := front_m - back_m
	var top_m := _hood_top_m
	var shape: String = _style["hood"]
	# The hood's body: down to 0.25 m below the usual top, whatever its height.
	var depth_m := 0.5 + (top_m - HOOD_TOP_M)
	var hood := Node3D.new()
	hood.name = "Hood"
	_body.add_child(hood)
	var fall_rad := 0.0
	if shape == "sloped":
		# Falling to the nose from the windscreen.
		fall_rad = atan2(0.18 * float(_style["hood_height"]), length_m)
	hood.position = TrackSpace.point(back_m, 0.0, 0.0)
	hood.rotation.x = -fall_rad
	# Wider and taller on a bigger rebuilt tractor (sideways and up only).
	hood.scale = Vector3(_hood_scale, _hood_scale, 1.0)
	var block_height_m := depth_m if shape != "round" else depth_m * 0.55
	var block := Art.box(Vector3(HOOD_WIDTH_M, block_height_m, length_m), look.paint, 0.35)
	block.material_override = Art.paint(look.paint)
	# Its bottom where the usual hood's is, over the engine.
	block.position = TrackSpace.point(length_m / 2.0, 0.0,
			HOOD_TOP_M - 0.5 + block_height_m / 2.0)
	hood.add_child(block)
	var top := Art.instance(Art.cylinder_mesh(HOOD_WIDTH_M / 2.0, HOOD_WIDTH_M / 2.0, length_m, 18),
			Art.paint(look.paint))
	top.rotation.x = PI / 2.0
	# Square: a gently rounded top; round: a full half round down the sides.
	top.scale = Vector3(1.0, 1.0, 0.35 if shape != "round" else 1.25)
	top.position = TrackSpace.point(length_m / 2.0, 0.0,
			top_m - (0.0 if shape != "round" else HOOD_WIDTH_M * 0.62))
	hood.add_child(top)
	for side: float in [-1.0, 1.0]:
		var stripe := Art.box(Vector3(0.02, 0.06, length_m * 0.8), look.trim)
		stripe.position = TrackSpace.point(length_m * 0.55, side * (HOOD_WIDTH_M / 2.0 + 0.005),
				top_m - 0.12)
		hood.add_child(stripe)
		for i in 6:
			var louvre := Art.box(Vector3(0.02, 0.03, 0.18), look.paint_dark)
			louvre.position = TrackSpace.point(length_m - 0.35, side * (HOOD_WIDTH_M / 2.0 + 0.005),
					top_m - 0.22 - i * 0.05)
			hood.add_child(louvre)
		var number := Art.label(look.number, 90, Color.WHITE, Color.BLACK)
		number.position = TrackSpace.point(length_m * 0.35, side * (HOOD_WIDTH_M / 2.0 + 0.012),
				top_m - 0.3)
		number.rotation.y = side * -PI / 2.0
		hood.add_child(number)
	# The grille, its bars (chrome on a model that has them) and the lamps.
	var grille_height_m := depth_m * 1.1
	var grille := Art.box(Vector3(HOOD_WIDTH_M - 0.06, grille_height_m, 0.04), FRAME, 0.6)
	grille.position = TrackSpace.point(length_m + 0.01, 0.0, top_m - 0.05 - grille_height_m / 2.0)
	hood.add_child(grille)
	var bars := int(grille_height_m / 0.1)
	var bar_colour := CHROME if _style["chrome"] or shape != "sloped" else IRON
	for i in bars:
		var bar := Art.box(Vector3(HOOD_WIDTH_M - 0.1, 0.025, 0.03), bar_colour, 0.25, 0.8)
		bar.position = TrackSpace.point(length_m + 0.03, 0.0, top_m - 0.08 - i * 0.1)
		hood.add_child(bar)
	if _style["grille_lamps"] and shape == "sloped":
		# Lamp strips across the nose, a modern tractor's.
		for side: float in [-1.0, 1.0]:
			var strip := Art.instance(_box_mesh(Vector3(0.2, 0.04, 0.02)), _lamp(LAMP, 1.5))
			strip.position = TrackSpace.point(length_m + 0.05, side * 0.16, top_m - 0.12)
			hood.add_child(strip)
	else:
		for side: float in [-1.0, 1.0]:
			var lamp := Art.axle_cylinder(0.07, 0.05, LAMP, 12, 0.1)
			lamp.rotation = Vector3(PI / 2.0, 0.0, 0.0)
			var at_m := 0.22 if shape != "round" else 0.27
			lamp.position = TrackSpace.point(length_m + 0.04, side * at_m, top_m - 0.02)
			hood.add_child(lamp)
	if shape == "round":
		# An older tractor's: a round badge on the nose.
		var badge := Art.axle_cylinder(0.06, 0.02, look.trim, 16, 0.3)
		badge.rotation = Vector3(PI / 2.0, 0.0, 0.0)
		badge.position = TrackSpace.point(length_m + 0.04, 0.0, top_m - 0.18)
		hood.add_child(badge)


# A pulling tractor's bonnet: a rounded body in the paint over black lower
# panels, a white top, a dark nose with the grille and two lamp strips;
# tall at the dashboard and falling to the nose, as today's are. The
# model sets its height (its bottom stays on the frame) and whether it is
# rounded or square-edged.
func _add_bonnet(wheelbase_m: float) -> void:
	var front_m := wheelbase_m + HOOD_PAST_FRONT_AXLE_M
	var back_m := 0.75
	var length_m := front_m - back_m
	var width_m := BONNET_WIDTH_M * _body_scale
	var height_m := BONNET_HEIGHT_M * _body_scale * _bonnet_share
	var round_m := BONNET_ROUND_M * _body_scale if _style["hood"] != "square" else 0.04
	# Everything below hangs from the bonnet's back top edge, tilted down
	# to the front: x along the bonnet, z down from its top.
	var bonnet := Node3D.new()
	bonnet.name = "Bonnet"
	bonnet.position = TrackSpace.point(back_m, 0.0, bonnet_top_m())
	bonnet.rotation.x = -BONNET_FALL_RAD
	_body.add_child(bonnet)
	var middle_m := length_m / 2.0
	var body := Art.instance(Art.rounded_box(Vector3(width_m, height_m, length_m),
			round_m, Vector3.AXIS_Z), Art.paint(look.paint))
	body.position = TrackSpace.point(middle_m, 0.0, -height_m / 2.0)
	bonnet.add_child(body)
	var lower := Art.instance(Art.rounded_box(Vector3(width_m + 0.02, 0.22, length_m - 0.1),
			0.06, Vector3.AXIS_Z), Art.material(BONNET_DARK, 0.6))
	lower.position = TrackSpace.point(middle_m, 0.0, -height_m + 0.1)
	bonnet.add_child(lower)
	var top := Art.instance(Art.rounded_box(Vector3(width_m - 0.08, 0.1, length_m * 0.92),
			0.05, Vector3.AXIS_Z), Art.paint(look.trim))
	top.position = TrackSpace.point(middle_m + length_m * 0.03, 0.0, -0.02)
	bonnet.add_child(top)
	# The side vents, dark, and the number on each side.
	for side: float in [-1.0, 1.0]:
		var vent := Art.instance(Art.rounded_box(Vector3(0.02, 0.14, length_m * 0.35), 0.01,
				Vector3.AXIS_Z), Art.material(BONNET_DARK, 0.5))
		vent.position = TrackSpace.point(length_m * 0.72, side * (width_m / 2.0 + 0.004),
				-0.2)
		bonnet.add_child(vent)
		var number := Art.label(look.number, 70, Color.WHITE, Color.BLACK)
		number.position = TrackSpace.point(length_m * 0.3, side * (width_m / 2.0 + 0.012),
				-0.38)
		number.rotation.y = side * -PI / 2.0
		bonnet.add_child(number)
	var nose := Art.instance(Art.rounded_box(Vector3(width_m - 0.04, height_m - 0.1,
			0.12), 0.14, Vector3.AXIS_Z), Art.material(BONNET_DARK, 0.4))
	nose.position = TrackSpace.point(length_m + 0.03, 0.0, -height_m / 2.0)
	bonnet.add_child(nose)
	var strip := StandardMaterial3D.new()
	strip.albedo_color = LAMP
	strip.emission_enabled = true
	strip.emission = LAMP
	strip.emission_energy_multiplier = 1.5
	for side: float in [-1.0, 1.0]:
		var lamp := Art.instance(_box_mesh(LAMP_STRIP_M), strip)
		lamp.position = TrackSpace.point(length_m + 0.095, side * 0.2 * _body_scale, -0.14)
		bonnet.add_child(lamp)


# The bonnet's back top edge above the rear axle: a taller or lower
# bonnet on the same frame [m].
func bonnet_top_m() -> float:
	return (BONNET_TOP_M - BONNET_HEIGHT_M * (1.0 - _bonnet_share)) * _body_scale


# The black frame of a pulling tractor out to its front weights: two rails
# under the bonnet and a bumper ahead of the nose.
func _add_front_frame(wheelbase_m: float, axle_height_m: float) -> void:
	var from_m := wheelbase_m - 0.6
	var to_m := wheelbase_m + HOOD_PAST_FRONT_AXLE_M + _frame_past_hood_m
	var z_m := _front_radius_m - axle_height_m + FRAME_ABOVE_FRONT_AXLE_M
	var rail_size := Vector3(FRAME_RAIL_M.x * _body_scale, FRAME_RAIL_M.y * _body_scale,
			to_m - from_m)
	var rails_apart_m := FRAME_RAILS_APART_M * _body_scale
	for side: float in [-1.0, 1.0]:
		var rail := Art.instance(Art.rounded_box(rail_size, 0.03, Vector3.AXIS_Z),
				Art.material(BONNET_DARK, 0.5, 0.3))
		rail.position = TrackSpace.point((from_m + to_m) / 2.0, side * rails_apart_m / 2.0, z_m)
		_body.add_child(rail)
	var bumper: MeshInstance3D
	if _weight_box:
		# A big weight box in the trim, its front a grid of slots, sitting
		# on the frame's end.
		bumper = Art.instance(Art.rounded_box(Vector3(BONNET_WIDTH_M * _body_scale + BOX_WIDER_M,
				BOX_HEIGHT_M * _body_scale, BOX_DEPTH_M), 0.04, Vector3.AXIS_Z),
				Art.paint(look.trim))
		bumper.position = TrackSpace.point(to_m, 0.0, z_m + BOX_HEIGHT_M * _body_scale * BOX_RISE)
		for row in 2:
			var slot := Art.box(Vector3(BONNET_WIDTH_M * _body_scale * 0.7, 0.04, 0.02),
					BONNET_DARK, 0.6)
			slot.position = TrackSpace.point(to_m + BOX_DEPTH_M / 2.0 + 0.005, 0.0,
					z_m + BOX_HEIGHT_M * _body_scale * (0.42 - 0.22 * row))
			_body.add_child(slot)
	else:
		bumper = Art.instance(Art.rounded_box(Vector3(BONNET_WIDTH_M * _body_scale + 0.1,
				0.18 * _body_scale, BUMPER_DEPTH_M), 0.05,
				Vector3.AXIS_Z), Art.material(BONNET_DARK, 0.5, 0.3))
		bumper.position = TrackSpace.point(to_m, 0.0, z_m)
	bumper.name = "Bumper"
	_body.add_child(bumper)


# The floor, the seat, the steering wheel and the driver, in a cab frame
# (four posts, a roof and a screen) or, for a pulling tractor, open behind
# a roll bar.
func _add_cab(rear_track_m: float) -> void:
	var half_width_m := rear_track_m / 2.0 - 0.35
	_cab_half_width_m = half_width_m
	var floor := Art.box(Vector3(2.0 * half_width_m, 0.06, 2.0 * CAB_HALF_DEPTH_M), IRON)
	floor.position = TrackSpace.point(0.15, 0.0, 0.35)
	_body.add_child(floor)
	var cab: String = _style["cab"]
	match cab:
		"cab":
			_add_cab_frame(half_width_m)
		"canopy", "open":
			_add_frame_posts(half_width_m, cab == "canopy")
		"cage", "open_cage":
			_add_cage(half_width_m, cab == "cage")
		"cell":
			_add_cell()
		_:
			_add_roll_bar(half_width_m)
	# A pulling tractor's high-backed seat, but in a closed cab.
	_add_seat_and_driver(cab != "cab" and (_pulling_look or cab == "roll_bar"))


# A farm tractor's safety frame: two square posts behind the seat joined
# at the top, in black; with a canopy, a flat roof over the seat in the
# trim.
func _add_frame_posts(half_width_m: float, canopy: bool) -> void:
	var x_m := -0.42
	var height_m := _cab_roof_m - CAB_FLOOR_M
	for side: float in [-1.0, 1.0]:
		var post := Art.box(Vector3(0.08, height_m, 0.08), FRAME, 0.4)
		post.position = TrackSpace.point(x_m, side * (half_width_m - 0.05),
				CAB_FLOOR_M + height_m / 2.0)
		_body.add_child(post)
	var beam := Art.box(Vector3(2.0 * half_width_m, 0.08, 0.08), FRAME, 0.4)
	beam.position = TrackSpace.point(x_m, 0.0, _cab_roof_m)
	_body.add_child(beam)
	if canopy:
		var roof := Art.instance(Art.rounded_box(Vector3(2.0 * half_width_m + 0.1, 0.06, 1.2), 0.03,
				Vector3.AXIS_Z), Art.paint(look.trim))
		roof.name = "CabRoof"
		roof.position = TrackSpace.point(x_m + 0.5, 0.0, _cab_roof_m + 0.05)
		_body.add_child(roof)
		_add_roof_extras(x_m + 0.5, half_width_m, _cab_roof_m + 0.08)


# A Supersport driver's roll cage, as the class's real ones are: a narrow
# cell round the seat, three hoops one behind the other, each two posts up
# from the floor and an arch over the helmet, joined front to back by bars
# over the top and up the sides like a ladder; in black, chrome or the
# paint, as the model has it.
func _add_cell() -> void:
	var half_width_m := CELL_HALF_WIDTH_M
	var top_m := CAB_FLOOR_M + (CELL_TOP_M - CAB_FLOOR_M) * float(_style["cab_height"])
	var spring_m := top_m - half_width_m
	var finish: String = _style.get("cell", "black")
	var tube := Art.paint(CHROME, 0.3) if finish == "chrome" \
			else (Art.paint(look.paint) if finish == "paint" else Art.material(FRAME, 0.4, 0.4))
	var cell := Node3D.new()
	cell.name = "Cell"
	_body.add_child(cell)
	# Each hoop's points, from one foot over the arch to the other.
	var hoops: Array[PackedVector3Array] = []
	for x_m: float in CELL_HOOPS_M:
		var points := PackedVector3Array()
		points.append(Vector3(x_m, half_width_m, CAB_FLOOR_M))
		for i in CELL_ARCH_SEGMENTS + 1:
			var angle := PI * float(i) / CELL_ARCH_SEGMENTS
			points.append(Vector3(x_m, half_width_m * cos(angle),
					spring_m + half_width_m * sin(angle)))
		points.append(Vector3(x_m, -half_width_m, CAB_FLOOR_M))
		hoops.append(points)
		for i in points.size() - 1:
			_add_tube(cell, points[i], points[i + 1], ROLL_BAR_TUBE_M, tube)
	# Front to back: over the top and the arch's shoulders, and the ladder
	# rungs up each side.
	var quarter := floori(CELL_ARCH_SEGMENTS / 4.0)
	var joins: Array[int] = [1 + 2 * quarter, 1 + quarter, 1 + 3 * quarter, 1,
			1 + CELL_ARCH_SEGMENTS]
	for h in hoops.size() - 1:
		for k: int in joins:
			_add_tube(cell, hoops[h][k], hoops[h + 1][k], ROLL_BAR_TUBE_M * 0.8, tube)
		for side: float in [-1.0, 1.0]:
			for share: float in CELL_RUNGS:
				var z_m := CAB_FLOOR_M + (spring_m - CAB_FLOOR_M) * share
				_add_tube(cell, Vector3(CELL_HOOPS_M[h], side * half_width_m, z_m),
						Vector3(CELL_HOOPS_M[h + 1], side * half_width_m, z_m),
						ROLL_BAR_TUBE_M * 0.8, tube)


# A tube from `from` to `to` (the tractor's x along, y left, z up) [m].
func _add_tube(parent: Node3D, from: Vector3, to: Vector3, radius_m: float,
		material: Material) -> void:
	var a := TrackSpace.point(from.x, from.y, from.z)
	var b := TrackSpace.point(to.x, to.y, to.z)
	var length_m := a.distance_to(b)
	if length_m <= 0.0:
		return
	var tube := Art.instance(Art.cylinder_mesh(radius_m, radius_m, length_m, 8), material)
	tube.position = (a + b) / 2.0
	tube.basis = Basis(Quaternion(Vector3.UP, (b - a) / length_m))
	parent.add_child(tube)


# A pulling tractor's safety cage: four posts of tube, two rails along
# each side, with a roof over it all in the trim or open; no glass.
func _add_cage(half_width_m: float, roof: bool) -> void:
	var back_m := 0.15 - CAB_HALF_DEPTH_M
	var front_m := 0.15 + CAB_HALF_DEPTH_M
	var height_m := _cab_roof_m - CAB_FLOOR_M
	var cage := Art.paint(CHROME, 0.3) if _style["chrome"] else Art.material(FRAME, 0.4, 0.4)
	for x_m: float in [back_m, front_m]:
		for side: float in [-1.0, 1.0]:
			var post := Art.cylinder(ROLL_BAR_TUBE_M, height_m, CHROME, 10)
			post.material_override = cage
			post.position = TrackSpace.point(x_m, side * half_width_m, CAB_FLOOR_M + height_m / 2.0)
			_body.add_child(post)
	for side: float in [-1.0, 1.0]:
		for z_m: float in [CAB_FLOOR_M + height_m * 0.45, _cab_roof_m]:
			var rail := Art.cylinder(ROLL_BAR_TUBE_M * 0.8, front_m - back_m, CHROME, 8)
			rail.material_override = cage
			rail.rotation.x = PI / 2.0
			rail.position = TrackSpace.point((back_m + front_m) / 2.0, side * half_width_m, z_m)
			_body.add_child(rail)
	if roof:
		_add_roof(half_width_m, (back_m + front_m) / 2.0)
		return
	# Open: tubes across the top, front and back, and one corner to corner.
	for x_m: float in [back_m, front_m]:
		var across := Art.cylinder(ROLL_BAR_TUBE_M * 0.8, 2.0 * half_width_m, CHROME, 8)
		across.material_override = cage
		across.rotation.z = PI / 2.0
		across.position = TrackSpace.point(x_m, 0.0, _cab_roof_m)
		_body.add_child(across)
	var diagonal := Vector3(front_m - back_m, 2.0 * half_width_m, 0.0)
	var brace := Art.cylinder(ROLL_BAR_TUBE_M * 0.8, diagonal.length(), CHROME, 8)
	brace.name = "CageBrace"
	brace.material_override = cage
	brace.rotation = Vector3(PI / 2.0, atan2(diagonal.y, diagonal.x), 0.0)
	brace.position = TrackSpace.point((back_m + front_m) / 2.0, 0.0, _cab_roof_m)
	_body.add_child(brace)
	# And across the back, corner to corner: a full roll cage.
	var back_brace := Vector2(2.0 * half_width_m, height_m)
	var cross := Art.cylinder(ROLL_BAR_TUBE_M * 0.8, back_brace.length(), CHROME, 8)
	cross.name = "CageBackBrace"
	cross.material_override = cage
	cross.rotation.z = atan2(back_brace.x, back_brace.y)
	cross.position = TrackSpace.point(back_m, 0.0, CAB_FLOOR_M + height_m / 2.0)
	_body.add_child(cross)


# The roof over a cab or a cage: flat or rounded, as the model has it, in
# the trim, a little wider than the posts.
func _add_roof(half_width_m: float, middle_m: float) -> void:
	var size := Vector3(2.0 * (half_width_m + CAB_ROOF_OVERHANG_M), CAB_ROOF_THICK_M,
			2.0 * (CAB_HALF_DEPTH_M + CAB_ROOF_OVERHANG_M))
	var rounded: bool = _style["roof"] == "rounded"
	var roof := Art.instance(Art.rounded_box(size, 0.05 if not rounded else 0.1,
			Vector3.AXIS_Z), Art.paint(look.trim))
	roof.name = "CabRoof"
	roof.position = TrackSpace.point(middle_m, 0.0, _cab_roof_m + CAB_ROOF_THICK_M / 2.0)
	_body.add_child(roof)
	if rounded:
		# A domed top over the flat one.
		var dome := Art.instance(Art.rounded_box(Vector3(size.x - 0.12, 0.1, size.z - 0.16), 0.05,
				Vector3.AXIS_Z), Art.paint(look.trim))
		dome.position = TrackSpace.point(middle_m, 0.0, _cab_roof_m + CAB_ROOF_THICK_M + 0.03)
		_body.add_child(dome)
	_add_roof_extras(middle_m, half_width_m, _cab_roof_m + CAB_ROOF_THICK_M
			+ (0.08 if rounded else 0.0))


# On a roof whose middle is `middle_m` along and whose top is `top_m` up:
# the amber beacon and the work lamps at its front, as the model has them.
func _add_roof_extras(middle_m: float, half_width_m: float, top_m: float) -> void:
	if _style["beacon"]:
		var beacon := Art.cylinder(BEACON_RADIUS_M, 0.1, BEACON, 12, 0.3)
		beacon.name = "Beacon"
		beacon.material_override = _lamp(BEACON, 1.2)
		beacon.position = TrackSpace.point(middle_m - CAB_HALF_DEPTH_M + 0.15,
				half_width_m - 0.12, top_m + 0.05)
		_body.add_child(beacon)
	if _style["roof_lamps"]:
		for side: float in [-1.0, 1.0]:
			var lamp := Art.instance(_box_mesh(WORK_LAMP_M), _lamp(LAMP, 1.0))
			lamp.position = TrackSpace.point(middle_m + CAB_HALF_DEPTH_M + CAB_ROOF_OVERHANG_M,
					side * (half_width_m - 0.12), top_m - CAB_ROOF_THICK_M / 2.0)
			_body.add_child(lamp)


# A hoop of tube behind the seat, braced back to the frame.
func _add_roll_bar(half_width_m: float) -> void:
	var bar_half_width_m := half_width_m - 0.1
	var x_m := -ROLL_BAR_BEHIND_M
	var top_m := 0.35 + (ROLL_BAR_TOP_M - 0.35) * float(_style["cab_height"])
	var height_m := top_m - 0.35
	for side: float in [-1.0, 1.0]:
		var post := Art.cylinder(ROLL_BAR_TUBE_M, height_m, CHROME, 10)
		post.material_override = Art.paint(CHROME, 0.3)
		post.position = TrackSpace.point(x_m, side * bar_half_width_m, 0.35 + height_m / 2.0)
		_body.add_child(post)
		# The brace: from two thirds up the post back down to the frame.
		var from := Vector3(x_m, side * bar_half_width_m, 0.35 + height_m * 0.66)
		var to := Vector3(-ROLL_BAR_BRACE_BEHIND_M, side * bar_half_width_m, 0.38)
		var brace := Art.cylinder(ROLL_BAR_TUBE_M * 0.8, (to - from).length(), CHROME, 8)
		brace.material_override = Art.paint(CHROME, 0.3)
		brace.position = TrackSpace.point((from.x + to.x) / 2.0, from.y, (from.z + to.z) / 2.0)
		# Tilted in the tractor's x-z plane, about TrackSpace's sideways axis.
		brace.rotation.x = atan2(to.x - from.x, from.z - to.z)
		_body.add_child(brace)
	var top := Art.cylinder(ROLL_BAR_TUBE_M, 2.0 * bar_half_width_m + 2.0 * ROLL_BAR_TUBE_M,
			CHROME, 10)
	top.material_override = Art.paint(CHROME, 0.3)
	top.rotation.z = PI / 2.0
	top.position = TrackSpace.point(x_m, 0.0, top_m)
	_body.add_child(top)


# A closed farm cab: black corner posts, glass all round, a roof in the
# look's trim over a dark lining, an amber beacon and work lamps on the
# roof, a mirror out on each side and a handle on the left door.
func _add_cab_frame(half_width_m: float) -> void:
	var back_m := 0.15 - CAB_HALF_DEPTH_M
	var front_m := 0.15 + CAB_HALF_DEPTH_M
	var middle_m := 0.15
	var height_m := _cab_roof_m - CAB_FLOOR_M
	var middle_z_m := CAB_FLOOR_M + height_m / 2.0
	for x_m: float in [back_m, front_m]:
		for side: float in [-1.0, 1.0]:
			var post := Art.box(Vector3(CAB_POST_M, height_m, CAB_POST_M), FRAME, 0.4)
			post.position = TrackSpace.point(x_m, side * half_width_m, middle_z_m)
			_body.add_child(post)
	# The sills along the doors' feet.
	for side: float in [-1.0, 1.0]:
		var sill := Art.box(Vector3(CAB_POST_M, CAB_POST_M, 2.0 * CAB_HALF_DEPTH_M), FRAME, 0.4)
		sill.position = TrackSpace.point(middle_m, side * half_width_m, CAB_FLOOR_M)
		_body.add_child(sill)
	# The glass: the screen, both doors and the back window.
	var glass := _glass()
	var panes: Array[Array] = [
		["Front", Vector3(2.0 * half_width_m, height_m, 0.02), Vector3(front_m, 0.0, middle_z_m)],
		["Back", Vector3(2.0 * half_width_m, _cab_roof_m - CAB_BACK_SILL_M, 0.02), Vector3(back_m,
				0.0, (CAB_BACK_SILL_M + _cab_roof_m) / 2.0)],
		["Left", Vector3(0.02, height_m, 2.0 * CAB_HALF_DEPTH_M), Vector3(middle_m, half_width_m,
				middle_z_m)],
		["Right", Vector3(0.02, height_m, 2.0 * CAB_HALF_DEPTH_M), Vector3(middle_m,
				-half_width_m, middle_z_m)],
	]
	for pane: Array in panes:
		var at: Vector3 = pane[2]
		var window := Art.instance(_box_mesh(pane[1]), glass)
		window.name = "CabGlass" + str(pane[0])
		window.position = TrackSpace.point(at.x, at.y, at.z)
		window.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_body.add_child(window)
	# The back wall below the window, and the sill across its top.
	var wall_height_m := CAB_BACK_SILL_M - CAB_FLOOR_M
	var wall := Art.box(Vector3(2.0 * half_width_m, wall_height_m, 0.03), FRAME, 0.6)
	wall.name = "CabBackWall"
	wall.position = TrackSpace.point(back_m, 0.0, CAB_FLOOR_M + wall_height_m / 2.0)
	_body.add_child(wall)
	var back_sill := Art.box(Vector3(2.0 * half_width_m, CAB_POST_M, CAB_POST_M), FRAME, 0.4)
	back_sill.position = TrackSpace.point(back_m, 0.0, CAB_BACK_SILL_M)
	_body.add_child(back_sill)
	_add_roof(half_width_m, middle_m)
	var lining := Art.box(Vector3(2.0 * half_width_m + 0.04, 0.05,
			2.0 * CAB_HALF_DEPTH_M + 0.04), FRAME, 0.6)
	lining.position = TrackSpace.point(middle_m, 0.0, _cab_roof_m - 0.01)
	_body.add_child(lining)
	for side: float in [-1.0, 1.0]:
		# The mirror: an arm out from the front post, the glass at its end.
		var arm := Art.box(Vector3(MIRROR_ARM_M, 0.025, 0.025), FRAME, 0.4)
		arm.position = TrackSpace.point(front_m, side * (half_width_m + MIRROR_ARM_M / 2.0),
				_cab_roof_m - 0.25)
		_body.add_child(arm)
		var mirror := Art.box(MIRROR_M, FRAME, 0.3)
		mirror.name = "Mirror"
		mirror.position = TrackSpace.point(front_m, side * (half_width_m + MIRROR_ARM_M),
				_cab_roof_m - 0.35)
		_body.add_child(mirror)
	var handle := Art.box(Vector3(0.03, 0.02, 0.14), CHROME, 0.25, 0.8)
	handle.position = TrackSpace.point(back_m + 0.2, half_width_m + 0.02, CAB_FLOOR_M + 0.75)
	_body.add_child(handle)


# Clear glass that casts no shadow of its own.
static func _glass() -> StandardMaterial3D:
	var glass := StandardMaterial3D.new()
	glass.albedo_color = GLASS
	glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	glass.roughness = 0.05
	return glass


# A lamp that glows in its colour.
static func _lamp(colour: Color, energy: float) -> StandardMaterial3D:
	var lit := StandardMaterial3D.new()
	lit.albedo_color = colour
	lit.emission_enabled = true
	lit.emission = colour
	lit.emission_energy_multiplier = energy
	return lit


func _add_seat_and_driver(racing: bool) -> void:
	var seat := Art.box(Vector3(0.45, 0.1, 0.45), FRAME, 0.7)
	seat.position = TrackSpace.point(-0.05, 0.0, 0.62)
	_body.add_child(seat)
	# A pulling tractor's seat: a high back with side wings.
	var back_m := RACING_SEAT_BACK_M if racing else 0.45
	var back := Art.instance(Art.rounded_box(Vector3(0.45, back_m, 0.08), 0.06 if racing else 0.0,
			Vector3.AXIS_Z), Art.material(FRAME, 0.7))
	back.position = TrackSpace.point(-0.27, 0.0, 0.62 + back_m / 2.0)
	_body.add_child(back)
	if racing:
		for side: float in [-1.0, 1.0]:
			var wing := Art.box(Vector3(0.06, back_m * 0.6, 0.18), FRAME, 0.7)
			wing.position = TrackSpace.point(-0.21, side * 0.22, 0.62 + back_m * 0.4)
			_body.add_child(wing)
	_add_dashboard()
	_add_steering_wheel()
	# The driver in blue overalls, seated: People face +z, the tractor -z
	# (TrackSpace's x); their hips 0.42 m behind their feet, on the seat.
	var driver := People.one(true, look.driver_shirt)
	driver.rotation.y = PI
	driver.position = TrackSpace.point(0.37, 0.0, 0.3)
	_body.add_child(driver)
	if _pulling_look:
		# A puller on a pulling frame wears a helmet, as the rules ask.
		driver.add_child(People.helmet(look.trim))


# The steering wheel on its column, tilted back towards the driver: a rim,
# three spokes and a hub, turned with the front wheels (show_snapshot).
func _add_steering_wheel() -> void:
	var mount := Node3D.new()
	mount.position = TrackSpace.point(WHEEL_AT.x, WHEEL_AT.y, WHEEL_AT.z)
	mount.rotation.x = WHEEL_TILT_RAD
	_body.add_child(mount)
	var column := Art.cylinder(0.03, 0.5, FRAME, 8)
	column.position = Vector3(0.0, -0.27, 0.0)
	mount.add_child(column)
	mount.add_child(_steering_wheel)
	var rim := Art.instance(_torus(WHEEL_RADIUS_M, 0.02), Art.material(FRAME, 0.45))
	_steering_wheel.add_child(rim)
	var hub := Art.cylinder(0.045, 0.04, FRAME, 12, 0.4)
	_steering_wheel.add_child(hub)
	# A T: two spokes across and one down towards the driver.
	for angle: float in [0.0, PI / 2.0, -PI / 2.0]:
		var spoke := Art.box(Vector3(0.025, 0.012, WHEEL_RADIUS_M), IRON, 0.4, 0.5)
		spoke.position = Vector3(sin(angle), 0.0, cos(angle)) * WHEEL_RADIUS_M / 2.0
		spoke.rotation.y = angle
		_steering_wheel.add_child(spoke)


# The dashboard: a panel leaning back towards the driver, its rev counter
# and boost gauge with needles (show_snapshot), a warning lamp each side.
func _add_dashboard() -> void:
	var panel := Node3D.new()
	panel.position = TrackSpace.point(DASH_AT.x, DASH_AT.y, DASH_AT.z)
	panel.rotation.x = -DASH_TILT_RAD
	_body.add_child(panel)
	var board := Art.box(DASH_SIZE, FRAME, 0.7)
	panel.add_child(board)
	var cowl := Art.box(Vector3(DASH_SIZE.x, 0.03, 0.12), FRAME, 0.7)
	cowl.position = Vector3(0.0, DASH_SIZE.y / 2.0, -0.03)
	panel.add_child(cowl)
	# Red on the rev counter from the governor's limit up; the boost gauge
	# has none.
	var red_from := _high_idle_rad_s / (_rated_rad_s * REV_COUNTER_HEADROOM)
	for gauge: Array in [[-0.1, _rev_needle, red_from], [0.1, _boost_needle, 2.0]]:
		var x_m: float = gauge[0]
		var red_share: float = gauge[2]
		var bezel := Art.cylinder(GAUGE_RADIUS_M + 0.012, 0.012, CHROME, 24, 0.25, 0.8)
		bezel.rotation.x = PI / 2.0
		bezel.position = Vector3(x_m, 0.0, DASH_SIZE.z / 2.0)
		panel.add_child(bezel)
		# The dials lit from behind, so they read in the cab's shade.
		var face := Art.cylinder(GAUGE_RADIUS_M, 0.014, DIAL, 24, 0.6)
		face.material_override = _backlit(DIAL)
		face.rotation.x = PI / 2.0
		face.position = Vector3(x_m, 0.0, DASH_SIZE.z / 2.0 + 0.002)
		panel.add_child(face)
		# Ticks round the dial, red from `red_share` of its sweep up.
		for t in DIAL_TICKS:
			var share := t / float(DIAL_TICKS - 1)
			var a := NEEDLE_SWEEP_RAD * (share - 0.5)
			var colour := NEEDLE if share >= red_share - 1e-6 else FRAME
			if colour == NEEDLE and gauge[1] == _rev_needle:
				rev_red_ticks += 1
			var tick := Art.box(Vector3(0.006, 0.016, 0.004), colour)
			tick.material_override = _backlit(colour)
			tick.position = Vector3(x_m + sin(a) * GAUGE_RADIUS_M * 0.8,
					cos(a) * GAUGE_RADIUS_M * 0.8, DASH_SIZE.z / 2.0 + 0.011)
			tick.rotation.z = -a
			panel.add_child(tick)
		var needle: Node3D = gauge[1]
		needle.position = Vector3(x_m, 0.0, DASH_SIZE.z / 2.0 + 0.014)
		panel.add_child(needle)
		var pointer := Art.box(Vector3(0.008, GAUGE_RADIUS_M * 0.85, 0.004), NEEDLE, 0.5)
		pointer.material_override = _backlit(NEEDLE)
		pointer.position = Vector3(0.0, GAUGE_RADIUS_M * 0.4, 0.0)
		needle.add_child(pointer)
		needle.rotation.z = NEEDLE_SWEEP_RAD / 2.0
	for x_m: float in [-0.21, 0.21]:
		var lamp := Art.cylinder(0.014, 0.01, Color(0.95, 0.6, 0.1), 10, 0.3)
		lamp.rotation.x = PI / 2.0
		lamp.position = Vector3(x_m, 0.05, DASH_SIZE.z / 2.0 + 0.003)
		panel.add_child(lamp)


# A colour that shows as it is, unshaded: a backlit dial.
static func _backlit(colour: Color) -> StandardMaterial3D:
	var lit := StandardMaterial3D.new()
	lit.albedo_color = colour
	lit.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	return lit


# Fenders over the rear wheels: an arc of panels from the cab to the front
# of the wheel.
# The model's fenders: round (the arc), wide (an arc past the tire's
# sides, with a lamp at its front) or flat (a modern tractor's: a flat top
# over the wheel and a short slope down at each end).
func _add_fenders(rear_track_m: float, rear_width_m: float, panels: int) -> void:
	var radius_m := _rear_radius_m + 0.12
	var kind: String = _style["fenders"]
	var width_m := rear_width_m + (0.1 if kind != "wide" else 0.24)
	for side: float in [-1.0, 1.0]:
		var y_m := side * (rear_track_m / 2.0 + (0.0 if kind != "wide" else 0.04))
		if kind == "flat":
			var top := Art.box(Vector3(width_m, 0.04, radius_m * 1.3), look.paint, 0.35)
			top.material_override = Art.paint(look.paint)
			top.position = TrackSpace.point(-0.05, y_m, radius_m)
			_body.add_child(top)
			for end: float in [-1.0, 1.0]:
				var slope := Art.box(Vector3(width_m, 0.04, radius_m * 0.55), look.paint, 0.35)
				slope.material_override = Art.paint(look.paint)
				slope.position = TrackSpace.point(-0.05 + end * radius_m * 0.85, y_m,
						radius_m * 0.82)
				# Falling away from the flat top at each end.
				slope.rotation.x = -end * 0.75
				_body.add_child(slope)
			var lamp := Art.instance(_box_mesh(WORK_LAMP_M), _lamp(LAMP, 1.0))
			lamp.position = TrackSpace.point(radius_m * 0.75, y_m, radius_m + 0.06)
			_body.add_child(lamp)
			continue
		for i in panels:
			var angle := lerpf(0.25, PI - 0.55, (float(i) + 0.5) / panels)
			var panel := Art.box(Vector3(width_m, 0.03, radius_m * PI / panels * 0.75),
					look.paint, 0.35)
			panel.material_override = Art.paint(look.paint)
			panel.position = TrackSpace.point(radius_m * cos(angle), y_m, radius_m * sin(angle))
			# Tangent to the arc: turned about the axle's line.
			panel.rotation.x = angle - PI / 2.0
			_body.add_child(panel)
		if kind == "wide" or (_style["grille_lamps"] and _style["body"] == TractorModels.FARM):
			# A lamp at the fender's front edge, an older or a wider tractor's.
			var lamp := Art.axle_cylinder(0.06, 0.06, LAMP, 12, 0.1)
			lamp.material_override = _lamp(LAMP, 0.8)
			lamp.rotation = Vector3(PI / 2.0, 0.0, 0.0)
			lamp.position = TrackSpace.point(radius_m * 0.55, y_m, radius_m * 0.86 + 0.06)
			_body.add_child(lamp)


# The drawbar out to the hitch, and the wheelie bar behind the tractor.
func _add_hitch_and_bar(d: Dictionary, axle_height_m: float, rear_track_m: float) -> void:
	var behind_m: float = d["hitch_behind_rear_axle_m"]
	var hitch_z_m := float(d["hitch_height_m"]) - axle_height_m
	_hitch.position = TrackSpace.point(-behind_m, 0.0, hitch_z_m)
	_body.add_child(_hitch)
	var drawbar := Art.box(Vector3(0.12, 0.06, behind_m + 0.2), IRON, 0.4, 0.5)
	drawbar.position = TrackSpace.point(-(behind_m - 0.2) / 2.0, 0.0, hitch_z_m)
	_body.add_child(drawbar)
	var clevis := Art.box(Vector3(0.16, 0.14, 0.1), FRAME, 0.4, 0.5)
	clevis.position = TrackSpace.point(-behind_m, 0.0, hitch_z_m)
	_body.add_child(clevis)
	var bar_m: float = d["wheelie_bar_behind_rear_axle_m"]
	var tip_z_m := float(d["wheelie_bar_tip_height_m"]) - axle_height_m
	var start := Vector3(-0.3, 0.0, -0.25)
	var tip := Vector3(-bar_m, 0.0, tip_z_m + 0.12)
	for side: float in [-1.0, 1.0]:
		var y_m := side * minf(0.35, rear_track_m / 4.0)
		var from := TrackSpace.point(start.x, y_m, start.z)
		var to := TrackSpace.point(tip.x, y_m, tip.z)
		var rail := Art.box(Vector3(0.07, 0.07, from.distance_to(to)), Color(0.9, 0.75, 0.1), 0.4,
				0.4)
		rail.position = (from + to) / 2.0
		rail.basis = Basis.looking_at(to - from, Vector3.UP)
		_body.add_child(rail)
		var skid := Art.axle_cylinder(0.12, 0.08, FRAME, 12)
		skid.position = TrackSpace.point(-bar_m, y_m, tip_z_m + 0.12)
		_body.add_child(skid)
	var cross := Art.box(Vector3(2.0 * minf(0.35, rear_track_m / 4.0), 0.06, 0.06),
			Color(0.9, 0.75, 0.1))
	cross.position = TrackSpace.point(-bar_m, 0.0, tip_z_m + 0.12)
	_body.add_child(cross)


func _add_rear_wheel(spin: Node3D, side_m: float, width_m: float, disc: bool) -> void:
	spin.position = TrackSpace.point(0.0, side_m, 0.0)
	_body.add_child(spin)
	var lug_height_m: float = _tread["height_m"]
	var carcass_m := _rear_radius_m - lug_height_m
	Art.tire(spin, carcass_m, width_m, TIRE)
	var outward := signf(side_m)
	Art.rim(spin, _rear_radius_m * 0.62, width_m, outward, 10, look.rim)
	if disc:
		# A pulling tractor's rim under a flat disc, a white ring round it.
		var cover := Art.axle_cylinder(_rear_radius_m * 0.6, 0.015, look.disc, 32, 0.5)
		cover.name = "RimDisc"
		cover.position = TrackSpace.point(0.0, outward * (width_m * 0.5 + Art.RIM_PROUD_M + 0.01),
				0.0)
		spin.add_child(cover)
		var ring := Art.instance(Art.torus_across(_rear_radius_m * 0.6, 0.025),
				Art.material(look.ring, 0.5))
		ring.position = cover.position
		spin.add_child(ring)
	# The tread as fitted (TREADS): lugs in a chevron, each half of the
	# tread angled towards the middle, reaching over the shoulder onto the
	# sidewall; a farm tire's apart in the middle, a puller tire's meeting.
	var lugs: Array[Transform3D] = []
	var lug := BoxMesh.new()
	var length_m := width_m * float(_tread["length"])
	lug.size = Vector3(length_m, lug_height_m * 2.0, float(_tread["thick_m"]))
	var per_side: int = _tread["lugs"]
	var turn_rad: float = _tread["turn_rad"]
	var middle_m := width_m * (float(_tread["from_middle"]) + float(_tread["length"]) / 2.0) \
			* 0.9
	for i in per_side:
		for half: float in [-1.0, 1.0]:
			var angle := TAU * (float(i) + (0.5 if half > 0.0 else 0.0)) / per_side
			var basis := Basis(Vector3.RIGHT, -angle) * Basis(Vector3.UP, half * turn_rad)
			var at := Basis(Vector3.RIGHT, -angle) * Vector3(half * middle_m, carcass_m, 0.0)
			lugs.append(Transform3D(basis, at))
	var tread := Art.many(lug, lugs, Art.material(TIRE, 0.9))
	tread.name = "Lugs"
	spin.add_child(tread)
	# A wheel weight's place: on the hub, outboard of the rim and its cap.
	var weights := Node3D.new()
	weights.name = "weights"
	weights.set_meta("outward", outward)
	weights.position = TrackSpace.point(0.0, outward * (width_m * 0.5 + Art.RIM_PROUD_M
			+ Art.HUB_REACH_M + DISC_CLEAR_OF_CAP_M + DISC_THICKNESS_M / 2.0), 0.0)
	spin.add_child(weights)


# `side` is 1 on the left, -1 on the right (TrackSpace's y).
func _add_front_wheel(steer: Node3D, spin: Node3D, width_m: float, side: float) -> void:
	steer.add_child(spin)
	Art.tire(spin, _front_radius_m - 0.015, width_m, TIRE)
	for rib in 3:
		var ring := Art.instance(Art.torus_across(_front_radius_m - 0.01, 0.018),
				Art.material(TIRE.lightened(0.05), 0.85))
		ring.position = TrackSpace.point(0.0, (float(rib) - 1.0) * width_m * 0.28, 0.0)
		spin.add_child(ring)
	Art.rim(spin, _front_radius_m * 0.58, width_m, side, 6, look.rim)


# Suitcase weights on the front bracket and wheel weights on the rear hubs,
# each in proportion to the mass of the ballast at that end.
func _show_weights(d: Dictionary) -> void:
	# The last pull's weights come off first, at both ends.
	var holders: Array[Node3D] = []
	for spin: Node3D in [_rear_left, _rear_right]:
		var holder := spin.get_node_or_null("weights") as Node3D
		if holder != null:
			holders.append(holder)
	for parent: Node3D in [_front_weights] + holders:
		for child in parent.get_children():
			child.queue_free()
	var mass_kg: float = d["ballast_mass_kg"]
	var position_share: float = d["ballast_position"]
	if mass_kg <= 0.0:
		return
	# Each end gets its share of the mass, in plates or discs of their size.
	# Rounded up, so a little weight at an end still shows.
	var plates := mini(ceili(position_share * mass_kg / PLATE_KG - WEIGHT_COUNT_TOLERANCE), MAX_PLATES)
	if _weight_box:
		# A weight box: its plates are the box's own, its shape and paint,
		# stacked onto its front; no suitcase weights, no bracket.
		var size := Vector3(BONNET_WIDTH_M * _body_scale + BOX_WIDER_M,
				BOX_HEIGHT_M * _body_scale, PLATE_THICK_M)
		for i in plates:
			var slab := Art.instance(_box_mesh(size), Art.paint(look.trim if i % 2 == 0
					else look.trim.darkened(0.15)))
			slab.position = TrackSpace.point(i * PLATE_PITCH_M, 0.0,
					BOX_HEIGHT_M * _body_scale * BOX_RISE)
			_front_weights.add_child(slab)
	else:
		var bracket := Art.box(Vector3(0.7, 0.08, 0.12), IRON)
		bracket.set_meta("bracket", true)
		bracket.position = TrackSpace.point(0.0, 0.0, -0.32)
		_front_weights.add_child(bracket)
		for i in plates:
			var plate := Art.box(Vector3(0.68, 0.5, PLATE_THICK_M),
					WEIGHT if i % 2 == 0 else WEIGHT.darkened(0.2), 0.6)
			plate.position = TrackSpace.point(0.03 + i * PLATE_PITCH_M, 0.0, 0.0)
			_front_weights.add_child(plate)
	var discs := mini(ceili((1.0 - position_share) * mass_kg / 2.0 / DISC_KG
			- WEIGHT_COUNT_TOLERANCE), MAX_DISCS_A_SIDE)
	for holder: Node3D in holders:
		var outward: float = holder.get_meta("outward")
		for i in discs:
			var disc := Art.axle_cylinder(_rear_radius_m * 0.42, DISC_THICKNESS_M,
					WEIGHT.darkened(0.1), 20, 0.6)
			disc.position = TrackSpace.point(0.0, outward * i * DISC_PITCH_M, 0.0)
			holder.add_child(disc)


# The exhaust as the model has it, and the smoke from it: one stack up
# through the hood (single, or tall), one each side (twin), or one up the
# cab's front corner (side). Twin stacks smoke from the left one.
func _add_smoke(wheelbase_m: float) -> void:
	var kind: String = _style["stack"]
	var x_m := wheelbase_m - 0.55
	# Out of the hood's top: a bigger bonnet's as much higher, and thicker.
	var base_m := (bonnet_top_m() - (BONNET_TOP_M - HOOD_TOP_M)) \
			if _style["body"] == TractorModels.PULLING else _hood_top_m * _hood_scale
	var length_m := 1.0 if kind != "tall" else 1.35
	if kind == "fat":
		length_m *= FAT_STACK_LENGTH
	var at := Vector3(x_m, 0.18, base_m + length_m / 2.0 - 0.05)
	var places: Array[Vector3] = [at]
	if kind == "twin":
		places = [Vector3(x_m - 0.3, 0.22, at.z), Vector3(x_m - 0.3, -0.22, at.z)]
	elif kind == "side":
		# Up outside the cab's right front post, from the hood's side, to
		# just over the roof; bright, so it stands out from the black post.
		length_m = _cab_roof_m - 0.2 + SIDE_STACK_OVER_ROOF_M
		places = [Vector3(0.15 + CAB_HALF_DEPTH_M + 0.04, -(_cab_half_width_m + 0.12),
				0.2 + length_m / 2.0)]
	var radius_m := 0.055 * _body_scale * (0.85 if kind == "twin" else 1.0) \
			* (FAT_STACK_RADIUS if kind == "fat" else 1.0)
	var finish := CHROME
	for place: Vector3 in places:
		var stack := Art.cylinder(radius_m, length_m, finish, 12, 0.2, 0.9)
		stack.position = TrackSpace.point(place.x, place.y, place.z)
		_body.add_child(stack)
		var cap := Art.cylinder(radius_m * 1.36, 0.05, FRAME, 12)
		cap.position = TrackSpace.point(place.x, place.y, place.z + length_m / 2.0 - 0.03)
		_body.add_child(cap)
	var out: Vector3 = places[0]
	_smoke.position = TrackSpace.point(out.x, out.y, out.z + length_m / 2.0)
	_smoke.amount = 60
	_smoke.lifetime = 2.2
	_smoke.emitting = false
	_smoke.local_coords = false
	_smoke.direction = Vector3.UP
	_smoke.spread = 12.0
	_smoke.initial_velocity_min = 1.5
	_smoke.initial_velocity_max = 2.5
	_smoke.gravity = Vector3(0.0, 0.6, 0.0)
	_smoke.scale_amount_min = 0.7
	_smoke.scale_amount_max = 1.1
	# Each puff turns a little as it rises, so the column does not repeat.
	_smoke.angle_min = 0.0
	_smoke.angle_max = 360.0
	_smoke.angular_velocity_min = -40.0
	_smoke.angular_velocity_max = 40.0
	_smoke.damping_min = 0.6
	_smoke.damping_max = 1.2
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.35))
	grow.add_point(Vector2(0.3, 1.4))
	grow.add_point(Vector2(1.0, 3.8))
	_smoke.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(SMOKE, 0.0))
	fade.add_point(0.08, SMOKE)
	fade.set_color(fade.get_point_count() - 1,
			Color(SMOKE.r * 3.5, SMOKE.g * 3.5, SMOKE.b * 3.5, 0.0))
	_smoke.color_ramp = fade
	_smoke.mesh = Art.puff_mesh(0.7)
	_body.add_child(_smoke)


func _add_dirt(side_m: float, axle_height_m: float) -> void:
	var dirt := CPUParticles3D.new()
	dirt.position = TrackSpace.point(-_rear_radius_m * 0.6, side_m, -axle_height_m + 0.15)
	dirt.amount = 40
	dirt.lifetime = 0.9
	dirt.emitting = false
	dirt.local_coords = false
	# Thrown back and up from behind the tire.
	dirt.direction = Vector3(0.0, 0.8, 1.0)
	dirt.spread = 25.0
	dirt.initial_velocity_min = 2.0
	dirt.initial_velocity_max = 5.0
	dirt.gravity = Vector3(0.0, -9.8, 0.0)
	dirt.scale_amount_min = 0.5
	dirt.scale_amount_max = 1.2
	dirt.color = DIRT
	dirt.mesh = _puff_mesh(0.07)
	_body.add_child(dirt)
	_dirt.append(dirt)
	# The dust the spinning lugs raise: soft and low, it hangs behind.
	var dust := CPUParticles3D.new()
	dust.position = dirt.position
	dust.amount = 30
	dust.lifetime = 2.5
	dust.emitting = false
	dust.local_coords = false
	dust.direction = Vector3(0.0, 0.5, 1.0)
	dust.spread = 40.0
	dust.initial_velocity_min = 0.5
	dust.initial_velocity_max = 1.5
	dust.gravity = Vector3(0.0, 0.15, 0.0)
	dust.damping_min = 0.5
	dust.damping_max = 1.0
	dust.angle_max = 360.0
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.5))
	grow.add_point(Vector2(1.0, 3.0))
	dust.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(DUST, 0.0))
	fade.add_point(0.15, Color(DUST, 0.45))
	fade.set_color(fade.get_point_count() - 1, Color(DUST, 0.0))
	dust.color_ramp = fade
	dust.mesh = Art.puff_mesh(0.9)
	_body.add_child(dust)
	_dust.append(dust)


# A soft round puff for smoke and dirt, lit and shaded but casting nothing.
func _puff_mesh(radius_m: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius_m
	mesh.height = 2.0 * radius_m
	mesh.radial_segments = 6
	mesh.rings = 3
	var m := StandardMaterial3D.new()
	m.vertex_color_use_as_albedo = true
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.roughness = 1.0
	mesh.material = m
	return mesh


# Drawn as a pulling tractor (behind a roll bar) rather than a farm one.
func pulling_look() -> bool:
	return _pulling_look


# How much bigger than drawn the pulling tractor's body is: 1 for the
# Sport 2.5 t's wheelbase and a farm tractor, more for a longer one.
func body_scale() -> float:
	return _body_scale


# The lugs drawn round a rear tire: the tread as fitted (TREADS).
func rear_lug_count() -> int:
	var tread := _rear_left.get_node_or_null("Lugs") as MultiMeshInstance3D
	return tread.multimesh.instance_count if tread != null else 0


# The front weights' node, its children the weights (and a bracket, marked
# with the "bracket" meta).
func front_weights_node() -> Node3D:
	return _front_weights


# Where the front weights hang, in the world.
func front_weights_position() -> Vector3:
	return _front_weights.global_position


# The weights drawn: suitcase weights on the front bracket (x) and wheel
# weights on each rear hub (y).
func weight_counts() -> Vector2i:
	var plates := 0
	for child: Node in _front_weights.get_children():
		# The bracket is not a weight.
		if not child.is_queued_for_deletion() and child is MeshInstance3D \
				and not child.has_meta("bracket"):
			plates += 1
	var discs := 0
	var holder := _rear_left.get_node_or_null("weights")
	if holder != null:
		for child: Node in holder.get_children():
			if not child.is_queued_for_deletion():
				discs += 1
	return Vector2i(plates, discs)


# A camera at the driver's eyes, looking ahead: it turns and pitches with
# the tractor (CameraRig.cab). Made once.
func cab_camera() -> Camera3D:
	if _cab_camera == null:
		_cab_camera = Camera3D.new()
		_cab_camera.near = CAB_NEAR_M
		_cab_camera.far = CameraRig.FAR_M
		_cab_camera.position = TrackSpace.point(EYES_AHEAD_M, 0.0, EYES_UP_M)
		_cab_camera.rotation.x = -EYES_DOWN_RAD
		# From inside the driver's helmet: not drawn.
		_cab_camera.set_cull_mask_value(People.HELMET_LAYER, false)
		_body.add_child(_cab_camera)
	return _cab_camera


# Where the chain pulls, in the world.
func hitch_position() -> Vector3:
	return _hitch.global_position


func show_snapshot(s: Dictionary, delta: float) -> void:
	position = TrackSpace.point(s["tractor_x_m"], s["tractor_y_m"], 0.0)
	rotation.y = s["tractor_heading_rad"]
	_body.rotation.x = s["tractor_pitch_rad"]
	var steering: float = s["steering_rad"]
	_front_left_steer.rotation.y = steering
	_front_right_steer.rotation.y = steering
	show_cockpit(s)
	# Rolling forward turns a wheel about -x (TrackSpace): the angle grows
	# with the wheel's speed, and the front wheels roll with the ground.
	_rear_left_angle -= float(s["rear_left_omega_rad_s"]) * delta
	_rear_right_angle -= float(s["rear_right_omega_rad_s"]) * delta
	_front_angle -= float(s["tractor_speed_m_s"]) / _front_radius_m * delta
	_rear_left.rotation.x = wrapf(_rear_left_angle, -PI, PI)
	_rear_right.rotation.x = wrapf(_rear_right_angle, -PI, PI)
	_front_left.rotation.x = wrapf(_front_angle, -PI, PI)
	_front_right.rotation.x = wrapf(_front_angle, -PI, PI)
	_show_smoke(s)
	var slip: float = s["slip_ratio"]
	var throwing := clampf((slip - DIRT_FROM_SLIP) / (DIRT_FULL_SLIP - DIRT_FROM_SLIP), 0.0, 1.0)
	for dirt in _dirt:
		dirt.emitting = throwing > 0.0
		dirt.initial_velocity_max = 2.0 + 4.0 * throwing
		dirt.color = Color(DIRT, 0.3 + 0.7 * throwing)
	# Dust whenever the tires move dry clay, more as they slip.
	var moving := absf(float(s["tractor_speed_m_s"])) > DUST_FROM_SPEED_M_S
	for dust in _dust:
		dust.emitting = moving or throwing > 0.0
		dust.color = Color(1.0, 1.0, 1.0, clampf(0.35 + 0.65 * throwing, 0.0, 1.0))


# The steering wheel turned with the front wheels; the rev counter from
# standing to past the rated speed, and the boost.
func show_cockpit(s: Dictionary) -> void:
	_steering_wheel.rotation.y = float(s["steering_rad"]) * STEERING_RATIO
	var revs := clampf(float(s["engine_omega_rad_s"]) / (_rated_rad_s * REV_COUNTER_HEADROOM),
			0.0, 1.0)
	_rev_needle.rotation.z = NEEDLE_SWEEP_RAD * (0.5 - revs)
	var boost := clampf(float(s.get("boost", 0.0)), 0.0, 1.0)
	_boost_needle.rotation.z = NEEDLE_SWEEP_RAD * (0.5 - boost)


# More smoke with more throttle, and blacker as the sled pulls the engine
# down from its rated speed towards the stall.
func _show_smoke(s: Dictionary) -> void:
	var throttle: float = s["throttle"]
	var running: bool = s["engine_running"]
	var omega: float = s["engine_omega_rad_s"]
	var lugging := clampf((_rated_rad_s - omega) / maxf(_rated_rad_s - _stall_rad_s, 1.0), 0.0, 1.0)
	_smoke.emitting = running and throttle > 0.05
	# The ramp fades each puff out; this sets how dark and thick it starts.
	var thick := clampf(0.25 + 0.75 * throttle * (0.4 + 0.6 * lugging), 0.0, 1.0)
	var shade := lerpf(1.3, 0.45, lugging)
	_smoke.color = Color(shade, shade, shade, thick)
	_smoke.initial_velocity_max = 1.5 + 2.5 * throttle


static func _box_mesh(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


static func _torus(radius_m: float, thickness_m: float) -> TorusMesh:
	var mesh := TorusMesh.new()
	mesh.inner_radius = radius_m - thickness_m
	mesh.outer_radius = radius_m + thickness_m
	mesh.rings = 16
	mesh.ring_segments = 6
	return mesh
