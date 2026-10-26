# What the driver reads during a pull: which pull it is, a rev counter with
# the stall and rated speeds marked, the gear, the speed, and a bar down
# the track with the full-pull line, the stretch where the sled's push-down
# comes on (orange: from there the drag rises to its most) and how far the
# sled has got, with "PUSH-DOWN" beside it once it is on; in the pro
# layout also how far the clutch is in. It only shows the run's snapshot.
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
const FONT_SIZE := 30
const SMALL_FONT_SIZE := 22
const OUTLINE := 6
const BACK_COLOUR := Color(0.1, 0.1, 0.1, 0.5)
const REV_COLOUR := Color(0.95, 0.75, 0.2)
const STALL_COLOUR := Color(0.9, 0.2, 0.15)
const RATED_COLOUR := Color(0.3, 0.85, 0.3)
const SLED_COLOUR := Color(0.95, 0.95, 0.9)
const LINE_COLOUR := Color(0.9, 0.2, 0.15)
const PUSHDOWN_COLOUR := Color(1.0, 0.55, 0.1)
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
var _pushdown_start_m := 0.0
var _pushdown_full_m := 0.0
var _pushdown := 0.0
var _pushdown_label := Label.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for label: Label in [_pull, _numbers, _pushdown_label]:
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", OUTLINE)
		add_child(label)
	_pull.position = Vector2(MARGIN, MARGIN)
	_pull.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_numbers.position = Vector2(MARGIN, MARGIN + 34.0)
	_numbers.add_theme_font_size_override("font_size", FONT_SIZE)
	_pushdown_label.position = Vector2(BAR_LEFT + BAR_WIDTH + MARGIN / 2.0, TRACK_TOP - 6.0)
	_pushdown_label.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_pushdown_label.add_theme_color_override("font_color", PUSHDOWN_COLOUR)


# The run's fixed sizes (TractorPullRun.get_dimensions()).
func build(dimensions: Dictionary) -> void:
	_stall_rad_s = dimensions["stall_omega_rad_s"]
	_rated_rad_s = dimensions["rated_omega_rad_s"]
	_top_rad_s = float(dimensions["high_idle_omega_rad_s"]) * REV_HEADROOM
	_line_m = dimensions["full_pull_line_m"]
	_bar_m = _line_m * TRACK_HEADROOM
	_pushdown_start_m = dimensions["pushdown_start_m"]
	_pushdown_full_m = dimensions["pushdown_full_m"]


# `title` names the pull ("" none), `pro` whether to show the clutch.
func show_snapshot(s: Dictionary, title: String, pro: bool) -> void:
	_pull.text = title
	_omega_rad_s = s["engine_omega_rad_s"]
	_sled_m = s["sled_x_m"]
	_pushdown = s["pushdown"]
	if _pushdown <= 0.0:
		_pushdown_label.text = ""
	elif _pushdown < 1.0:
		_pushdown_label.text = "PUSH-DOWN %3.0f %%" % (_pushdown * 100.0)
	else:
		_pushdown_label.text = "PUSH-DOWN SET"
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
	# The push-down's stretch, under the sled's bar.
	var from := BAR_WIDTH * clampf(_pushdown_start_m / _bar_m, 0.0, 1.0)
	var to := BAR_WIDTH * clampf(_pushdown_full_m / _bar_m, 0.0, 1.0)
	draw_rect(Rect2(track.position + Vector2(from, 0.0), Vector2(maxf(to - from, 2.0),
			BAR_HEIGHT)), PUSHDOWN_COLOUR.darkened(0.4))
	draw_rect(Rect2(track.position, Vector2(BAR_WIDTH * sled_share, BAR_HEIGHT)),
			PUSHDOWN_COLOUR if _pushdown > 0.0 else SLED_COLOUR)
	_mark(track, _pushdown_start_m / _bar_m, PUSHDOWN_COLOUR)
	_mark(track, _line_m / _bar_m, LINE_COLOUR)


func _mark(bar: Rect2, share: float, colour: Color) -> void:
	var x := bar.position.x + bar.size.x * clampf(share, 0.0, 1.0)
	draw_line(Vector2(x, bar.position.y - 4.0), Vector2(x, bar.end.y + 4.0), colour, 4.0)
