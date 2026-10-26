# The garage, from the start screen: the money the player has won and each
# part of the tractor they can improve (TractorPullEvent.get_garage()),
# with what it is now, what the next level is and what it costs. "Buy"
# buys it; the game asks the extension, which holds the money and the
# class's rules. During an event, once the player has pulled, the tractor
# stays as it is.
class_name GarageScreen
extends MenuPanel

signal buy_requested(index: int)
signal done

const GARAGE_SIZE := Vector2(1180.0, 660.0)
const TITLE_SIZE := 36
const LINE_SIZE := 24
const ROW_SIZE := 22
const COLUMN_GAP := 24
const BUY_SIZE := Vector2(130.0, 64.0)
const TEXT_COLOUR := Color(0.95, 0.95, 0.95)
const DONE_COLOUR := Color(0.6, 0.85, 0.6)

var done_button: TouchButton
# One per upgrade, in the garage's order.
var buy_buttons: Array[TouchButton] = []
var _money: Label
var _table := GridContainer.new()
var _status: Label


func _init() -> void:
	panel_size = GARAGE_SIZE


func _ready() -> void:
	super()
	add_line(TITLE_SIZE).text = "Garage"
	_money = add_line(LINE_SIZE)
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
	var upgrades: Array = garage.get("upgrades", [])
	for i in upgrades.size():
		var line: Dictionary = upgrades[i]
		var at_top: bool = String(line["next_label"]).is_empty()
		_cell(line["name"])
		_cell("%s  (%d/%d)" % [line["label"], int(line["level"]) + 1, line["levels"]])
		_cell("top level" if at_top
				else "%s: %s" % [line["next_label"], euros(int(line["next_cost_eur"]))],
				DONE_COLOUR if at_top else TEXT_COLOUR)
		var button := TouchButton.new()
		button.text = "Buy"
		button.fires_on_release = true
		button.custom_minimum_size = BUY_SIZE
		button.visible = can_buy and not at_top
		button.disabled = int(line["next_cost_eur"]) > money
		var index := i
		button.tapped.connect(func() -> void: buy_requested.emit(index))
		_table.add_child(button)
		buy_buttons.append(button)
	if not message.is_empty():
		_status.text = message
	elif not can_buy:
		_status.text = "The tractor stays as it is until this event is over."
	elif garage.get("fully_upgraded", false):
		_status.text = "Fully upgraded: as far as the class allows."
	else:
		_status.text = "Events pay by place. Every part stays within the class's rules."


# Whole euros, thousands apart: "€ 1,250".
static func euros(amount: int) -> String:
	var digits := str(absi(amount))
	var grouped := ""
	while digits.length() > 3:
		grouped = "," + digits.right(3) + grouped
		digits = digits.left(digits.length() - 3)
	return "%s€ %s%s" % ["-" if amount < 0 else "", digits, grouped]


func _cell(text: String, colour: Color = TEXT_COLOUR) -> void:
	var cell := Label.new()
	cell.text = text
	cell.add_theme_font_size_override("font_size", ROW_SIZE)
	cell.add_theme_color_override("font_color", colour)
	cell.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_table.add_child(cell)
