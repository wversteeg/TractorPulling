# Pictures of every tractor model the dealers sell (TractorModels), for
# reviewing them: each class's tractor (its real sizes, from the data)
# in each of its models, painted in one of the dealer's colours, from the
# front quarter and from the side, standing at the start line in the day.
# Then each class's dealer screen (DealerScreen), as a career entering the
# class sees it, on each of its models in turn. Each shot is a PNG in the
# directory given after `--`. Needs a display (or Xvfb) and a renderer:
#
#   godot --fixed-fps 60 --rendering-method gl_compatibility --resolution 1280x720 \
#       --path app/godot --script res://tests/dealer_models.gd -- /tmp/models
extends SceneTree

# Each class's first rung on the shipped career, for its dealer screen.
const CAREER := "res://data/careers/tractor_path.toml"
const RUNGS := {"standard_3500": 0, "sport_2500": 1, "sport_4500": 3}
# Each class's tractor and sled, and the models its dealer sells.
const CLASSES := [
	["standard_3500", "res://data/tractors/farm_stock_light.toml",
			"res://data/sleds/light_class.toml",
			["farm_classic", "farm_vintage", "farm_low", "farm_high", "farm_modern"]],
	["sport_2500", "res://data/tractors/sport_2500.toml", "res://data/sleds/sport_2500.toml",
			["pull_classic", "pull_low", "pull_high", "pull_cage", "pull_farm"]],
	["sport_4500", "res://data/tractors/sport_4500.toml", "res://data/sleds/sport_4500.toml",
			["pull_classic", "pull_low", "pull_high", "pull_farm", "pull_cab"]],
]
# A colour for each picture in turn: paint, trim, rims.
const PAINTS := [
	[Color("#b3241c"), Color("#f2f0e8"), Color("#e8dcb4")],
	[Color("#1f5e2e"), Color("#f2d230"), Color("#f2d230")],
	[Color("#1d4f9c"), Color("#f2f0e8"), Color("#d8d8dc")],
	[Color("#e3741c"), Color("#20201f"), Color("#20201f")],
	[Color("#9aa0a6"), Color("#1c1c1e"), Color("#b3241c")],
	[Color("#121214"), Color("#e0aa2a"), Color("#e0aa2a")],
	[Color("#f0efe9"), Color("#b3241c"), Color("#b3241c")],
	[Color("#5c1a6e"), Color("#f2f0e8"), Color("#d8d8dc")],
]

var _pull: Node
var _directory := "user://"
var _camera := Camera3D.new()


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_directory = args[0]
	_play.call_deferred()


func _frames(count: int) -> void:
	for i in count:
		await process_frame


func _play() -> void:
	var shot := 0
	for entry: Array in CLASSES:
		_pull = (load("res://scenes/pull.tscn") as PackedScene).instantiate()
		_pull.set("autoplay", true)
		_pull.set("tractor_path", entry[1])
		_pull.set("sled_path", entry[2])
		root.add_child(_pull)
		_camera.far = 600.0
		_pull.add_child(_camera)
		await _frames(10)
		# Stood still at the start line, as the run set it up.
		_pull.process_mode = Node.PROCESS_MODE_DISABLED
		for name: String in ["hud", "controls"]:
			(_pull.get(name) as CanvasItem).visible = false
		# The driver waits: the tractor stands at the start line.
		var run: TractorPullRun = _pull.get("run")
		var view: TractorView = _pull.get("tractor_view")
		var dimensions := run.get_dimensions()
		for style: String in entry[3]:
			var colours: Array = PAINTS[shot % PAINTS.size()]
			var look := view.look.painted(colours[0], colours[1], colours[2])
			look.style_id = style
			view.look = look
			view.build(dimensions)
			await _frames(4)
			var t := float(run.get_snapshot()["tractor_x_m"])
			var name := "%s_%d_%s" % [entry[0], entry[3].find(style) + 1, style]
			await _shoot(name + "_front", Vector3(t + 4.6, -3.6, 1.8), Vector3(t + 1.0, 0.0, 1.0))
			await _shoot(name + "_side", Vector3(t + 1.0, -6.4, 1.3), Vector3(t + 1.0, 0.0, 1.0))
			shot += 1
		_pull.remove_child(_camera)
		_pull.queue_free()
		await _frames(2)
	await _dealers()
	quit(0)


# Each class's dealer screen on each of its models, a colour each.
func _dealers() -> void:
	var event := TractorPullEvent.new()
	if not event.load_career(CAREER).is_empty():
		push_error("cannot load the career")
		return
	var screen := DealerScreen.new()
	root.add_child(screen)
	for entry: Array in CLASSES:
		var error := event.new_career_at(RUNGS[entry[0]], 1)
		if not error.is_empty():
			push_error(error)
			continue
		var dealer := event.get_dealer()
		var dimensions := event.get_tractor_dimensions()
		var class_name_now := String(event.get_career().get("class_name", ""))
		var base := TractorLook.for_class(class_name_now, str(dimensions.get("protection", "cab")))
		screen.show_dealer(dealer, dimensions, base, false)
		for model in (dealer["models"] as Array).size():
			screen.pick_model(model)
			screen.pick_paint((model * 3 + 1) % (dealer["paints"] as Array).size())
			await _frames(30)
			var path := "%s/dealer_%s_%d.png" % [_directory, entry[0], model + 1]
			root.get_texture().get_image().save_png(path)
			print("saved ", path)


func _shoot(name: String, from: Vector3, at: Vector3) -> void:
	_camera.fov = 50.0
	_camera.global_position = TrackSpace.point(from.x, from.y, from.z)
	_camera.look_at(TrackSpace.point(at.x, at.y, at.z), Vector3.UP)
	_camera.make_current()
	await _frames(3)
	var path := "%s/%s.png" % [_directory, name]
	root.get_texture().get_image().save_png(path)
	print("saved ", path)
