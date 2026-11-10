# The garage, from the start screen: the money the player has won, the
# parts the player chooses between (a row of buttons each: the one fitted
# picked out, one owned fits it for nothing, one not owned shows its price
# and buys it), and each part of the tractor they can improve
# (TractorPullEvent.get_garage()), with what it is now and what the next
# level is, in a grid of fixed columns; the button with the next level's
# price buys it. A part's build that belongs to an alternative not fitted
# shows greyed: it stays bought. The game asks
# the extension, which holds the money and the class's rules. During an
# event, once the player has pulled, the tractor stays as it is: the prices
# show, their buttons greyed.
class_name GarageScreen
extends MenuPanel

signal buy_requested(index: int)
# Alternative `alternative` of choice `choice`: to buy (and fit) or to fit.
signal choice_requested(choice: int, alternative: int)
signal done

const GARAGE_SIZE := Vector2(1180.0, 700.0)
const TITLE_SIZE := 36
const LINE_SIZE := 24
const ROW_SIZE := 21
const COLUMN_GAP := 20
# Six parts, each a row, fit the panel at this height.
const BUY_SIZE := Vector2(180.0, 56.0)
const BUY_FONT_SIZE := 26
# The columns' widths [px]: the part, what it is now and the next level;
# the price button is BUY_SIZE wide. Fixed, so the grid does not shift with
# the texts: a longer one wraps onto a second line.
const COLUMN_WIDTHS: Array[float] = [200.0, 330.0, 330.0]
const TEXT_COLOUR := Color(0.95, 0.95, 0.95)
const DONE_COLOUR := Color(0.6, 0.85, 0.6)
const OFF_COLOUR := Color(0.6, 0.6, 0.6)
# A choice's alternatives: up to four buttons beside its name.
const CHOICE_SIZE := Vector2(250.0, 56.0)
const CHOICE_FONT_SIZE := 20

var done_button: TouchButton
# One per upgrade, in the garage's order.
var buy_buttons: Array[TouchButton] = []
# Per choice, one per alternative, in the garage's order.
var choice_buttons: Array[Array] = []
var _money: Label
var _choices := VBoxContainer.new()
var _table := GridContainer.new()
var _status: Label


func _init() -> void:
	panel_size = GARAGE_SIZE


func _ready() -> void:
	super()
	add_line(TITLE_SIZE).text = "Garage"
	_money = add_line(LINE_SIZE)
	_choices.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_item(_choices)
	_table.columns = 4
	_table.add_theme_constant_override("h_separation", COLUMN_GAP)
	_table.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_table.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_item(_table)
	_status = add_line(LINE_SIZE)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var go := add_button_row()
	done_button = add_button(go, "Done")
	done_button.tapped.connect(func() -> void: done.emit())


# `garage` is TractorPullEvent.get_garage(); `message` what the last
# purchase said, if anything.
func show_garage(garage: Dictionary, message: String = "") -> void:
	for child in _table.get_children():
		_table.remove_child(child)
		child.queue_free()
	buy_buttons.clear()
	var money := int(garage.get("money_eur", 0))
	_money.text = "Money: %s" % euros(money)
	var can_buy: bool = garage.get("can_buy", false)
	_show_choices(garage.get("choices", []), money, can_buy)
	var upgrades: Array = garage.get("upgrades", [])
	for i in upgrades.size():
		var line: Dictionary = upgrades[i]
		var at_top: bool = String(line["next_label"]).is_empty()
		var counts: bool = line.get("counts", true)
		var owner := String(line.get("for", ""))
		_cell(line["name"] if owner.is_empty() else "%s (%s)" % [line["name"], owner], 0,
				TEXT_COLOUR if counts else OFF_COLOUR)
		_cell("%s  (%d/%d)" % [line["label"], int(line["level"]) + 1, line["levels"]], 1,
				TEXT_COLOUR if counts else OFF_COLOUR)
		_cell("top level" if at_top else "next: %s" % line["next_label"], 2,
				(DONE_COLOUR if at_top else TEXT_COLOUR) if counts else OFF_COLOUR)
		var button := TouchButton.new()
		button.text = euros(int(line["next_cost_eur"]))
		button.font_size = BUY_FONT_SIZE
		button.fires_on_release = true
		button.custom_minimum_size = BUY_SIZE
		button.visible = not at_top
		button.disabled = not can_buy or int(line["next_cost_eur"]) > money
		var index := i
		button.tapped.connect(func() -> void: buy_requested.emit(index))
		# In a slot of its own: a grid leaves hidden children out, so a
		# hidden button (a part at its top level) would shift every cell
		# after it one column along.
		var slot := Control.new()
		slot.custom_minimum_size = BUY_SIZE
		button.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		slot.add_child(button)
		_table.add_child(slot)
		buy_buttons.append(button)
	if not message.is_empty():
		_status.text = message
	elif not can_buy:
		_status.text = "The tractor stays as it is until this event is over."
	elif garage.get("fully_upgraded", false):
		_status.text = "Fully upgraded: as far as the class allows."
	else:
		_status.text = "Events pay by place. Every part stays within the class's rules."


# A row per choice: its name, then a button per alternative.
func _show_choices(choices: Array, money: int, can_buy: bool) -> void:
	for child in _choices.get_children():
		_choices.remove_child(child)
		child.queue_free()
	choice_buttons.clear()
	for c in choices.size():
		var choice: Dictionary = choices[c]
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", COLUMN_GAP)
		row.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_choices.add_child(row)
		var title := Label.new()
		title.text = choice["name"]
		title.custom_minimum_size = Vector2(COLUMN_WIDTHS[0], CHOICE_SIZE.y)
		title.add_theme_font_size_override("font_size", ROW_SIZE)
		title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		row.add_child(title)
		var buttons: Array[TouchButton] = []
		var alternatives: Array = choice["alternatives"]
		for a in alternatives.size():
			var alternative: Dictionary = alternatives[a]
			var fitted := int(choice["fitted"]) == a
			var owned: bool = alternative["owned"]
			var button := TouchButton.new()
			button.text = alternative_text(alternative, fitted)
			button.font_size = CHOICE_FONT_SIZE
			button.fires_on_release = true
			button.custom_minimum_size = CHOICE_SIZE
			button.selected = fitted
			# The fitted one stays readable: tapped, it is fitted again.
			button.disabled = not can_buy or (not owned and int(alternative["cost_eur"]) > money)
			var at := Vector2i(c, a)
			button.tapped.connect(func() -> void: choice_requested.emit(at.x, at.y))
			row.add_child(button)
			buttons.append(button)
		choice_buttons.append(buttons)


# What an alternative's button says: fitted, owned (to fit) or its price.
static func alternative_text(alternative: Dictionary, fitted: bool) -> String:
	var label := String(alternative["label"])
	if fitted:
		return "✓ %s" % label
	if alternative["owned"]:
		return "Fit: %s" % label
	return "%s  %s" % [label, euros(int(alternative["cost_eur"]))]


# Whole euros, thousands apart: "€ 1,250".
static func euros(amount: int) -> String:
	var digits := str(absi(amount))
	var grouped := ""
	while digits.length() > 3:
		grouped = "," + digits.right(3) + grouped
		digits = digits.left(digits.length() - 3)
	return "%s€ %s%s" % ["-" if amount < 0 else "", digits, grouped]


func _cell(text: String, column: int, colour: Color = TEXT_COLOUR) -> void:
	var cell := Label.new()
	cell.text = text
	cell.custom_minimum_size = Vector2(COLUMN_WIDTHS[column], BUY_SIZE.y)
	cell.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	cell.add_theme_font_size_override("font_size", ROW_SIZE)
	cell.add_theme_color_override("font_color", colour)
	cell.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_table.add_child(cell)
