# The lines on the track, from the run's dimensions: the lane's chalk
# lines, the start line and the full-pull line (which a test pull can move
# to 110 m, so it is drawn again for every pull). The ground, the boards
# every 10 m and all around it are VenueView's.
class_name TrackView
extends Node3D

const CHALK_COLOUR := Color(0.95, 0.95, 0.9)
const LINE_COLOUR := Color(0.9, 0.2, 0.15)
const CHALK_WIDTH_M := 0.1
const LINE_HEIGHT_M := 0.01
# Track behind the start line, where the tractor and sled stand, and past
# its end [m]: as VenueView's clay strip.
const RUN_UP_M := 40.0
const OVERRUN_M := 30.0


func build(dimensions: Dictionary) -> void:
	var length_m: float = dimensions["track_length_m"]
	var lane_m: float = dimensions["lane_width_m"]
	var full_pull_m: float = dimensions["full_pull_line_m"]
	var ground_length_m := RUN_UP_M + length_m + OVERRUN_M
	for side: float in [-1.0, 1.0]:
		var chalk := Graybox.box(Vector3(CHALK_WIDTH_M, LINE_HEIGHT_M, ground_length_m),
				CHALK_COLOUR)
		chalk.position = TrackSpace.point(length_m + OVERRUN_M - ground_length_m / 2.0,
				side * lane_m / 2.0, 0.0)
		add_child(chalk)
	_add_line(0.0, lane_m, CHALK_COLOUR)
	_add_line(full_pull_m, lane_m, LINE_COLOUR)


# A line across the lane, `x_m` down the track.
func _add_line(x_m: float, lane_m: float, colour: Color) -> void:
	var line := Graybox.box(Vector3(lane_m, LINE_HEIGHT_M, CHALK_WIDTH_M), colour)
	line.position = TrackSpace.point(x_m, 0.0, 0.0)
	add_child(line)
