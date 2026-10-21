# Before an event: the season's event to come (each on its own track), and
# "Event" to go to it, or "Practice" for a free pull on its track first
# (the setup screen comes before every pull).
class_name StartScreen
extends MenuPanel

signal practice_requested
signal event_requested

const TITLE_SIZE := 44
const LINE_SIZE := 28

var practice_button: TouchButton
var event_button: TouchButton
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
	_error = add_line(LINE_SIZE)
	practice_button.tapped.connect(func() -> void: practice_requested.emit())
	event_button.tapped.connect(func() -> void: event_requested.emit())


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
	_error.text = error
