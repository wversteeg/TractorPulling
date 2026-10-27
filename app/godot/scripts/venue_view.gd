# Where the pull takes place, around the track TrackView draws: a summer
# sky, grass, the prepared clay strip, a fence with straw bales along each
# side, a covered grandstand full of people on the left and a standing crowd
# behind the fence on the right, distance boards every 10 m, a banner over
# the full-pull line and a scoreboard showing how far the sled has got;
# beyond them food tents, a barn, trees and flags. Built once, from the
# first run's sizes; only the scoreboard changes during a pull.
#
# The people, trees, tents, barn, flags and clouds are Kenney's CC0 models
# (assets/kenney); the rest is built here. Copies of a thing are one
# MultiMesh each, and the scenery casts no shadows, for a phone's frame.
class_name VenueView
extends Node3D

const GRASS_DARK := Color(0.16, 0.3, 0.1)
const GRASS_LIGHT := Color(0.28, 0.45, 0.16)
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
const FIELD_M := Vector2(420.0, 520.0)
const STRIP_MARGIN_M := 3.0
const RUN_UP_M := 40.0
const OVERRUN_M := 30.0
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
# A person: the character model is 1.1 m tall with its antenna, which goes.
const PERSON_SCALE := 1.5
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

var _scoreboard: Label3D
var _built := false
var _random := RandomNumberGenerator.new()


func build(d: Dictionary) -> void:
	if _built:
		return
	_built = true
	# The same venue at every launch.
	_random.seed = 2026
	var length_m: float = d["track_length_m"]
	var lane_m: float = d["lane_width_m"]
	var full_pull_m: float = d["full_pull_line_m"]
	_add_ground(length_m, lane_m)
	for side: float in [-1.0, 1.0]:
		_add_fence(length_m, lane_m, side)
		_add_boards(length_m, lane_m, side)
	_add_grandstand(lane_m)
	_add_standing_crowd(length_m, lane_m)
	_add_banner(full_pull_m, lane_m)
	_add_scoreboard(full_pull_m, lane_m)
	_add_scenery(length_m, lane_m)


# How far the sled has got, on the board by the full-pull line.
func show_distance(sled_m: float) -> void:
	if _scoreboard != null:
		_scoreboard.text = "%6.2f m" % maxf(sled_m, 0.0)


# The sky, its light and a little haze: added by the scene once.
static func environment() -> WorldEnvironment:
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color(0.18, 0.4, 0.78)
	sky_material.sky_horizon_color = Color(0.62, 0.74, 0.88)
	sky_material.ground_horizon_color = Color(0.55, 0.62, 0.55)
	sky_material.ground_bottom_color = Color(0.3, 0.36, 0.25)
	sky_material.sun_angle_max = 20.0
	var sky := Sky.new()
	sky.sky_material = sky_material
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.45
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.tonemap_exposure = 0.95
	env.fog_enabled = true
	env.fog_light_color = Color(0.62, 0.72, 0.85)
	env.fog_density = 0.0006
	var world := WorldEnvironment.new()
	world.environment = env
	return world


func _add_ground(length_m: float, lane_m: float) -> void:
	var grass_material := StandardMaterial3D.new()
	grass_material.albedo_texture = Art.ground_texture(GRASS_DARK, GRASS_LIGHT, 0.02, 7)
	grass_material.uv1_scale = Vector3(FIELD_M.x / 12.0, FIELD_M.y / 12.0, 1.0)
	grass_material.roughness = 1.0
	var grass := PlaneMesh.new()
	grass.size = FIELD_M
	var field := Art.instance(grass, grass_material)
	field.position = TrackSpace.point(length_m / 2.0, 0.0, -0.06)
	_quiet(field)
	add_child(field)

	var strip_length_m := RUN_UP_M + length_m + OVERRUN_M
	var clay_material := StandardMaterial3D.new()
	clay_material.albedo_texture = Art.ground_texture(CLAY_DARK, CLAY_LIGHT, 0.05, 11)
	var strip_width_m := lane_m + 2.0 * (BALES_OUT_M - 0.5)
	clay_material.uv1_scale = Vector3(strip_width_m / 6.0, strip_length_m / 6.0, 1.0)
	clay_material.roughness = 1.0
	var clay := PlaneMesh.new()
	clay.size = Vector2(strip_width_m, strip_length_m)
	var strip := Art.instance(clay, clay_material)
	strip.position = TrackSpace.point(length_m + OVERRUN_M - strip_length_m / 2.0, 0.0, -0.03)
	_quiet(strip)
	add_child(strip)


func _add_fence(length_m: float, lane_m: float, side: float) -> void:
	var from_m := -RUN_UP_M
	var to_m := length_m + OVERRUN_M
	var posts: Array[Transform3D] = []
	var x_m := from_m
	while x_m <= to_m:
		posts.append(Transform3D(Basis(), TrackSpace.point(x_m, side * (lane_m / 2.0 + FENCE_OUT_M),
				0.55)))
		x_m += POST_EVERY_M
	var post_mesh := BoxMesh.new()
	post_mesh.size = Vector3(0.1, 1.1, 0.1)
	add_child(_quiet(Art.many(post_mesh, posts, Art.material(WOOD))))
	for height_m: float in [0.45, 0.95]:
		var rail := Art.box(Vector3(0.06, 0.12, to_m - from_m), BOARD)
		rail.position = TrackSpace.point((from_m + to_m) / 2.0, side * (lane_m / 2.0 + FENCE_OUT_M),
				height_m)
		add_child(_quiet(rail))

	# Straw bales two high inside the fence, a few knocked askew.
	var bales: Array[Transform3D] = []
	x_m = from_m + 1.0
	while x_m <= to_m:
		for layer in 2:
			var yaw := _random.randf_range(-0.06, 0.06)
			var at := TrackSpace.point(x_m + (0.6 if layer == 1 else 0.0),
					side * (lane_m / 2.0 + BALES_OUT_M), BALE_SIZE_M.y * (layer + 0.5))
			bales.append(Transform3D(Basis(Vector3.UP, yaw), at))
		x_m += BALE_SIZE_M.z * 2.6 + 0.05
	var bale_mesh := BoxMesh.new()
	bale_mesh.size = Vector3(BALE_SIZE_M.z, BALE_SIZE_M.y, BALE_SIZE_M.x)
	add_child(_quiet(Art.many(bale_mesh, bales, Art.material(STRAW, 1.0))))


# A white board on a post every 10 m, its distance facing the lane.
func _add_boards(length_m: float, lane_m: float, side: float) -> void:
	var x_m := BOARD_EVERY_M
	while x_m <= length_m + 0.5 * BOARD_EVERY_M:
		var at := TrackSpace.point(x_m, side * (lane_m / 2.0 + BALES_OUT_M - 1.2), 0.0)
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
				seats.append(Transform3D(basis, TrackSpace.point(x_m + _random.randf_range(-0.1, 0.1),
						y_m + 0.05, z_m)))
				colours.append(_shirt())
			x_m += SEAT_EVERY_M
	add_child(_crowd(seats, colours))

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
		var flag := KenneyModels.scene(KenneyModels.FLAG)
		flag.scale = Vector3.ONE * 2.0
		flag.position = TrackSpace.point(x_m, base_y_m - 0.6, roof_z_m + 0.1)
		_quiet_all(flag)
		add_child(flag)
		x_m += along_m / 6.0
	var sign := Art.label("TRACTOR PULLING  ·  CLUB PULL", 180, Color.WHITE, ROOF.darkened(0.4))
	sign.position = TrackSpace.point(middle_m, base_y_m - 1.0, roof_z_m - 0.5)
	sign.rotation.y = PI / 2.0
	add_child(sign)


# People standing two deep on a grass bank behind the fence on the right,
# so they see over it.
func _add_standing_crowd(length_m: float, lane_m: float) -> void:
	var bank_from_m := lane_m / 2.0 + FENCE_OUT_M + 0.4
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
				var y_m := -(lane_m / 2.0 + FENCE_OUT_M + 0.8 + row * 1.0 + _random.randf_range(0.0, 0.4))
				people.append(Transform3D(basis, TrackSpace.point(
						x_m + _random.randf_range(-0.3, 0.3), y_m, BANK_HEIGHT_M - 0.05)))
				colours.append(_shirt())
			x_m += STANDING_EVERY_M
	add_child(_crowd(people, colours))


func _crowd(transforms: Array[Transform3D], colours: Array[Color]) -> MultiMeshInstance3D:
	var materials: Array[Material] = []
	var person := KenneyModels.merged(KenneyModels.CHARACTER, PackedStringArray(["antenna"]),
			materials)
	return _quiet(Art.many(person, transforms, materials[0], colours)) as MultiMeshInstance3D


func _shirt() -> Color:
	var colour: Color = SHIRTS[_random.randi() % SHIRTS.size()]
	return colour.lerp(Color.WHITE, _random.randf_range(0.0, 0.25))


# A banner over the full-pull line on two poles.
func _add_banner(full_pull_m: float, lane_m: float) -> void:
	var half_m := lane_m / 2.0 + 1.5
	var height_m := 5.5
	for side: float in [-1.0, 1.0]:
		var pole := Art.cylinder(0.12, height_m, STEEL, 10, 0.4, 0.6)
		pole.position = TrackSpace.point(full_pull_m, side * half_m, height_m / 2.0)
		add_child(pole)
	var banner := Art.box(Vector3(2.0 * half_m, 1.0, 0.08), BANNER)
	banner.position = TrackSpace.point(full_pull_m, 0.0, height_m - 0.5)
	add_child(_quiet(banner))
	var text := Art.label("FULL PULL", 120, Color.WHITE, BANNER.darkened(0.5))
	text.position = TrackSpace.point(full_pull_m - 0.06, 0.0, height_m - 0.5)
	add_child(text)


# The scoreboard on the right of the full-pull line, facing the track.
func _add_scoreboard(full_pull_m: float, lane_m: float) -> void:
	var at := TrackSpace.point(full_pull_m - 8.0, -(lane_m / 2.0 + FENCE_OUT_M + 4.0), 0.0)
	for side: float in [-1.0, 1.0]:
		var leg := Art.box(Vector3(0.2, 4.0, 0.2), STEEL)
		leg.position = at + Vector3(side * 2.0, 2.0, 0.0)
		add_child(_quiet(leg))
	var panel := Art.box(Vector3(5.0, 2.0, 0.3), Color(0.08, 0.08, 0.1))
	panel.position = at + Vector3(0.0, 5.0, 0.0)
	panel.rotation.y = -0.5
	add_child(panel)
	_scoreboard = Art.label("  0.00 m", 160, Color(1.0, 0.75, 0.15), Color(0.0, 0.0, 0.0, 0.0))
	_scoreboard.position = at + Vector3(0.0, 5.0, 0.0) + Basis(Vector3.UP, -0.5) * Vector3(0.0, 0.0, 0.17)
	_scoreboard.rotation.y = -0.5
	add_child(_scoreboard)


# Tents behind the standing crowd, a barn at the far end, trees around, a
# few clouds.
func _add_scenery(length_m: float, lane_m: float) -> void:
	var right_m := -(lane_m / 2.0 + FENCE_OUT_M + 9.0)
	for x_m: float in [15.0, 45.0, 75.0]:
		_place(KenneyModels.TENTS, TrackSpace.point(x_m, right_m - 4.0, 0.0), 1.0, PI / 2.0)
	_place(KenneyModels.BARN, TrackSpace.point(length_m + 55.0, 18.0, 0.0), 14.0, 0.3)
	for x_m: float in [-60.0, 160.0]:
		for y_m: float in [-55.0, -35.0, 35.0, 55.0]:
			_place(KenneyModels.FOREST, TrackSpace.point(x_m + _random.randf_range(-6.0, 6.0),
					y_m, 0.0), 1.6, _random.randf_range(0.0, TAU))
	for i in 18:
		var x_m := _random.randf_range(-40.0, length_m + 60.0)
		var left := i % 2 == 0
		var y_m := _random.randf_range(32.0, 70.0) * (1.0 if left else -1.0)
		_place(KenneyModels.TALL_TREES if i % 3 == 0 else KenneyModels.TREES,
				TrackSpace.point(x_m, y_m, 0.0), 10.0, _random.randf_range(0.0, TAU))
	for i in 7:
		var cloud := KenneyModels.scene(KenneyModels.CLOUD)
		cloud.scale = Vector3(30.0, 8.0, 18.0) * _random.randf_range(0.7, 1.3)
		cloud.position = TrackSpace.point(_random.randf_range(-80.0, 250.0),
				_random.randf_range(-160.0, 160.0), _random.randf_range(60.0, 90.0))
		_quiet_all(cloud)
		add_child(cloud)


func _place(path: String, at: Vector3, scale_by: float, yaw: float) -> void:
	var model := KenneyModels.scene(path)
	model.scale = Vector3.ONE * scale_by
	var tile_m := 10.0 if path in [KenneyModels.FOREST, KenneyModels.TENTS] else 1.0
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
