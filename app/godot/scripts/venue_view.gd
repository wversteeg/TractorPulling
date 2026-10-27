# Where the pull takes place, around the track TrackView draws: a summer
# sky, grass, the prepared clay strip, a fence with straw bales along each
# side, a covered grandstand full of people on the left and a standing crowd
# behind the fence on the right, distance boards every 10 m, a banner over
# the full-pull line and a scoreboard showing how far the sled has got;
# beyond them food tents, a barn, trees and flags. Built from the run's
# sizes: again only when the track's length or lane width changes (another
# event's track), and the banner and scoreboard move with the full-pull
# line (to 110 m after a test pull past 100 m). Only the scoreboard changes
# during a pull.
#
# The people, trees, tents, barn, flags and clouds are Kenney's CC0 models
# (assets/kenney); the rest is built here. Copies of a thing are one
# MultiMesh each, and the scenery casts no shadows, for a phone's frame.
class_name VenueView
extends Node3D

const GRASS_DARK := Color(0.13, 0.25, 0.08)
const GRASS_LIGHT := Color(0.3, 0.43, 0.15)
const CLAY_DARK := Color(0.33, 0.22, 0.13)
const CLAY_LIGHT := Color(0.52, 0.37, 0.23)
const STRAW := Color(0.86, 0.72, 0.38)
const WOOD := Color(0.52, 0.36, 0.22)
const STEEL := Color(0.55, 0.57, 0.6)
const ROOF := Color(0.72, 0.12, 0.1)
const BOARD := Color(0.96, 0.96, 0.92)
const BANNER := Color(0.1, 0.32, 0.65)
const SHIRTS: Array[Color] = [Color(0.9, 0.2, 0.15), Color(0.2, 0.45, 0.85), Color(0.95, 0.8, 0.2),
		Color(0.25, 0.65, 0.3), Color(0.95, 0.95, 0.95), Color(0.15, 0.15, 0.2),
		Color(0.95, 0.5, 0.15), Color(0.6, 0.3, 0.7)]

# The grass field, and the clay strip's margin past the lane [m].
const FIELD_M := Vector2(1100.0, 1100.0)
# The hills round the field, and the tree line in front of them [m].
const HILLS_FROM_M := 420.0
const HILLS_TO_M := 560.0
const HILL_HEIGHT_M := 38.0
const HILL_SEGMENTS := 96
const HILLS := Color(0.3, 0.44, 0.24)
const FAR_HILLS := Color(0.42, 0.52, 0.5)
const TREE_LINE_M := 300.0
const TREE_LINE_TREES := 160
const COPSE_TREES := 9
# The fields between the venue and the tree line: their crops, with the
# rows they are sown in [m], and how many fields.
const CROPS: Array[Color] = [Color(0.82, 0.68, 0.32), Color(0.2, 0.36, 0.12),
		Color(0.4, 0.27, 0.16), Color(0.55, 0.6, 0.25)]
const CROP_ROW_M := 0.75
const FIELDS := 18
# The fields: lifted over the grass [m], tries to find room for them all,
# and how far past the lane's edges they stay clear (the stands, banks and
# tents are inside) [m].
const FIELD_LIFT_M := 0.15
const FIELD_TRIES := 20
const FIELDS_CLEAR_OF_LANE_M := 36.0
const GRASS_Y_M := -0.06
const STRIP_MARGIN_M := 3.0
const RUN_UP_M := 40.0
const OVERRUN_M := 30.0
# On the right, between the lane and the bales, room to drive back to the
# parking after a pull, and for the parking behind the start line: the
# fence, the bales and everything past them stand this much further out
# there [m].
const RETURN_LANE_M := 6.0
# The parking: its bays' middle right of the lane's right-hand chalk line,
# the player's bay and the rivals' behind it, down the track from the start
# line (behind it) [m]. The player's tractor leaves from its bay and comes
# back to it (TractorPullRun.set_parking).
const PARKING_RIGHT_OF_LANE_M := 6.0
const PLAYER_BAY_X_M := -12.0
const RIVAL_BAYS_X_M: Array[float] = [-19.0, -26.0, -33.0]
const BAY_SIZE_M := Vector2(6.0, 4.0)
# The signal light's lamps on the full-pull gantry.
const LAMP_RADIUS_M := 0.22
const LAMP_RED := Color(1.0, 0.12, 0.08)
const LAMP_GREEN := Color(0.15, 1.0, 0.3)
const LAMP_GLOW := 3.0
# The fence and the straw bales inside it, from the lane's edge [m].
const FENCE_OUT_M := 6.5
const BALES_OUT_M := 5.5
const POST_EVERY_M := 2.5
const BALE_SIZE_M := Vector3(1.2, 0.5, 0.45)
# The grandstand: from the fence back, rows rising like steps [m].
const STAND_FROM_M := 9.5
const STAND_ROWS := 4
const ROW_DEPTH_M := 0.9
const ROW_RISE_M := 0.5
const STAND_ALONG_M := Vector2(5.0, 95.0)
const SEAT_EVERY_M := 0.85
# People are built at their size (People); each varies a little.
const PERSON_SCALE := 1.0
const SEAT_FILL := 0.75
const STANDING_ROWS := 2
# The bank the standing crowd is on [m].
const BANK_HEIGHT_M := 0.9
const BANK_DEPTH_M := 3.5
# Kenney's scenery tiles stand on a plate of their own; sunk this share of
# their size into the grass, the plate is gone.
const TILE_PLATE_SHARE := 0.09
const STANDING_EVERY_M := 1.1
const BOARD_EVERY_M := 10.0
const CROWD_SHADER := preload("res://shaders/crowd.gdshader")
const FLAG_SHADER := preload("res://shaders/flag.gdshader")
const FLAGS: Array[Color] = [Color(0.85, 0.12, 0.1), Color(0.95, 0.95, 0.95),
		Color(0.1, 0.3, 0.7), Color(0.95, 0.6, 0.1)]
const FLAG_SIZE_M := Vector2(1.6, 1.0)
const FLAG_POLE_M := 2.4
# How excited the crowd is before a pull, and from where along the track
# it starts to cheer (a share of the full-pull line).
const CALM := 0.15
const CHEER_FROM := 0.75

var _scoreboard: Label3D
# The signal light on the full-pull gantry: its red and green lamps.
var _red_lamp := StandardMaterial3D.new()
var _green_lamp := StandardMaterial3D.new()
var _signal := ""
var _crowd_material: ShaderMaterial
# The track's length and lane width the venue was built for [m].
var _sizes := Vector2.ZERO
var _full_pull_m := -1.0
# The banner and the scoreboard, which follow the full-pull line.
var _line: Node3D
# The fields round the venue, in track space: each one's centre (x along,
# y across) and the circle it turns in (z) [m].
var _fields: Array[Vector3] = []
var _random := RandomNumberGenerator.new()


func build(d: Dictionary) -> void:
	var length_m: float = d["track_length_m"]
	var lane_m: float = d["lane_width_m"]
	var full_pull_m: float = d["full_pull_line_m"]
	var sizes := Vector2(length_m, lane_m)
	if sizes != _sizes:
		for child: Node in get_children():
			child.free()
		_line = null
		_sizes = sizes
		_full_pull_m = -1.0
		# The same venue at every launch.
		_random.seed = 2026
		_add_ground(length_m, lane_m)
		for side: float in [-1.0, 1.0]:
			_add_fence(length_m, lane_m, side)
			_add_boards(length_m, lane_m, side)
		_add_grandstand(lane_m)
		_add_standing_crowd(length_m, lane_m)
		_add_scenery(length_m, lane_m)
		_add_farmland(length_m, lane_m)
		_add_horizon(length_m)
	if full_pull_m != _full_pull_m:
		if _line != null:
			_line.free()
		_line = Node3D.new()
		add_child(_line)
		_add_banner(_line, full_pull_m, lane_m)
		_add_scoreboard(_line, full_pull_m, lane_m)
		_full_pull_m = full_pull_m


# Where the banner over the full-pull line stands, along the track [m].
func banner_at_m() -> float:
	return _full_pull_m


func fields() -> Array[Vector3]:
	return _fields


# How far the sled has got, on the board by the full-pull line; and the
# crowd gets louder as it nears the line.
func show_distance(sled_m: float) -> void:
	if _scoreboard != null:
		_scoreboard.text = "%6.2f m" % maxf(sled_m, 0.0)
	if _crowd_material != null and _full_pull_m > 0.0:
		var near := clampf((sled_m / _full_pull_m - CHEER_FROM) / (1.0 - CHEER_FROM), 0.0, 1.0)
		_crowd_material.set_shader_parameter("excitement", lerpf(CALM, 1.0, near))


# How excited the crowd is, 0 to 1.
func excitement() -> float:
	if _crowd_material == null:
		return 0.0
	return float(_crowd_material.get_shader_parameter("excitement"))


# The sky, its light and a little haze: added by the scene once. A summer
# afternoon: a deep blue sky paling to the horizon, the sun's disk, a soft
# glow on bright things, and haze that hides the field's far edge.
static func environment() -> WorldEnvironment:
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color(0.13, 0.33, 0.72)
	sky_material.sky_horizon_color = Color(0.66, 0.77, 0.9)
	sky_material.sky_curve = 0.12
	sky_material.ground_horizon_color = Color(0.6, 0.68, 0.6)
	sky_material.ground_bottom_color = Color(0.25, 0.32, 0.2)
	sky_material.sun_angle_max = 12.0
	sky_material.sun_curve = 0.08
	var sky := Sky.new()
	sky.sky_material = sky_material
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.55
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 1.05
	env.glow_enabled = true
	env.glow_intensity = 0.35
	env.glow_bloom = 0.04
	env.glow_hdr_threshold = 1.1
	env.adjustment_enabled = true
	env.adjustment_saturation = 1.18
	env.adjustment_contrast = 1.06
	env.fog_enabled = true
	env.fog_light_color = Color(0.7, 0.78, 0.88)
	env.fog_density = 0.0009
	env.fog_sky_affect = 0.0
	var world := WorldEnvironment.new()
	world.environment = env
	return world


func _add_ground(length_m: float, lane_m: float) -> void:
	var grass_material := StandardMaterial3D.new()
	grass_material.albedo_texture = Art.ground_texture(GRASS_DARK, GRASS_LIGHT, 0.02, 7)
	grass_material.normal_enabled = true
	grass_material.normal_texture = Art.ground_normal(0.08, 8, 6.0)
	grass_material.normal_scale = 0.6
	grass_material.uv1_scale = Vector3(FIELD_M.x / 12.0, FIELD_M.y / 12.0, 1.0)
	grass_material.roughness = 1.0
	var grass := PlaneMesh.new()
	grass.size = FIELD_M
	var field := Art.instance(grass, grass_material)
	field.position = TrackSpace.point(length_m / 2.0, 0.0, GRASS_Y_M)
	_quiet(field)
	add_child(field)

	var strip_length_m := RUN_UP_M + length_m + OVERRUN_M
	var clay_material := StandardMaterial3D.new()
	clay_material.albedo_texture = Art.ground_texture(CLAY_DARK, CLAY_LIGHT, 0.05, 11)
	var strip_width_m := lane_m + 2.0 * (BALES_OUT_M - 0.5) + RETURN_LANE_M
	# Clods catch the low sun: a fine normal map over the colour.
	clay_material.normal_enabled = true
	clay_material.normal_texture = Art.ground_normal(0.06, 12, 5.0)
	clay_material.normal_scale = 0.8
	clay_material.uv1_scale = Vector3(strip_width_m / 6.0, strip_length_m / 6.0, 1.0)
	clay_material.roughness = 0.95
	var clay := PlaneMesh.new()
	clay.size = Vector2(strip_width_m, strip_length_m)
	var strip := Art.instance(clay, clay_material)
	# Wider on the right, for the return lane and the parking.
	strip.position = TrackSpace.point(length_m + OVERRUN_M - strip_length_m / 2.0,
			-RETURN_LANE_M / 2.0, -0.03)
	_quiet(strip)
	add_child(strip)
	_add_bays(lane_m)


# The parking's bays behind the start line: white lines on the clay, the
# player's and the rivals'.
func _add_bays(lane_m: float) -> void:
	var y_m := -(lane_m / 2.0 + PARKING_RIGHT_OF_LANE_M)
	var lines: Array[Transform3D] = []
	var bays: Array[float] = [PLAYER_BAY_X_M]
	bays.append_array(RIVAL_BAYS_X_M)
	for x_m in bays:
		for side: float in [-1.0, 1.0]:
			lines.append(Transform3D(Basis(),
					TrackSpace.point(x_m, y_m + side * BAY_SIZE_M.y / 2.0, -0.02)))
	var line := BoxMesh.new()
	line.size = Vector3(0.12, 0.02, BAY_SIZE_M.x)
	add_child(_quiet(Art.many(line, lines, Art.material(Color(0.95, 0.95, 0.9), 0.9))))


# How much further out the venue stands beside the lane on `side` (-1 right,
# 1 left): the return lane is on the right.
static func past_lane_m(side: float) -> float:
	return RETURN_LANE_M if side < 0.0 else 0.0


func _add_fence(length_m: float, lane_m: float, side: float) -> void:
	var from_m := -RUN_UP_M
	var to_m := length_m + OVERRUN_M
	var posts: Array[Transform3D] = []
	var x_m := from_m
	while x_m <= to_m:
		posts.append(Transform3D(Basis(), TrackSpace.point(x_m,
				side * (lane_m / 2.0 + FENCE_OUT_M + past_lane_m(side)), 0.55)))
		x_m += POST_EVERY_M
	var post_mesh := BoxMesh.new()
	post_mesh.size = Vector3(0.1, 1.1, 0.1)
	add_child(_quiet(Art.many(post_mesh, posts, Art.material(WOOD))))
	for height_m: float in [0.45, 0.95]:
		var rail := Art.box(Vector3(0.06, 0.12, to_m - from_m), BOARD)
		rail.position = TrackSpace.point((from_m + to_m) / 2.0,
				side * (lane_m / 2.0 + FENCE_OUT_M + past_lane_m(side)), height_m)
		add_child(_quiet(rail))

	# Straw bales two high inside the fence, a few knocked askew.
	var bales: Array[Transform3D] = []
	x_m = from_m + 1.0
	while x_m <= to_m:
		for layer in 2:
			var yaw := _random.randf_range(-0.06, 0.06)
			var at := TrackSpace.point(x_m + (0.6 if layer == 1 else 0.0),
					side * (lane_m / 2.0 + BALES_OUT_M + past_lane_m(side)),
					BALE_SIZE_M.y * (layer + 0.5))
			bales.append(Transform3D(Basis(Vector3.UP, yaw), at))
		x_m += BALE_SIZE_M.z * 2.6 + 0.05
	var bale_mesh := BoxMesh.new()
	bale_mesh.size = Vector3(BALE_SIZE_M.z, BALE_SIZE_M.y, BALE_SIZE_M.x)
	add_child(_quiet(Art.many(bale_mesh, bales, Art.material(STRAW, 1.0))))


# A white board on a post every 10 m, its distance facing the lane.
func _add_boards(length_m: float, lane_m: float, side: float) -> void:
	var x_m := BOARD_EVERY_M
	while x_m <= length_m + 0.5 * BOARD_EVERY_M:
		var at := TrackSpace.point(x_m,
				side * (lane_m / 2.0 + BALES_OUT_M + past_lane_m(side) - 1.2), 0.0)
		var post := Art.box(Vector3(0.08, 1.4, 0.08), STEEL)
		post.position = at + Vector3(0.0, 0.7, 0.0)
		add_child(_quiet(post))
		var board := Art.box(Vector3(1.1, 0.7, 0.05), BOARD)
		board.position = at + Vector3(0.0, 1.6, 0.0)
		# Facing back down the track, where the cameras are.
		add_child(_quiet(board))
		var number := Art.label("%d" % int(x_m), 64, Color(0.1, 0.1, 0.1), BOARD)
		number.position = at + Vector3(0.0, 1.6, 0.03)
		add_child(number)
		x_m += BOARD_EVERY_M


func _add_grandstand(lane_m: float) -> void:
	var base_y_m := lane_m / 2.0 + STAND_FROM_M
	var along_m := STAND_ALONG_M.y - STAND_ALONG_M.x
	var middle_m := (STAND_ALONG_M.x + STAND_ALONG_M.y) / 2.0
	var seats: Array[Transform3D] = []
	var colours: Array[Color] = []
	for row in STAND_ROWS:
		var y_m := base_y_m + row * ROW_DEPTH_M
		var z_m := (row + 1) * ROW_RISE_M
		var step := Art.box(Vector3(ROW_DEPTH_M, z_m, along_m), Color(0.75, 0.75, 0.72))
		step.position = TrackSpace.point(middle_m, y_m, z_m / 2.0)
		add_child(_quiet(step))
		var bench := Art.box(Vector3(0.35, 0.08, along_m), WOOD)
		bench.position = TrackSpace.point(middle_m, y_m + 0.2, z_m + 0.4)
		add_child(_quiet(bench))
		var x_m := STAND_ALONG_M.x + 0.5
		while x_m < STAND_ALONG_M.y - 0.5:
			if _random.randf() < SEAT_FILL:
				# Facing the track (the model faces +z: turned to +x, the
				# track's right), a little turned towards the action.
				var basis := Basis(Vector3.UP, PI / 2.0 + _random.randf_range(-0.35, 0.35))
				basis = basis.scaled(Vector3.ONE * PERSON_SCALE * _random.randf_range(0.9, 1.05))
				# The hips on the bench, the feet in front of it.
				seats.append(Transform3D(basis, TrackSpace.point(x_m + _random.randf_range(-0.1, 0.1),
						y_m - 0.15, z_m)))
				colours.append(_shirt())
			x_m += SEAT_EVERY_M
	add_child(_crowd(seats, colours, true))

	# A roof on posts over the stand, and flags along its front.
	var back_y_m := base_y_m + STAND_ROWS * ROW_DEPTH_M + 0.3
	var roof_z_m := STAND_ROWS * ROW_RISE_M + 3.2
	var roof := Art.box(Vector3(STAND_ROWS * ROW_DEPTH_M + 1.6, 0.15, along_m + 1.0), ROOF)
	roof.position = TrackSpace.point(middle_m, base_y_m + STAND_ROWS * ROW_DEPTH_M / 2.0, roof_z_m)
	roof.rotation.z = -0.12
	add_child(_quiet(roof))
	var back := Art.box(Vector3(0.15, roof_z_m, along_m), Color(0.85, 0.85, 0.82))
	back.position = TrackSpace.point(middle_m, back_y_m, roof_z_m / 2.0)
	add_child(_quiet(back))
	var x_m := STAND_ALONG_M.x
	while x_m <= STAND_ALONG_M.y + 0.1:
		var pole := Art.cylinder(0.08, roof_z_m, STEEL, 8)
		pole.position = TrackSpace.point(x_m, base_y_m - 0.5, roof_z_m / 2.0)
		add_child(_quiet(pole))
		_add_flag(TrackSpace.point(x_m, base_y_m - 0.5, roof_z_m),
				FLAGS[int(x_m) % FLAGS.size()], x_m)
		x_m += along_m / 6.0
	var sign := Art.label("TRACTOR PULLING  ·  CLUB PULL", 180, Color.WHITE, ROOF.darkened(0.4))
	sign.position = TrackSpace.point(middle_m, base_y_m - 1.0, roof_z_m - 0.5)
	sign.rotation.y = PI / 2.0
	add_child(sign)


# A pole on the grandstand's roof with a flag waving from its top.
func _add_flag(foot: Vector3, colour: Color, phase: float) -> void:
	var pole := Art.cylinder(0.04, FLAG_POLE_M, STEEL, 6, 0.4, 0.6)
	pole.position = foot + Vector3(0.0, FLAG_POLE_M / 2.0, 0.0)
	add_child(_quiet(pole))
	var cloth := PlaneMesh.new()
	cloth.size = FLAG_SIZE_M
	cloth.subdivide_width = 10
	cloth.orientation = PlaneMesh.FACE_Z
	var material := ShaderMaterial.new()
	material.shader = FLAG_SHADER
	material.set_shader_parameter("colour", colour)
	material.set_shader_parameter("phase", phase)
	var flag := Art.instance(cloth, material)
	# Flying from the pole along the track (Godot's -z is down the track,
	# the plane spans x: turned so its pole edge is at the pole).
	flag.rotation.y = PI / 2.0
	flag.position = foot + Vector3(0.0, FLAG_POLE_M - FLAG_SIZE_M.y / 2.0, -FLAG_SIZE_M.x / 2.0)
	add_child(_quiet(flag))


# People standing two deep on a grass bank behind the fence on the right,
# so they see over it.
func _add_standing_crowd(length_m: float, lane_m: float) -> void:
	var bank_from_m := lane_m / 2.0 + FENCE_OUT_M + RETURN_LANE_M + 0.4
	var bank := Art.box(Vector3(BANK_DEPTH_M, BANK_HEIGHT_M, length_m + 30.0), GRASS_LIGHT, 1.0)
	bank.position = TrackSpace.point(length_m / 2.0, -(bank_from_m + BANK_DEPTH_M / 2.0),
			BANK_HEIGHT_M / 2.0 - 0.05)
	add_child(_quiet(bank))
	var people: Array[Transform3D] = []
	var colours: Array[Color] = []
	for row in STANDING_ROWS:
		var x_m := -10.0
		while x_m < length_m + 15.0:
			if _random.randf() < 0.8:
				var basis := Basis(Vector3.UP, -PI / 2.0 + _random.randf_range(-0.5, 0.5))
				basis = basis.scaled(Vector3.ONE * PERSON_SCALE * _random.randf_range(0.85, 1.05))
				var y_m := -(bank_from_m + 0.4 + row * 1.0 + _random.randf_range(0.0, 0.4))
				people.append(Transform3D(basis, TrackSpace.point(
						x_m + _random.randf_range(-0.3, 0.3), y_m, BANK_HEIGHT_M - 0.05)))
				colours.append(_shirt())
			x_m += STANDING_EVERY_M
	add_child(_crowd(people, colours, false))


# A crowd in one MultiMesh: each person's shirt and skin ride in its
# custom data (crowd.gdshader).
func _crowd(transforms: Array[Transform3D], shirts: Array[Color],
		seated: bool) -> MultiMeshInstance3D:
	# One moving crowd material for both crowds, so one call stirs them.
	if _crowd_material == null:
		_crowd_material = ShaderMaterial.new()
		_crowd_material.shader = CROWD_SHADER
		_crowd_material.set_shader_parameter("excitement", CALM)
	var custom: Array[Color] = []
	# White per copy: the vertex colour must reach the shader unchanged,
	# and a MultiMesh without colours can zero it.
	var white: Array[Color] = []
	for shirt in shirts:
		custom.append(Color(shirt.r, shirt.g, shirt.b, _random.randf()))
		white.append(Color.WHITE)
	var person := People.seated() if seated else People.standing()
	var crowd := Art.many(person, transforms, _crowd_material, white, custom)
	return _quiet(crowd) as MultiMeshInstance3D


func _shirt() -> Color:
	var colour: Color = SHIRTS[_random.randi() % SHIRTS.size()]
	return colour.lerp(Color.WHITE, _random.randf_range(0.0, 0.25))


# A banner over the full-pull line on two poles.
func _add_banner(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var half_m := lane_m / 2.0 + 1.5
	var height_m := 5.5
	for side: float in [-1.0, 1.0]:
		var pole := Art.cylinder(0.12, height_m, STEEL, 10, 0.4, 0.6)
		pole.position = TrackSpace.point(full_pull_m, side * half_m, height_m / 2.0)
		parent.add_child(pole)
	var banner := Art.box(Vector3(2.0 * half_m, 1.0, 0.08), BANNER)
	banner.position = TrackSpace.point(full_pull_m, 0.0, height_m - 0.5)
	parent.add_child(_quiet(banner))
	var text := Art.label("FULL PULL", 120, Color.WHITE, BANNER.darkened(0.5))
	text.position = TrackSpace.point(full_pull_m - 0.06, 0.0, height_m - 0.5)
	parent.add_child(text)
	# The signal light on top, facing down the track: red above green, as
	# the officials' flags show (show_signal()).
	var housing := Art.box(Vector3(0.7, 1.3, 0.35), Color(0.08, 0.08, 0.09))
	housing.position = TrackSpace.point(full_pull_m, 0.0, height_m + 0.65)
	parent.add_child(_quiet(housing))
	for lamp: Array in [[_red_lamp, 0.95], [_green_lamp, 0.35]]:
		var sphere := SphereMesh.new()
		sphere.radius = LAMP_RADIUS_M
		sphere.height = 2.0 * LAMP_RADIUS_M
		var bulb := Art.instance(sphere, lamp[0])
		bulb.position = TrackSpace.point(full_pull_m - 0.18, 0.0, height_m + float(lamp[1]))
		parent.add_child(_quiet(bulb))
	_signal = "?"
	show_signal("off")


# The signal light: "green" while the pull is good, "red" to stop, "off".
func show_signal(signal_name: String) -> void:
	if signal_name == _signal:
		return
	_signal = signal_name
	for pair: Array in [[_red_lamp, LAMP_RED, "red"], [_green_lamp, LAMP_GREEN, "green"]]:
		var lamp: StandardMaterial3D = pair[0]
		var lit: bool = signal_name == pair[2]
		lamp.albedo_color = pair[1] if lit else Color(pair[1]).darkened(0.8)
		lamp.emission_enabled = lit
		lamp.emission = pair[1]
		lamp.emission_energy_multiplier = LAMP_GLOW


# The signal light's state now (show_signal()), for tests.
func signal_shown() -> String:
	return _signal


# The scoreboard on the right of the full-pull line, facing the track.
func _add_scoreboard(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var at := TrackSpace.point(full_pull_m - 8.0,
			-(lane_m / 2.0 + FENCE_OUT_M + RETURN_LANE_M + 4.0), 0.0)
	for side: float in [-1.0, 1.0]:
		var leg := Art.box(Vector3(0.2, 4.0, 0.2), STEEL)
		leg.position = at + Vector3(side * 2.0, 2.0, 0.0)
		parent.add_child(_quiet(leg))
	var panel := Art.box(Vector3(5.0, 2.0, 0.3), Color(0.08, 0.08, 0.1))
	panel.position = at + Vector3(0.0, 5.0, 0.0)
	panel.rotation.y = -0.5
	parent.add_child(panel)
	_scoreboard = Art.label("  0.00 m", 160, Color(1.0, 0.75, 0.15), Color(0.0, 0.0, 0.0, 0.0))
	_scoreboard.position = at + Vector3(0.0, 5.0, 0.0) + Basis(Vector3.UP, -0.5) * Vector3(0.0, 0.0, 0.17)
	_scoreboard.rotation.y = -0.5
	parent.add_child(_scoreboard)


# Tents behind the standing crowd, a barn at the far end, trees around, a
# few clouds.
func _add_scenery(length_m: float, lane_m: float) -> void:
	var right_m := -(lane_m / 2.0 + FENCE_OUT_M + RETURN_LANE_M + 9.0)
	for x_m: float in [15.0, 45.0, 75.0]:
		_place(KenneyModels.TENTS, TrackSpace.point(x_m, right_m - 4.0, 0.0), 1.0, PI / 2.0)
	_place(KenneyModels.BARN, TrackSpace.point(length_m + 55.0, 18.0, 0.0), 14.0, 0.3)
	# Copses at the corners and single trees along the sides: pines and
	# broadleaves, one MultiMesh each.
	var pines: Array[Vector3] = []
	var broadleaves: Array[Vector3] = []
	for x_m: float in [-60.0, 160.0]:
		for y_m: float in [-55.0, -35.0, 35.0, 55.0]:
			for i in COPSE_TREES:
				var spot := TrackSpace.point(x_m + _random.randf_range(-9.0, 9.0),
						y_m + _random.randf_range(-7.0, 7.0), 0.0)
				(pines if _random.randf() < 0.6 else broadleaves).append(spot)
	for i in 18:
		var x_m := _random.randf_range(-40.0, length_m + 60.0)
		var left := i % 2 == 0
		var y_m := _random.randf_range(32.0, 70.0) * (1.0 if left else -1.0)
		(broadleaves if i % 3 else pines).append(TrackSpace.point(x_m, y_m, 0.0))
	add_child(Trees.many(Trees.pine(), pines, _random))
	add_child(Trees.many(Trees.broadleaf(), broadleaves, _random))
	for i in 7:
		var cloud := KenneyModels.scene(KenneyModels.CLOUD)
		cloud.scale = Vector3(30.0, 8.0, 18.0) * _random.randf_range(0.7, 1.3)
		cloud.position = TrackSpace.point(_random.randf_range(-80.0, 250.0),
				_random.randf_range(-160.0, 160.0), _random.randf_range(60.0, 90.0))
		_quiet_all(cloud)
		add_child(cloud)


# Fields of wheat, maize, ploughed land and grass ley round the venue,
# each a flat patch with its rows, so the land around reads as farmland.
func _add_farmland(length_m: float, lane_m: float) -> void:
	var materials: Array[StandardMaterial3D] = []
	var rows := _rows_texture()
	for crop in CROPS:
		var m := StandardMaterial3D.new()
		m.albedo_texture = rows
		m.albedo_color = crop
		m.roughness = 1.0
		materials.append(m)
	var centre := TrackSpace.point(length_m / 2.0, 0.0, 0.0)
	# The strip from the run-up to the overrun, the stands, banks and tents
	# beside it, in track space: no field reaches into it, since a field
	# lies above the clay and would cover the lane.
	var half_m := lane_m / 2.0 + FIELDS_CLEAR_OF_LANE_M
	var keep_clear := Rect2(-RUN_UP_M, -half_m, RUN_UP_M + length_m + OVERRUN_M, 2.0 * half_m)
	# Each field's circle (its centre and half its diagonal), so the next
	# can be kept off it: fields that overlapped would fight for depth.
	var taken: Array[Vector4] = []
	_fields.clear()
	for i in FIELDS * FIELD_TRIES:
		if taken.size() >= FIELDS:
			break
		var angle := _random.randf_range(0.0, TAU)
		var reach := _random.randf_range(110.0, 250.0)
		var size := Vector2(_random.randf_range(60.0, 110.0), _random.randf_range(50.0, 90.0))
		var at := centre + Vector3(cos(angle), 0.0, sin(angle)) * reach
		var radius := size.length() / 2.0
		# The field's centre in track space (TrackSpace.point inverted).
		var track_at := Vector2(-at.z, -at.x)
		var nearest := Vector2(clampf(track_at.x, keep_clear.position.x, keep_clear.end.x),
				clampf(track_at.y, keep_clear.position.y, keep_clear.end.y))
		if nearest.distance_to(track_at) < radius:
			continue
		var clear := true
		for other in taken:
			if Vector2(at.x, at.z).distance_to(Vector2(other.x, other.y)) < radius + other.z:
				clear = false
				break
		if not clear:
			continue
		taken.append(Vector4(at.x, at.z, radius, 0.0))
		_fields.append(Vector3(track_at.x, track_at.y, radius))
		var patch := PlaneMesh.new()
		patch.size = size
		# Its own copy: the rows tile to the field's size.
		var surface := materials[taken.size() % CROPS.size()].duplicate() as StandardMaterial3D
		surface.uv1_scale = Vector3(size.x / (CROP_ROW_M * 16.0), size.y / 8.0, 1.0)
		var field := Art.instance(patch, surface)
		# Lifted clear of the grass, far enough for a phone's depth buffer at
		# this distance.
		field.position = at + Vector3(0.0, GRASS_Y_M + FIELD_LIFT_M, 0.0)
		field.rotation.y = _random.randf_range(0.0, TAU)
		add_child(_quiet(field))


# Sixteen rows across, a little uneven along: shared by every crop.
static func _rows_texture() -> ImageTexture:
	var size := Vector2i(64, 32)
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGB8)
	for y in size.y:
		for x in size.x:
			var row := 0.78 + 0.22 * maxf(0.0, sin(TAU * 16.0 * (float(x) + 0.5) / size.x))
			var speck := 0.95 + 0.05 * sin(float(x * 7 + y * 13))
			image.set_pixel(x, y, Color(row * speck, row * speck, row * speck))
	return ImageTexture.create_from_image(image)


# Hills all round, beyond a tree line, so the field has no edge: one mesh
# of rolling ridges (its height a few sines of the angle, so it is the
# same every launch), shaded from green near to a hazy blue-green far, and
# the trees as one MultiMesh.
func _add_horizon(length_m: float) -> void:
	var centre := TrackSpace.point(length_m / 2.0, 0.0, 0.0)
	# Three rings of points: the near foot, the ridge, and the far slope;
	# each segment two quads between them.
	var vertices := PackedVector3Array()
	var colours := PackedColorArray()
	for i in HILL_SEGMENTS + 1:
		var angle := TAU * float(i) / HILL_SEGMENTS
		var out := Vector3(cos(angle), 0.0, sin(angle))
		var h := _hill_height(angle)
		vertices.append(centre + out * HILLS_FROM_M + Vector3(0.0, -2.0, 0.0))
		colours.append(HILLS)
		vertices.append(centre + out * lerpf(HILLS_FROM_M, HILLS_TO_M, 0.55) + Vector3(0.0, h, 0.0))
		colours.append(HILLS.lerp(FAR_HILLS, 0.5))
		vertices.append(centre + out * HILLS_TO_M + Vector3(0.0, h * 0.6, 0.0))
		colours.append(FAR_HILLS)
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in HILL_SEGMENTS:
		for ring in 2:
			var a := i * 3 + ring
			var b := (i + 1) * 3 + ring
			for index: int in [a, b, a + 1, b, b + 1, a + 1]:
				tool.set_color(colours[index])
				tool.add_vertex(vertices[index])
	tool.generate_normals()
	var hills_material := StandardMaterial3D.new()
	hills_material.vertex_color_use_as_albedo = true
	hills_material.vertex_color_is_srgb = true
	hills_material.roughness = 1.0
	hills_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	add_child(_quiet(Art.instance(tool.commit(), hills_material)))

	var pines: Array[Vector3] = []
	var broadleaves: Array[Vector3] = []
	for i in TREE_LINE_TREES:
		var angle := TAU * (float(i) + _random.randf_range(-0.3, 0.3)) / TREE_LINE_TREES
		var reach := TREE_LINE_M + _random.randf_range(-25.0, 25.0)
		var at := centre + Vector3(cos(angle), 0.0, sin(angle)) * reach
		(pines if _random.randf() < 0.5 else broadleaves).append(at)
	add_child(Trees.many(Trees.pine(), pines, _random, 1.0, 1.6))
	add_child(Trees.many(Trees.broadleaf(), broadleaves, _random, 1.0, 1.6))


# The ridge's height at `angle` round the field [m].
static func _hill_height(angle: float) -> float:
	var rolling := 0.55 + 0.25 * sin(3.0 * angle + 0.7) + 0.15 * sin(7.0 * angle + 2.1) \
			+ 0.05 * sin(13.0 * angle)
	return HILL_HEIGHT_M * rolling


func _place(path: String, at: Vector3, scale_by: float, yaw: float) -> void:
	var model := KenneyModels.scene(path)
	model.scale = Vector3.ONE * scale_by
	var tile_m := 10.0 if path == KenneyModels.TENTS else 1.0
	model.position = at - Vector3(0.0, TILE_PLATE_SHARE * tile_m * scale_by, 0.0)
	model.rotation.y = yaw
	_quiet_all(model)
	add_child(model)


# Scenery casts no shadows: only the tractor and the sled do.
static func _quiet(node: GeometryInstance3D) -> GeometryInstance3D:
	node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return node


static func _quiet_all(root: Node) -> void:
	for node in root.find_children("*", "GeometryInstance3D", true, false):
		_quiet(node as GeometryInstance3D)
