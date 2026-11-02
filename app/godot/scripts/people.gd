# People, built in code from rounded parts: legs, a body, arms, a head and
# hair, standing or seated, facing +z (as Kenney's models do) with their
# feet at the origin. Low-poly, so a crowd of hundreds in one MultiMesh
# stays cheap. The vertex colour carries what each part is: its alpha 0 is
# the shirt (tinted per person), 0.5 skin (shaded per person), 1 a fixed
# colour (jeans, shoes, hair). person.gdshader and crowd.gdshader read it.
class_name People

const JEANS := Color(0.17, 0.22, 0.34)
const SHOES := Color(0.12, 0.1, 0.09)
const HAIR := Color(0.2, 0.14, 0.09)
const SHIRT_PART := Color(1.0, 1.0, 1.0, 0.0)
const SKIN_PART := Color(1.0, 1.0, 1.0, 0.5)
const PERSON_SHADER := preload("res://shaders/person.gdshader")

static var _meshes := {}


static func standing() -> ArrayMesh:
	if _meshes.has("standing"):
		return _meshes["standing"]
	var tool := _begin()
	for side: float in [-1.0, 1.0]:
		Art.append_coloured(tool, _box(Vector3(0.11, 0.07, 0.26)), _at(side * 0.1, 0.035, 0.04), Color(SHOES, 1.0))
		Art.append_coloured(tool, _capsule(0.075, 0.86), _at(side * 0.1, 0.49, 0.0), Color(JEANS, 1.0))
		# Arms hang a little out from the body: a sleeve, then the forearm.
		Art.append_coloured(tool, _capsule(0.055, 0.34), _at(side * 0.27, 1.27, 0.0, 0.0, 0.0, side * 0.12),
				SHIRT_PART)
		Art.append_coloured(tool, _capsule(0.048, 0.34), _at(side * 0.31, 0.98, 0.03, -0.15, 0.0, side * 0.08),
				SKIN_PART)
	_add_upper_body(tool, 0.0)
	tool.index()
	_meshes["standing"] = tool.commit()
	return _meshes["standing"]


# Sitting on a bench or a seat 0.45 m up, thighs forward, hands on the
# knees; the feet are at the origin, the hips 0.15 m behind them.
static func seated() -> ArrayMesh:
	if _meshes.has("seated"):
		return _meshes["seated"]
	var tool := _begin()
	for side: float in [-1.0, 1.0]:
		Art.append_coloured(tool, _box(Vector3(0.11, 0.07, 0.26)), _at(side * 0.11, 0.035, 0.08), Color(SHOES, 1.0))
		Art.append_coloured(tool, _capsule(0.075, 0.48), _at(side * 0.11, 0.24, 0.0), Color(JEANS, 1.0))
		Art.append_coloured(tool, _capsule(0.08, 0.5), _at(side * 0.11, 0.47, -0.17, PI / 2.0), Color(JEANS, 1.0))
		Art.append_coloured(tool, _capsule(0.055, 0.34), _at(side * 0.27, 0.86, -0.32, 0.25, 0.0, side * 0.1),
				SHIRT_PART)
		Art.append_coloured(tool, _capsule(0.048, 0.34), _at(side * 0.24, 0.62, -0.12, 1.2, 0.0, 0.0), SKIN_PART)
	_add_upper_body(tool, -0.42)
	tool.index()
	_meshes["seated"] = tool.commit()
	return _meshes["seated"]


# A material for one person on their own (the driver, the sled's
# operator), in this shirt.
static func material(shirt: Color, skin: float = 0.3) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = PERSON_SHADER
	m.set_shader_parameter("shirt", shirt)
	m.set_shader_parameter("skin", skin)
	return m


static func one(seated_person: bool, shirt: Color) -> MeshInstance3D:
	var person := MeshInstance3D.new()
	person.mesh = seated() if seated_person else standing()
	person.material_override = material(shirt)
	return person


# The body, neck, head and hair, lowered by `drop_m` for the seated pose
# and leaning back a little there.
static func _add_upper_body(tool: SurfaceTool, drop_m: float) -> void:
	var back_z := -0.3 if drop_m < 0.0 else 0.0
	var body := _capsule(0.2, 0.66)
	Art.append_coloured(tool, body, Transform3D(Basis().scaled(Vector3(1.0, 1.0, 0.62)),
			Vector3(0.0, 1.2 + drop_m, back_z)), SHIRT_PART)
	Art.append_coloured(tool, _capsule(0.05, 0.14), _at(0.0, 1.55 + drop_m, back_z), SKIN_PART)
	Art.append_coloured(tool, _sphere(0.115), _at(0.0, 1.66 + drop_m, back_z + 0.01), SKIN_PART)
	Art.append_coloured(tool, _sphere(0.12), Transform3D(Basis().scaled(Vector3(1.0, 0.62, 1.0)),
			Vector3(0.0, 1.71 + drop_m, back_z - 0.01)), Color(HAIR, 1.0))


static func _begin() -> SurfaceTool:
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	return tool


# Placed at (x, y, z), turned about x, y and z in that order [rad].
static func _at(x: float, y: float, z: float, about_x: float = 0.0, about_y: float = 0.0,
		about_z: float = 0.0) -> Transform3D:
	return Transform3D(Basis.from_euler(Vector3(about_x, about_y, about_z)), Vector3(x, y, z))


static func _capsule(radius_m: float, height_m: float) -> CapsuleMesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius_m
	mesh.height = height_m
	mesh.radial_segments = 6
	mesh.rings = 1
	return mesh


static func _sphere(radius_m: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius_m
	mesh.height = 2.0 * radius_m
	mesh.radial_segments = 8
	mesh.rings = 4
	return mesh


static func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh
