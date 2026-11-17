# Pictures of the brake hold's button (DriveControls' HOLD) in each of its
# places, in the easy and the pro layout, on the Supersport 3.6 t tractor at
# the line, for picking where it goes. Each shot is a PNG in the directory
# given after `--`. Needs a display (or Xvfb) and a renderer:
#
#   godot --fixed-fps 60 --rendering-method gl_compatibility --resolution 1280x720 \
#       --path app/godot --script res://tests/hold_places.gd -- /tmp/hold
extends SceneTree

var _directory := "user://"


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_directory = args[0]
	_play.call_deferred()


func _frames(count: int) -> void:
	for i in count:
		await process_frame


func _play() -> void:
	for pro: bool in [false, true]:
		var pull: Node = (load("res://scenes/pull.tscn") as PackedScene).instantiate()
		pull.set("autoplay", true)
		pull.set("pro", pro)
		pull.set("tractor_path", "res://data/tractors/supersport_3600.toml")
		pull.set("sled_path", "res://data/sleds/supersport_3600.toml")
		root.add_child(pull)
		await _frames(20)
		# Stood at the line, the controls as a player has them.
		pull.process_mode = Node.PROCESS_MODE_DISABLED
		var controls: DriveControls = pull.get("controls")
		var hud: CanvasItem = pull.get("hud")
		hud.visible = true
		controls.visible = true
		controls.set_layout(pro)
		for place in DriveControls.HOLD_PLACES.size():
			controls.set_hold_place(place)
			controls.set_hold_available(true)
			# The middle one as it looks held.
			controls.hold = place == 1
			controls.hold_button.selected = controls.hold
			controls.hold_button.text = "HOLDING" if controls.hold else "HOLD"
			controls.hold_button.queue_redraw()
			await _frames(3)
			var path := "%s/hold_%s_%d_%s.png" % [_directory, "pro" if pro else "easy", place + 1,
					DriveControls.HOLD_PLACES[place].replace(" ", "_")]
			root.get_texture().get_image().save_png(path)
			print("saved ", path)
		pull.queue_free()
		await _frames(2)
	quit(0)
