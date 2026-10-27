# The settings, from the gear on the start screen: the control layout
# (easy, with the clutch assist, or pro, every pedal on its own) and the
# steering (the on-screen slider, or tilting the phone). Each tap applies
# at once; the game keeps them on the device (GameSettings). "New career"
# starts over, the garage's start money and nothing bought, on a second
# tap: the first asks to be sure.
class_name SettingsScreen
extends MenuPanel

signal changed(pro: bool, tilt: bool)
signal done
signal new_career_requested

const SETTINGS_SIZE := Vector2(820.0, 700.0)
const TITLE_SIZE := 36
const LINE_SIZE := 26
const HINT_SIZE := 20

var easy_button: TouchButton
var pro_button: TouchButton
var slider_button: TouchButton
var tilt_button: TouchButton
var new_career_button: TouchButton
var done_button: TouchButton
var _hint: Label
var _pro := false
var _tilt := false
var _tilt_available := true
# The first tap on "New career" made: the next one starts it.
var _career_asked := false


func _init() -> void:
	panel_size = SETTINGS_SIZE


func _ready() -> void:
	super()
	add_line(TITLE_SIZE).text = "Settings"
	add_line(LINE_SIZE).text = "Controls"
	var layouts := add_button_row()
	easy_button = add_button(layouts, "easy")
	pro_button = add_button(layouts, "pro")
	add_line(LINE_SIZE).text = "Steering"
	var steering := add_button_row()
	slider_button = add_button(steering, "slider")
	tilt_button = add_button(steering, "tilt")
	_hint = add_line(HINT_SIZE)
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var career := add_button_row()
	new_career_button = add_button(career, "New career")
	new_career_button.custom_minimum_size = StartScreen.CHOICE_BUTTON_SIZE
	var go := add_button_row()
	done_button = add_button(go, "Done")
	easy_button.tapped.connect(func() -> void: _pick(false, _tilt))
	pro_button.tapped.connect(func() -> void: _pick(true, _tilt))
	slider_button.tapped.connect(func() -> void: _pick(_pro, false))
	tilt_button.tapped.connect(func() -> void: _pick(_pro, true))
	done_button.tapped.connect(func() -> void: done.emit())
	new_career_button.tapped.connect(_ask_new_career)


# Shows the settings as they are; `tilt_available` whether the device can
# steer by tilting.
func show_settings(pro: bool, tilt: bool, tilt_available: bool) -> void:
	_pro = pro
	_tilt = tilt
	_tilt_available = tilt_available
	_career_asked = false
	_show()


func _pick(pro: bool, tilt: bool) -> void:
	_pro = pro
	_tilt = tilt
	_career_asked = false
	_show()
	changed.emit(pro, tilt)


func _show() -> void:
	easy_button.selected = not _pro
	pro_button.selected = _pro
	slider_button.selected = not _tilt
	tilt_button.selected = _tilt
	new_career_button.text = "Sure? Tap again" if _career_asked else "New career"
	if _career_asked:
		_hint.text = "A new career starts over: the garage's start money, nothing bought."
		return
	if _tilt:
		_hint.text = ("Hold the phone like a steering wheel. However you hold it when you "
				+ "tap Pull is straight ahead.") if _tilt_available \
				else "This device has no motion sensor: the slider steers."
	else:
		_hint.text = "Steer with the slider under your left thumb."


# The first tap asks to be sure; the second starts the new career.
func _ask_new_career() -> void:
	if _career_asked:
		_career_asked = false
		_show()
		new_career_requested.emit()
		return
	_career_asked = true
	_show()
