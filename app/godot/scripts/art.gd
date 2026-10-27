# Shapes and materials for the scene's models, all built in code: the
# tractor and the sled from the run's sizes, the venue around the track.
# Materials are shared by colour, so parts of one colour batch together;
# repeated parts (tire lugs, weights, the crowd) go into MultiMeshes, one
# draw call each, which keeps a phone's frame cheap.
class_name Art

# How far a rim's face stands out past its tire's sidewall [m].
const RIM_PROUD_M := 0.012

# Shared materials by colour, roughness and metalness.
static var _materials := {}


static func material(colour: Color, roughness: float = 0.8, metallic: float = 0.0,
		vertex_colours: bool = false) -> StandardMaterial3D:
	var key := "%s/%.2f/%.2f/%s" % [colour.to_html(), roughness, metallic, vertex_colours]
	if _materials.has(key):
		return _materials[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = colour
	m.roughness = roughness
	m.metallic = metallic
	m.vertex_color_use_as_albedo = vertex_colours
	_materials[key] = m
	return m


static func box(size: Vector3, colour: Color, roughness: float = 0.8,
		metallic: float = 0.0) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	return instance(mesh, material(colour, roughness, metallic))


# A cylinder standing along y (Godot's own axis).
static func cylinder(radius_m: float, height_m: float, colour: Color, segments: int = 16,
		roughness: float = 0.8, metallic: float = 0.0) -> MeshInstance3D:
	return instance(cylinder_mesh(radius_m, radius_m, height_m, segments),
			material(colour, roughness, metallic))


static func cylinder_mesh(top_m: float, bottom_m: float, height_m: float,
		segments: int = 16) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top_m
	mesh.bottom_radius = bottom_m
	mesh.height = height_m
	mesh.radial_segments = segments
	mesh.rings = 1
	return mesh


# A cylinder lying across the track, along x: a wheel, an axle.
static func axle_cylinder(radius_m: float, width_m: float, colour: Color,
		segments: int = 24, roughness: float = 0.8, metallic: float = 0.0) -> MeshInstance3D:
	var part := cylinder(radius_m, width_m, colour, segments, roughness, metallic)
	part.rotation.z = PI / 2.0
	return part


static func sphere(radius_m: float, colour: Color, roughness: float = 0.6) -> MeshInstance3D:
	var mesh := SphereMesh.new()
	mesh.radius = radius_m
	mesh.height = 2.0 * radius_m
	mesh.radial_segments = 12
	mesh.rings = 6
	return instance(mesh, material(colour, roughness))


static func instance(mesh: Mesh, surface: Material) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	part.mesh = mesh
	part.material_override = surface
	return part


# `mesh` once per transform, in one draw call; `colours` (optional, one per
# transform) tint each copy through the material's vertex colour, and
# `custom` (optional) hands each copy's shader four numbers of its own.
static func many(mesh: Mesh, transforms: Array[Transform3D], surface: Material,
		colours: Array[Color] = [], custom: Array[Color] = []) -> MultiMeshInstance3D:
	var multi := MultiMesh.new()
	multi.transform_format = MultiMesh.TRANSFORM_3D
	multi.use_colors = not colours.is_empty()
	multi.use_custom_data = not custom.is_empty()
	multi.mesh = mesh
	multi.instance_count = transforms.size()
	for i in transforms.size():
		multi.set_instance_transform(i, transforms[i])
		if multi.use_colors:
			multi.set_instance_color(i, colours[i])
		if multi.use_custom_data:
			multi.set_instance_custom_data(i, custom[i])
	var node := MultiMeshInstance3D.new()
	node.multimesh = multi
	node.material_override = surface
	return node


# A text in the world, facing +z (towards the start line in TrackSpace).
static func label(text: String, size_px: int, colour: Color,
		outline: Color = Color.BLACK) -> Label3D:
	var l := Label3D.new()
	l.text = text
	l.font_size = size_px
	l.pixel_size = 0.01
	l.modulate = colour
	l.outline_modulate = outline
	l.outline_size = size_px / 6
	l.shaded = false
	# Read from the front only: from behind a text would read mirrored.
	l.double_sided = false
	return l


# A tiling noise texture for a ground's colour: `dark` to `light`.
static func ground_texture(dark: Color, light: Color, frequency: float,
		seed_value: int) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = frequency
	noise.fractal_octaves = 4
	var texture := NoiseTexture2D.new()
	texture.width = 256
	texture.height = 256
	texture.seamless = true
	texture.noise = noise
	var ramp := Gradient.new()
	ramp.set_color(0, dark)
	ramp.set_color(1, light)
	texture.color_ramp = ramp
	return texture


# The same noise as a normal map, so a ground catches the low sun: clods
# and ruts in the clay, tufts in the grass. `strength` is how deep.
static func ground_normal(frequency: float, seed_value: int, strength: float) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = frequency
	noise.fractal_octaves = 5
	var texture := NoiseTexture2D.new()
	texture.width = 256
	texture.height = 256
	texture.seamless = true
	texture.as_normal_map = true
	texture.bump_strength = strength
	texture.noise = noise
	return texture


# Car paint: glossy, with a clear coat that keeps a sharp highlight on top
# of the colour.
static func paint(colour: Color, roughness: float = 0.4) -> StandardMaterial3D:
	var key := "paint/%s/%.2f" % [colour.to_html(), roughness]
	if _materials.has(key):
		return _materials[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = colour
	m.roughness = roughness
	m.metallic = 0.15
	m.clearcoat_enabled = true
	m.clearcoat = 0.8
	m.clearcoat_roughness = 0.15
	_materials[key] = m
	return m


# A soft round puff that always faces the camera, for smoke, dust and
# thrown dirt: a radial fade on a quad, tinted by each particle's colour.
# Far cheaper than a lit sphere, and soft at its edge.
static func puff_mesh(size_m: float) -> QuadMesh:
	var mesh := QuadMesh.new()
	mesh.size = Vector2(size_m, size_m)
	mesh.material = _puff_material()
	return mesh


static func _puff_material() -> StandardMaterial3D:
	if _materials.has("puff"):
		return _materials["puff"]
	var fade := Gradient.new()
	fade.set_color(0, Color(1.0, 1.0, 1.0, 1.0))
	fade.add_point(0.45, Color(1.0, 1.0, 1.0, 0.55))
	fade.set_color(fade.get_point_count() - 1, Color(1.0, 1.0, 1.0, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = fade
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	texture.width = 64
	texture.height = 64
	var m := StandardMaterial3D.new()
	m.albedo_texture = texture
	m.vertex_color_use_as_albedo = true
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.billboard_keep_scale = true
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	_materials["puff"] = m
	return m


# A torus about the wheel's axle (across the track), for tire shoulders,
# ribs and rims.
static func torus_across(radius_m: float, thickness_m: float) -> Mesh:
	var mesh := TorusMesh.new()
	mesh.inner_radius = radius_m - thickness_m
	mesh.outer_radius = radius_m + thickness_m
	mesh.ring_segments = 6
	mesh.rings = 36
	# TorusMesh lies in Godot's xz plane, around y; the axle is TrackSpace's
	# y, Godot's x: turned a quarter about z.
	var tool := SurfaceTool.new()
	tool.append_from(mesh, 0, Transform3D(Basis(Vector3.BACK, PI / 2.0), Vector3.ZERO))
	return tool.commit()


# A tire's carcass: the tread as a cylinder, the shoulders rounded by a
# torus at each edge, and a lighter band on the sidewall where the
# lettering sits.
static func tire(spin: Node3D, radius_m: float, width_m: float, colour: Color) -> void:
	var shoulder_m := minf(0.1, width_m * 0.22)
	spin.add_child(axle_cylinder(radius_m, width_m - shoulder_m, colour, 36, 0.9))
	for edge: float in [-1.0, 1.0]:
		var shoulder := instance(torus_across(radius_m - shoulder_m * 0.5, shoulder_m * 0.5),
				material(colour, 0.9))
		shoulder.position = TrackSpace.point(0.0, edge * (width_m - shoulder_m) / 2.0, 0.0)
		spin.add_child(shoulder)
		var wall := axle_cylinder(radius_m - shoulder_m * 0.4, 0.01, colour, 36, 0.9)
		wall.position = TrackSpace.point(0.0, edge * (width_m / 2.0 - 0.005), 0.0)
		spin.add_child(wall)
		var band := instance(torus_across(radius_m * 0.86, 0.012),
				material(colour.lightened(0.18), 0.7))
		band.position = TrackSpace.point(0.0, edge * width_m / 2.0, 0.0)
		spin.add_child(band)


# A steel rim seen from outside: the dished face a shade darker, its lip,
# the centre disc, the bolt circle and the hub cap. All of it stands just
# outboard of the tire's sidewall (Art.tire), which would hide it.
static func rim(spin: Node3D, radius_m: float, width_m: float, outward: float, bolts: int,
		colour: Color) -> void:
	var face_m := outward * (width_m / 2.0 + RIM_PROUD_M)
	var dish := axle_cylinder(radius_m, 0.012, colour.darkened(0.15), 28)
	dish.material_override = paint(colour.darkened(0.15), 0.45)
	dish.position = TrackSpace.point(0.0, face_m, 0.0)
	spin.add_child(dish)
	var lip := instance(torus_across(radius_m, 0.03), paint(colour, 0.35))
	lip.position = TrackSpace.point(0.0, face_m, 0.0)
	spin.add_child(lip)
	var centre := axle_cylinder(radius_m * 0.45, 0.03, colour, 20)
	centre.material_override = paint(colour, 0.35)
	centre.position = TrackSpace.point(0.0, face_m + outward * 0.015, 0.0)
	spin.add_child(centre)
	var bolt_mesh := cylinder_mesh(0.018, 0.018, 0.03, 6)
	var at: Array[Transform3D] = []
	for i in bolts:
		var angle := TAU * float(i) / bolts
		var spot := TrackSpace.point(radius_m * 0.32 * cos(angle), face_m + outward * 0.035,
				radius_m * 0.32 * sin(angle))
		at.append(Transform3D(Basis(Vector3.FORWARD, PI / 2.0), spot))
	spin.add_child(many(bolt_mesh, at, material(Color(0.22, 0.22, 0.24), 0.35, 0.7)))
	var cap := axle_cylinder(radius_m * 0.14, 0.06, Color(0.8, 0.8, 0.82), 14, 0.2, 0.9)
	cap.position = TrackSpace.point(0.0, face_m + outward * 0.04, 0.0)
	spin.add_child(cap)


# `mesh`'s triangles into `tool`, moved by `at`, in one vertex colour: for
# models built from primitives in one mesh (people, trees).
static func append_coloured(tool: SurfaceTool, mesh: PrimitiveMesh, at: Transform3D,
		colour: Color) -> void:
	var arrays := mesh.get_mesh_arrays()
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	var turn := at.basis.inverse().transposed()
	for i in indices:
		tool.set_color(colour)
		tool.set_normal((turn * normals[i]).normalized())
		tool.add_vertex(at * vertices[i])
