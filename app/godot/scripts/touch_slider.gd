# A slider for one finger of a multi-touch screen: the finger that lands on
# it holds it, wherever it slides, until it lifts, so other fingers work the
# other controls at the same time. Godot's own sliders follow the one
# emulated mouse, so two thumbs could not drive at once.
#
# `value` runs from `start_value` at the left (or bottom) end to
# `end_value` at the right (or top). With `springs_back` it returns to
# `rest_value` when the finger lifts, like a steering wheel let go;
# without, it stays where it was left, like a hand throttle.
class_name TouchSlider
extends Control

const TRACK_COLOUR := Color(0.1, 0.1, 0.1, 0.45)
const THUMB_COLOUR := Color(0.95, 0.95, 0.95, 0.85)
const HELD_COLOUR := Color(1.0, 0.8, 0.2, 0.95)
const THUMB_SHARE := 0.22  # of the slider's length

@export var vertical := false
@export var start_value := 0.0
@export var end_value := 1.0
@export var rest_value := 0.0
@export var springs_back := false

var value := 0.0
# The finger holding the slider, -1 for none.
var _finger := -1


func _ready() -> void:
	value = rest_value
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func is_held() -> bool:
	return _finger >= 0


# Touches come in through _input, not _gui_input: each finger is followed
# by its index, whichever control it is over now.
func _input(event: InputEvent) -> void:
	var touch := event as InputEventScreenTouch
	if touch != null:
		if touch.pressed and _finger < 0 and get_global_rect().has_point(touch.position):
			_finger = touch.index
			_slide_to(touch.position)
			get_viewport().set_input_as_handled()
		elif not touch.pressed and touch.index == _finger:
			_finger = -1
			if springs_back:
				value = rest_value
			queue_redraw()
			get_viewport().set_input_as_handled()
		return
	var drag := event as InputEventScreenDrag
	if drag != null and drag.index == _finger:
		_slide_to(drag.position)
		get_viewport().set_input_as_handled()


# Lets go of a held finger without a release, as when the app loses focus.
func release() -> void:
	_finger = -1
	if springs_back:
		value = rest_value
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT:
		release()


func _slide_to(global_point: Vector2) -> void:
	var local := global_point - global_position
	var share: float
	if vertical:
		share = 1.0 - local.y / size.y
	else:
		share = local.x / size.x
	value = lerpf(start_value, end_value, clampf(share, 0.0, 1.0))
	queue_redraw()


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), TRACK_COLOUR)
	var share := inverse_lerp(start_value, end_value, value)
	var colour := HELD_COLOUR if is_held() else THUMB_COLOUR
	if vertical:
		var thumb := size.y * THUMB_SHARE
		var y := (1.0 - share) * (size.y - thumb)
		draw_rect(Rect2(0.0, y, size.x, thumb), colour)
	else:
		var thumb := size.x * THUMB_SHARE
		var x := share * (size.x - thumb)
		draw_rect(Rect2(x, 0.0, thumb, size.y), colour)
