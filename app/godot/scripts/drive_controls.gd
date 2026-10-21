# The easy layout's controls on a phone: a steering slider under the left
# thumb (it springs back to straight ahead), a hand throttle under the right
# (it stays where it is left, as a tractor's does), and gear buttons beside
# it. Camera and restart buttons sit in the top corner. All of them are
# multi-touch, so both thumbs drive at once.
#
# For development the keyboard (W throttle, A and D steer, 0 to 8 gear,
# C camera, R restart) and a gamepad (right trigger throttle, left stick
# steer, shoulder buttons gear, X neutral, Y camera, Back restart) drive the
# same controls. The run turns them into the tractor's input: the dead zone
# and the steering rate are the core's (data/controls/easy.toml).
class_name DriveControls
extends Control

signal camera_requested
signal restart_requested

# Sizes and margins in the 1280×720 base layout; the window stretches it.
const MARGIN := 48.0
const GAP := 24.0
const SLIDER_THICKNESS := 130.0
const STEERING_LENGTH := 480.0
const THROTTLE_LENGTH := 440.0
const BUTTON_SIZE := 130.0
const SMALL_BUTTON := Vector2(120.0, 80.0)
# The keyboard's steering: half the travel, the wheels at most of lock.
const KEY_STEERING := 0.3
const GAMEPAD := 0

var gear := 0
# The gearbox's forward gears; the gear buttons stop at the top one.
var gear_count := 8

var steering_slider := TouchSlider.new()
var throttle_slider := TouchSlider.new()
var gear_up := TouchButton.new()
var gear_down := TouchButton.new()
var neutral := TouchButton.new()
var camera_button := TouchButton.new()
var restart_button := TouchButton.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	# Left is +1 (the run's convention), so the slider runs 1 to -1.
	steering_slider.start_value = 1.0
	steering_slider.end_value = -1.0
	steering_slider.springs_back = true
	_place(steering_slider, Vector2(0.0, 1.0),
			Rect2(MARGIN, -MARGIN - SLIDER_THICKNESS, STEERING_LENGTH, SLIDER_THICKNESS))

	throttle_slider.vertical = true
	var throttle_left := -MARGIN - SLIDER_THICKNESS
	_place(throttle_slider, Vector2(1.0, 1.0),
			Rect2(throttle_left, -MARGIN - THROTTLE_LENGTH, SLIDER_THICKNESS, THROTTLE_LENGTH))

	var buttons_left := throttle_left - GAP - BUTTON_SIZE
	gear_down.text = "-"
	_place(gear_down, Vector2(1.0, 1.0),
			Rect2(buttons_left, -MARGIN - BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE))
	gear_up.text = "+"
	_place(gear_up, Vector2(1.0, 1.0),
			Rect2(buttons_left, -MARGIN - 2.0 * BUTTON_SIZE - GAP, BUTTON_SIZE, BUTTON_SIZE))
	neutral.text = "N"
	_place(neutral, Vector2(1.0, 1.0),
			Rect2(buttons_left, -MARGIN - 3.0 * BUTTON_SIZE - 2.0 * GAP, BUTTON_SIZE, BUTTON_SIZE))

	camera_button.text = "cam"
	_place(camera_button, Vector2(1.0, 0.0),
			Rect2(-MARGIN - SMALL_BUTTON.x, GAP, SMALL_BUTTON.x, SMALL_BUTTON.y))
	restart_button.text = "again"
	_place(restart_button, Vector2(1.0, 0.0), Rect2(-MARGIN - 2.0 * SMALL_BUTTON.x - GAP, GAP,
			SMALL_BUTTON.x, SMALL_BUTTON.y))

	gear_up.tapped.connect(func() -> void: set_gear(gear + 1))
	gear_down.tapped.connect(func() -> void: set_gear(gear - 1))
	neutral.tapped.connect(func() -> void: set_gear(0))
	camera_button.tapped.connect(func() -> void: camera_requested.emit())
	restart_button.tapped.connect(func() -> void: restart_requested.emit())


func set_gear(wanted: int) -> void:
	gear = clampi(wanted, 0, gear_count)


# Back to the start: neutral, throttle closed, nothing held.
func reset() -> void:
	gear = 0
	throttle_slider.value = 0.0
	throttle_slider.release()
	steering_slider.release()


# 0 to 1: the hand throttle, or the W key or the gamepad's trigger if
# they open it further.
func throttle() -> float:
	var keys := 1.0 if Input.is_physical_key_pressed(KEY_W) else 0.0
	var trigger := Input.get_joy_axis(GAMEPAD, JOY_AXIS_TRIGGER_RIGHT)
	return maxf(throttle_slider.value, maxf(keys, trigger))


# -1 (full right) to 1 (full left): the keys while held, else the slider
# while a thumb holds it, else the gamepad's stick.
func steering() -> float:
	var keys := 0.0
	if Input.is_physical_key_pressed(KEY_A):
		keys += KEY_STEERING
	if Input.is_physical_key_pressed(KEY_D):
		keys -= KEY_STEERING
	if keys != 0.0:
		return keys
	if steering_slider.is_held():
		return steering_slider.value
	return -Input.get_joy_axis(GAMEPAD, JOY_AXIS_LEFT_X)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key != null and key.pressed and not key.echo:
		if key.keycode == KEY_C:
			camera_requested.emit()
		elif key.keycode == KEY_R:
			restart_requested.emit()
		elif key.keycode >= KEY_0 and key.keycode <= KEY_9:
			set_gear(key.keycode - KEY_0)
		return
	var button := event as InputEventJoypadButton
	if button != null and button.pressed:
		match button.button_index:
			JOY_BUTTON_RIGHT_SHOULDER:
				set_gear(gear + 1)
			JOY_BUTTON_LEFT_SHOULDER:
				set_gear(gear - 1)
			JOY_BUTTON_X:
				set_gear(0)
			JOY_BUTTON_Y:
				camera_requested.emit()
			JOY_BUTTON_BACK:
				restart_requested.emit()


# Anchors `control` to a point of the screen (0 to 1 each way) at `rect`
# from it.
func _place(control: Control, anchor: Vector2, rect: Rect2) -> void:
	control.anchor_left = anchor.x
	control.anchor_right = anchor.x
	control.anchor_top = anchor.y
	control.anchor_bottom = anchor.y
	control.offset_left = rect.position.x
	control.offset_top = rect.position.y
	control.offset_right = rect.end.x
	control.offset_bottom = rect.end.y
	add_child(control)
