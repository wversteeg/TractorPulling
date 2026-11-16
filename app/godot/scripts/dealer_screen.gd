# The class's tractor dealer (TractorPullEvent.get_dealer()), in the
# garage's look, in two modes:
#
# - Buying: a new class (and a new career) starts without its tractor. The
#   dealer's models stand on a turntable, one at a time: the player picks a
#   model and a colour and buys it. There is no way back: a season in the
#   class waits for its tractor.
# - The paint shop, from the garage: the tractor bought, in another of the
#   dealer's colours, for the paint's price.
#
# For now the models only look different (TractorModels): each class's
# tractor pulls the same whatever its model.
class_name DealerScreen
extends Control

signal buy_requested(model: int, paint: int)
signal paint_requested(paint: int)
signal back_requested

enum Mode { BUY, PAINT }

const STAGE := Vector2(1280.0, 720.0)
const BACKGROUND := Color(0.06, 0.07, 0.09, 0.985)
const TOP_BAR_HEIGHT := 64.0
const TITLE_SIZE := 26
const MONEY_SIZE := 24
const NAME_SIZE := 30
const LINE_SIZE := 17
const CARD_SIZE := 20
const CORNER := 14
const PREVIEW_RECT := Rect2(24, 84, 760, 470)
const ABOUT_RECT := Rect2(24, 566, 760, 100)
const SIDE_X := 808.0
const SIDE_WIDTH := 448.0
const MODEL_CARD_HEIGHT := 56.0
const MODEL_CARD_GAP := 8.0
const SWATCH_SIZE := 48.0
const SWATCH_GAP := 8.0
const ACTION_RECT := Rect2(808, 596, 448, 74)
# The turntable: how fast it turns [rad/s], the camera's place and aim
# from the tractor's middle (Godot's axes) [m], and its field of view [deg].
const TURN_RATE_RAD_S := 0.35
# Where the turntable starts for each model: the nose towards the camera,
# a front quarter [rad].
const FRONT_QUARTER_RAD := -1.86
const CAMERA_FROM := Vector3(5.2, 2.3, 5.6)
const CAMERA_AT := Vector3(0.0, 0.95, 0.0)
const CAMERA_FOV_DEG := 38.0
const FLOOR_RADIUS_M := 3.6
const FLOOR := Color(0.1, 0.105, 0.12)
const SHOWROOM := Color(0.09, 0.1, 0.13)

var mode := Mode.BUY
# The model and paint picked (the dealer's indices).
var picked_model := 0
var picked_paint := 0
var back_button: TouchButton
var action_button: TouchButton
var model_buttons: Array[TouchButton] = []
var swatches: Array[TouchButton] = []
# The tractor on the turntable, as the player would get it.
var preview := TractorView.new()

var _dealer: Dictionary = {}
var _dimensions: Dictionary = {}
var _base: TractorLook = TractorLook.new()
var _stage := Control.new()
var _title: Label
var _subtitle: Label
var _money: Label
var _models_heading: Label
var _name: Label
var _about: Label
var _paint_name: Label
var _status: Label
var _models := Control.new()
var _swatch_row := Control.new()
var _viewport := SubViewport.new()
var _turntable := Node3D.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var background := ColorRect.new()
	background.color = BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	_stage.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stage.set_anchors_preset(Control.PRESET_CENTER)
	_stage.offset_left = -STAGE.x / 2.0
	_stage.offset_right = STAGE.x / 2.0
	_stage.offset_top = -STAGE.y / 2.0
	_stage.offset_bottom = STAGE.y / 2.0
	add_child(_stage)
	_build_top_bar()
	_build_preview()
	_build_side()


func _process(delta: float) -> void:
	if is_visible_in_tree():
		_turntable.rotation.y = wrapf(_turntable.rotation.y + TURN_RATE_RAD_S * delta, -PI, PI)


# `dealer` is TractorPullEvent.get_dealer(); `dimensions` the class's
# tractor (get_tractor_dimensions()) and `base` its class's look, which
# the dealer's paints colour. Opens on the tractor bought (in the paint
# shop) or the dealer's first model; `message` what the last try said.
func show_dealer(dealer: Dictionary, dimensions: Dictionary, base: TractorLook,
		paint_shop: bool, message: String = "") -> void:
	_dealer = dealer
	_dimensions = dimensions
	_base = base
	mode = Mode.PAINT if paint_shop else Mode.BUY
	var models: Array = dealer.get("models", [])
	picked_model = clampi(int(dealer.get("model", 0)), 0, maxi(models.size() - 1, 0))
	picked_paint = clampi(int(dealer.get("paint", 0)), 0,
			maxi((dealer.get("paints", []) as Array).size() - 1, 0))
	_build_cards()
	_turntable.rotation.y = FRONT_QUARTER_RAD
	_show(message)


# Picks model `model` (buying only) to show on the turntable.
func pick_model(model: int) -> void:
	if mode != Mode.BUY or model < 0 or model >= (_dealer.get("models", []) as Array).size():
		return
	picked_model = model
	_turntable.rotation.y = FRONT_QUARTER_RAD
	_show("")


# Picks paint `paint` to show on the turntable.
func pick_paint(paint: int) -> void:
	if paint < 0 or paint >= (_dealer.get("paints", []) as Array).size():
		return
	picked_paint = paint
	_show("")


# What the tractor looks like as model `model` in paint `paint` of
# `dealer` (TractorPullEvent.get_dealer()), from its class's look `base`;
# `base` itself without a dealer or before a model is bought (model -1).
static func look_for(base: TractorLook, dealer: Dictionary, model: int,
		paint: int) -> TractorLook:
	var models: Array = dealer.get("models", [])
	var paints: Array = dealer.get("paints", [])
	if model < 0 or model >= models.size():
		return base
	var look := base
	if paint >= 0 and paint < paints.size():
		var colours: Dictionary = paints[paint]
		look = base.painted(Color(String(colours["paint"])), Color(String(colours["trim"])),
				Color(String(colours["rims"])))
	else:
		look = base.painted(base.paint, base.trim, base.rim)
	look.style_id = String(models[model]["style"])
	return look


# Lets go of any finger on the buttons, as when the screen is hidden.
func release_buttons() -> void:
	for button in find_children("*", "TouchButton", true, false):
		(button as TouchButton).release()


func _show(message: String) -> void:
	var models: Array = _dealer.get("models", [])
	var paints: Array = _dealer.get("paints", [])
	var buying := mode == Mode.BUY
	var class_label := _base.name if not _base.name.is_empty() else "your class"
	_title.text = "Tractor dealer" if buying else "Paint shop"
	_subtitle.text = ("A new tractor for %s: pick a model and its colour." % class_label) \
			if buying else "Your tractor in another colour. The garage's parts stay as they are."
	_money.text = GarageScreen.euros(int(_dealer.get("money_eur", 0)))
	back_button.visible = not buying
	_models.visible = buying
	_models_heading.text = "MODELS" if buying else "YOUR TRACTOR"
	for i in model_buttons.size():
		model_buttons[i].selected = i == picked_model
	for i in swatches.size():
		swatches[i].selected = i == picked_paint
	var model: Dictionary = models[picked_model] if picked_model < models.size() else {}
	_name.text = String(model.get("name", ""))
	_about.text = String(model.get("about", ""))
	_paint_name.text = String(paints[picked_paint]["name"]) if picked_paint < paints.size() else ""
	var money := int(_dealer.get("money_eur", 0))
	var can_buy: bool = _dealer.get("can_buy", true)
	var price := 0
	if buying:
		price = int(model.get("price_eur", 0))
		action_button.text = "Buy · %s" % GarageScreen.euros(price)
		action_button.disabled = model.is_empty() or price > money or not can_buy
	else:
		var same := picked_paint == int(_dealer.get("paint", 0))
		price = 0 if same else int(_dealer.get("paint_cost_eur", 0))
		action_button.text = "Your colour" if same else "Paint · %s" % GarageScreen.euros(price)
		action_button.disabled = same or price > money or not can_buy
	if not message.is_empty():
		_status.text = message
	elif not can_buy:
		_status.text = "The tractor stays as it is until this event is over."
	elif price > money:
		_status.text = "Not enough money for this one."
	elif buying:
		_status.text = "Every model pulls the same: pick the one you like. %s" % (
				"" if price == int(model.get("cost_eur", 0))
				else "The dealer lets this one go for the money you have.")
	else:
		_status.text = "The first colour came with the tractor; another costs %s." % \
				GarageScreen.euros(int(_dealer.get("paint_cost_eur", 0)))
	if not _dimensions.is_empty() and not model.is_empty():
		preview.look = look_for(_base, _dealer, picked_model, picked_paint)
		preview.build(_dimensions)
		# The turntable turns about the tractor's middle, between its axles.
		preview.position = Vector3(0.0, 0.0, float(_dimensions["wheelbase_m"]) / 2.0)


# ---------------------------------------------------------------- building


func _build_top_bar() -> void:
	var bar := _panel(_stage, Rect2(0, 0, STAGE.x, TOP_BAR_HEIGHT), Color(0.04, 0.05, 0.07, 0.9), 0)
	bar.z_index = -1
	back_button = _button(_stage, "‹", Rect2(24, 10, 46, 44), 30)
	back_button.tapped.connect(func() -> void: back_requested.emit())
	_title = _label(_stage, Rect2(88, 12, 600, 40), TITLE_SIZE, GarageArt.TEXT)
	_subtitle = _label(_stage, Rect2(88, 40, 760, 22), 13, GarageArt.DIM)
	_money = _label(_stage, Rect2(1000, 12, 256, 40), MONEY_SIZE, GarageArt.ORANGE_LIGHT)
	_money.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT


# The turntable in its own world: a showroom floor, a key light and the
# tractor turning slowly before a still camera.
func _build_preview() -> void:
	var frame := _panel(_stage, PREVIEW_RECT, GarageArt.PANEL, CORNER)
	var container := SubViewportContainer.new()
	container.stretch = true
	container.position = Vector2(2, 2)
	container.size = PREVIEW_RECT.size - Vector2(4, 4)
	container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	frame.add_child(container)
	_viewport.own_world_3d = true
	_viewport.msaa_3d = Viewport.MSAA_2X
	container.add_child(_viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = SHOWROOM
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.75, 0.78, 0.85)
	environment.ambient_light_energy = 0.4
	var world := WorldEnvironment.new()
	world.environment = environment
	_viewport.add_child(world)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-50.0, 35.0, 0.0)
	key.light_energy = 0.9
	key.shadow_enabled = true
	_viewport.add_child(key)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-20.0, -140.0, 0.0)
	fill.light_energy = 0.35
	_viewport.add_child(fill)
	var floor_mesh := CylinderMesh.new()
	floor_mesh.top_radius = FLOOR_RADIUS_M
	floor_mesh.bottom_radius = FLOOR_RADIUS_M
	floor_mesh.height = 0.06
	floor_mesh.radial_segments = 64
	var showroom_floor := MeshInstance3D.new()
	showroom_floor.mesh = floor_mesh
	showroom_floor.material_override = Art.material(FLOOR, 0.6)
	showroom_floor.position.y = -0.03
	_turntable.add_child(showroom_floor)
	_turntable.add_child(preview)
	_viewport.add_child(_turntable)
	var camera := Camera3D.new()
	camera.fov = CAMERA_FOV_DEG
	camera.position = CAMERA_FROM
	_viewport.add_child(camera)
	camera.look_at_from_position(CAMERA_FROM, CAMERA_AT, Vector3.UP)
	camera.current = true
	_name = _label(_stage, Rect2(ABOUT_RECT.position, Vector2(ABOUT_RECT.size.x, 40)), NAME_SIZE,
			GarageArt.TEXT)
	_about = _label(_stage, Rect2(ABOUT_RECT.position + Vector2(0, 44),
			Vector2(ABOUT_RECT.size.x, 56)), LINE_SIZE, GarageArt.DIM)
	_about.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART


func _build_side() -> void:
	_models_heading = _label(_stage, Rect2(SIDE_X, 84, SIDE_WIDTH, 20), 13, GarageArt.DIM)
	_models.position = Vector2(SIDE_X, 108)
	_models.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stage.add_child(_models)
	_label(_stage, Rect2(SIDE_X, 440, SIDE_WIDTH, 20), 13, GarageArt.DIM).text = "COLOUR"
	_swatch_row.position = Vector2(SIDE_X, 464)
	_swatch_row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stage.add_child(_swatch_row)
	_paint_name = _label(_stage, Rect2(SIDE_X, 524, SIDE_WIDTH, 30), LINE_SIZE + 3, GarageArt.TEXT)
	action_button = TouchButton.new()
	action_button.rounded = true
	action_button.primary = true
	action_button.font_size = MenuPanel.BUTTON_FONT_SIZE
	action_button.fires_on_release = true
	action_button.position = ACTION_RECT.position
	action_button.size = ACTION_RECT.size
	action_button.tapped.connect(func() -> void:
		if mode == Mode.BUY:
			buy_requested.emit(picked_model, picked_paint)
		else:
			paint_requested.emit(picked_paint))
	_stage.add_child(action_button)
	_status = _label(_stage, Rect2(24, 676, 1232, 30), 15, GarageArt.DIM)


# A card per model and a swatch per paint, for the dealer shown.
func _build_cards() -> void:
	for child in _models.get_children() + _swatch_row.get_children():
		child.free()
	model_buttons.clear()
	swatches.clear()
	var models: Array = _dealer.get("models", [])
	for i in models.size():
		var card := TouchButton.new()
		card.rounded = true
		card.font_size = CARD_SIZE
		card.fires_on_release = true
		card.text = "%s  ·  %s" % [models[i]["name"], GarageScreen.euros(int(models[i]["price_eur"]))]
		card.position = Vector2(0, i * (MODEL_CARD_HEIGHT + MODEL_CARD_GAP))
		card.size = Vector2(SIDE_WIDTH, MODEL_CARD_HEIGHT)
		card.tapped.connect(pick_model.bind(i))
		_models.add_child(card)
		model_buttons.append(card)
	var paints: Array = _dealer.get("paints", [])
	for i in paints.size():
		var swatch := Swatch.new()
		swatch.paint = Color(String(paints[i]["paint"]))
		swatch.trim = Color(String(paints[i]["trim"]))
		swatch.fires_on_release = true
		swatch.position = Vector2(i * (SWATCH_SIZE + SWATCH_GAP), 0)
		swatch.size = Vector2(SWATCH_SIZE, SWATCH_SIZE)
		swatch.tapped.connect(pick_paint.bind(i))
		_swatch_row.add_child(swatch)
		swatches.append(swatch)


func _panel(parent: Control, rect: Rect2, colour: Color, corner: int) -> Panel:
	var panel := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = colour
	style.border_color = GarageArt.LINE
	style.set_border_width_all(1 if corner > 0 else 0)
	style.set_corner_radius_all(corner)
	panel.add_theme_stylebox_override("panel", style)
	panel.position = rect.position
	panel.size = rect.size
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(panel)
	return panel


func _label(parent: Control, rect: Rect2, font_size: int, colour: Color) -> Label:
	var label := Label.new()
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", colour)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label


func _button(parent: Control, text: String, rect: Rect2, font_size: int) -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.font_size = font_size
	button.fires_on_release = true
	button.position = rect.position
	button.size = rect.size
	parent.add_child(button)
	return button


# A paint to pick: a disc of the paint with a band of its trim, ringed in
# blue when picked.
class Swatch:
	extends TouchButton

	const RING := 3.0

	var paint := Color.RED
	var trim := Color.WHITE

	func _draw() -> void:
		var middle := size / 2.0
		var radius := minf(size.x, size.y) / 2.0 - RING * 2.0
		if selected or is_held():
			draw_circle(middle, radius + RING * 1.5, GarageArt.BLUE_LIGHT)
		draw_circle(middle, radius, paint)
		draw_rect(Rect2(middle.x - radius * 0.7, middle.y + radius * 0.25, radius * 1.4,
				radius * 0.3), trim)
		draw_arc(middle, radius, 0.0, TAU, 32, GarageArt.LINE, 1.0, true)
