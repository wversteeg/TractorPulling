# The CC0 models from Kenney's kits (assets/kenney, see its README): placed
# as scenes, or merged into one mesh to draw many copies of in a MultiMesh
# (the crowd), tinted per copy.
class_name KenneyModels

const CHARACTER := "res://assets/kenney/platformer/character.glb"
const FLAG := "res://assets/kenney/platformer/flag.glb"
const CLOUD := "res://assets/kenney/platformer/cloud.glb"
const FOREST := "res://assets/kenney/racing/decoration-forest.glb"
const TENTS := "res://assets/kenney/racing/decoration-tents.glb"
const BARN := "res://assets/kenney/city/building-garage.glb"
const TREES := "res://assets/kenney/city/grass-trees.glb"
const TALL_TREES := "res://assets/kenney/city/grass-trees-tall.glb"


static func scene(path: String) -> Node3D:
	var packed := load(path) as PackedScene
	return packed.instantiate() as Node3D


# Every mesh of the model at `path` in one mesh, in the model's own frame,
# leaving out the parts named in `skip` (the character's antenna); its
# material, set to take each copy's tint (MultiMesh colours multiply the
# vertex colour), comes back through `out_material`'s first element.
static func merged(path: String, skip: PackedStringArray,
		out_material: Array[Material]) -> ArrayMesh:
	var root := scene(path)
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	var found: Material = null
	for node in root.find_children("*", "MeshInstance3D", true, false):
		var part := node as MeshInstance3D
		if skip.has(String(part.name)) or part.mesh == null:
			continue
		var to_root := Transform3D.IDENTITY
		var walk: Node = part
		while walk != root and walk is Node3D:
			to_root = (walk as Node3D).transform * to_root
			walk = walk.get_parent()
		for surface in part.mesh.get_surface_count():
			tool.append_from(part.mesh, surface, to_root)
			if found == null:
				found = part.get_active_material(surface)
	root.free()
	var tinted: StandardMaterial3D
	if found is StandardMaterial3D:
		tinted = (found as StandardMaterial3D).duplicate() as StandardMaterial3D
	else:
		tinted = StandardMaterial3D.new()
	tinted.vertex_color_use_as_albedo = true
	out_material.append(tinted)
	return tool.commit()
