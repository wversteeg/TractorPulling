# What a pull leaves in the clay: the rear tires' ruts, printed with their
# lugs, and behind the sled the wide band its pan smooths over them. Each
# mark is a strip of quads along the points the snapshot passes, a new
# point every STEP_M; the strip is rebuilt only when a point is added.
# Cleared for every pull (the track is prepared again between pulls).
class_name TrackMarks
extends Node3D

const RUT := Color(0.3, 0.2, 0.12)
const SMOOTHED := Color(0.56, 0.41, 0.27)
# A new point every this far [m], and at most this many a mark (a pull
# past 130 m stops adding: the track ends there).
const STEP_M := 0.2
const MAX_POINTS := 700
# Above the clay strip (which lies 3 cm under the track's zero, so the
# chalk lines show), the smoothed band over the ruts [m].
const CLAY_TOP_M := -0.03
const RUT_LIFT_M := 0.004
const BAND_LIFT_M := 0.008
# A lug print every this far along a rut [m], and the band's streaks.
const LUG_PITCH_M := 0.3
const BAND_REPEAT_M := 2.0


class Mark:
	var width_m := 0.5
	var repeat_m := 1.0
	var lift_m := 0.0
	var points := PackedVector2Array()
	var lefts := PackedVector2Array()
	var mesh := ImmediateMesh.new()

	func clear() -> void:
		points.clear()
		lefts.clear()
		mesh.clear_surfaces()

	# Adds the point if it is STEP_M on from the last; true if added.
	func add(at: Vector2, left: Vector2) -> bool:
		if points.size() >= MAX_POINTS:
			return false
		if not points.is_empty() and points[points.size() - 1].distance_to(at) < STEP_M:
			return false
		points.append(at)
		lefts.append(left)
		return true

	func rebuild() -> void:
		mesh.clear_surfaces()
		if points.size() < 2:
			return
		mesh.surface_begin(Mesh.PRIMITIVE_TRIANGLE_STRIP)
		var along_m := 0.0
		for i in points.size():
			if i > 0:
				along_m += points[i].distance_to(points[i - 1])
			var half := lefts[i] * width_m / 2.0
			# Right edge first: the strip's triangles then face up.
			for edge: float in [-1.0, 1.0]:
				var p := points[i] + half * edge
				mesh.surface_set_normal(Vector3.UP)
				mesh.surface_set_uv(Vector2(0.5 - edge * 0.5, along_m / repeat_m))
				mesh.surface_add_vertex(TrackSpace.point(p.x, p.y, CLAY_TOP_M + lift_m))
		mesh.surface_end()


var _left_rut := Mark.new()
var _right_rut := Mark.new()
var _band := Mark.new()
var _rear_track_m := 1.6
var _pan_ahead_m := 1.0
var _built := false


func build(d: Dictionary) -> void:
	_rear_track_m = d["rear_track_m"]
	_pan_ahead_m = d["pan_ahead_of_wheels_m"]
	for rut: Mark in [_left_rut, _right_rut]:
		rut.width_m = d["rear_tire_width_m"]
		rut.repeat_m = LUG_PITCH_M
		rut.lift_m = RUT_LIFT_M
	_band.width_m = SledView.WIDTH_M
	_band.repeat_m = BAND_REPEAT_M
	_band.lift_m = BAND_LIFT_M
	if not _built:
		_built = true
		var rut_material := _material(_rut_texture(), RUT, 0)
		for rut: Mark in [_left_rut, _right_rut]:
			add_child(_strip(rut, rut_material))
		add_child(_strip(_band, _material(_band_texture(), SMOOTHED, 1)))
	for mark: Mark in [_left_rut, _right_rut, _band]:
		mark.clear()


func show_snapshot(s: Dictionary) -> void:
	var tractor := Vector2(s["tractor_x_m"], s["tractor_y_m"])
	var heading: float = s["tractor_heading_rad"]
	# Left of the direction of travel (x forward, y left).
	var left := Vector2(-sin(heading), cos(heading))
	var half_track := left * _rear_track_m / 2.0
	for pair: Array in [[_left_rut, tractor + half_track], [_right_rut, tractor - half_track]]:
		var rut: Mark = pair[0]
		if rut.add(pair[1], left):
			rut.rebuild()
	var sled_heading: float = s["sled_heading_rad"]
	var forward := Vector2(cos(sled_heading), sin(sled_heading))
	var pan := Vector2(s["sled_x_m"], s["sled_y_m"]) + forward * _pan_ahead_m
	# The pan smooths what it slides over, from its back edge.
	if _band.add(pan - forward * SledView.PAN_DEPTH_M / 2.0,
			Vector2(-sin(sled_heading), cos(sled_heading))):
		_band.rebuild()


# How many points each mark has: the left rut, the right rut, the band.
func point_counts() -> Vector3i:
	return Vector3i(_left_rut.points.size(), _right_rut.points.size(), _band.points.size())


static func _strip(mark: Mark, surface: Material) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.mesh = mark.mesh
	node.material_override = surface
	node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return node


static func _material(texture: Texture2D, colour: Color, priority: int) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_texture = texture
	m.albedo_color = colour
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.roughness = 0.9
	m.render_priority = priority
	return m


# A rut: soft at its edges, darker lug prints in a chevron, pointing the
# way the tire turned.
static func _rut_texture() -> ImageTexture:
	var size := Vector2i(32, 32)
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
	for y in size.y:
		for x in size.x:
			var u := (float(x) + 0.5) / size.x
			var v := (float(y) + 0.5) / size.y
			var edge := clampf(minf(u, 1.0 - u) / 0.18, 0.0, 1.0)
			var chevron := fposmod(v + absf(u - 0.5) * 0.7, 1.0)
			var lug := 0.62 if chevron < 0.38 else 1.0
			image.set_pixel(x, y, Color(lug, lug, lug, 0.85 * edge))
	return ImageTexture.create_from_image(image)


# The smoothed band: even, with faint streaks along it, soft at its edges.
static func _band_texture() -> ImageTexture:
	var size := Vector2i(64, 16)
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
	for y in size.y:
		for x in size.x:
			var u := (float(x) + 0.5) / size.x
			var edge := clampf(minf(u, 1.0 - u) / 0.06, 0.0, 1.0)
			var streak := 0.93 + 0.07 * sin(u * 61.0) * sin(u * 23.0 + 1.0)
			image.set_pixel(x, y, Color(streak, streak, streak, 0.9 * edge))
	return ImageTexture.create_from_image(image)
