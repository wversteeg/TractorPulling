# Before an event: the season's event to come (each on its own track), and
# "Event" to go to it, "Practice" for a free pull on its track first (the
# setup screen comes before every pull), or "Garage" to spend the prize
# money on the tractor. The gear in the top right
# corner opens the settings. At launch, with a season saved, it first asks
# whether to continue it, start a new season (the career, money and
# upgrades, carries over) or a new career (show_choice), which starts over
# on a second tap: the first asks to be sure.
class_name StartScreen
extends MenuPanel

signal practice_requested
signal event_requested
signal garage_requested
signal settings_requested
signal continue_requested
signal new_season_requested
signal new_career_requested

const TITLE_SIZE := 44
const LINE_SIZE := 28
# The gear's size and its distance from the screen's top right corner, in
# the 1280×720 base layout.
const GEAR_SIZE := 96.0
const GEAR_MARGIN := 24.0
# Wider than the panel's other buttons: "New season" fits on one line;
# the panel is wider too, for three of them in a row: 3 × 280 + 2 gaps of
# MenuPanel.GAP + 2 × MenuPanel.PADDING = 960, so a wider button needs a
# wider panel.
const CHOICE_BUTTON_SIZE := Vector2(280.0, 90.0)
const START_PANEL_SIZE := Vector2(960.0, 470.0)

var practice_button: TouchButton
var event_button: TouchButton
var garage_button: TouchButton
var continue_button: TouchButton
var new_season_button: TouchButton
var new_career_button: TouchButton
var settings_button := GearButton.new()
var _event: Label
var _name: Label
var _error: Label
# The first tap on "New career" made: the next one starts it.
var _career_asked := false


func _init() -> void:
	panel_size = START_PANEL_SIZE


func _ready() -> void:
	super()
	add_line(TITLE_SIZE).text = "Tractor Pulling"
	_event = add_line(LINE_SIZE)
	_name = add_line(LINE_SIZE)
	var go := add_button_row()
	practice_button = add_button(go, "Practice")
	garage_button = add_button(go, "Garage")
	event_button = add_button(go, "Event")
	var choice := add_button_row()
	new_career_button = add_button(choice, "New career")
	new_season_button = add_button(choice, "New season")
	continue_button = add_button(choice, "Continue")
	for button: TouchButton in [new_career_button, new_season_button, continue_button]:
		button.custom_minimum_size = CHOICE_BUTTON_SIZE
	_error = add_line(LINE_SIZE)
	practice_button.tapped.connect(func() -> void: practice_requested.emit())
	event_button.tapped.connect(func() -> void: event_requested.emit())
	garage_button.tapped.connect(func() -> void: garage_requested.emit())
	continue_button.tapped.connect(func() -> void: continue_requested.emit())
	new_season_button.tapped.connect(func() -> void: new_season_requested.emit())
	new_career_button.tapped.connect(_ask_new_career)
	settings_button.fires_on_release = true
	settings_button.anchor_left = 1.0
	settings_button.anchor_right = 1.0
	settings_button.offset_left = -GEAR_MARGIN - GEAR_SIZE
	settings_button.offset_right = -GEAR_MARGIN
	settings_button.offset_top = GEAR_MARGIN
	settings_button.offset_bottom = GEAR_MARGIN + GEAR_SIZE
	add_child(settings_button)
	settings_button.tapped.connect(func() -> void: settings_requested.emit())


func release_buttons() -> void:
	super()
	settings_button.release()


# `event` is TractorPullEvent.get_event() for the event to come; `error`
# what went wrong setting it up, if anything.
func show_event(event: Dictionary, error: String) -> void:
	if event.is_empty():
		_event.text = ""
		_name.text = ""
	else:
		_event.text = "Event %d of %d" % [int(event["index"]) + 1, event["count"]]
		_name.text = event["name"]
	practice_button.visible = error.is_empty()
	event_button.visible = error.is_empty()
	garage_button.visible = error.is_empty()
	continue_button.visible = false
	new_season_button.visible = false
	new_career_button.visible = false
	settings_button.visible = true
	_error.text = error


# A saved season to continue, or a new one: `season` is
# TractorPullEvent.get_season() of the saved season, picked up again.
func show_choice(season: Dictionary) -> void:
	_event.text = "A season saved after event %d of %d" % [season.get("scored", 0),
			season.get("events", 0)]
	_name.text = "Continue it, or start a new season or career?"
	practice_button.visible = false
	event_button.visible = false
	garage_button.visible = false
	continue_button.visible = true
	new_season_button.visible = true
	new_career_button.visible = true
	_career_asked = false
	new_career_button.text = "New career"
	# The settings wait for the choice.
	settings_button.visible = false
	_error.text = ""


# The first tap asks to be sure; the second starts the new career.
func _ask_new_career() -> void:
	if _career_asked:
		_career_asked = false
		new_career_button.text = "New career"
		new_career_requested.emit()
		return
	_career_asked = true
	new_career_button.text = "Sure? Tap again"
	_name.text = "A new career starts over: the garage's start money, nothing bought."
