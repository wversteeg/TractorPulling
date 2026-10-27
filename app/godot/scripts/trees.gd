# Trees, built in code in one mesh each with their colours in the vertices
# (trunk and foliage), so a copse or a whole tree line is one MultiMesh,
# each copy tinted a little by its instance colour. Feet at the origin.
class_name Trees

const BARK := Color(0.33, 0.24, 0.16)
const PINE := Color(0.13, 0.3, 0.14)
const LEAVES := Color(0.24, 0.42, 0.16)

static var _meshes := {}
static var _material: StandardMaterial3D


# A spruce about 10 m tall: a trunk under three cones, darker below.
static func pine() -> ArrayMesh:
	if _meshes.has("pine"):
		return _meshes["pine"]
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	Art.append_coloured(tool, Art.cylinder_mesh(0.2, 0.28, 2.4, 6), _up(1.2), BARK)
	var tiers := [[2.4, 3.6, 3.3], [1.85, 3.2, 5.4], [1.2, 2.8, 7.3]]
	for i in tiers.size():
		var t: Array = tiers[i]
		Art.append_coloured(tool, Art.cylinder_mesh(0.0, t[0], t[1], 8), _up(t[2]),
				PINE.lightened(0.06 * i))
	tool.index()
	_meshes["pine"] = tool.commit()
	return _meshes["pine"]


# A broadleaf about 9 m tall: a trunk and a crown of three rounded clumps.
static func broadleaf() -> ArrayMesh:
	if _meshes.has("broadleaf"):
		return _meshes["broadleaf"]
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	Art.append_coloured(tool, Art.cylinder_mesh(0.25, 0.35, 4.0, 6), _up(2.0), BARK)
	var clumps := [[2.8, Vector3(0.0, 6.0, 0.0)], [2.1, Vector3(1.4, 5.0, 0.6)],
			[2.2, Vector3(-1.2, 5.2, -0.7)]]
	for i in clumps.size():
		var c: Array = clumps[i]
		var crown := SphereMesh.new()
		crown.radius = c[0]
		crown.height = 1.7 * float(c[0])
		crown.radial_segments = 8
		crown.rings = 4
		Art.append_coloured(tool, crown, Transform3D(Basis(), c[1]), LEAVES.darkened(0.08 * i))
	tool.index()
	_meshes["broadleaf"] = tool.commit()
	return _meshes["broadleaf"]


# The one material: the vertex colour times each copy's tint.
static func material() -> StandardMaterial3D:
	if _material == null:
		_material = StandardMaterial3D.new()
		_material.vertex_color_use_as_albedo = true
		_material.roughness = 0.95
	return _material


# Copies of `mesh` at `places` (feet), each turned, sized and tinted a
# little differently by `random`.
static func many(mesh: Mesh, places: Array[Vector3], random: RandomNumberGenerator,
		smallest: float = 0.8, largest: float = 1.25) -> MultiMeshInstance3D:
	var transforms: Array[Transform3D] = []
	var tints: Array[Color] = []
	for at in places:
		var basis := Basis(Vector3.UP, random.randf_range(0.0, TAU)).scaled(
				Vector3.ONE * random.randf_range(smallest, largest))
		transforms.append(Transform3D(basis, at))
		tints.append(Color.WHITE.darkened(random.randf_range(0.0, 0.25)).lerp(
				Color(0.9, 1.0, 0.75), random.randf_range(0.0, 0.3)))
	var trees := Art.many(mesh, transforms, material(), tints)
	trees.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return trees


static func _up(y_m: float) -> Transform3D:
	return Transform3D(Basis(), Vector3(0.0, y_m, 0.0))
