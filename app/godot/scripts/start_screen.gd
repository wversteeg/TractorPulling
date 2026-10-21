# Before a pull: the event the player pulls next (the season's events in
# turn, each on its own track), the control layout to pull in, and "Next"
# to start.
class_name StartScreen
extends MenuPanel

signal layout_picked(pro: bool)
signal start_requested

const TITLE_SIZE := 44
const LINE_SIZE := 28

var easy_button: TouchButton
var pro_button: TouchButton
var next_button: TouchButton
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
	next_button = add_button(add_button_row(), "Next")
	_error = add_line(LINE_SIZE)
	easy_button.tapped.connect(func() -> void: layout_picked.emit(false))
	pro_button.tapped.connect(func() -> void: layout_picked.emit(true))
	next_button.tapped.connect(func() -> void: start_requested.emit())


# `event` is TractorPullRun.get_event() for the pull to come; `error` what
# went wrong setting it up, if anything.
func show_event(event: Dictionary, pro: bool, error: String) -> void:
	if event.is_empty():
		_event.text = ""
		_name.text = ""
	else:
		_event.text = "Event %d of %d" % [int(event["index"]) + 1, event["count"]]
		_name.text = event["name"]
	easy_button.selected = not pro
	pro_button.selected = pro
	next_button.visible = error.is_empty()
	_error.text = error
