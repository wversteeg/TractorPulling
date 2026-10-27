# Shapes and materials for the scene's models, all built in code: the
# tractor and the sled from the run's sizes, the venue around the track.
# Materials are shared by colour, so parts of one colour batch together;
# repeated parts (tire lugs, weights, the crowd) go into MultiMeshes, one
# draw call each, which keeps a phone's frame cheap.
class_name Art

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
# transform) tint each copy through the material's vertex colour.
static func many(mesh: Mesh, transforms: Array[Transform3D], surface: Material,
		colours: Array[Color] = []) -> MultiMeshInstance3D:
	var multi := MultiMesh.new()
	multi.transform_format = MultiMesh.TRANSFORM_3D
	multi.use_colors = not colours.is_empty()
	multi.mesh = mesh
	multi.instance_count = transforms.size()
	for i in transforms.size():
		multi.set_instance_transform(i, transforms[i])
		if multi.use_colors:
			multi.set_instance_color(i, colours[i])
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
