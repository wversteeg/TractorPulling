# Before an event: the season's event to come (each on its own track), the
# control layout to pull in, and "Event" to go to it, or "Practice" for a
# free pull on its track first.
class_name StartScreen
extends MenuPanel

signal layout_picked(pro: bool)
signal practice_requested
signal event_requested

const TITLE_SIZE := 44
const LINE_SIZE := 28

var easy_button: TouchButton
var pro_button: TouchButton
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
	var layouts := add_button_row()
	easy_button = add_button(layouts, "easy")
	pro_button = add_button(layouts, "pro")
	var go := add_button_row()
	practice_button = add_button(go, "Practice")
	event_button = add_button(go, "Event")
	_error = add_line(LINE_SIZE)
	easy_button.tapped.connect(func() -> void: layout_picked.emit(false))
	pro_button.tapped.connect(func() -> void: layout_picked.emit(true))
	practice_button.tapped.connect(func() -> void: practice_requested.emit())
	event_button.tapped.connect(func() -> void: event_requested.emit())


# `event` is TractorPullEvent.get_event() for the event to come; `error`
# what went wrong setting it up, if anything.
func show_event(event: Dictionary, pro: bool, error: String) -> void:
	if event.is_empty():
		_event.text = ""
		_name.text = ""
	else:
		_event.text = "Event %d of %d" % [int(event["index"]) + 1, event["count"]]
		_name.text = event["name"]
	easy_button.selected = not pro
	pro_button.selected = pro
	practice_button.visible = error.is_empty()
	event_button.visible = error.is_empty()
	_error.text = error
