# The tractor, built in code from the run's sizes so it matches the
# simulation: a farm-stock 2WD tractor with its hood and grille, a cab
# frame with the driver, fenders over the big rear wheels and their lugs,
# the exhaust stack, the drawbar at the hitch and the wheelie bar behind.
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

const PAINT := Color(0.72, 0.1, 0.08)
const PAINT_DARK := Color(0.45, 0.06, 0.05)
const RIM := Color(0.93, 0.88, 0.72)
const TIRE := Color(0.08, 0.08, 0.08)
const IRON := Color(0.22, 0.22, 0.24)
const FRAME := Color(0.06, 0.06, 0.06)
const CHROME := Color(0.8, 0.8, 0.82)
const WEIGHT := Color(0.9, 0.72, 0.1)
const GLASS := Color(0.75, 0.85, 0.95, 0.35)
const LAMP := Color(1.0, 0.95, 0.75)
const SMOKE := Color(0.16, 0.16, 0.16, 0.8)
const DIRT := Color(0.45, 0.31, 0.19)

# Shapes the simulation does not size [m].
const HOOD_WIDTH_M := 0.62
const HOOD_TOP_M := 0.65          # above the rear axle
const HOOD_PAST_FRONT_AXLE_M := 0.3
const CAB_ROOF_M := 1.75          # above the rear axle
const CAB_HALF_DEPTH_M := 0.55
const LUGS_PER_SIDE := 22
# The weights at the sizes they come in, so any class's ballast draws
# right: a suitcase weight on the front bracket, a wheel weight on a rear
# hub [kg]; and as many as the bracket and a hub take.
const PLATE_KG := 45.0
const DISC_KG := 70.0
const MAX_PLATES := 12
const MAX_DISCS_A_SIDE := 4
# Slip above which the rear tires throw dirt, and where they throw most.
const DIRT_FROM_SLIP := 0.08
const DIRT_FULL_SLIP := 0.5

var _body := Node3D.new()        # pivots about the rear axle
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
var _built := false
var _rear_radius_m := 1.0
var _front_radius_m := 0.5
var _rated_rad_s := 230.0
var _stall_rad_s := 70.0
var _rear_left_angle := 0.0
var _rear_right_angle := 0.0
var _front_angle := 0.0


# The parts join the tree in build(), for the first pull; the game can quit
# before one (on the start screen), so the parts never added are freed here.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		for part: Node in [_body, _hitch, _rear_left, _rear_right, _front_left_steer,
				_front_right_steer, _front_left, _front_right, _front_weights, _rear_weights,
				_smoke]:
			if part.get_parent() == null:
				part.free()


# The tractor from the run's sizes, once; every pull after that only moves
# the weights to where the puller set them.
func build(d: Dictionary) -> void:
	if not _built:
		_built = true
		_build_body(d)
	_rated_rad_s = d["rated_omega_rad_s"]
	_stall_rad_s = d["stall_omega_rad_s"]
	_show_weights(d)


func _build_body(d: Dictionary) -> void:
	var wheelbase_m: float = d["wheelbase_m"]
	var axle_height_m: float = d["rear_axle_height_m"]
	var rear_track_m: float = d["rear_track_m"]
	var front_track_m: float = d["front_track_m"]
	_rear_radius_m = d["rear_tire_radius_m"]
	_front_radius_m = d["front_tire_radius_m"]
	var rear_width_m: float = d["rear_tire_width_m"]
	var front_width_m: float = d["front_tire_width_m"]

	_body.position = TrackSpace.point(0.0, 0.0, axle_height_m)
	add_child(_body)
	_add_driveline(wheelbase_m, rear_track_m, front_track_m, axle_height_m)
	_add_hood(wheelbase_m)
	_add_cab(rear_track_m)
	_add_fenders(rear_track_m, rear_width_m)
	_add_hitch_and_bar(d, axle_height_m, rear_track_m)

	_add_rear_wheel(_rear_left, rear_track_m / 2.0, rear_width_m)
	_add_rear_wheel(_rear_right, -rear_track_m / 2.0, rear_width_m)
	# The front axle sits at the front tires' radius above the ground.
	var front_axle := TrackSpace.point(wheelbase_m, 0.0, _front_radius_m - axle_height_m)
	for pair: Array in [[_front_left_steer, _front_left, 1.0],
			[_front_right_steer, _front_right, -1.0]]:
		var steer: Node3D = pair[0]
		steer.position = front_axle + TrackSpace.point(0.0, float(pair[2]) * front_track_m / 2.0,
				0.0)
		_body.add_child(steer)
		_add_front_wheel(steer, pair[1], front_width_m)

	_front_weights.position = TrackSpace.point(wheelbase_m + HOOD_PAST_FRONT_AXLE_M + 0.05, 0.0,
			_front_radius_m - axle_height_m + 0.35)
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


func _add_hood(wheelbase_m: float) -> void:
	var front_m := wheelbase_m + HOOD_PAST_FRONT_AXLE_M
	var back_m := 0.75
	var length_m := front_m - back_m
	var hood := Art.box(Vector3(HOOD_WIDTH_M, 0.5, length_m), PAINT, 0.35)
	hood.position = TrackSpace.point(back_m + length_m / 2.0, 0.0, HOOD_TOP_M - 0.25)
	_body.add_child(hood)
	# A rounded top: a half cylinder along the hood.
	var top := Art.instance(Art.cylinder_mesh(HOOD_WIDTH_M / 2.0, HOOD_WIDTH_M / 2.0, length_m, 16),
			Art.material(PAINT, 0.35))
	top.rotation.x = PI / 2.0
	top.scale = Vector3(1.0, 1.0, 0.35)
	top.position = TrackSpace.point(back_m + length_m / 2.0, 0.0, HOOD_TOP_M)
	_body.add_child(top)
	# Side panels with louvres, the model's stripe and its number.
	for side: float in [-1.0, 1.0]:
		var stripe := Art.box(Vector3(0.02, 0.06, length_m * 0.8), RIM)
		stripe.position = TrackSpace.point(back_m + length_m * 0.55,
				side * (HOOD_WIDTH_M / 2.0 + 0.005), HOOD_TOP_M - 0.12)
		_body.add_child(stripe)
		for i in 6:
			var louvre := Art.box(Vector3(0.02, 0.03, 0.18), PAINT_DARK)
			louvre.position = TrackSpace.point(front_m - 0.35, side * (HOOD_WIDTH_M / 2.0 + 0.005),
					HOOD_TOP_M - 0.22 - i * 0.05)
			_body.add_child(louvre)
		var number := Art.label("7", 90, Color.WHITE, Color.BLACK)
		number.position = TrackSpace.point(back_m + length_m * 0.35,
				side * (HOOD_WIDTH_M / 2.0 + 0.012), HOOD_TOP_M - 0.3)
		number.rotation.y = side * -PI / 2.0
		_body.add_child(number)
	# The grille, its bars and the two lamps.
	var grille := Art.box(Vector3(HOOD_WIDTH_M - 0.06, 0.55, 0.04), FRAME, 0.6)
	grille.position = TrackSpace.point(front_m + 0.01, 0.0, HOOD_TOP_M - 0.3)
	_body.add_child(grille)
	for i in 5:
		var bar := Art.box(Vector3(HOOD_WIDTH_M - 0.1, 0.025, 0.03), CHROME, 0.25, 0.8)
		bar.position = TrackSpace.point(front_m + 0.03, 0.0, HOOD_TOP_M - 0.08 - i * 0.1)
		_body.add_child(bar)
	for side: float in [-1.0, 1.0]:
		var lamp := Art.axle_cylinder(0.07, 0.05, LAMP, 12, 0.1)
		lamp.rotation = Vector3(PI / 2.0, 0.0, 0.0)
		lamp.position = TrackSpace.point(front_m + 0.04, side * 0.22, HOOD_TOP_M - 0.02)
		_body.add_child(lamp)


# The cab frame (four posts and a roof), the floor, the seat, the steering
# wheel and the driver.
func _add_cab(rear_track_m: float) -> void:
	var half_width_m := rear_track_m / 2.0 - 0.35
	var floor := Art.box(Vector3(2.0 * half_width_m, 0.06, 2.0 * CAB_HALF_DEPTH_M), IRON)
	floor.position = TrackSpace.point(0.15, 0.0, 0.35)
	_body.add_child(floor)
	for x_m: float in [0.15 - CAB_HALF_DEPTH_M, 0.15 + CAB_HALF_DEPTH_M]:
		for side: float in [-1.0, 1.0]:
			var post := Art.box(Vector3(0.07, CAB_ROOF_M - 0.35, 0.07), FRAME, 0.4)
			post.position = TrackSpace.point(x_m, side * half_width_m, 0.35 + (CAB_ROOF_M - 0.35) / 2.0)
			_body.add_child(post)
	var roof := Art.box(Vector3(2.0 * half_width_m + 0.12, 0.08, 2.0 * CAB_HALF_DEPTH_M + 0.2),
			PAINT, 0.35)
	roof.position = TrackSpace.point(0.15, 0.0, CAB_ROOF_M)
	_body.add_child(roof)
	var screen_material := StandardMaterial3D.new()
	screen_material.albedo_color = GLASS
	screen_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	screen_material.roughness = 0.05
	var screen := Art.instance(_box_mesh(Vector3(2.0 * half_width_m, CAB_ROOF_M - HOOD_TOP_M - 0.1,
			0.02)), screen_material)
	screen.position = TrackSpace.point(0.15 + CAB_HALF_DEPTH_M, 0.0,
			HOOD_TOP_M + (CAB_ROOF_M - HOOD_TOP_M) / 2.0)
	screen.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_body.add_child(screen)
	var seat := Art.box(Vector3(0.45, 0.1, 0.45), FRAME, 0.7)
	seat.position = TrackSpace.point(-0.05, 0.0, 0.62)
	_body.add_child(seat)
	var back := Art.box(Vector3(0.45, 0.45, 0.08), FRAME, 0.7)
	back.position = TrackSpace.point(-0.27, 0.0, 0.85)
	_body.add_child(back)
	var column := Art.cylinder(0.03, 0.55, FRAME, 8)
	column.position = TrackSpace.point(0.5, 0.0, 0.75)
	column.rotation.x = -0.6
	_body.add_child(column)
	var wheel := Art.instance(_torus(0.19, 0.022), Art.material(FRAME, 0.5))
	wheel.position = TrackSpace.point(0.4, 0.0, 0.98)
	wheel.rotation.x = -0.6
	_body.add_child(wheel)
	var driver := KenneyModels.scene(KenneyModels.CHARACTER)
	var antenna := driver.find_child("antenna", true, false)
	if antenna != null:
		antenna.queue_free()
	driver.scale = Vector3.ONE * 1.05
	# The model faces +z; the tractor faces -z (TrackSpace's x).
	driver.rotation.y = PI
	driver.position = TrackSpace.point(-0.02, 0.0, 0.42)
	_body.add_child(driver)


# Fenders over the rear wheels: an arc of panels from the cab to the front
# of the wheel.
func _add_fenders(rear_track_m: float, rear_width_m: float) -> void:
	var radius_m := _rear_radius_m + 0.12
	var panels := 7
	for side: float in [-1.0, 1.0]:
		for i in panels:
			var angle := lerpf(0.25, PI - 0.55, (float(i) + 0.5) / panels)
			var panel := Art.box(Vector3(rear_width_m + 0.1, 0.03, radius_m * PI / panels * 0.75),
					PAINT, 0.35)
			panel.position = TrackSpace.point(radius_m * cos(angle), side * rear_track_m / 2.0,
					radius_m * sin(angle))
			# Tangent to the arc: turned about the axle's line.
			panel.rotation.x = angle - PI / 2.0
			_body.add_child(panel)


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


func _add_rear_wheel(spin: Node3D, side_m: float, width_m: float) -> void:
	spin.position = TrackSpace.point(0.0, side_m, 0.0)
	_body.add_child(spin)
	var lug_height_m := 0.05
	spin.add_child(Art.axle_cylinder(_rear_radius_m - lug_height_m, width_m, TIRE, 32, 0.95))
	var outward := signf(side_m)
	var rim := Art.axle_cylinder(_rear_radius_m * 0.6, width_m * 0.6, RIM, 24, 0.4, 0.2)
	rim.position = TrackSpace.point(0.0, outward * width_m * 0.21, 0.0)
	spin.add_child(rim)
	var hub := Art.axle_cylinder(0.14, width_m * 0.66, IRON, 12, 0.4, 0.5)
	hub.position = TrackSpace.point(0.0, outward * width_m * 0.21, 0.0)
	spin.add_child(hub)
	# Chevron lugs: on each half of the tread, angled towards the middle.
	var lugs: Array[Transform3D] = []
	var lug := BoxMesh.new()
	lug.size = Vector3(width_m * 0.5, lug_height_m * 2.0, 0.07)
	for i in LUGS_PER_SIDE:
		for half: float in [-1.0, 1.0]:
			var angle := TAU * (float(i) + (0.5 if half > 0.0 else 0.0)) / LUGS_PER_SIDE
			var basis := Basis(Vector3.RIGHT, -angle) * Basis(Vector3.UP, half * 0.45)
			var at := Basis(Vector3.RIGHT, -angle) * Vector3(half * width_m * 0.24,
					_rear_radius_m - lug_height_m, 0.0)
			lugs.append(Transform3D(basis, at))
	spin.add_child(Art.many(lug, lugs, Art.material(TIRE, 0.95)))
	# A wheel weight's place: on the hub, outboard.
	var weights := Node3D.new()
	weights.name = "weights"
	weights.position = TrackSpace.point(0.0, outward * (width_m * 0.5 + 0.04), 0.0)
	spin.add_child(weights)


func _add_front_wheel(steer: Node3D, spin: Node3D, width_m: float) -> void:
	steer.add_child(spin)
	spin.add_child(Art.axle_cylinder(_front_radius_m, width_m, TIRE, 24, 0.95))
	for rib in 3:
		var ring := Art.axle_cylinder(_front_radius_m + 0.012, 0.025, TIRE.lightened(0.08), 24)
		ring.position = TrackSpace.point(0.0, (float(rib) - 1.0) * width_m * 0.3, 0.0)
		spin.add_child(ring)
	var rim := Art.axle_cylinder(_front_radius_m * 0.55, width_m * 1.02, RIM, 18, 0.4, 0.2)
	spin.add_child(rim)
	spin.add_child(Art.axle_cylinder(0.08, width_m * 1.08, IRON, 10))


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
	var plates := mini(ceili(position_share * mass_kg / PLATE_KG), MAX_PLATES)
	var bracket := Art.box(Vector3(0.7, 0.08, 0.12), IRON)
	bracket.position = TrackSpace.point(0.0, 0.0, -0.32)
	_front_weights.add_child(bracket)
	for i in plates:
		var plate := Art.box(Vector3(0.68, 0.5, 0.045), WEIGHT if i % 2 == 0 else WEIGHT.darkened(0.2),
				0.6)
		plate.position = TrackSpace.point(0.03 + i * 0.05, 0.0, 0.0)
		_front_weights.add_child(plate)
	var discs := mini(ceili((1.0 - position_share) * mass_kg / 2.0 / DISC_KG), MAX_DISCS_A_SIDE)
	for holder: Node3D in holders:
		var outward := signf(holder.get_parent().position.x * -1.0)
		for i in discs:
			var disc := Art.axle_cylinder(_rear_radius_m * 0.42, 0.06, WEIGHT.darkened(0.1), 20, 0.6)
			disc.position = TrackSpace.point(0.0, outward * i * 0.065, 0.0)
			holder.add_child(disc)


func _add_smoke(wheelbase_m: float) -> void:
	var x_m := wheelbase_m - 0.55
	var stack := Art.cylinder(0.055, 1.0, CHROME, 12, 0.2, 0.9)
	stack.position = TrackSpace.point(x_m, 0.18, HOOD_TOP_M + 0.45)
	_body.add_child(stack)
	var cap := Art.cylinder(0.075, 0.05, FRAME, 12)
	cap.position = TrackSpace.point(x_m, 0.18, HOOD_TOP_M + 0.97)
	_body.add_child(cap)
	_smoke.position = TrackSpace.point(x_m, 0.18, HOOD_TOP_M + 1.0)
	_smoke.amount = 48
	_smoke.lifetime = 1.6
	_smoke.emitting = false
	_smoke.local_coords = false
	_smoke.direction = Vector3.UP
	_smoke.spread = 12.0
	_smoke.initial_velocity_min = 1.5
	_smoke.initial_velocity_max = 2.5
	_smoke.gravity = Vector3(0.0, 0.6, 0.0)
	_smoke.scale_amount_min = 0.6
	_smoke.scale_amount_max = 1.0
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.5))
	grow.add_point(Vector2(1.0, 3.5))
	_smoke.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, SMOKE)
	fade.set_color(1, Color(SMOKE.r * 3.0, SMOKE.g * 3.0, SMOKE.b * 3.0, 0.0))
	_smoke.color_ramp = fade
	_smoke.mesh = _puff_mesh(0.3)
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


# The weights drawn: suitcase weights on the front bracket (x) and wheel
# weights on each rear hub (y).
func weight_counts() -> Vector2i:
	var plates := 0
	for child: Node in _front_weights.get_children():
		if not child.is_queued_for_deletion() and child is MeshInstance3D:
			plates += 1
	var discs := 0
	var holder := _rear_left.get_node_or_null("weights")
	if holder != null:
		for child: Node in holder.get_children():
			if not child.is_queued_for_deletion():
				discs += 1
	# The bracket is not a weight.
	return Vector2i(maxi(plates - 1, 0), discs)


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
