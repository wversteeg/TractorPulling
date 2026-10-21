# After a pull: how far the sled went and how the pull ended, then the next
# pull the rules give (the run decides: a pull-off after a full pull in
# qualifying), or on to the next event.
class_name ResultScreen
extends MenuPanel

signal pull_off_requested
signal next_requested

const LINE_SIZE := 28
const DISTANCE_SIZE := 64
const KMH_PER_M_S := 3.6
# How each ending reads; the run names them (TractorPullRun.get_result()).
const ENDINGS := {
	"full_pull": "Full pull!",
	"stalled": "The engine stalled",
	"spun_out": "The wheels spun out",
	"stopped": "The sled stopped",
	"timed_out": "Time is up",
	"out_of_bounds": "Out of bounds: a tire touched the chalk line",
}

var pull_off_button: TouchButton
var next_button: TouchButton
var _pull: Label
var _distance: Label
var _ending: Label
var _speed: Label


func _ready() -> void:
	super()
	_pull = add_line(LINE_SIZE)
	_distance = add_line(DISTANCE_SIZE)
	_ending = add_line(LINE_SIZE)
	_speed = add_line(LINE_SIZE)
	var row := add_button_row()
	pull_off_button = add_button(row, "Pull-off")
	next_button = add_button(row, "Next")
	pull_off_button.tapped.connect(func() -> void: pull_off_requested.emit())
	next_button.tapped.connect(func() -> void: next_requested.emit())


# `event` and `result` are the run's (get_event(), get_result()); `next_round`
# its get_next_round(): a pull-off to pull, or -1 for the next event.
func show_result(event: Dictionary, result: Dictionary, next_round: int) -> void:
	var pull_off_round: int = event.get("round", 0)
	var which := "Qualifying" if pull_off_round == 0 else "Pull-off %d" % pull_off_round
	_pull.text = "%s  ·  %s" % [which, event.get("name", "")]
	_distance.text = "%.2f m" % float(result["distance_m"])
	_ending.text = ENDINGS.get(result["ending"], str(result["ending"]))
	_speed.text = "%.1f s, top speed %.1f km/h" % [float(result["time_s"]),
			float(result["top_speed_m_s"]) * KMH_PER_M_S]
	pull_off_button.visible = next_round > 0
	next_button.visible = next_round <= 0
