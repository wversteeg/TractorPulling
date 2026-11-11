# The settings' Haptics menu: which vibrations the phone gives (Haptics),
# each a button that is lit while it is on; a tap switches it. "All on"
# and "All off" set them all; "Back" returns to the settings. Each tap
# applies at once, and the game keeps them on the device (GameSettings).
class_name HapticsScreen
extends MenuPanel

signal changed(by_key: Dictionary)
signal done

const HAPTICS_SIZE := Vector2(960.0, 640.0)
const TITLE_SIZE := 36
const HINT_SIZE := 20
const KIND_BUTTON_SIZE := Vector2(420.0, 70.0)
const ROW_BUTTON_SIZE := Vector2(200.0, 70.0)
const PER_ROW := 2

var all_on_button: TouchButton
var all_off_button: TouchButton
var back_button: TouchButton
# The kinds' buttons, by Haptics.Kind.
var kind_buttons := {}
var _on := {}


func _init() -> void:
	panel_size = HAPTICS_SIZE


func _ready() -> void:
	super()
	add_title("Haptics", TITLE_SIZE, "Settings")
	var hint := add_line(HINT_SIZE, GarageArt.DIM)
	hint.text = "Lit: the phone vibrates for it. Tap to switch."
	var row: HBoxContainer = null
	for kind: int in Haptics.Kind.values():
		if kind % PER_ROW == 0:
			row = add_button_row()
		var button := add_button(row, Haptics.LABELS[kind])
		button.custom_minimum_size = KIND_BUTTON_SIZE
		button.tapped.connect(func() -> void: _switch(kind))
		kind_buttons[kind] = button
		_on[kind] = true
	var last := add_button_row()
	all_on_button = add_button(last, "All on")
	all_off_button = add_button(last, "All off")
	back_button = add_button(last, "Back")
	back_button.primary = true
	for button: TouchButton in [all_on_button, all_off_button, back_button]:
		button.custom_minimum_size = ROW_BUTTON_SIZE
	all_on_button.tapped.connect(func() -> void: _set_all(true))
	all_off_button.tapped.connect(func() -> void: _set_all(false))
	back_button.tapped.connect(func() -> void: done.emit())
	_show()


# Shows the kinds as they are: Haptics.enabled_by_key()'s dictionary.
func show_haptics(by_key: Dictionary) -> void:
	for kind: int in Haptics.Kind.values():
		_on[kind] = bool(by_key.get(Haptics.KEYS[kind], true))
	_show()


func _switch(kind: int) -> void:
	_on[kind] = not _on[kind]
	_changed()


func _set_all(on: bool) -> void:
	for kind: int in Haptics.Kind.values():
		_on[kind] = on
	_changed()


func _changed() -> void:
	_show()
	var by_key := {}
	for kind: int in Haptics.Kind.values():
		by_key[Haptics.KEYS[kind]] = _on[kind]
	changed.emit(by_key)


func _show() -> void:
	for kind: int in kind_buttons:
		var button: TouchButton = kind_buttons[kind]
		button.selected = _on[kind]
		button.queue_redraw()
