# What the driver reads during a pull: which pull it is, a rev counter with
# the stall and rated speeds marked, the gear, the speed, a bar down the
# track with the full-pull line, the stretch where the sled's push-down
# comes on (orange: from there the drag rises to its most) and how far the
# sled has got, with "PUSH-DOWN" beside it once it is on, and how much the
# front tires still carry, against what they carry at rest (marked): the
# pull takes weight off the front, and with none left the front is up and
# the front wheels do not steer. In the pro layout also how far the clutch
# is in. In a whole turn, a line in the middle says what to do next: drive
# to the sled and back up to it (how far the hitch still has to go, and
# which way to steer), take up the slack, wait for the flag, back off to
# release the chain, drive back to the bay. A lamp right of the bars
# repeats the officials' signal: green GO while the pull is good,
# red STOP to wait or to stop (the pull is over: close the throttle). It
# only shows the run's snapshot.
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
# The turn's line, under the top edge in the middle.
const TURN_TOP := 250.0
const TURN_COLOUR := Color(1.0, 0.95, 0.6)
# Backing up: off the sled's line by more than this share of how far off
# the chain still goes on (turn::kHookOffLine_m), it says which way to steer.
const STEER_HINT_SHARE := 0.6
# Farther from the hook than this many chain lengths, plus a margin for
# the turn into the lane, the tractor is still on its way past the sled.
const PAST_THE_SLED_CHAINS := 3.0
const PAST_THE_SLED_MARGIN_M := 10.0
# The signal lamp: its middle, right of the bars' labels and clear of the
# numbers line, the buttons and the gear buttons; its radius.
const LAMP_CENTRE := Vector2(740.0, 155.0)
const LAMP_RADIUS := 30.0
const LAMP_LABEL_WIDTH := 120.0
const LAMP_RED := Color(1.0, 0.15, 0.1)
const LAMP_GREEN := Color(0.2, 0.95, 0.35)
# Still rolling forwards, or the throttle still open, after the red flag
# [m/s, share of the throttle].
const STOPPED_M_S := 0.05
const THROTTLE_CLOSED := 0.05
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
var _pushdown_start_m := 0.0
var _pushdown_full_m := 0.0
var _pushdown := 0.0
var _pushdown_label := Label.new()
var _front_rest_n := 1.0
var _front_n := 0.0
var _front := Label.new()
var _turn := Label.new()
var _lamp := Label.new()
# The officials' signal: "green", "red" or "off".
var _signal := "off"
var _chain_m := 1.0
var _steer_hint_m := 0.25
# The pro layout: the driver works the clutch (the easy one's assist does).
var _pro := false
# The player's bay in the parking, on the track; INF for no whole turn.
var _parking := Vector2.INF


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for label: Label in [_pull, _numbers, _pushdown_label, _front, _turn, _lamp]:
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
	_front.position = Vector2(BAR_LEFT + BAR_WIDTH + MARGIN / 2.0, FRONT_TOP - 6.0)
	_front.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_turn.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	_turn.offset_top = TURN_TOP
	_turn.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_turn.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_turn.add_theme_font_size_override("font_size", FONT_SIZE)
	_turn.add_theme_color_override("font_color", TURN_COLOUR)
	_lamp.position = Vector2(LAMP_CENTRE.x - LAMP_LABEL_WIDTH / 2.0, LAMP_CENTRE.y + LAMP_RADIUS + 4.0)
	_lamp.size.x = LAMP_LABEL_WIDTH
	_lamp.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_lamp.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)


# The run's fixed sizes (TractorPullRun.get_dimensions()).
func build(dimensions: Dictionary) -> void:
	_stall_rad_s = dimensions["stall_omega_rad_s"]
	_rated_rad_s = dimensions["rated_omega_rad_s"]
	_top_rad_s = float(dimensions["high_idle_omega_rad_s"]) * REV_HEADROOM
	_line_m = dimensions["full_pull_line_m"]
	_bar_m = _line_m * TRACK_HEADROOM
	_pushdown_start_m = dimensions["pushdown_start_m"]
	_pushdown_full_m = dimensions["pushdown_full_m"]
	_front_rest_n = maxf(float(dimensions["front_axle_rest_load_n"]), 1.0)
	_chain_m = dimensions["chain_length_m"]
	_steer_hint_m = STEER_HINT_SHARE * float(dimensions["hook_off_line_m"])


# The officials' signal (pull.gd's signal_for()): "green", "red" or "off".
func show_signal(signal_name: String) -> void:
	if signal_name == _signal:
		return
	_signal = signal_name
	_lamp.text = {"green": "GO", "red": "STOP"}.get(signal_name, "")
	_lamp.add_theme_color_override("font_color",
			LAMP_GREEN if signal_name == "green" else LAMP_RED)
	queue_redraw()


# The signal lamp's state now, for tests.
func signal_shown() -> String:
	return _signal


# The player's bay (x, y on the track), for the way back; Vector2.INF
# when the run is the pull alone.
func set_parking(bay: Vector2) -> void:
	_parking = bay


# `title` names the pull ("" none), `pro` whether to show the clutch.
func show_snapshot(s: Dictionary, title: String, pro: bool) -> void:
	_pro = pro
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
	_front_n = s["front_axle_load_n"]
	_front.text = "FRONT UP" if _front_n <= 0.0 else "front %3.0f %%" % (
			_front_n / _front_rest_n * 100.0)
	_front.add_theme_color_override("font_color",
			FRONT_UP_COLOUR if _front_n <= 0.0 else Color.WHITE)
	var gear: int = s["gear"]
	var text := "%s   %4.0f rpm   %4.1f km/h   %6.2f m" % [
			gear_name(gear), _omega_rad_s * RPM_PER_RAD_S,
			float(s["tractor_speed_m_s"]) * KMH_PER_M_S, _sled_m]
	if pro:
		# The clutch as the run has it: 0 pressed, 100 % in.
		text += "   clutch %3.0f %%" % ((1.0 - float(s["clutch_pedal"])) * 100.0)
	_numbers.text = text
	_turn.text = turn_text(s)
	queue_redraw()


# What to do next in a whole turn, from the snapshot; "" during the pull.
func turn_text(s: Dictionary) -> String:
	match s.get("stage", "pulling"):
		"to_the_sled":
			var along: float = s["hitch_along_m"]
			if along <= 0.0 or float(s["hitch_to_hook_m"]) \
					> PAST_THE_SLED_CHAINS * _chain_m + PAST_THE_SLED_MARGIN_M:
				return "Drive past the sled into the lane, then back up to it in R"
			var to_go := maxf(float(s["hitch_to_hook_m"]) - _chain_m, 0.0)
			var across: float = s["hitch_across_m"]
			var steer := ""
			if absf(across) > _steer_hint_m:
				# Backing up, the tail goes the way the wheel is turned: a
				# hitch left of the line wants the wheel to the right.
				steer = "   steer %s" % ("right" if across > 0.0 else "left")
			if to_go <= 0.0:
				return "Stop: the chain goes on" + steer
			return "Back up to the sled: %.1f m%s" % [to_go, steer]
		"taking_up_slack":
			return "Hooked on. Creep forward to take up the slack"
		"waiting_for_the_flag":
			if float(s.get("flag_progress", 0.0)) > 0.0:
				return "Hold it there... the flag is coming"
			if _pro:
				return "Chain tight: press the clutch, stand still in your pulling gear"
			return "Chain tight: let go of the gas and stand still in your pulling gear"
		"releasing":
			var rolling := float(s["tractor_speed_m_s"]) > STOPPED_M_S
			if rolling or float(s["throttle"]) > THROTTLE_CLOSED:
				return "Red flag: stop! Close the throttle"
			return "Back off in R to release the chain"
		"to_the_parking":
			if _parking == Vector2.INF:
				return ""
			var gap := Vector2(float(s["tractor_x_m"]), float(s["tractor_y_m"])) - _parking
			return "Drive back to your bay: %.0f m" % gap.length()
		"parked":
			return "Parked" if _parking != Vector2.INF else ""
	return ""


# N for neutral, R for reverse, else the gear's number.
static func gear_name(gear: int) -> String:
	if gear == 0:
		return "N"
	return "R" if gear < 0 else str(gear)


func _draw() -> void:
	# The officials' signal.
	if _signal != "off":
		draw_circle(LAMP_CENTRE, LAMP_RADIUS + 5.0, BACK_COLOUR)
		draw_circle(LAMP_CENTRE, LAMP_RADIUS, LAMP_GREEN if _signal == "green" else LAMP_RED)
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
