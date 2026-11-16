# The tractor dealer (DealerScreen), played with simulated fingers on the
# shipped career (data/careers/tractor_path.toml): a new career opens on
# the Standard 3.5 t dealer, with no way past it; a model and a colour
# picked turn on the turntable, bought they come off the money, go in the
# save and onto the tractor the game draws, the rivals in the dealer's
# other models. The garage's paint shop paints it another colour for the
# paint's price (the colour it has costs nothing and is not offered).
# Launched again, the career goes on with its tractor: no dealer. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it). With a
# directory after `--` it also saves a screenshot of each screen there
# (that needs a display, or Xvfb, and a renderer).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/dealer_test.gd [-- /tmp/shots]
extends SceneTree

const SEED := 2026
const SETTINGS_PATH := "user://dealer_test_settings.cfg"
const SAVE_PATH := "user://dealer_test_season.toml"
const TAP_FINGER := 1
# data/garages/standard_3500.toml: the start money, the third model and
# the fourth paint, and a paint job.
const START_MONEY_EUR := 2500
const MODEL := 2
const MODEL_NAME := "Laagland 70"
const MODEL_STYLE := "farm_low"
const MODEL_COST_EUR := 2000
const PAINT := 3
const PAINT_COLOUR := "#e3741c"
const PAINT_COST_EUR := 300

var _pull: Node
var _failures := 0
var _shots := ""


func _initialize() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SETTINGS_PATH))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SAVE_PATH))
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		_shots = args[0]
	_launch()
	_play.call_deferred()


func _launch() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("season_seed", SEED)
	_pull.set("settings_path", SETTINGS_PATH)
	_pull.set("save_path", SAVE_PATH)
	_pull.set("whole_turn", false)
	root.add_child(_pull)


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _frames(count: int) -> void:
	for i in count:
		await process_frame


func _touch(at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = TAP_FINGER
	event.position = at
	event.pressed = pressed
	root.push_input(event, true)


func _tap(control: Control) -> void:
	var at := control.get_global_rect().get_center()
	_touch(at, true)
	_touch(at, false)


func _shot(name: String) -> void:
	if _shots.is_empty():
		return
	await _frames(3)
	var path := "%s/%s.png" % [_shots, name]
	root.get_texture().get_image().save_png(path)
	print("saved ", path)


func _play() -> void:
	await _frames(3)
	var event: TractorPullEvent = _pull.get("event")
	var dealer_screen: DealerScreen = _pull.get("dealer_screen")
	var start: StartScreen = _pull.get("start_screen")
	var dealer := event.get_dealer()
	_expect(dealer_screen.visible and dealer_screen.mode == DealerScreen.Mode.BUY,
			"a new career opens at the dealer")
	_expect(not dealer_screen.back_button.visible, "no way past the dealer")
	_expect(dealer.get("needs_tractor", false) and int(dealer["money_eur"]) == START_MONEY_EUR,
			"the tractor not bought, the start money there: %s" % dealer)
	_expect(dealer_screen.model_buttons.size() == 5 and dealer_screen.swatches.size() == 8,
			"five models, eight paints")
	await _shot("dealer_buy")
	_tap(dealer_screen.model_buttons[MODEL])
	_tap(dealer_screen.swatches[PAINT])
	await _frames(2)
	_expect(dealer_screen.preview.look.style_id == MODEL_STYLE
			and dealer_screen.preview.look.paint.is_equal_approx(Color(PAINT_COLOUR)),
			"the turntable shows the model and paint picked")
	_expect(dealer_screen.preview.get_child_count() > 0, "the turntable's tractor is built")
	await _shot("dealer_picked")
	_tap(dealer_screen.action_button)
	await _frames(2)
	dealer = event.get_dealer()
	_expect(start.visible and not dealer_screen.visible, "bought, the start screen")
	_expect(int(dealer["model"]) == MODEL and int(dealer["paint"]) == PAINT
			and int(dealer["money_eur"]) == START_MONEY_EUR - MODEL_COST_EUR,
			"the model and paint bought for its price: %s" % dealer)
	var save := FileAccess.get_file_as_string(SAVE_PATH)
	_expect(save.contains("model = \"%s\"" % MODEL_NAME), "the save keeps the model")
	var look: TractorLook = _pull.call("class_look", event.get_tractor_dimensions())
	_expect(look.style_id == MODEL_STYLE and look.paint.is_equal_approx(Color(PAINT_COLOUR)),
			"the game draws the tractor bought")
	var rival := look.rival(0)
	_expect(rival.style_id != MODEL_STYLE and not rival.style_id.is_empty(),
			"the rivals in the dealer's other models: %s" % rival.style_id)
	await _paint_shop(event, dealer_screen)
	# Launched again: the career goes on with its tractor.
	_pull.queue_free()
	await _frames(2)
	_launch()
	await _frames(3)
	event = _pull.get("event")
	dealer = event.get_dealer()
	_expect(not (_pull.get("dealer_screen") as DealerScreen).visible
			and (_pull.get("start_screen") as StartScreen).visible,
			"launched again, no dealer")
	_expect(int(dealer["model"]) == MODEL and int(dealer["paint"]) == 0,
			"launched again, the tractor as it was: %s" % dealer)
	print("dealer test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)


# The garage's paint shop: the colour the tractor has is not offered,
# another costs the paint job.
func _paint_shop(event: TractorPullEvent, dealer_screen: DealerScreen) -> void:
	var start: StartScreen = _pull.get("start_screen")
	var garage: GarageScreen = _pull.get("garage_screen")
	start.garage_requested.emit()
	await _frames(2)
	_expect(garage.visible and garage.paint_shop_button.visible, "the garage has a paint shop")
	_tap(garage.paint_shop_button)
	await _frames(2)
	_expect(dealer_screen.visible and dealer_screen.mode == DealerScreen.Mode.PAINT
			and dealer_screen.back_button.visible, "the paint shop, with a way back")
	_expect(dealer_screen.action_button.disabled, "the colour it has is not for sale")
	_tap(dealer_screen.swatches[0])
	await _frames(1)
	_expect(not dealer_screen.action_button.disabled, "another colour is")
	await _shot("paint_shop")
	var money := int(event.get_dealer()["money_eur"])
	_tap(dealer_screen.action_button)
	await _frames(2)
	var dealer := event.get_dealer()
	_expect(garage.visible and int(dealer["paint"]) == 0
			and int(dealer["money_eur"]) == money - PAINT_COST_EUR,
			"painted for the paint job's price: %s" % dealer)
	_tap(garage.paint_shop_button)
	await _frames(2)
	_tap(dealer_screen.back_button)
	await _frames(2)
	_expect(garage.visible, "back from the paint shop to the garage")
