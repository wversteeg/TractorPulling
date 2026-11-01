# After a pull: how far the sled went and how the pull ended. After a pull
# at the event, the sled operator's verdict and "Continue" back to the
# event; after a practice pull, "Again" or "Menu".
class_name ResultScreen
extends MenuPanel

signal again_requested
signal menu_requested
signal continue_requested

const LINE_SIZE := 28
const DISTANCE_SIZE := 64
const KMH_PER_M_S := 3.6
# How each ending reads; the run names them (TractorPullRun.get_result()).
# A run at an event goes on past the line until the sled stops, so a full
# pull there is judged by the distance.
const ENDINGS := {
	"full_pull": "Full pull!",
	"stalled": "The engine stalled",
	"spun_out": "The wheels spun out",
	"stopped": "The sled stopped",
	"timed_out": "Time is up",
	"out_of_bounds": "Out of bounds: a tire touched the chalk line",
}
# What to try on the next pull, by ending.
const TIPS := {
	"stalled": "The sled pulled the engine down: try a lower gear, or keep the revs up.",
	"spun_out": "The tires lost their grip: less throttle, a higher gear, more weight "
			+ "at the back or lower tire pressure.",
	"stopped": "The tractor stopped pulling with the engine running and the tires gripping.",
	"timed_out": "Too slow: a higher gear keeps the sled moving.",
	"out_of_bounds": "Keep the tractor between the chalk lines until the full-pull line.",
}
const RESULT_SIZE := Vector2(820.0, 580.0)
const ENDING_SIZE := 36
const TIP_SIZE := 22
const ENDING_COLOUR := Color(1.0, 0.55, 0.4)
const FULL_PULL_COLOUR := Color(0.45, 1.0, 0.5)

var again_button: TouchButton
var menu_button: TouchButton
var continue_button: TouchButton
var _pull: Label
var _distance: Label
var _ending: Label
var _tip: Label
var _speed: Label
var _verdict: Label


func _init() -> void:
	panel_size = RESULT_SIZE


func _ready() -> void:
	super()
	_pull = add_line(LINE_SIZE)
	_distance = add_line(DISTANCE_SIZE)
	_ending = add_line(ENDING_SIZE)
	_tip = add_line(TIP_SIZE)
	_tip.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_speed = add_line(LINE_SIZE)
	_verdict = add_line(LINE_SIZE)
	var row := add_button_row()
	again_button = add_button(row, "Again")
	menu_button = add_button(row, "Menu")
	continue_button = add_button(row, "Continue")
	again_button.tapped.connect(func() -> void: again_requested.emit())
	menu_button.tapped.connect(func() -> void: menu_requested.emit())
	continue_button.tapped.connect(func() -> void: continue_requested.emit())


# `title` names the pull, `result` is the run's get_result() and
# `full_pull_m` the line it was pulled to. `entry` is the event's log entry
# for the pull (TractorPullEvent.record_pull()), or {} for a practice pull.
func show_result(title: String, result: Dictionary, full_pull_m: float,
		entry: Dictionary) -> void:
	var distance_m: float = result["distance_m"]
	_pull.text = title
	_distance.text = "%.2f m" % distance_m
	var ending := ending_name(result, full_pull_m)
	_ending.text = ENDINGS.get(ending, ending)
	_ending.add_theme_color_override("font_color",
			FULL_PULL_COLOUR if ending == "full_pull" else ENDING_COLOUR)
	_tip.text = TIPS.get(ending, "")
	_speed.text = "%.1f s, top speed %.1f km/h" % [float(result["time_s"]),
			float(result["top_speed_m_s"]) * KMH_PER_M_S]
	var verdict := "" if entry.is_empty() else EventTexts.verdict(entry)
	_verdict.text = "" if verdict.is_empty() else "The sled operator: " + verdict
	var practice := entry.is_empty()
	again_button.visible = practice
	menu_button.visible = practice
	continue_button.visible = not practice


# How the run ended (TractorPullRun.get_result()): a run at an event goes on
# past the line until the sled stops, so a full pull there is judged by the
# distance.
static func ending_name(result: Dictionary, full_pull_m: float) -> String:
	var ending: String = result.get("ending", "")
	if ending != "out_of_bounds" and float(result.get("distance_m", 0.0)) >= full_pull_m:
		return "full_pull"
	return ending


# The line that says how the run ended, "" while it is on.
static func ending_text(result: Dictionary, full_pull_m: float) -> String:
	if result.is_empty():
		return ""
	var ending := ending_name(result, full_pull_m)
	return String(ENDINGS.get(ending, ending))
