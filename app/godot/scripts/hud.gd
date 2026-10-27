# What the driver reads during a pull: which pull it is, a rev counter with
# the stall and rated speeds marked, the gear, the speed, a bar down the
# track with the full-pull line and how far the sled has got, and how much
# the front tires still carry, against what they carry at rest (marked): the
# pull takes weight off the front, and with none left the front is up and
# the front wheels do not steer. In the pro layout also how far the clutch
# is in. It only shows the run's snapshot.
class_name Hud
extends Control

const MARGIN := 24.0
# The two bars under the text, clear of the buttons along the top right and
# the pro layout's brake sliders below.
const BAR_LEFT := MARGIN
const BAR_WIDTH := 460.0
const BAR_HEIGHT := 22.0
const REV_TOP := 112.0
const TRACK_TOP := 148.0
const FRONT_TOP := 184.0
const FONT_SIZE := 30
const SMALL_FONT_SIZE := 22
const OUTLINE := 6
const BACK_COLOUR := Color(0.1, 0.1, 0.1, 0.5)
const REV_COLOUR := Color(0.95, 0.75, 0.2)
const STALL_COLOUR := Color(0.9, 0.2, 0.15)
const RATED_COLOUR := Color(0.3, 0.85, 0.3)
const SLED_COLOUR := Color(0.95, 0.95, 0.9)
const LINE_COLOUR := Color(0.9, 0.2, 0.15)
const FRONT_COLOUR := Color(0.45, 0.7, 0.95)
const FRONT_UP_COLOUR := Color(0.95, 0.3, 0.2)
# The front bar ends a little past the load at rest: a dip of the nose
# (a hard shift) loads the front more.
const FRONT_HEADROOM := 1.5
const RPM_PER_RAD_S := 60.0 / TAU
const KMH_PER_M_S := 3.6
# The rev counter ends a little past high idle, the track bar past the line.
const REV_HEADROOM := 1.05
const TRACK_HEADROOM := 1.1

var _pull := Label.new()
var _numbers := Label.new()
var _stall_rad_s := 0.0
var _rated_rad_s := 1.0
var _top_rad_s := 1.0
var _line_m := 100.0
var _bar_m := 110.0
var _omega_rad_s := 0.0
var _sled_m := 0.0
var _front_rest_n := 1.0
var _front_n := 0.0
var _front := Label.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for label: Label in [_pull, _numbers, _front]:
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", OUTLINE)
		add_child(label)
	_pull.position = Vector2(MARGIN, MARGIN)
	_pull.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_numbers.position = Vector2(MARGIN, MARGIN + 34.0)
	_numbers.add_theme_font_size_override("font_size", FONT_SIZE)
	_front.position = Vector2(BAR_LEFT + BAR_WIDTH + MARGIN / 2.0, FRONT_TOP - 6.0)
	_front.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)


# The run's fixed sizes (TractorPullRun.get_dimensions()).
func build(dimensions: Dictionary) -> void:
	_stall_rad_s = dimensions["stall_omega_rad_s"]
	_rated_rad_s = dimensions["rated_omega_rad_s"]
	_top_rad_s = float(dimensions["high_idle_omega_rad_s"]) * REV_HEADROOM
	_line_m = dimensions["full_pull_line_m"]
	_bar_m = _line_m * TRACK_HEADROOM
	_front_rest_n = maxf(float(dimensions["front_axle_rest_load_n"]), 1.0)


# `title` names the pull ("" none), `pro` whether to show the clutch.
func show_snapshot(s: Dictionary, title: String, pro: bool) -> void:
	_pull.text = title
	_omega_rad_s = s["engine_omega_rad_s"]
	_sled_m = s["sled_x_m"]
	_front_n = s["front_axle_load_n"]
	_front.text = "FRONT UP" if _front_n <= 0.0 else "front %3.0f %%" % (
			_front_n / _front_rest_n * 100.0)
	_front.add_theme_color_override("font_color",
			FRONT_UP_COLOUR if _front_n <= 0.0 else Color.WHITE)
	var gear: int = s["gear"]
	var text := "%s   %4.0f rpm   %4.1f km/h   %6.2f m" % [
			"N" if gear == 0 else str(gear), _omega_rad_s * RPM_PER_RAD_S,
			float(s["tractor_speed_m_s"]) * KMH_PER_M_S, _sled_m]
	if pro:
		# The clutch as the run has it: 0 pressed, 100 % in.
		text += "   clutch %3.0f %%" % ((1.0 - float(s["clutch_pedal"])) * 100.0)
	_numbers.text = text
	queue_redraw()


func _draw() -> void:
	# The rev counter, with the stall and rated speeds marked.
	var revs := Rect2(BAR_LEFT, REV_TOP, BAR_WIDTH, BAR_HEIGHT)
	draw_rect(revs, BACK_COLOUR)
	var rev_share := clampf(_omega_rad_s / _top_rad_s, 0.0, 1.0)
	draw_rect(Rect2(revs.position, Vector2(BAR_WIDTH * rev_share, BAR_HEIGHT)), REV_COLOUR)
	_mark(revs, _stall_rad_s / _top_rad_s, STALL_COLOUR)
	_mark(revs, _rated_rad_s / _top_rad_s, RATED_COLOUR)

	# Down the track: how far the sled has got, and the full-pull line.
	var track := Rect2(BAR_LEFT, TRACK_TOP, BAR_WIDTH, BAR_HEIGHT)
	draw_rect(track, BACK_COLOUR)
	var sled_share := clampf(_sled_m / _bar_m, 0.0, 1.0)
	draw_rect(Rect2(track.position, Vector2(BAR_WIDTH * sled_share, BAR_HEIGHT)), SLED_COLOUR)
	_mark(track, _line_m / _bar_m, LINE_COLOUR)

	# What the front tires carry, the load at rest marked.
	var front := Rect2(BAR_LEFT, FRONT_TOP, BAR_WIDTH, BAR_HEIGHT)
	draw_rect(front, BACK_COLOUR)
	var front_top_n := _front_rest_n * FRONT_HEADROOM
	var front_share := clampf(_front_n / front_top_n, 0.0, 1.0)
	draw_rect(Rect2(front.position, Vector2(BAR_WIDTH * front_share, BAR_HEIGHT)),
			FRONT_COLOUR)
	_mark(front, 1.0 / FRONT_HEADROOM, Color.WHITE)
	if _front_n <= 0.0:
		draw_rect(front, FRONT_UP_COLOUR, false, 3.0)


func _mark(bar: Rect2, share: float, colour: Color) -> void:
	var x := bar.position.x + bar.size.x * clampf(share, 0.0, 1.0)
	draw_line(Vector2(x, bar.position.y - 4.0), Vector2(x, bar.end.y + 4.0), colour, 4.0)
