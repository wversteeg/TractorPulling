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
# "+ € 1000" (for testing, for now) puts that much in the bank. The build
# (BuildInfo) shows small at the panel's bottom edge, so a tester can say
# which one they play.
class_name SettingsScreen
extends MenuPanel

signal changed(pro: bool, tilt: bool, guidance: bool)
signal sound_changed(on: bool)
signal haptics_requested
signal done
signal new_career_requested(rung: int)
signal money_requested(amount_eur: int)

# Laid out as the pit board: the title with Done beside it, a card of the
# driving settings and one of sound and feel, each a row with its name, a
# short note and a pair of buttons, the hint under them and the career's
# strip at the bottom [px].
const SETTINGS_SIZE := Vector2(1180.0, 664.0)
const TITLE_SIZE := 34
const NAME_SIZE := 24
const NOTE_SIZE := 15
const HINT_SIZE := 18
const BUILD_SIZE := 15
const SETTING_FONT_SIZE := 24
const DONE_RECT := Rect2(928.0, 4.0, 180.0, 64.0)
const DRIVING_RECT := Rect2(0.0, 88.0, 640.0, 318.0)
const FEEL_RECT := Rect2(660.0, 88.0, 448.0, 318.0)
const HINT_RECT := Rect2(0.0, 414.0, 1108.0, 52.0)
const CAREER_RECT := Rect2(0.0, 476.0, 1108.0, 112.0)
const CARD_PADDING := 24.0
const CARD_HEADING_M := 46.0
const ROW_HEIGHT := 88.0
const PAIR_BUTTON := Vector2(120.0, 58.0)
const PAIR_GAP := 8.0
const CAREER_BUTTON := Vector2(220.0, 64.0)
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
# The build, in the panel's bottom padding, below the last row.
var build_label := Label.new()
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
	var layout := Control.new()
	layout.custom_minimum_size = panel_size - 2.0 * Vector2(PADDING, PADDING)
	layout.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_item(layout)
	_text(layout, "GAME", Rect2(0.0, 0.0, 600.0, 22.0), MenuPanel.EYEBROW_SIZE, GarageArt.ORANGE)
	_text(layout, "Settings", Rect2(0.0, 20.0, 600.0, 48.0), TITLE_SIZE, GarageArt.TEXT)
	done_button = _button(layout, "Done", DONE_RECT)
	done_button.primary = true
	var driving := _card(layout, DRIVING_RECT, "Driving")
	var pair := _row(driving, 0, "Controls", "Easy works the clutch for you",
			"Easy", "Pro")
	easy_button = pair[0]
	pro_button = pair[1]
	pair = _row(driving, 1, "Steering", "A slider, or tilt the phone",
			"Slider", "Tilt")
	slider_button = pair[0]
	tilt_button = pair[1]
	pair = _row(driving, 2, "Guidance", "What to do next, on screen", "On", "Off")
	guidance_on_button = pair[0]
	guidance_off_button = pair[1]
	var feel := _card(layout, FEEL_RECT, "Sound and feel")
	pair = _row(feel, 0, "Engine", "Heard on the track", "On", "Off")
	sound_on_button = pair[0]
	sound_off_button = pair[1]
	_row_text(feel, 1, "Haptics", "Vibrations in the pull")
	var inner := FEEL_RECT.size.x - 2.0 * CARD_PADDING
	haptics_button = _button(feel, "Choose  ›", Rect2(
			Vector2(CARD_PADDING + inner - 2.0 * PAIR_BUTTON.x - PAIR_GAP, _row_y(1)),
			Vector2(2.0 * PAIR_BUTTON.x + PAIR_GAP, PAIR_BUTTON.y)))
	_hint = _text(layout, "", HINT_RECT, HINT_SIZE, GarageArt.DIM)
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hint.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	var career := _card(layout, CAREER_RECT, "Career")
	var y := (CAREER_RECT.size.y - CAREER_BUTTON.y) / 2.0
	var x := CAREER_RECT.size.x - CARD_PADDING - 3.0 * CAREER_BUTTON.x - 2.0 * PAIR_GAP * 2.0
	new_career_button = _button(career, "New career", Rect2(Vector2(x, y), CAREER_BUTTON), "new")
	x += CAREER_BUTTON.x + 2.0 * PAIR_GAP
	start_button = _button(career, "Start: 1", Rect2(Vector2(x, y), CAREER_BUTTON))
	x += CAREER_BUTTON.x + 2.0 * PAIR_GAP
	money_button = _button(career, "+ € %d" % TEST_MONEY_EUR, Rect2(Vector2(x, y), CAREER_BUTTON))
	_text(career, "Start over. For testing: the class to start in, money",
			Rect2(CARD_PADDING, CARD_HEADING_M - 4.0, 330.0, 50.0), NOTE_SIZE, GarageArt.DIM) \
			.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
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
	build_label.text = BuildInfo.describe()
	build_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	build_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	build_label.add_theme_font_size_override("font_size", BUILD_SIZE)
	build_label.add_theme_color_override("font_color", GarageArt.DIM)
	build_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	build_label.set_anchors_preset(Control.PRESET_CENTER)
	build_label.offset_left = -panel_size.x / 2.0
	build_label.offset_right = panel_size.x / 2.0
	build_label.offset_top = panel_size.y / 2.0 - PADDING
	build_label.offset_bottom = panel_size.y / 2.0
	add_child(build_label)


# The buttons are laid out by hand, not in the panel's rows.
func release_buttons() -> void:
	for button: TouchButton in [easy_button, pro_button, slider_button, tilt_button,
			guidance_on_button, guidance_off_button, sound_on_button, sound_off_button,
			haptics_button, new_career_button, start_button, money_button, done_button]:
		button.release()


# A card with its heading, at `rect` in `parent`.
func _card(parent: Control, rect: Rect2, heading: String) -> Panel:
	var card := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = GarageArt.PANEL_LIGHT
	style.border_color = GarageArt.LINE
	style.set_border_width_all(1)
	style.set_corner_radius_all(12)
	card.add_theme_stylebox_override("panel", style)
	card.position = rect.position
	card.size = rect.size
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(card)
	_text(card, heading.to_upper(), Rect2(CARD_PADDING, 14.0, rect.size.x, 22.0),
			MenuPanel.HEADING_SIZE, GarageArt.ORANGE)
	return card


func _row_y(row: int) -> float:
	return CARD_HEADING_M + row * ROW_HEIGHT


# A row's name and note, on the left of the card.
func _row_text(card: Panel, row: int, name: String, note: String) -> void:
	var y := _row_y(row)
	_text(card, name, Rect2(CARD_PADDING, y - 2.0, 300.0, 32.0), NAME_SIZE, GarageArt.TEXT)
	var width := card.size.x - 2.0 * CARD_PADDING - 2.0 * PAIR_BUTTON.x - PAIR_GAP - 16.0
	var label := _text(card, note, Rect2(CARD_PADDING, y + 30.0, width, 40.0), NOTE_SIZE,
			GarageArt.DIM)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART


# A row with a pair of buttons on the right: returns them.
func _row(card: Panel, row: int, name: String, note: String, first: String,
		second: String) -> Array[TouchButton]:
	_row_text(card, row, name, note)
	var right := card.size.x - CARD_PADDING
	var y := _row_y(row)
	var a := _button(card, first, Rect2(Vector2(right - 2.0 * PAIR_BUTTON.x - PAIR_GAP, y),
			PAIR_BUTTON))
	var b := _button(card, second, Rect2(Vector2(right - PAIR_BUTTON.x, y), PAIR_BUTTON))
	return [a, b]


func _text(parent: Control, text: String, rect: Rect2, font_size: int, colour: Color) -> Label:
	var label := Label.new()
	label.text = text
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", colour)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label


func _button(parent: Control, text: String, rect: Rect2, icon: String = "") -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.icon = icon
	button.rounded = true
	button.font_size = SETTING_FONT_SIZE
	button.fires_on_release = true
	button.position = rect.position
	button.size = rect.size
	parent.add_child(button)
	return button


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
