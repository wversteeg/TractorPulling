# Draws the game's icons from the menus' backdrop (MenuBackdrop): close in
# on the tractor's wheelie, the sled's front on the chain behind it and the
# dirt in the air. Writes res://icons/; run it again after changing the
# drawing, under Xvfb if there is no display:
#
#   godot --rendering-method gl_compatibility --path app/godot \
#       --script res://tools/make_icons.gd
#
# icon.png is the project's icon (the Windows game's too); Android takes the
# legacy 192 px icon and the adaptive icon's two 432 px layers, the
# background (sky and ground) and the foreground (the tractor and sled),
# whose middle two thirds every launcher's mask keeps.
extends SceneTree

# Drawn this large, then scaled down: smooth edges at every size [px].
const DRAW_SIZE := 1024
# The world in the icon: from the sled's weight box to the tractor's nose,
# the ground low down [m].
const ICON_VIEW_M := Rect2(-3.4, -0.9, 7.1, 7.1)
# The adaptive icon's layers show the middle 66 % in the launcher's mask:
# the same picture with a margin all round.
const ADAPTIVE_SHARE := 0.66
# The moment the dirt and smoke are caught at [s].
const MOMENT_S := 0.37

const JOBS := [
	["res://icons/icon.png", 512, false, MenuBackdrop.Layer.BACK | MenuBackdrop.Layer.FRONT],
	["res://icons/android_192.png", 192, false, MenuBackdrop.Layer.BACK | MenuBackdrop.Layer.FRONT],
	["res://icons/android_background_432.png", 432, true, MenuBackdrop.Layer.BACK],
	["res://icons/android_foreground_432.png", 432, true, MenuBackdrop.Layer.FRONT],
]

var _viewport := SubViewport.new()
var _backdrop := MenuBackdrop.new()
var _job := 0
var _frames := 0


func _initialize() -> void:
	_viewport.size = Vector2i(DRAW_SIZE, DRAW_SIZE)
	_viewport.transparent_bg = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_backdrop.animated = false
	_backdrop.time_s = MOMENT_S
	_viewport.add_child(_backdrop)
	root.add_child(_viewport)
	_set_up(JOBS[0])


func _set_up(job: Array) -> void:
	var view := ICON_VIEW_M
	if job[2]:
		view = view.grow(view.size.x * (1.0 / ADAPTIVE_SHARE - 1.0) / 2.0)
	_backdrop.view_m = view
	_backdrop.layers = job[3]
	_backdrop.size = Vector2(DRAW_SIZE, DRAW_SIZE)
	_backdrop.queue_redraw()
	_frames = 0


func _process(_delta: float) -> bool:
	_frames += 1
	if _frames < 3:
		return false
	var job: Array = JOBS[_job]
	var image := _viewport.get_texture().get_image()
	image.resize(job[1], job[1], Image.INTERPOLATE_LANCZOS)
	var error := image.save_png(ProjectSettings.globalize_path(job[0]))
	print("%s: %s" % [job[0], error_string(error)])
	_job += 1
	if _job >= JOBS.size():
		quit(0)
		return true
	_set_up(JOBS[_job])
	return false
