# A centred panel of lines and touch buttons, the base of the start and
# result screens: the screens fill in the text and say which buttons show.
class_name MenuPanel
extends Control

const PANEL_SIZE := Vector2(720.0, 470.0)
const PANEL_COLOUR := Color(0.08, 0.08, 0.1, 0.8)
const PADDING := 36.0
const BUTTON_SIZE := Vector2(200.0, 90.0)
const GAP := 24.0
const OUTLINE := 4

var _box := VBoxContainer.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var panel := ColorRect.new()
	panel.color = PANEL_COLOUR
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.offset_left = -PANEL_SIZE.x / 2.0
	panel.offset_right = PANEL_SIZE.x / 2.0
	panel.offset_top = -PANEL_SIZE.y / 2.0
	panel.offset_bottom = PANEL_SIZE.y / 2.0
	add_child(panel)
	_box.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT, Control.PRESET_MODE_MINSIZE,
			int(PADDING))
	_box.add_theme_constant_override("separation", int(GAP / 2.0))
	_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.add_child(_box)


# A line of text, `font_size` high.
func add_line(font_size: int) -> Label:
	var label := Label.new()
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", OUTLINE)
	_box.add_child(label)
	return label


# A row of buttons; add_button() fills the last one.
func add_button_row() -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", int(GAP))
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_box.add_child(row)
	return row


func add_button(row: HBoxContainer, text: String) -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.fires_on_release = true
	button.custom_minimum_size = BUTTON_SIZE
	row.add_child(button)
	return button


# Lets go of any finger on the buttons, as when the screen is hidden.
func release_buttons() -> void:
	for row in _box.get_children():
		for button in row.get_children():
			if button is TouchButton:
				(button as TouchButton).release()
