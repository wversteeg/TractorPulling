# Plain shapes for the graybox scene: a box or a wheel with a flat colour.
# The real models replace them later; sizes come from the run's dimensions.
class_name Graybox


static func box(size: Vector3, colour: Color) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	return _instance(mesh, colour)


# A wheel with its axle along x, as Godot's cylinder stands along y.
static func wheel(radius_m: float, width_m: float, colour: Color) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius_m
	mesh.bottom_radius = radius_m
	mesh.height = width_m
	mesh.radial_segments = 24
	var instance := _instance(mesh, colour)
	instance.rotation.z = PI / 2.0
	return instance


static func _instance(mesh: Mesh, colour: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	return instance
