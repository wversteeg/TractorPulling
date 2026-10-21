# The driver's controls on a phone, for either layout. Both have a steering
# slider under the left thumb (it springs back to straight ahead), a hand
# throttle under the right (moved like a lever, it stays where it is left,
# as a tractor's does) and gear buttons beside it. The pro layout adds a
# clutch slider beside the throttle (it stays where it is left too, so a
# thumb can slip the clutch in and leave it) and a brake slider for each
# rear side above the steering (they spring back to off). Camera, restart
# and layout buttons sit in the top corner. All of them are multi-touch, so
# both thumbs drive at once.
#
# For development the keyboard (W throttle, A and D steer, the digits for a
# gear, held to the gearbox, Space presses the clutch, Q and E brake left
# and right, C camera, R restart, L layout) and a gamepad (right trigger
# throttle, left trigger clutch, left stick steer, shoulder buttons gear,
# X neutral, Y camera, Back restart) drive the same controls. The run turns
# them into the tractor's input: the steering's dead zone and rate are the
# core's (data/controls/easy.toml and pro.toml).
class_name DriveControls
extends Control

signal camera_requested
signal restart_requested
signal layout_requested

# Sizes and margins in the 1280×720 base layout; the window stretches it.
const MARGIN := 48.0
const GAP := 24.0
const SLIDER_THICKNESS := 130.0
const STEERING_LENGTH := 480.0
const THROTTLE_LENGTH := 440.0
const BRAKE_SIZE := Vector2(110.0, 300.0)
const BUTTON_SIZE := 130.0
const SMALL_BUTTON := Vector2(120.0, 80.0)
# The keyboard's steering: about half lock (past the core's dead zone, and
# under the share of the travel that turns the wheels).
const KEY_STEERING := 0.3
const GAMEPAD := 0

var gear := 0
# The gearbox's forward gears; the gear buttons stop at the top one.
var gear_count := 8
# The pro layout's controls are shown and read; else the easy layout's.
var pro := false

var steering_slider := TouchSlider.new()
var throttle_slider := TouchSlider.new()
var clutch_slider := TouchSlider.new()
var brake_left_slider := TouchSlider.new()
var brake_right_slider := TouchSlider.new()
var gear_up := TouchButton.new()
var gear_down := TouchButton.new()
var neutral := TouchButton.new()
var camera_button := TouchButton.new()
var restart_button := TouchButton.new()
var layout_button := TouchButton.new()


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
	throttle_slider.relative = true
	_place(throttle_slider, Vector2(1.0, 1.0), _right_column(0, SLIDER_THICKNESS, THROTTLE_LENGTH))

	# The clutch pedal: pressed (1) at the top, released (in) at the bottom;
	# it starts pressed, so a new pull does not lurch away.
	clutch_slider.vertical = true
	clutch_slider.relative = true
	clutch_slider.rest_value = 1.0
	_place(clutch_slider, Vector2(1.0, 1.0), _right_column(1, SLIDER_THICKNESS, THROTTLE_LENGTH))

	# The brakes: off at the bottom, full at the top, off again when let go.
	var brake_top := -MARGIN - SLIDER_THICKNESS - GAP - BRAKE_SIZE.y
	for slider: TouchSlider in [brake_left_slider, brake_right_slider]:
		slider.vertical = true
		slider.springs_back = true
	_place(brake_left_slider, Vector2(0.0, 1.0), Rect2(Vector2(MARGIN, brake_top), BRAKE_SIZE))
	_place(brake_right_slider, Vector2(0.0, 1.0),
			Rect2(Vector2(MARGIN + BRAKE_SIZE.x + GAP, brake_top), BRAKE_SIZE))

	gear_down.text = "-"
	gear_up.text = "+"
	neutral.text = "N"
	for button: TouchButton in [gear_down, gear_up, neutral]:
		add_child(button)

	camera_button.text = "cam"
	_place(camera_button, Vector2(1.0, 0.0), _top_row(0))
	restart_button.text = "again"
	restart_button.fires_on_release = true
	_place(restart_button, Vector2(1.0, 0.0), _top_row(1))
	layout_button.fires_on_release = true
	_place(layout_button, Vector2(1.0, 0.0), _top_row(2))

	gear_up.tapped.connect(func() -> void: set_gear(gear + 1))
	gear_down.tapped.connect(func() -> void: set_gear(gear - 1))
	neutral.tapped.connect(func() -> void: set_gear(0))
	camera_button.tapped.connect(func() -> void: camera_requested.emit())
	restart_button.tapped.connect(func() -> void: restart_requested.emit())
	layout_button.tapped.connect(func() -> void: layout_requested.emit())
	set_layout(pro)


# Shows the layout's controls: the pro layout's clutch and brakes, with the
# gear buttons beside the clutch, or the easy layout's without them.
func set_layout(is_pro: bool) -> void:
	pro = is_pro
	for slider: TouchSlider in [clutch_slider, brake_left_slider, brake_right_slider]:
		slider.visible = pro
		slider.release()
	layout_button.text = "pro" if pro else "easy"
	var column := 2 if pro else 1
	_anchor(gear_down, Vector2(1.0, 1.0), _right_column(column, BUTTON_SIZE, BUTTON_SIZE))
	_anchor(gear_up, Vector2(1.0, 1.0),
			_right_column(column, BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE + GAP))
	_anchor(neutral, Vector2(1.0, 1.0),
			_right_column(column, BUTTON_SIZE, BUTTON_SIZE, 2.0 * (BUTTON_SIZE + GAP)))


func set_gear(wanted: int) -> void:
	gear = clampi(wanted, 0, gear_count)


# Back to the start: neutral, throttle closed, clutch pressed, nothing held.
func reset() -> void:
	gear = 0
	throttle_slider.value = 0.0
	clutch_slider.value = clutch_slider.rest_value
	for slider: TouchSlider in [steering_slider, throttle_slider, clutch_slider,
			brake_left_slider, brake_right_slider]:
		slider.release()
	for button: TouchButton in [gear_up, gear_down, neutral, camera_button, restart_button,
			layout_button]:
		button.release()


# 0 to 1: the hand throttle, or the W key or the gamepad's trigger if
# they open it further.
func throttle() -> float:
	var keys := 1.0 if Input.is_physical_key_pressed(KEY_W) else 0.0
	var trigger := Input.get_joy_axis(GAMEPAD, JOY_AXIS_TRIGGER_RIGHT)
	return maxf(throttle_slider.value, maxf(keys, trigger))


# 0 released to 1 pressed: the clutch slider, or Space or the gamepad's
# left trigger if they press it further. The easy layout's run ignores it.
func clutch() -> float:
	var keys := 1.0 if Input.is_physical_key_pressed(KEY_SPACE) else 0.0
	var trigger := Input.get_joy_axis(GAMEPAD, JOY_AXIS_TRIGGER_LEFT)
	return maxf(clutch_slider.value, maxf(keys, trigger))


# 0 to 1 each: the brake sliders, or Q and E. The easy layout's run ignores
# them.
func brake_left() -> float:
	return maxf(brake_left_slider.value, 1.0 if Input.is_physical_key_pressed(KEY_Q) else 0.0)


func brake_right() -> float:
	return maxf(brake_right_slider.value, 1.0 if Input.is_physical_key_pressed(KEY_E) else 0.0)


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
		elif key.keycode == KEY_L:
			layout_requested.emit()
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


# A place in the columns from the right edge (0 the rightmost), `size_x`
# wide and `height` tall, standing `lift` above the bottom margin.
func _right_column(column: int, size_x: float, height: float, lift: float = 0.0) -> Rect2:
	var right := -MARGIN - column * (SLIDER_THICKNESS + GAP)
	return Rect2(right - size_x, -MARGIN - lift - height, size_x, height)


# A place in the row of small buttons along the top, from the right edge.
func _top_row(index: int) -> Rect2:
	var right := -MARGIN - index * (SMALL_BUTTON.x + GAP)
	return Rect2(right - SMALL_BUTTON.x, GAP, SMALL_BUTTON.x, SMALL_BUTTON.y)


func _place(control: Control, anchor: Vector2, rect: Rect2) -> void:
	_anchor(control, anchor, rect)
	add_child(control)


# Anchors `control` to a point of the screen (0 to 1 each way) at `rect`
# from it.
func _anchor(control: Control, anchor: Vector2, rect: Rect2) -> void:
	control.anchor_left = anchor.x
	control.anchor_right = anchor.x
	control.anchor_top = anchor.y
	control.anchor_bottom = anchor.y
	control.offset_left = rect.position.x
	control.offset_top = rect.position.y
	control.offset_right = rect.end.x
	control.offset_bottom = rect.end.y
