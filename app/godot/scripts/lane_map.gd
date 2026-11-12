# The lane from above, as the crew walked it before the pull
# (TractorPullEvent.get_crew_report()): the start line at the bottom, the
# full-pull line at the top, the chalk lines at the sides, the driver's left
# on the left. Each spot the crew reports is drawn where they think it is,
# blurred by how sure they are: hard spots (where the sled drags harder) in
# dry clay's tan, weak spots (where the tires grip less) in a wet blue, the
# stronger the darker. Over them the line the player has chosen: the rear
# tires' tracks and the sled's pan between them, and a notch where the crew
# would start.
class_name LaneMap
extends Control

const SOIL := Color(0.3, 0.22, 0.15)
const SOIL_DARK := Color(0.25, 0.18, 0.12)
const CHALK := Color(0.93, 0.93, 0.88)
const HARD := Color(0.86, 0.69, 0.42)
const WEAK := Color(0.36, 0.55, 0.82)
const TIRE := Color(0.08, 0.08, 0.08, 0.75)
const PAN := Color(0.95, 0.95, 0.95, 0.16)
const SUGGESTION := Color(0.42, 0.77, 0.42)
# Inside the control: room for the chalk lines and the start and full-pull
# lines [px].
const MARGIN := 8.0
# A spot is drawn at least this big, however short, and its blur at most
# this strong [px, share].
const MIN_SPOT := 4.0
const BLUR_ALPHA := 0.2
# A spot at this strength or more is drawn at full colour.
const FULL_STRENGTH := 0.3
const NOTCH := 8.0

var report: Dictionary = {}
# Where the player starts the sled, to the left of the middle [m].
var start_y_m := 0.0


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	clip_contents = true


func show_report(crew: Dictionary) -> void:
	report = crew
	start_y_m = float(crew.get("start_y_m", 0.0))
	queue_redraw()


func show_start(y_m: float) -> void:
	start_y_m = y_m
	queue_redraw()


# The lane's width and the track's length drawn [m].
func _lane_m() -> float:
	return maxf(float(report.get("lane_width_m", 12.0)), 1.0)


func _length_m() -> float:
	return maxf(float(report.get("full_pull_m", 100.0)), 1.0)


# Where `y_m` across the lane (positive left) and `x_m` along it are drawn.
func _point(x_m: float, y_m: float) -> Vector2:
	var across := (size.x - 2.0 * MARGIN) / _lane_m()
	var along := (size.y - 2.0 * MARGIN) / _length_m()
	return Vector2(size.x / 2.0 - y_m * across, size.y - MARGIN - x_m * along)


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), SOIL)
	var half := _lane_m() / 2.0
	# Furrows down the lane from the preparation.
	for i in range(-5, 6):
		var y := half * i / 5.5
		draw_line(_point(0.0, y), _point(_length_m(), y), SOIL_DARK, 2.0)
	for spot: Dictionary in report.get("spots", []):
		_draw_spot(spot)
	for side: float in [-1.0, 1.0]:
		draw_line(_point(0.0, side * half), _point(_length_m(), side * half), CHALK, 2.0)
	draw_line(_point(0.0, half), _point(0.0, -half), CHALK, 2.0)
	draw_line(_point(_length_m(), half), _point(_length_m(), -half), Color(CHALK, 0.5), 2.0)
	if report.is_empty():
		return
	_draw_line_chosen()
	var suggested := _point(0.0, float(report.get("suggested_y_m", 0.0)))
	draw_colored_polygon(PackedVector2Array([suggested + Vector2(0.0, -NOTCH),
			suggested + Vector2(-NOTCH, 0.0), suggested + Vector2(NOTCH, 0.0)]), SUGGESTION)


func _draw_spot(spot: Dictionary) -> void:
	var at := float(spot.get("at_m", 0.0))
	if at > _length_m():
		return
	var y := float(spot.get("y_m", 0.0))
	var half_width := float(spot.get("width_m", 1.0)) / 2.0
	var half_length := float(spot.get("length_m", 1.0)) / 2.0
	var colour: Color = HARD if spot.get("kind", "") == "hard" else WEAK
	var strength := clampf(float(spot.get("strength", 0.0)) / FULL_STRENGTH, 0.35, 1.0)
	# The blur: as far as the crew may be off.
	var blur := _rect(at, y, half_length + float(report.get("along_error_m", 0.0)),
			half_width + float(report.get("across_error_m", 0.0)))
	draw_rect(blur, Color(colour, BLUR_ALPHA * strength))
	draw_rect(_rect(at, y, half_length, half_width), Color(colour, strength))


# A spot's rectangle, at least MIN_SPOT on each side.
func _rect(at_m: float, y_m: float, half_length_m: float, half_width_m: float) -> Rect2:
	var a := _point(at_m - half_length_m, y_m + half_width_m)
	var b := _point(at_m + half_length_m, y_m - half_width_m)
	var rect := Rect2(a, Vector2.ZERO).expand(b)
	var grow := Vector2(maxf(MIN_SPOT - rect.size.x, 0.0), maxf(MIN_SPOT - rect.size.y, 0.0))
	return rect.grow_individual(grow.x / 2.0, grow.y / 2.0, grow.x / 2.0, grow.y / 2.0)


# The line chosen: the pan's band and the two rear tires' tracks, straight
# down the lane from the start.
func _draw_line_chosen() -> void:
	var length := _length_m()
	var pan := float(report.get("pan_half_width_m", 1.0))
	draw_rect(Rect2(_point(length, start_y_m + pan), Vector2.ZERO).expand(
			_point(0.0, start_y_m - pan)), PAN)
	var half_track := float(report.get("rear_track_m", 1.8)) / 2.0
	var tire_px := maxf(float(report.get("rear_tire_width_m", 0.5)) * (size.x - 2.0 * MARGIN)
			/ _lane_m(), 2.0)
	for side: float in [-1.0, 1.0]:
		var y := start_y_m + side * half_track
		draw_line(_point(0.0, y), _point(length, y), TIRE, tire_px)
