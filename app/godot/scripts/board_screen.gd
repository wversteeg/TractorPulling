# A panel with a table: the event as it goes (the pulls and the sled
# operator's verdicts, the pulls to beat in bold), the event's results and
# the season's standings, every puller in each. A title,
# the rows of columns (the player's highlighted) in a list that scrolls
# with a finger and follows the newest row, a status line and up to two
# buttons; the game fills them in.
class_name BoardScreen
extends MenuPanel

signal first_tapped
signal second_tapped

const BOARD_SIZE := Vector2(1000.0, 660.0)
const TITLE_SIZE := 36
const ROW_SIZE := 24
const COLUMN_GAP := 28
const TEXT_COLOUR := GarageArt.TEXT
const PLAYER_COLOUR := GarageArt.ORANGE_LIGHT
# How much heavier a bold row's strokes are (FontVariation).
const BOLD_EMBOLDEN := 1.0
# Wider than the other screens' buttons: "Next event", "Pull again".
const WIDE_BUTTON := Vector2(260.0, 90.0)

var first_button: TouchButton
var second_button: TouchButton
var _title: Label
var _table := GridContainer.new()
var _scroll := ScrollContainer.new()
var _status: Label
# The list was at its end before the rows changed: it follows the newest.
var _follow := true
var _bold := FontVariation.new()


func _init() -> void:
	panel_size = BOARD_SIZE


func _ready() -> void:
	super()
	_bold.base_font = ThemeDB.fallback_font
	_bold.variation_embolden = BOLD_EMBOLDEN
	_title = add_title("", TITLE_SIZE)
	_table.add_theme_constant_override("h_separation", COLUMN_GAP)
	_table.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_scroll.add_child(_table)
	var bar := _scroll.get_v_scroll_bar()
	# The finger scrolls: the list follows the newest rows again once it is
	# back at its end. New rows lengthen it: a list that follows goes along.
	bar.value_changed.connect(func(_value: float) -> void: _follow = _at_end())
	bar.changed.connect(func() -> void:
		if _follow:
			_scroll.scroll_vertical = int(bar.max_value))
	add_item(_scroll)
	_status = add_line(ROW_SIZE - 2, GarageArt.DIM)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var row := add_button_row()
	first_button = add_button(row, "")
	second_button = add_button(row, "")
	first_button.custom_minimum_size = WIDE_BUTTON
	first_button.primary = true
	second_button.custom_minimum_size = WIDE_BUTTON
	first_button.tapped.connect(func() -> void: first_tapped.emit())
	second_button.tapped.connect(func() -> void: second_tapped.emit())


func set_title(text: String) -> void:
	_title.text = text


# `rows`: rows of the same number of columns; the rows in `highlights` are
# the player's, and those in `strong` are bold (the pulls to beat). A list
# (`newest_last`, as the event's pulls) scrolled to its end stays there,
# showing the newest; a table (the standings) opens at its top.
func set_rows(rows: Array[PackedStringArray], highlights: PackedInt32Array = [],
		newest_last: bool = true, strong: PackedInt32Array = []) -> void:
	if not newest_last:
		_follow = false
		_scroll.scroll_vertical = 0
	for child in _table.get_children():
		_table.remove_child(child)
		child.queue_free()
	_table.columns = maxi(rows[0].size(), 1) if not rows.is_empty() else 1
	for i in rows.size():
		for text in rows[i]:
			var cell := Label.new()
			cell.text = text
			cell.add_theme_font_size_override("font_size", ROW_SIZE)
			cell.add_theme_color_override("font_color",
					PLAYER_COLOUR if highlights.has(i) else TEXT_COLOUR)
			if strong.has(i):
				cell.add_theme_font_override("font", _bold)
			_table.add_child(cell)


# The list shows its last row, for tests.
func at_end() -> bool:
	return _at_end()


func _at_end() -> bool:
	var bar := _scroll.get_v_scroll_bar()
	return bar.value + bar.page >= bar.max_value - 1.0


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
