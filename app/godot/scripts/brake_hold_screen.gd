# The settings' Brake hold menu: how the HOLD button works at the start of a
# pull on a pulling tractor ("optoeren", DriveControls). "Auto" lets the
# brakes go once the boost reaches the share the slider sets (50 to 80 % of
# full boost); "By hand" leaves them held until HOLD is tapped again. Where
# the button sits is one of DriveControls.HOLD_PLACES. Each change applies
# at once (changed), and the game keeps it on the device (GameSettings);
# "Back" returns to the settings.
class_name BrakeHoldScreen
extends MenuPanel

signal changed(auto_release: bool, release_boost: float, place: int)
signal done

const HOLD_PANEL_SIZE := Vector2(960.0, 640.0)
const TITLE_SIZE := 36
const HINT_SIZE := 20
const LINE_SIZE := 22
const PAIR_BUTTON_SIZE := Vector2(260.0, 70.0)
const PLACE_BUTTON_SIZE := Vector2(270.0, 70.0)
const PLACE_FONT_SIZE := 21
const SLIDER_SIZE := Vector2(560.0, 90.0)
const BOOST_STEP := 0.05

var auto_button: TouchButton
var hand_button: TouchButton
var boost_slider := TouchSlider.new()
var place_buttons: Array[TouchButton] = []
var back_button: TouchButton
var _boost_line: Label
var _auto := true
var _boost := 0.75
var _place := 1


func _init() -> void:
	panel_size = HOLD_PANEL_SIZE


func _ready() -> void:
	super()
	add_title("Brake hold", TITLE_SIZE, "Settings")
	var hint := add_line(HINT_SIZE, GarageArt.DIM)
	hint.text = "On a pulling tractor, HOLD holds both brakes at the start while the boost builds."
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_heading("Let the brakes go")
	var row := add_button_row()
	auto_button = add_button(row, "Auto")
	hand_button = add_button(row, "By hand")
	for button: TouchButton in [auto_button, hand_button]:
		button.custom_minimum_size = PAIR_BUTTON_SIZE
	_boost_line = add_line(LINE_SIZE)
	boost_slider.start_value = GameSettings.HOLD_BOOST_MIN
	boost_slider.end_value = GameSettings.HOLD_BOOST_MAX
	boost_slider.colour = GarageArt.ORANGE
	boost_slider.filled = true
	boost_slider.custom_minimum_size = SLIDER_SIZE
	boost_slider.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	add_item(boost_slider)
	add_heading("The button")
	var places := add_button_row()
	for i in DriveControls.HOLD_PLACES.size():
		var button := add_button(places, DriveControls.HOLD_PLACES[i].capitalize())
		button.custom_minimum_size = PLACE_BUTTON_SIZE
		button.font_size = PLACE_FONT_SIZE
		button.tapped.connect(func() -> void:
			_place = i
			_changed())
		place_buttons.append(button)
	var last := add_button_row()
	back_button = add_button(last, "Back")
	back_button.primary = true
	auto_button.tapped.connect(func() -> void:
		_auto = true
		_changed())
	hand_button.tapped.connect(func() -> void:
		_auto = false
		_changed())
	back_button.tapped.connect(func() -> void: done.emit())
	_show()


# Shows the settings as they are.
func show_hold(auto_release: bool, release_boost: float, place: int) -> void:
	_auto = auto_release
	_boost = release_boost
	_place = place
	boost_slider.value = _boost
	_show()


func _process(_delta: float) -> void:
	if not visible or not boost_slider.is_held():
		return
	# In whole steps of 5 %.
	var stepped := snappedf(boost_slider.value, BOOST_STEP)
	if not is_equal_approx(stepped, _boost):
		_boost = stepped
		_changed()


func release_buttons() -> void:
	super()
	boost_slider.release()


func _changed() -> void:
	_show()
	changed.emit(_auto, _boost, _place)


func _show() -> void:
	auto_button.selected = _auto
	hand_button.selected = not _auto
	boost_slider.visible = _auto
	_boost_line.text = "Let go at %d %% of full boost" % roundi(_boost * 100.0) if _auto \
			else "Held until you tap HOLD again"
	for i in place_buttons.size():
		place_buttons[i].selected = i == _place
	for button: TouchButton in [auto_button, hand_button] + place_buttons:
		button.queue_redraw()
	boost_slider.queue_redraw()
