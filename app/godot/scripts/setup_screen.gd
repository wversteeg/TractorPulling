# Just before a pull (or a practice pull): how the tractor is set up, and
# a line naming the controls (picked in the settings). The weights slide from the rear wheels to the front
# bracket (more on the rear axle grips harder but lifts the nose sooner),
# and the rear tires' pressure from soft (a bigger footprint for soft
# tracks) to hard (lugs that bite on firm ones). TractorPullEvent keeps the
# setting for the season; the run takes it from there.
class_name SetupScreen
extends MenuPanel

signal tuning_changed(ballast_position: float, pressure_bar: float)
signal pull_requested
signal back_requested

const SETUP_SIZE := Vector2(900.0, 640.0)
const TITLE_SIZE := 36
const LINE_SIZE := 26
const SLIDER_HEIGHT := 64.0
# The pressure moves in steps of this much [bar].
const PRESSURE_STEP_BAR := 0.05
const WEIGHT_COLOUR := Color(0.85, 0.6, 0.2, 0.9)
const TIRE_COLOUR := Color(0.6, 0.6, 0.65, 0.9)

var ballast_slider := TouchSlider.new()
var pressure_slider := TouchSlider.new()
var pull_button: TouchButton
var back_button: TouchButton
var _title: Label
var _weights: Label
var _pressure: Label
var _controls: Label
var _sent := Vector2(-1.0, -1.0)


func _init() -> void:
	panel_size = SETUP_SIZE


func _ready() -> void:
	super()
	_title = add_line(TITLE_SIZE)
	_weights = add_line(LINE_SIZE)
	_add_slider(ballast_slider, "REAR  ·  WEIGHTS  ·  FRONT", WEIGHT_COLOUR)
	_pressure = add_line(LINE_SIZE)
	_add_slider(pressure_slider, "SOFT  ·  REAR TIRES  ·  HARD", TIRE_COLOUR)
	_controls = add_line(LINE_SIZE)
	var go := add_button_row()
	back_button = add_button(go, "Back")
	pull_button = add_button(go, "Pull")
	pull_button.tapped.connect(func() -> void: pull_requested.emit())
	back_button.tapped.connect(func() -> void: back_requested.emit())


func _add_slider(slider: TouchSlider, text: String, colour: Color) -> void:
	slider.label = text
	slider.colour = colour
	slider.custom_minimum_size = Vector2(0.0, SLIDER_HEIGHT)
	add_item(slider)


# `title` names the pull to come; `tuning` is TractorPullEvent.get_tuning();
# `pro` and `tilt` the controls that will drive (`tilt` only where the
# phone can steer by tilting).
func show_setup(title: String, tuning: Dictionary, pro: bool, tilt: bool) -> void:
	_title.text = title
	ballast_slider.visible = tuning.get("ballast", false)
	pressure_slider.visible = tuning.get("pressure", false)
	pressure_slider.start_value = tuning.get("min_bar", 0.0)
	pressure_slider.end_value = tuning.get("max_bar", 1.0)
	ballast_slider.value = tuning.get("ballast_position", 0.0)
	pressure_slider.value = tuning.get("pressure_bar", 0.0)
	_sent = Vector2(ballast_slider.value, pressure_slider.value)
	show_values(tuning)
	_controls.text = "Controls: %s, %s steering" % ["pro" if pro else "easy",
			"tilt" if tilt else "slider"]


# The settings in words, from TractorPullEvent.get_tuning().
func show_values(tuning: Dictionary) -> void:
	_weights.text = "Weights: %.0f %% on the rear axle" % (float(tuning.get("rear_share", 0.0)) * 100.0) \
			if tuning.get("ballast", false) else "No weights to move"
	_pressure.text = "Rear tires: %.2f bar" % float(tuning.get("pressure_bar", 0.0)) \
			if tuning.get("pressure", false) else "The tire pressure is fixed"


func _process(_delta: float) -> void:
	if not visible:
		return
	var bar := snappedf(pressure_slider.value, PRESSURE_STEP_BAR)
	var now := Vector2(ballast_slider.value, bar)
	if now != _sent:
		_sent = now
		tuning_changed.emit(now.x, now.y)


func release_sliders() -> void:
	ballast_slider.release()
	pressure_slider.release()
