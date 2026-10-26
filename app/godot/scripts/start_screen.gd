# Before an event: the season's event to come (each on its own track), and
# "Event" to go to it, or "Practice" for a free pull on its track first
# (the setup screen comes before every pull). The gear in the top right
# corner opens the settings. At launch, with a season saved, it first asks
# whether to continue it or start a new one (show_choice).
class_name StartScreen
extends MenuPanel

signal practice_requested
signal event_requested
signal settings_requested
signal continue_requested
signal new_season_requested

const TITLE_SIZE := 44
const LINE_SIZE := 28
# The gear's size and its distance from the screen's top right corner, in
# the 1280×720 base layout.
const GEAR_SIZE := 96.0
const GEAR_MARGIN := 24.0

var practice_button: TouchButton
var event_button: TouchButton
var continue_button: TouchButton
var new_season_button: TouchButton
var settings_button := GearButton.new()
var _event: Label
var _name: Label
var _error: Label


func _ready() -> void:
	super()
	add_line(TITLE_SIZE).text = "Tractor Pulling"
	_event = add_line(LINE_SIZE)
	_name = add_line(LINE_SIZE)
	var go := add_button_row()
	practice_button = add_button(go, "Practice")
	event_button = add_button(go, "Event")
	var choice := add_button_row()
	new_season_button = add_button(choice, "New season")
	continue_button = add_button(choice, "Continue")
	_error = add_line(LINE_SIZE)
	practice_button.tapped.connect(func() -> void: practice_requested.emit())
	event_button.tapped.connect(func() -> void: event_requested.emit())
	continue_button.tapped.connect(func() -> void: continue_requested.emit())
	new_season_button.tapped.connect(func() -> void: new_season_requested.emit())
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
	continue_button.visible = false
	new_season_button.visible = false
	settings_button.visible = true
	_error.text = error


# A saved season to continue, or a new one: `season` is
# TractorPullEvent.get_season() of the saved season, picked up again.
func show_choice(season: Dictionary) -> void:
	_event.text = "A season saved after event %d of %d" % [season.get("scored", 0),
			season.get("events", 0)]
	_name.text = "Continue it with the next event, or start a new season?"
	practice_button.visible = false
	event_button.visible = false
	continue_button.visible = true
	new_season_button.visible = true
	# The settings wait for the choice.
	settings_button.visible = false
	_error.text = ""
