# A slider for one finger of a multi-touch screen: the finger that lands on
# it holds it, wherever it slides, until it lifts, so other fingers work the
# other controls at the same time. Godot's own sliders follow the one
# emulated mouse, so two thumbs could not drive at once.
#
# `value` runs from `start_value` at the left (or bottom) end to
# `end_value` at the right (or top). With `springs_back` it returns to
# `rest_value` when the finger lifts, like a steering wheel let go;
# without, it stays where it was left, like a hand throttle. A `relative`
# slider is grabbed where its thumb is: a touch does not move it, only the
# finger's slide does, as a lever is moved, so a tap to nudge the throttle
# does not throw it open. Otherwise it jumps to where the finger lands.
#
# Each control has its own `colour` and `label`, so a thumb finds the
# throttle, the clutch and the brakes apart at a glance.
class_name TouchSlider
extends Control

const TRACK_COLOUR := Color(0.1, 0.1, 0.1, 0.45)
const HELD_COLOUR := Color(1.0, 0.95, 0.75, 0.95)
const LABEL_COLOUR := Color(0.95, 0.95, 0.95, 0.9)
const THUMB_SHARE := 0.22  # of the slider's length
const LABEL_SIZE := 22
# The track behind the thumb, filled to where it is in the control's colour
# at this strength, so how far a lever is open shows from afar.
const FILL_ALPHA := 0.35

@export var colour := Color(0.95, 0.95, 0.95, 0.85)
@export var label := ""
# The track filled from the start end up to the thumb (a lever's travel).
@export var filled := false

@export var vertical := false
@export var start_value := 0.0
@export var end_value := 1.0
@export var rest_value := 0.0
@export var springs_back := false
@export var relative := false

var value := 0.0
# The finger holding the slider, -1 for none.
var _finger := -1
# Where the finger landed along the slider (0 to 1) and the value then, for
# a relative slide.
var _grab_share := 0.0
var _grab_value := 0.0


func _ready() -> void:
	value = rest_value
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func is_held() -> bool:
	return _finger >= 0


# Touches come in through _input, not _gui_input: each finger is followed
# by its index, whichever control it is over now.
func _input(event: InputEvent) -> void:
	if not is_visible_in_tree():
		return
	var touch := event as InputEventScreenTouch
	if touch != null:
		if touch.pressed and _finger < 0 and get_global_rect().has_point(touch.position):
			_finger = touch.index
			_grab_share = _share_at(touch.position)
			_grab_value = value
			if not relative:
				_slide_to(touch.position)
			queue_redraw()
			get_viewport().set_input_as_handled()
		elif not touch.pressed and touch.index == _finger:
			release()
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


# How far along the slider a point is: 0 at the start end, 1 at the other,
# beyond them off the ends.
func _share_at(global_point: Vector2) -> float:
	var local := global_point - global_position
	if vertical:
		return 1.0 - local.y / size.y
	return local.x / size.x


func _slide_to(global_point: Vector2) -> void:
	var share := _share_at(global_point)
	if relative:
		share += inverse_lerp(start_value, end_value, _grab_value) - _grab_share
	value = lerpf(start_value, end_value, clampf(share, 0.0, 1.0))
	queue_redraw()


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), TRACK_COLOUR)
	var share := inverse_lerp(start_value, end_value, value)
	var thumb_colour := HELD_COLOUR if is_held() else colour
	var fill := Color(colour, FILL_ALPHA)
	if vertical:
		var thumb := size.y * THUMB_SHARE
		var y := (1.0 - share) * (size.y - thumb)
		if filled:
			draw_rect(Rect2(0.0, y, size.x, size.y - y), fill)
		draw_rect(Rect2(0.0, y, size.x, thumb), thumb_colour)
	else:
		var thumb := size.x * THUMB_SHARE
		var x := share * (size.x - thumb)
		if filled:
			draw_rect(Rect2(0.0, 0.0, x + thumb, size.y), fill)
		draw_rect(Rect2(x, 0.0, thumb, size.y), thumb_colour)
	_draw_label()


# The label across the track's far end (the top of a vertical slider), in
# a dark outline so it reads over the thumb too.
func _draw_label() -> void:
	if label.is_empty():
		return
	var font := get_theme_default_font()
	var text_size := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE)
	var at := Vector2((size.x - text_size.x) / 2.0, font.get_ascent(LABEL_SIZE) + 6.0)
	if not vertical:
		at.y = (size.y + font.get_ascent(LABEL_SIZE) - font.get_descent(LABEL_SIZE)) / 2.0
	draw_string_outline(font, at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE, 6,
			Color.BLACK)
	draw_string(font, at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE, LABEL_COLOUR)
