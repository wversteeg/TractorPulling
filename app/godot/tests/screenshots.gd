# Screenshots of the pull scene from each camera, for reviewing the look:
# the scripted driver pulls for a while, then each camera saves a PNG to
# the directory given after `--`, in the easy layout or, with `pro` after
# it, the pro layout. Needs a display (or Xvfb) and a renderer:
#
#   godot --fixed-fps 60 --rendering-method gl_compatibility --path app/godot \
#       --script res://tests/screenshots.gd -- /tmp/shots [pro]
extends SceneTree

const FRAMES_BEFORE := 900

var _pull: Node
var _frames := 0
var _view := 0
var _directory := "user://"


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_directory = args[0]
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("autoplay", true)
	_pull.set("pro", args.has("pro"))
	root.add_child(_pull)


func _process(_delta: float) -> bool:
	_frames += 1
	if _frames < FRAMES_BEFORE:
		return false
	# A few frames per camera, so it has caught up before the shot.
	if (_frames - FRAMES_BEFORE) % 30 != 29:
		return false
	var image := root.get_texture().get_image()
	var path := "%s/view_%d.png" % [_directory, _view]
	image.save_png(path)
	print("saved ", path)
	_view += 1
	if _view >= 3:
		quit(0)
		return true
	_pull.get("cameras").call("next")
	return false
