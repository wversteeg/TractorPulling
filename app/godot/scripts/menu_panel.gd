# A centred panel of lines and touch buttons, the base of the game's
# screens: the screens fill in the text and say which buttons show. In the
# garage's look (GarageArt): a dark rounded card with a thin rim, a small
# orange line over the title, section headings in small capitals and the
# buttons as rounded cards.
class_name MenuPanel
extends Control

const PANEL_SIZE := Vector2(720.0, 470.0)
const CORNER := 16
const SHADOW_SIZE := 18
const SHADOW := Color(0.0, 0.0, 0.0, 0.45)
const EYEBROW_SIZE := 16
const HEADING_SIZE := 15
const PADDING := 36.0
const BUTTON_SIZE := Vector2(200.0, 90.0)
const BUTTON_FONT_SIZE := 30
const GAP := 24.0

# Set before the panel enters the tree.
var panel_size := PANEL_SIZE
# The small orange line over the title (add_title()).
var eyebrow_label: Label = null
var _box := VBoxContainer.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var panel := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = GarageArt.PANEL
	style.border_color = GarageArt.LINE
	style.set_border_width_all(1)
	style.set_corner_radius_all(CORNER)
	style.shadow_size = SHADOW_SIZE
	style.shadow_color = SHADOW
	panel.add_theme_stylebox_override("panel", style)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.offset_left = -panel_size.x / 2.0
	panel.offset_right = panel_size.x / 2.0
	panel.offset_top = -panel_size.y / 2.0
	panel.offset_bottom = panel_size.y / 2.0
	add_child(panel)
	_box.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT, Control.PRESET_MODE_MINSIZE,
			int(PADDING))
	_box.add_theme_constant_override("separation", int(GAP / 2.0))
	_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.add_child(_box)


# A line of text, `font_size` high, in `colour`.
func add_line(font_size: int, colour: Color = GarageArt.TEXT) -> Label:
	var label := Label.new()
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", colour)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_box.add_child(label)
	return label


# The screen's title, `font_size` high, under a small orange line
# (`eyebrow`, in capitals) that says where the player is; returns the
# title, and the eyebrow's label is eyebrow_label.
func add_title(title: String, font_size: int, eyebrow: String = "") -> Label:
	eyebrow_label = add_line(EYEBROW_SIZE, GarageArt.ORANGE)
	eyebrow_label.text = eyebrow.to_upper()
	eyebrow_label.visible = not eyebrow.is_empty()
	var label := add_line(font_size)
	label.text = title
	return label


# A section's heading: small capitals, dimmed.
func add_heading(text: String) -> Label:
	var label := add_line(HEADING_SIZE, GarageArt.DIM)
	label.text = text.to_upper()
	return label


# Any control, in the panel's column.
func add_item(item: Control) -> void:
	_box.add_child(item)


# A row of buttons; add_button() fills the last one.
func add_button_row() -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", int(GAP))
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_box.add_child(row)
	return row


func add_button(row: HBoxContainer, text: String, icon: String = "") -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.icon = icon
	button.rounded = true
	button.font_size = BUTTON_FONT_SIZE
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
