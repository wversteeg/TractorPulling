# Over the pull, when the sled stopped, or the tractor went out of bounds,
# short of the restart line (30 m: TractorPullRun.may_restart()): the player chooses to pull again from the
# start line on the track as it is, to pull again later (at an event: six
# places further down the running order), or to let the pull count.
class_name RestartScreen
extends MenuPanel

signal restart_requested
signal move_back_requested
signal finish_requested

const RESTART_SIZE := Vector2(900.0, 360.0)
const TITLE_SIZE := 34
const LINE_SIZE := 24
const CHOICE_BUTTON := Vector2(250.0, 90.0)

var restart_button: TouchButton
var move_back_button: TouchButton
var finish_button: TouchButton
var _title: Label
var _line: Label


func _init() -> void:
	panel_size = RESTART_SIZE


func _ready() -> void:
	super()
	_title = add_title("", TITLE_SIZE, "Pull stopped")
	_line = add_line(LINE_SIZE, GarageArt.DIM)
	var row := add_button_row()
	restart_button = add_button(row, "Restart", "restart")
	restart_button.primary = true
	move_back_button = add_button(row, "")
	finish_button = add_button(row, "Finish")
	for button: TouchButton in [restart_button, move_back_button, finish_button]:
		button.custom_minimum_size = CHOICE_BUTTON
	restart_button.tapped.connect(func() -> void: restart_requested.emit())
	move_back_button.tapped.connect(func() -> void: move_back_requested.emit())
	finish_button.tapped.connect(func() -> void: finish_requested.emit())


# `ending` says why the pull stopped (ResultScreen's texts), at
# `distance_m`, short of `restart_m`; `places` how far back the player may
# move (0: not at all, as in practice).
func show_choice(ending: String, distance_m: float, restart_m: float, places: int) -> void:
	_title.text = "%s at %.2f m" % [ending, distance_m]
	_line.text = "Before the %.0f m line" % restart_m
	move_back_button.text = "%d places later" % places
	move_back_button.visible = places > 0
