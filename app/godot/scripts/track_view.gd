# The track: the ground, the lane's chalk lines, the start and full-pull
# lines and a marker every 10 m, from the run's dimensions.
class_name TrackView
extends Node3D

const GROUND_COLOUR := Color(0.45, 0.33, 0.22)
const CHALK_COLOUR := Color(0.95, 0.95, 0.9)
const LINE_COLOUR := Color(0.9, 0.2, 0.15)
const MARKER_COLOUR := Color(0.95, 0.8, 0.1)
const CHALK_WIDTH_M := 0.1
const LINE_HEIGHT_M := 0.01
# Track behind the start line, where the tractor and sled stand [m].
const RUN_UP_M := 40.0
# Ground past the end of the track and either side of the lane [m].
const OVERRUN_M := 20.0
const VERGE_M := 15.0
const MARKER_EVERY_M := 10.0
const MARKER_SIZE_M := Vector3(0.3, 0.8, 0.3)


func build(dimensions: Dictionary) -> void:
	var length_m: float = dimensions["track_length_m"]
	var lane_m: float = dimensions["lane_width_m"]
	var full_pull_m: float = dimensions["full_pull_line_m"]
	var ground_length_m := RUN_UP_M + length_m + OVERRUN_M
	var ground := Graybox.box(Vector3(lane_m + 2.0 * VERGE_M, 0.1, ground_length_m), GROUND_COLOUR)
	ground.position = TrackSpace.point(length_m + OVERRUN_M - ground_length_m / 2.0, 0.0, -0.05)
	add_child(ground)
	for side: float in [-1.0, 1.0]:
		var chalk := Graybox.box(Vector3(CHALK_WIDTH_M, LINE_HEIGHT_M, ground_length_m),
				CHALK_COLOUR)
		chalk.position = TrackSpace.point(length_m + OVERRUN_M - ground_length_m / 2.0,
				side * lane_m / 2.0, 0.0)
		add_child(chalk)
	_add_line(0.0, lane_m, CHALK_COLOUR)
	_add_line(full_pull_m, lane_m, LINE_COLOUR)
	var x_m := MARKER_EVERY_M
	while x_m <= length_m:
		for side: float in [-1.0, 1.0]:
			var marker := Graybox.box(MARKER_SIZE_M, MARKER_COLOUR)
			marker.position = TrackSpace.point(x_m, side * (lane_m / 2.0 + 1.0),
					MARKER_SIZE_M.y / 2.0)
			add_child(marker)
		x_m += MARKER_EVERY_M


# A line across the lane, `x_m` down the track.
func _add_line(x_m: float, lane_m: float, colour: Color) -> void:
	var line := Graybox.box(Vector3(lane_m, LINE_HEIGHT_M, CHALK_WIDTH_M), colour)
	line.position = TrackSpace.point(x_m, 0.0, 0.0)
	add_child(line)
