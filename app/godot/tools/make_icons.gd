# Draws the game's icons from the menus' backdrop (MenuBackdrop): close in
# on the tractor's wheelie, the sled's front on the chain behind it and the
# dirt in the air, seen a little from the front: the flat picture is laid
# on a card turned in 3D, so the tractor comes towards the viewer and the
# sled falls away. Writes res://icons/; run it again after changing the
# drawing, under Xvfb if there is no display:
#
#   godot --rendering-method gl_compatibility --path app/godot \
#       --script res://tools/make_icons.gd
#
# icon.png is the project's icon; Android takes the legacy 192 px icon and
# the adaptive icon's two 432 px layers, the background (sky and ground)
# and the foreground (the tractor and sled), whose middle two thirds every
# launcher's mask keeps.
extends SceneTree

# Rendered this large, then scaled down: smooth edges at every size [px].
const DRAW_SIZE := 1024
# The flat picture on the card, larger: the turned card still fills the
# icon to its corners [px].
const FLAT_SIZE := 4096
# The world in the icon: from the sled's weight box to the tractor's nose,
# the ground low down [m].
const ICON_VIEW_M := Rect2(-3.4, -0.9, 7.1, 7.1)
# The card shows this much more of the world than the icon, all round.
const CARD_SHARE := 3.6
# The adaptive icon's layers show the middle 66 % in the launcher's mask:
# the same picture with a margin all round.
const ADAPTIVE_SHARE := 0.66
# How the card is turned: about the vertical, the tractor's nose towards
# the viewer, and a little about the view, the nose up.
const YAW_DEG := -45.0
const ROLL_DEG := 6.0
const FOV_DEG := 50.0
# The moment the dirt and smoke are caught at [s].
const MOMENT_S := 0.37

const JOBS := [
	["res://icons/icon.png", 512, false, MenuBackdrop.Layer.BACK | MenuBackdrop.Layer.FRONT],
	["res://icons/android_192.png", 192, false, MenuBackdrop.Layer.BACK | MenuBackdrop.Layer.FRONT],
	["res://icons/android_background_432.png", 432, true, MenuBackdrop.Layer.BACK],
	["res://icons/android_foreground_432.png", 432, true, MenuBackdrop.Layer.FRONT],
]

var _flat := SubViewport.new()
var _backdrop := MenuBackdrop.new()
var _view := SubViewport.new()
var _camera := Camera3D.new()
var _quad := QuadMesh.new()
var _job := 0
var _frames := 0


func _initialize() -> void:
	_flat.size = Vector2i(FLAT_SIZE, FLAT_SIZE)
	_flat.transparent_bg = true
	_flat.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_backdrop.animated = false
	_backdrop.time_s = MOMENT_S
	_flat.add_child(_backdrop)
	root.add_child(_flat)

	# The card: 2 units across for the icon's view (_set_up() sizes it).
	_view.size = Vector2i(DRAW_SIZE, DRAW_SIZE)
	_view.transparent_bg = true
	_view.own_world_3d = true
	_view.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	var card := MeshInstance3D.new()
	card.mesh = _quad
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_texture = _flat.get_texture()
	material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	card.material_override = material
	card.rotation = Vector3(0.0, deg_to_rad(YAW_DEG), deg_to_rad(ROLL_DEG))
	_view.add_child(card)
	_camera.fov = FOV_DEG
	_view.add_child(_camera)
	root.add_child(_view)
	_set_up(JOBS[0])


func _set_up(job: Array) -> void:
	# An adaptive layer: the camera stands back for the mask's margin, and
	# the card grows to fill the frame still.
	var back := 1.0 / ADAPTIVE_SHARE if job[2] else 1.0
	var share := CARD_SHARE * back
	_quad.size = Vector2(2.0, 2.0) * share
	_backdrop.view_m = ICON_VIEW_M.grow(ICON_VIEW_M.size.x * (share - 1.0) / 2.0)
	# Far enough that the unturned card's icon view just fills the frame.
	_camera.position = Vector3(0.0, 0.0, back / tan(deg_to_rad(FOV_DEG) / 2.0))
	_backdrop.layers = job[3]
	_backdrop.size = Vector2(FLAT_SIZE, FLAT_SIZE)
	_backdrop.queue_redraw()
	_frames = 0


func _process(_delta: float) -> bool:
	_frames += 1
	if _frames < 4:
		return false
	var job: Array = JOBS[_job]
	var image := _view.get_texture().get_image()
	image.resize(job[1], job[1], Image.INTERPOLATE_LANCZOS)
	var error := image.save_png(ProjectSettings.globalize_path(job[0]))
	print("%s: %s" % [job[0], error_string(error)])
	_job += 1
	if _job >= JOBS.size():
		quit(0)
		return true
	_set_up(JOBS[_job])
	return false
