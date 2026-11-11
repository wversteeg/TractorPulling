# A button for one finger of a multi-touch screen, so a thumb can tap it
# while the other holds a slider. It fires as the finger lands, for a quick
# gear change, or with `fires_on_release` only when the finger lifts on
# it, so a stray touch that slides off does nothing (restarting a pull).
class_name TouchButton
extends Control

signal tapped

const COLOUR := Color(0.1, 0.1, 0.1, 0.45)
const HELD_COLOUR := Color(1.0, 0.8, 0.2, 0.8)
const SELECTED_COLOUR := Color(0.2, 0.45, 0.8, 0.85)
const TEXT_COLOUR := Color(0.95, 0.95, 0.95)
const DISABLED_TEXT_COLOUR := Color(0.55, 0.55, 0.55)
const FONT_SIZE := 36
# An icon button's key: its corners' radius (a share of its smaller side)
# and a thin light rim.
const KEY_CORNER_SHARE := 0.18
const KEY_BORDER := 3
const KEY_BORDER_COLOUR := Color(1.0, 1.0, 1.0, 0.35)
# The menus' cards: corner radius, and the text on an orange card.
const CARD_CORNER := 12
const CARD_BORDER := 2
const PRIMARY_TEXT_COLOUR := Color(0.13, 0.08, 0.02)
# An icon's size, a share of the font's, and the gap to the text.
const ICON_SHARE := 1.05
const ICON_GAP := 14.0

@export var text := ""
@export var font_size := FONT_SIZE
@export var fires_on_release := false
# Shown picked, as one of a set of choices (the layout in the settings).
@export var selected := false:
	set(value):
		selected = value
		queue_redraw()
# The menus' look (MenuPanel): a rounded card with a thin rim, picked in
# blue, held in orange; `primary` fills it orange, the screen's main
# action; `icon` (GarageArt.draw_icon()'s names) stands before the text.
@export var rounded := false:
	set(value):
		rounded = value
		queue_redraw()
@export var primary := false:
	set(value):
		primary = value
		queue_redraw()
@export var icon := "":
	set(value):
		icon = value
		queue_redraw()
# Shows something, not to be tapped, drawn as a button (the gear in).
@export var readout := false:
	set(value):
		readout = value
		release()
# Shown, but not to be tapped (an upgrade the player cannot afford).
@export var disabled := false:
	set(value):
		disabled = value
		release()

# Whether a finger holds the button, and which: any index, negative too
# (iPhone Safari's, as TouchSlider explains).
var _held := false
var _finger := 0
# The rounded key icon buttons draw (draw_key), made once.
var _key := StyleBoxFlat.new()


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _input(event: InputEvent) -> void:
	if not is_visible_in_tree():
		return
	var touch := event as InputEventScreenTouch
	if touch == null:
		return
	if touch.pressed and not _held and not disabled and not readout \
			and get_global_rect().has_point(touch.position):
		_held = true
		_finger = touch.index
		queue_redraw()
		get_viewport().set_input_as_handled()
		if not fires_on_release:
			tapped.emit()
	elif not touch.pressed and _held and touch.index == _finger:
		release()
		get_viewport().set_input_as_handled()
		if fires_on_release and get_global_rect().has_point(touch.position):
			tapped.emit()


func is_held() -> bool:
	return _held


# Lets go of a held finger without a release, as when the app loses focus:
# otherwise the button would wait for that finger and ignore new ones.
func release() -> void:
	_held = false
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT:
		release()


func _draw() -> void:
	var text_colour := DISABLED_TEXT_COLOUR if disabled else TEXT_COLOUR
	if rounded:
		text_colour = draw_card()
	else:
		var colour := COLOUR
		if _held:
			colour = HELD_COLOUR
		elif selected:
			colour = SELECTED_COLOUR
		draw_rect(Rect2(Vector2.ZERO, size), colour)
	var font := get_theme_default_font()
	var line := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
	var icon_size := font_size * ICON_SHARE if not icon.is_empty() else 0.0
	var gap := ICON_GAP if not icon.is_empty() and not text.is_empty() else 0.0
	var left := (size.x - line.x - icon_size - gap) / 2.0
	if not icon.is_empty():
		GarageArt.draw_icon(self, icon, Vector2(left + icon_size / 2.0, size.y / 2.0), icon_size,
				text_colour)
	var at := Vector2(left + icon_size + gap, (size.y + font.get_ascent(font_size)
			- font.get_descent(font_size)) / 2.0)
	draw_string(font, at, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, text_colour)


# The menus' card behind the text (GearButton's too); returns the text's
# colour on it.
func draw_card() -> Color:
	var fill := GarageArt.PANEL_LIGHT
	var rim := GarageArt.LINE
	var text_colour := GarageArt.TEXT
	if _held:
		fill = GarageArt.ORANGE_LIGHT
		rim = GarageArt.ORANGE_LIGHT
		text_colour = PRIMARY_TEXT_COLOUR
	elif primary and not disabled:
		fill = GarageArt.ORANGE
		rim = GarageArt.ORANGE_LIGHT
		text_colour = PRIMARY_TEXT_COLOUR
	elif selected:
		fill = Color(GarageArt.BLUE, 0.9)
		rim = GarageArt.BLUE_LIGHT
	if disabled:
		text_colour = DISABLED_TEXT_COLOUR
	_key.bg_color = fill
	_key.set_corner_radius_all(CARD_CORNER)
	_key.set_border_width_all(CARD_BORDER)
	_key.border_color = rim
	draw_style_box(_key, Rect2(Vector2.ZERO, size))
	return text_colour


# The rounded key behind an icon button (ShiftButton, CameraButton), in
# the button's colour: held, picked or at rest.
func draw_key() -> void:
	var colour := COLOUR
	if _held:
		colour = HELD_COLOUR
	elif selected:
		colour = SELECTED_COLOUR
	_key.bg_color = colour
	_key.set_corner_radius_all(int(minf(size.x, size.y) * KEY_CORNER_SHARE))
	_key.set_border_width_all(KEY_BORDER)
	_key.border_color = KEY_BORDER_COLOUR
	draw_style_box(_key, Rect2(Vector2.ZERO, size))


# An icon's colour: greyed while the button is disabled.
func icon_colour(colour: Color) -> Color:
	return DISABLED_TEXT_COLOUR if disabled else colour
