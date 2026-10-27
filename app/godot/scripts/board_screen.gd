# A panel with a table: the event as it goes (the pulls and the sled
# operator's verdicts), the event's top ten and the season's. A title,
# up to ten rows of columns (the player's highlighted), a
# status line and up to two buttons; the game fills them in.
class_name BoardScreen
extends MenuPanel

signal first_tapped
signal second_tapped

const BOARD_SIZE := Vector2(1000.0, 660.0)
const TITLE_SIZE := 36
const ROW_SIZE := 24
const ROWS := 10
const COLUMN_GAP := 28
const TEXT_COLOUR := Color(0.95, 0.95, 0.95)
const PLAYER_COLOUR := Color(1.0, 0.8, 0.2)
# Wider than the other screens' buttons: "Next event", "Pull again".
const WIDE_BUTTON := Vector2(260.0, 90.0)

var first_button: TouchButton
var second_button: TouchButton
var _title: Label
var _table := GridContainer.new()
var _status: Label


func _init() -> void:
	panel_size = BOARD_SIZE


func _ready() -> void:
	super()
	_title = add_line(TITLE_SIZE)
	_table.add_theme_constant_override("h_separation", COLUMN_GAP)
	_table.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_table.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_item(_table)
	_status = add_line(ROW_SIZE)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var row := add_button_row()
	first_button = add_button(row, "")
	second_button = add_button(row, "")
	first_button.custom_minimum_size = WIDE_BUTTON
	second_button.custom_minimum_size = WIDE_BUTTON
	first_button.tapped.connect(func() -> void: first_tapped.emit())
	second_button.tapped.connect(func() -> void: second_tapped.emit())


func set_title(text: String) -> void:
	_title.text = text


# `rows`: rows of the same number of columns (the last ROWS show); the
# rows in `highlights` are the player's.
func set_rows(rows: Array[PackedStringArray], highlights: PackedInt32Array = []) -> void:
	for child in _table.get_children():
		_table.remove_child(child)
		child.queue_free()
	var first := maxi(rows.size() - ROWS, 0)
	_table.columns = maxi(rows[0].size(), 1) if not rows.is_empty() else 1
	for i in range(first, rows.size()):
		for text in rows[i]:
			var cell := Label.new()
			cell.text = text
			cell.add_theme_font_size_override("font_size", ROW_SIZE)
			cell.add_theme_color_override("font_color",
					PLAYER_COLOUR if highlights.has(i) else TEXT_COLOUR)
			_table.add_child(cell)


# The rows' cells as shown, for tests.
func row_count() -> int:
	return _table.get_child_count() / maxi(_table.columns, 1)


func set_status(text: String) -> void:
	_status.text = text


func status() -> String:
	return _status.text


# The buttons' texts; "" hides a button.
func set_buttons(first: String, second: String = "") -> void:
	first_button.text = first
	first_button.visible = not first.is_empty()
	second_button.text = second
	second_button.visible = not second.is_empty()
	first_button.queue_redraw()
	second_button.queue_redraw()
