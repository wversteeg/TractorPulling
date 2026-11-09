# The settings, from the gear on the start screen: the control layout
# (easy, with the clutch assist, or pro, every pedal on its own) and the
# steering (the on-screen slider, or tilting the phone) and the guidance
# (the HUD's line on what to do next in a turn, on or off) and the sound
# (the engine heard, or not); "haptics" opens the Haptics menu
# (HapticsScreen). Each tap applies
# at once; the game keeps them on the device (GameSettings). "New career"
# starts over, the garage's start money and nothing bought, on a second
# tap: the first asks to be sure. "Start" (for testing a class) picks the
# rung of the career's ladder, a class at a level, the new career starts on.
# "+ € 1000" (for testing, for now) puts that much in the bank.
class_name SettingsScreen
extends MenuPanel

signal changed(pro: bool, tilt: bool, guidance: bool)
signal sound_changed(on: bool)
signal haptics_requested
signal done
signal new_career_requested(rung: int)
signal money_requested(amount_eur: int)

# Wide enough for New career and three buttons beside it.
const SETTINGS_SIZE := Vector2(960.0, 680.0)
const TITLE_SIZE := 36
const LINE_SIZE := 26
const HINT_SIZE := 20
# Lower than the other screens' buttons: four rows fit the screen's height.
const SETTING_BUTTON_SIZE := Vector2(200.0, 70.0)
# Narrower, so the last row's four buttons fit the panel.
const LAST_ROW_BUTTON_WIDTH := 160.0
# Narrower, so the guidance, sound and haptics row's five fit.
const FEEDBACK_BUTTON_WIDTH := 160.0
# What "+ € 1000" adds to the bank [EUR].
const TEST_MONEY_EUR := 1000

var easy_button: TouchButton
var pro_button: TouchButton
var slider_button: TouchButton
var tilt_button: TouchButton
var guidance_on_button: TouchButton
var guidance_off_button: TouchButton
var sound_on_button: TouchButton
var sound_off_button: TouchButton
var haptics_button: TouchButton
var new_career_button: TouchButton
var start_button: TouchButton
var money_button: TouchButton
var done_button: TouchButton
var _hint: Label
var _pro := false
var _tilt := false
var _guidance := true
var _sound := true
var _tilt_available := true
# The first tap on "New career" made: the next one starts it.
var _career_asked := false
# The career's rungs ("class · level", TractorPullEvent.get_career()) and
# the one a new career starts on.
var _rungs := PackedStringArray()
var _start_rung := 0
# The last tap was on "Start": the hint names the rung.
var _start_shown := false
# The bank after the last tap on "+ € 1000", or -1: the hint shows it.
var _money_shown_eur := -1


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
	# The guidance, the sound and the haptics share a row: the panel keeps
	# its height.
	add_line(LINE_SIZE).text = "Guidance  ·  Sound  ·  Haptics"
	var guidance := add_button_row()
	guidance_on_button = add_button(guidance, "guide on")
	guidance_off_button = add_button(guidance, "guide off")
	sound_on_button = add_button(guidance, "sound on")
	sound_off_button = add_button(guidance, "sound off")
	haptics_button = add_button(guidance, "haptics")
	_hint = add_line(HINT_SIZE)
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	# New career beside Done: the panel keeps its height.
	var go := add_button_row()
	new_career_button = add_button(go, "New career")
	new_career_button.custom_minimum_size = StartScreen.CHOICE_BUTTON_SIZE
	start_button = add_button(go, "Start: 1")
	money_button = add_button(go, "+ € %d" % TEST_MONEY_EUR)
	done_button = add_button(go, "Done")
	for button: TouchButton in [easy_button, pro_button, slider_button, tilt_button,
			guidance_on_button, guidance_off_button, sound_on_button, sound_off_button,
			start_button, money_button, done_button]:
		button.custom_minimum_size = SETTING_BUTTON_SIZE
	for button: TouchButton in [start_button, money_button, done_button]:
		button.custom_minimum_size.x = LAST_ROW_BUTTON_WIDTH
	haptics_button.custom_minimum_size = SETTING_BUTTON_SIZE
	for button: TouchButton in [guidance_on_button, guidance_off_button, sound_on_button,
			sound_off_button, haptics_button]:
		button.custom_minimum_size.x = FEEDBACK_BUTTON_WIDTH
	new_career_button.custom_minimum_size.y = SETTING_BUTTON_SIZE.y
	easy_button.tapped.connect(func() -> void: _pick(false, _tilt, _guidance))
	pro_button.tapped.connect(func() -> void: _pick(true, _tilt, _guidance))
	slider_button.tapped.connect(func() -> void: _pick(_pro, false, _guidance))
	tilt_button.tapped.connect(func() -> void: _pick(_pro, true, _guidance))
	guidance_on_button.tapped.connect(func() -> void: _pick(_pro, _tilt, true))
	guidance_off_button.tapped.connect(func() -> void: _pick(_pro, _tilt, false))
	sound_on_button.tapped.connect(func() -> void: _pick_sound(true))
	sound_off_button.tapped.connect(func() -> void: _pick_sound(false))
	haptics_button.tapped.connect(func() -> void: haptics_requested.emit())
	done_button.tapped.connect(func() -> void: done.emit())
	new_career_button.tapped.connect(_ask_new_career)
	start_button.tapped.connect(_next_start)
	money_button.tapped.connect(func() -> void: money_requested.emit(TEST_MONEY_EUR))


# Shows the settings as they are; `tilt_available` whether the device can
# steer by tilting; `rungs` the career's rungs, lowest first.
func show_settings(pro: bool, tilt: bool, tilt_available: bool, guidance: bool = true,
		rungs := PackedStringArray(), sound: bool = true) -> void:
	_sound = sound
	_pro = pro
	_tilt = tilt
	_guidance = guidance
	_tilt_available = tilt_available
	_rungs = rungs
	_start_rung = clampi(_start_rung, 0, maxi(rungs.size() - 1, 0))
	_career_asked = false
	_start_shown = false
	_money_shown_eur = -1
	_show()


# The bank after "+ € 1000" added to it, `money_eur`.
func show_money(money_eur: int) -> void:
	_career_asked = false
	_start_shown = false
	_money_shown_eur = money_eur
	_show()


# The rung a new career starts on, from "Start".
func start_rung() -> int:
	return _start_rung


# The next rung up, round to the first after the top.
func _next_start() -> void:
	_start_rung = 0 if _rungs.is_empty() else (_start_rung + 1) % _rungs.size()
	_career_asked = false
	_start_shown = true
	_money_shown_eur = -1
	_show()


func _pick(pro: bool, tilt: bool, guidance: bool) -> void:
	_pro = pro
	_tilt = tilt
	_guidance = guidance
	_career_asked = false
	_start_shown = false
	_money_shown_eur = -1
	_show()
	changed.emit(pro, tilt, guidance)


func _pick_sound(on: bool) -> void:
	_sound = on
	_career_asked = false
	_start_shown = false
	_money_shown_eur = -1
	_show()
	sound_changed.emit(on)


func _show() -> void:
	easy_button.selected = not _pro
	pro_button.selected = _pro
	slider_button.selected = not _tilt
	tilt_button.selected = _tilt
	guidance_on_button.selected = _guidance
	guidance_off_button.selected = not _guidance
	sound_on_button.selected = _sound
	sound_off_button.selected = not _sound
	new_career_button.text = "Sure? Tap again" if _career_asked else "New career"
	start_button.text = "Start: %d" % (_start_rung + 1)
	var start_at := _rungs[_start_rung] if _start_rung < _rungs.size() else ""
	if _career_asked:
		_hint.text = "A new career starts over: the garage's start money, nothing bought%s." \
				% ("" if _start_rung == 0 else ", at " + start_at)
		return
	if _start_shown:
		_hint.text = "For testing: a new career starts at %s." % start_at
		return
	if _money_shown_eur >= 0:
		_hint.text = "For testing: %s added. Money: %s." % [GarageScreen.euros(TEST_MONEY_EUR),
				GarageScreen.euros(_money_shown_eur)]
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
		new_career_requested.emit(_start_rung)
		return
	_career_asked = true
	_start_shown = false
	_money_shown_eur = -1
	_show()
