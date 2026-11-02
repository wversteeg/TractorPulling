# The CC0 models from Kenney's kits (assets/kenney, see its README), placed
# as scenes: the tents, the barn and the clouds.
class_name KenneyModels

const CLOUD := "res://assets/kenney/platformer/cloud.glb"
const TENTS := "res://assets/kenney/racing/decoration-tents.glb"
const BARN := "res://assets/kenney/city/building-garage.glb"


static func scene(path: String) -> Node3D:
	var packed := load(path) as PackedScene
	return packed.instantiate() as Node3D
