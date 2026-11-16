# Before a new season, when more than one class or level is open: where to
# pull it. Each class of the career (TractorPullEvent.get_career()["choices"])
# with its levels as cards, those not yet open greyed, the one picked in
# blue (the highest open unless the caller says otherwise); under each
# class what the player's tractor there has (each class has its own
# tractor, kept when the player pulls elsewhere; a class not pulled in yet,
# a new one), and the money, one wallet for every class. "Start season"
# starts it there (start_requested), "Back" returns.
class_name CareerScreen
extends MenuPanel

signal start_requested(rung: int)
signal back_requested

const TITLE_SIZE := 40
const LINE_SIZE := 22
const LEVEL_BUTTON_SIZE := Vector2(190.0, 58.0)
const LEVEL_FONT_SIZE := 24
const CAREER_PANEL_SIZE := Vector2(960.0, 680.0)
# Wide enough for "Start season" with its icon.
const START_BUTTON_SIZE := Vector2(320.0, 90.0)

var start_button: TouchButton
var back_button: TouchButton
# The level cards, by rung.
var level_buttons: Array[TouchButton] = []
var _money: Label
var _picked_line: Label
var _classes := VBoxContainer.new()
var _choices: Array = []
var _picked := -1


func _init() -> void:
	panel_size = CAREER_PANEL_SIZE


func _ready() -> void:
	super()
	add_title("Next season", TITLE_SIZE, "Career")
	_money = add_line(LINE_SIZE, GarageArt.DIM)
	_classes.add_theme_constant_override("separation", int(GAP / 3.0))
	_classes.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_item(_classes)
	_picked_line = add_line(LINE_SIZE)
	_picked_line.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var row := add_button_row()
	back_button = add_button(row, "Back", "")
	start_button = add_button(row, "Start season", "event")
	start_button.primary = true
	start_button.custom_minimum_size = START_BUTTON_SIZE
	back_button.tapped.connect(func() -> void: back_requested.emit())
	start_button.tapped.connect(func() -> void:
		if _picked >= 0:
			start_requested.emit(_picked))


# `career` is TractorPullEvent.get_career(); `pick` the rung to start on
# picked (the highest open when -1 or not open); `can_go_back` whether
# "Back" shows.
func show_career(career: Dictionary, pick: int = -1, can_go_back: bool = true) -> void:
	eyebrow_label.text = String(career.get("name", "Career")).to_upper()
	_money.text = "€ %s in the bank  ·  each class keeps its own tractor" % _thousands(
			int(career.get("money_eur", 0)))
	_choices = career.get("choices", [])
	for child in _classes.get_children():
		child.free()
	level_buttons.clear()
	var row: HBoxContainer = null
	var class_name_now := ""
	for choice: Dictionary in _choices:
		if choice["class_name"] != class_name_now:
			class_name_now = choice["class_name"]
			var heading := Label.new()
			heading.text = "%s  ·  %s" % [String(class_name_now).to_upper(), _tractor_words(choice)]
			heading.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			heading.add_theme_font_size_override("font_size", HEADING_SIZE)
			heading.add_theme_color_override("font_color", GarageArt.DIM)
			heading.mouse_filter = Control.MOUSE_FILTER_IGNORE
			_classes.add_child(heading)
			row = HBoxContainer.new()
			row.alignment = BoxContainer.ALIGNMENT_CENTER
			row.add_theme_constant_override("separation", int(GAP / 2.0))
			row.mouse_filter = Control.MOUSE_FILTER_IGNORE
			_classes.add_child(row)
		var button := TouchButton.new()
		var open: bool = choice.get("open", false)
		button.text = String(choice["level_name"]) if open else "%s · locked" % choice["level_name"]
		button.rounded = true
		button.font_size = LEVEL_FONT_SIZE
		button.fires_on_release = true
		button.custom_minimum_size = LEVEL_BUTTON_SIZE
		button.disabled = not open
		var rung: int = choice["rung"]
		button.tapped.connect(func() -> void: pick_rung(rung))
		row.add_child(button)
		level_buttons.append(button)
	if pick < 0 or pick >= _choices.size() or not _choices[pick].get("open", false):
		pick = int(career.get("unlocked", 0))
	pick_rung(pick)
	back_button.visible = can_go_back


# Picks rung `rung` to start the season on, if it is open.
func pick_rung(rung: int) -> void:
	if rung < 0 or rung >= _choices.size() or not _choices[rung].get("open", false):
		return
	_picked = rung
	for i in level_buttons.size():
		level_buttons[i].selected = i == rung
	var choice: Dictionary = _choices[rung]
	_picked_line.text = "%s · %s%s: %s." % [choice["class_name"], choice["level_name"],
			" (your level now)" if choice.get("current", false) else "",
			_tractor_words(choice)]


# The rung picked.
func picked_rung() -> int:
	return _picked


func release_buttons() -> void:
	super()
	for button in level_buttons:
		button.release()


# What the player's tractor in `choice`'s class has.
static func _tractor_words(choice: Dictionary) -> String:
	if String(choice.get("tractor", "")).is_empty():
		return "a new tractor"
	return "your tractor, %d of %d stages bought" % [int(choice.get("stages_bought", 0)),
			int(choice.get("stages", 0))]


# 12345 as "12,345".
static func _thousands(amount: int) -> String:
	var digits := str(absi(amount))
	var out := ""
	while digits.length() > 3:
		out = "," + digits.substr(digits.length() - 3) + out
		digits = digits.substr(0, digits.length() - 3)
	return ("-" if amount < 0 else "") + digits + out
