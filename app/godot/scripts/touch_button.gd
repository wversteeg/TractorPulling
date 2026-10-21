# A button for one finger of a multi-touch screen, so a thumb can tap it
# while the other holds a slider. It fires as the finger lands, for a quick
# gear change, or with `fires_on_release` only when the finger lifts on
# it, so a stray touch that slides off does nothing (restarting a pull).
class_name TouchButton
extends Control

signal tapped

const COLOUR := Color(0.1, 0.1, 0.1, 0.45)
const HELD_COLOUR := Color(1.0, 0.8, 0.2, 0.8)
const TEXT_COLOUR := Color(0.95, 0.95, 0.95)
const FONT_SIZE := 36

@export var text := ""
@export var fires_on_release := false

var _finger := -1


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _input(event: InputEvent) -> void:
	if not is_visible_in_tree():
		return
	var touch := event as InputEventScreenTouch
	if touch == null:
		return
	if touch.pressed and _finger < 0 and get_global_rect().has_point(touch.position):
		_finger = touch.index
		queue_redraw()
		get_viewport().set_input_as_handled()
		if not fires_on_release:
			tapped.emit()
	elif not touch.pressed and touch.index == _finger:
		release()
		get_viewport().set_input_as_handled()
		if fires_on_release and get_global_rect().has_point(touch.position):
			tapped.emit()


# Lets go of a held finger without a release, as when the app loses focus:
# otherwise the button would wait for that finger and ignore new ones.
func release() -> void:
	_finger = -1
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT:
		release()


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), HELD_COLOUR if _finger >= 0 else COLOUR)
	var font := get_theme_default_font()
	var line := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE)
	var at := Vector2((size.x - line.x) / 2.0, (size.y + font.get_ascent(FONT_SIZE)
			- font.get_descent(FONT_SIZE)) / 2.0)
	draw_string(font, at, text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, TEXT_COLOUR)
