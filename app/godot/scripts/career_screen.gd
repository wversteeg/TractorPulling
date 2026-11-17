# Before a new season, when more than one class or level is open: where to
# pull it, in the garage's look (GarageScreen). The career's classes
# (TractorPullEvent.get_career()["choices"]) stand side by side as cards,
# a ladder from left to right, joined by chevrons: each card in its class's
# colours (TractorLook) with what the player's tractor there has (each class
# its own, kept when the player pulls elsewhere: its stages bought, or a new
# one from the dealer) and its levels stacked as a ladder, the top level
# highest. A level not yet open has a padlock and says what opens it; the
# player's level now is marked; the one picked is blue (the highest open
# unless the caller says otherwise). The money, one wallet for every class,
# sits top right; "Start season" starts it there (start_requested), "‹"
# goes back (back_requested).
class_name CareerScreen
extends Control

signal start_requested(rung: int)
signal back_requested

const STAGE := Vector2(1280.0, 720.0)
const BACKGROUND := GarageScreen.BACKGROUND
const TOP_BAR_HEIGHT := GarageScreen.TOP_BAR_HEIGHT
const CORNER := GarageScreen.CORNER
const TITLE_SIZE := GarageScreen.TITLE_SIZE
const MONEY_SIZE := GarageScreen.MONEY_SIZE
const SMALL_SIZE := 13
const LINE_SIZE := 15
const NAME_SIZE := 24
const STAGES_SIZE := 20
const LEVEL_FONT_SIZE := 22
const PICKED_SIZE := 17
# The cards: where their row starts and how tall, the widest a card gets,
# the gap between two (a chevron in it) and the margin at each side [px].
const CARDS_TOP := 112.0
const CARD_HEIGHT := 488.0
const CARD_WIDEST := 290.0
const CARD_GAP := 40.0
const MARGIN := 24.0
const CARD_PADDING := 18.0
# The class's colours across the card's top [px].
const STRIPE_HEIGHT := 10.0
# The levels' rows: where the first (the top level) starts in a card, how
# tall each is and the gap between them [px].
const LEVELS_TOP := 172.0
const LEVEL_HEIGHT := 58.0
# The note at a card's foot: its height above the card's bottom edge [px].
# The levels' rows shrink to fit above it when a class has more than fit.
const NOTE_HEIGHT := 70.0
const LEVEL_GAP := 10.0
const PROGRESS_HEIGHT := 8.0
# A class not open yet, faded.
const LOCKED_FADE := 0.55
const START_RECT := Rect2(996, 618, 256, 78)
const PICKED_RECT := Rect2(28, 624, 940, 70)

var start_button: TouchButton
var back_button: TouchButton
# The level buttons, by rung.
var level_buttons: Array[TouchButton] = []
var _stage := Control.new()
var _cards := Control.new()
var _title: Label
var _subtitle: Label
var _money: Label
var _picked_line: Label
var _choices: Array = []
var _picked := -1
# The places that open the level above (the career's promotion_place).
var _promotion_place := 2


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
	var bar := _panel(_stage, Rect2(0, 0, STAGE.x, TOP_BAR_HEIGHT), Color(0.04, 0.05, 0.07, 0.9), 0)
	bar.z_index = -1
	back_button = _button(_stage, "‹", Rect2(24, 10, 46, 44), 30)
	back_button.tapped.connect(func() -> void: back_requested.emit())
	_title = _label(_stage, Rect2(80, 12, 520, 40), TITLE_SIZE, GarageArt.TEXT)
	_title.text = "CAREER"
	_subtitle = _label(_stage, Rect2(80, 40, 760, 22), SMALL_SIZE, GarageArt.DIM)
	_money = _label(_stage, Rect2(980, 12, 272, 40), MONEY_SIZE, GarageArt.ORANGE_LIGHT)
	_money.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_label(_stage, Rect2(MARGIN, 80, 800, 22), SMALL_SIZE, GarageArt.DIM).text = \
			"WHERE TO PULL THE NEXT SEASON"
	_cards.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cards.size = STAGE
	_stage.add_child(_cards)
	_picked_line = _label(_stage, PICKED_RECT, PICKED_SIZE, GarageArt.TEXT)
	_picked_line.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_picked_line.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	start_button = _button(_stage, "Start season", START_RECT, 26)
	start_button.rounded = true
	start_button.primary = true
	start_button.icon = "event"
	start_button.tapped.connect(func() -> void:
		if _picked >= 0:
			start_requested.emit(_picked))


# `career` is TractorPullEvent.get_career(); `pick` the rung to start on
# picked (the highest open when -1 or not open); `can_go_back` whether
# "‹" shows.
func show_career(career: Dictionary, pick: int = -1, can_go_back: bool = true) -> void:
	_subtitle.text = "%s  ·  each class keeps its own tractor" % String(career.get("name",
			"Career"))
	_money.text = GarageScreen.euros(int(career.get("money_eur", 0)))
	back_button.visible = can_go_back
	_title.position.x = 80.0 if can_go_back else 28.0
	_subtitle.position.x = _title.position.x
	_choices = career.get("choices", [])
	_promotion_place = maxi(int(career.get("promotion_place", 2)), 1)
	for child in _cards.get_children():
		child.free()
	level_buttons.clear()
	level_buttons.resize(_choices.size())
	var classes := _by_class()
	var count := classes.size()
	var width := minf(CARD_WIDEST, (STAGE.x - 2.0 * MARGIN - CARD_GAP * (count - 1)) / count)
	var left := (STAGE.x - count * width - CARD_GAP * (count - 1)) / 2.0
	for i in count:
		var x := left + i * (width + CARD_GAP)
		_add_card(classes[i], i, Rect2(x, CARDS_TOP, width, CARD_HEIGHT))
		if i + 1 < count:
			# The ladder goes on: orange where the next class is open.
			var chevron := _label(_cards, Rect2(x + width, CARDS_TOP + CARD_HEIGHT / 2.0 - 30.0,
					CARD_GAP, 50), 40, GarageArt.ORANGE if _any_open(classes[i + 1])
					else GarageArt.LINE)
			chevron.text = "›"
			chevron.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	if pick < 0 or pick >= _choices.size() or not _choices[pick].get("open", false):
		pick = int(career.get("unlocked", 0))
	pick_rung(pick)


# Picks rung `rung` to start the season on, if it is open.
func pick_rung(rung: int) -> void:
	if rung < 0 or rung >= _choices.size() or not _choices[rung].get("open", false):
		return
	_picked = rung
	for i in level_buttons.size():
		if level_buttons[i] != null:
			level_buttons[i].selected = i == rung
	var choice: Dictionary = _choices[rung]
	_picked_line.text = "%s · %s%s: %s." % [choice["class_name"], choice["level_name"],
			" (your level now)" if choice.get("current", false) else "",
			_tractor_words(choice)]


# The rung picked.
func picked_rung() -> int:
	return _picked


func release_buttons() -> void:
	for button: TouchButton in [start_button, back_button]:
		button.release()
	for button in level_buttons:
		if button != null:
			button.release()


# The choices grouped by class, in the career's order.
func _by_class() -> Array[Array]:
	var out: Array[Array] = []
	var name_now := ""
	for choice: Dictionary in _choices:
		if choice["class_name"] != name_now:
			name_now = choice["class_name"]
			out.append([])
		out.back().append(choice)
	return out


static func _any_open(levels: Array) -> bool:
	return levels.any(func(c: Dictionary) -> bool: return c.get("open", false))


# One class's card: its colours, its tractor, its levels as a ladder.
func _add_card(levels: Array, index: int, rect: Rect2) -> void:
	var first: Dictionary = levels[0]
	var open := _any_open(levels)
	var card := _panel(_cards, rect, GarageArt.PANEL, CORNER)
	if not open:
		card.modulate = Color(1.0, 1.0, 1.0, LOCKED_FADE)
	var look := TractorLook.for_class(String(first["class_name"]), "cab")
	var stripe := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = look.paint
	style.border_color = look.trim
	style.border_width_bottom = 3
	style.corner_radius_top_left = CORNER
	style.corner_radius_top_right = CORNER
	stripe.add_theme_stylebox_override("panel", style)
	stripe.size = Vector2(rect.size.x, STRIPE_HEIGHT)
	stripe.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(stripe)
	var inner := rect.size.x - 2.0 * CARD_PADDING
	_label(card, Rect2(CARD_PADDING, 22, inner, 18), SMALL_SIZE, GarageArt.DIM).text = \
			"CLASS %d" % (index + 1)
	_label(card, Rect2(CARD_PADDING, 40, inner, 34), NAME_SIZE, GarageArt.TEXT).text = \
			String(first["class_name"])
	_add_tractor(card, first, inner)
	# Top level highest, as a ladder.
	var count := levels.size()
	var room := rect.size.y - NOTE_HEIGHT - LEVELS_TOP
	var row := minf(LEVEL_HEIGHT, (room - LEVEL_GAP * count) / count)
	var y := LEVELS_TOP
	for k in range(count - 1, -1, -1):
		var choice: Dictionary = levels[k]
		_add_level(card, choice, Rect2(CARD_PADDING, y, inner, row))
		y += row + LEVEL_GAP
	var note := _label(card, Rect2(CARD_PADDING, rect.size.y - NOTE_HEIGHT, inner, 56),
			SMALL_SIZE, GarageArt.DIM)
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
	note.text = _note(levels)


# What the player's tractor in this class has: its stages bought, on a bar
# as in the workshop; or a new one, from the class's dealer.
func _add_tractor(card: Control, choice: Dictionary, inner: float) -> void:
	var yours := not String(choice.get("tractor", "")).is_empty()
	_label(card, Rect2(CARD_PADDING, 86, inner, 18), SMALL_SIZE, GarageArt.DIM).text = \
			"YOUR TRACTOR" if yours else "NEW TRACTOR"
	if not yours:
		_label(card, Rect2(CARD_PADDING, 106, inner, 40), LINE_SIZE, GarageArt.DIM).text = \
				"Bought from the class's dealer"
		return
	var bought := int(choice.get("stages_bought", 0))
	var stages := maxi(int(choice.get("stages", 0)), 1)
	_label(card, Rect2(CARD_PADDING, 104, inner, 28), STAGES_SIZE, GarageArt.TEXT).text = \
			"%d of %d stages" % [bought, stages]
	var track := ColorRect.new()
	track.color = GarageArt.LINE
	track.position = Vector2(CARD_PADDING, 140)
	track.size = Vector2(inner, PROGRESS_HEIGHT)
	track.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(track)
	var fill := ColorRect.new()
	fill.color = GarageArt.ORANGE
	fill.position = track.position
	fill.size = Vector2(inner * clampf(float(bought) / stages, 0.0, 1.0), PROGRESS_HEIGHT)
	fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(fill)


# One level's row: tap to pick it; a padlock while it is not open; "NOW"
# on the player's level.
func _add_level(card: Control, choice: Dictionary, rect: Rect2) -> void:
	var open: bool = choice.get("open", false)
	var button := _button(card, String(choice["level_name"]), rect, LEVEL_FONT_SIZE)
	button.rounded = true
	button.disabled = not open
	if not open:
		button.icon = "lock"
	var rung: int = choice["rung"]
	button.tapped.connect(func() -> void: pick_rung(rung))
	level_buttons[rung] = button
	if choice.get("current", false):
		var now := _label(card, Rect2(rect.position.x, rect.position.y, rect.size.x - 12.0,
				rect.size.y), SMALL_SIZE, GarageArt.ORANGE_LIGHT)
		now.text = "NOW"
		now.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		now.vertical_alignment = VERTICAL_ALIGNMENT_CENTER


# The card's foot: what opens its first level not yet open (a place up to
# the career's promotion_place in the championship of the rung before it),
# or that every level is open.
func _note(levels: Array) -> String:
	for choice: Dictionary in levels:
		if not choice.get("open", false):
			var rung: int = choice["rung"]
			if rung <= 0:
				return ""
			var before: Dictionary = _choices[rung - 1]
			return "%s opens with %s at %s · %s." % [choice["level_name"],
					places_words(_promotion_place),
					before["class_name"], before["level_name"]]
	return "Every level open." if levels.size() > 1 else "Open."


# The places up to `places`: "1st", "1st or 2nd", "1st to 3rd".
static func places_words(places: int) -> String:
	match places:
		1:
			return "1st"
		2:
			return "1st or 2nd"
	return "1st to %s" % _ordinal(places)


static func _ordinal(n: int) -> String:
	var suffix := "th"
	if n % 100 < 11 or n % 100 > 13:
		suffix = ["th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"][n % 10]
	return "%d%s" % [n, suffix]


# What the player's tractor in `choice`'s class has.
static func _tractor_words(choice: Dictionary) -> String:
	if String(choice.get("tractor", "")).is_empty():
		return "a new tractor, bought from the class's dealer"
	return "your tractor, %d of %d stages bought" % [int(choice.get("stages_bought", 0)),
			int(choice.get("stages", 0))]


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
