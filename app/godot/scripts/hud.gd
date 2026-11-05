# What the driver reads during a pull: which pull it is, a rev counter with
# the stall and rated speeds marked, the gear, the speed, a bar down the
# track with the full-pull line, the stretch where the sled's push-down
# comes on (orange: from there the drag rises to its most) and how far the
# sled has got, with "PUSH-DOWN" beside it once it is on, and how much the
# front tires still carry, against what they carry at rest (marked): the
# pull takes weight off the front, and with none left the front is up and
# the front wheels do not steer. Left of the bars a boost gauge: the turbo's
# pressure in bar, its full boost marked (shown when the engine gives one).
# In the pro layout also how far the clutch
# is in. In a whole turn, a short line along the top in the middle says
# what to do next (it
# can be turned off in the settings: `guidance`): drive
# to the sled and back up to it (how far the hitch still has to go, and
# which way to steer), take up the slack (how far still to creep, with a
# gauge under the line: the slack, and the band where it is taken up),
# wait for the flag, back off to
# release the chain, drive back to the bay. After the pull the numbers keep
# its distance (hold_distance) while the sled drives back, and the bar down
# the track is empty again once the chain is off. A lamp right of the bars
# repeats the officials' signal: green GO while the pull is good,
# red STOP to wait or to stop (the pull is over: close the throttle). Once
# the pull is over, a large line under it says why it ended (show_ending):
# what to change for the next pull. It only shows the run's snapshot.
class_name Hud
extends Control

const MARGIN := 24.0
# The bars under the text, right of the boost gauge, clear of the buttons
# along the top right and the pro layout's sliders below.
const BAR_LEFT := MARGIN + 110.0
const BAR_WIDTH := 380.0
const BAR_HEIGHT := 22.0
const REV_TOP := 112.0
const TRACK_TOP := 148.0
const FRONT_TOP := 184.0
# The boost gauge: a dial as tall as the three bars, left of them, its
# scale over 270° from the bottom left round to the bottom right, a tick
# every half bar, its top a little past full boost.
const GAUGE_CENTRE := Vector2(MARGIN + 48.0, REV_TOP + 47.0)
const GAUGE_RADIUS := 46.0
const GAUGE_FROM_RAD := 0.75 * PI
const GAUGE_SWEEP_RAD := 1.5 * PI
const GAUGE_TICK_BAR := 0.5
const GAUGE_HEADROOM := 1.15
const GAUGE_COLOUR := Color(0.35, 0.8, 1.0)
# The turn's line, short, along the top edge in the middle on a dark band:
# between the pull's name on the left and the buttons on the right.
const TURN_TOP := 8.0
const TURN_WIDTH := 380.0
const TURN_PADDING := 6.0
const TURN_FONT_SIZE := 22
const TURN_COLOUR := Color(1.0, 0.95, 0.6)
# Why the pull ended, large, in the middle under the top.
const ENDING_TOP := 230.0
const ENDING_FONT_SIZE := 40
# How long it stays, and how long it takes to fade out at the end [s].
const ENDING_SHOWN_S := 4.0
const ENDING_FADE_S := 1.0
const ENDING_COLOUR := Color(1.0, 0.45, 0.3)
const FULL_PULL_COLOUR := Color(0.4, 1.0, 0.45)
# Backing up: off the sled's line by more than this share of how far off
# the chain still goes on (turn::kHookOffLine_m), it says which way to steer.
const STEER_HINT_SHARE := 0.6
# Farther from the hook than this many chain lengths, plus a margin for
# the turn into the lane, the tractor is still on its way past the sled.
const PAST_THE_SLED_CHAINS := 3.0
const PAST_THE_SLED_MARGIN_M := 10.0
# Taking up the slack, a gauge under the turn's line: the chain's slack
# from GUIDE_SLACK_M (left) to GUIDE_STRETCH_M stretched (right), the band
# the slack counts as taken up in green, a mark where the chain is now
# [px, m].
const GUIDE_WIDTH := 300.0
const GUIDE_TOP := 48.0
const GUIDE_HEIGHT := 9.0
const GUIDE_SLACK_M := 1.5
const GUIDE_STRETCH_M := 0.15
const GUIDE_COLOUR := Color(1.0, 0.95, 0.6, 0.35)
const GUIDE_OK_COLOUR := Color(0.2, 0.95, 0.35)
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
# The distance the numbers show: the pull's once it is over [m].
var _held_m := 0.0
var _pushdown_start_m := 0.0
var _pushdown_full_m := 0.0
var _pushdown := 0.0
var _pushdown_label := Label.new()
var _front_rest_n := 1.0
var _front_n := 0.0
var _front := Label.new()
# The boost: its share (0 to 1) and the pressure at full boost, 0 when the
# engine gives none [bar].
var _boost := 0.0
var _full_boost_bar := 0.0
var _gauge := Label.new()
var _turn := Label.new()
var _ending := Label.new()
var _ending_left_s := 0.0
var _lamp := Label.new()
# The turn's line is shown (the settings' guidance).
var guidance := true
# The officials' signal: "green", "red" or "off".
var _signal := "off"
var _chain_m := 1.0
var _steer_hint_m := 0.25
# Shorter than the chain's length by more than this, the chain is not
# tight enough for the flag [m].
var _taut_within_m := 0.1
# The chain's slack now, and whether the gauge shows it [m].
var _slack_m := 0.0
var _guide := false
# The pro layout: the driver works the clutch (the easy one's assist does).
var _pro := false
# The player's bay in the parking, on the track; INF for no whole turn.
var _parking := Vector2.INF


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for label: Label in [_pull, _numbers, _pushdown_label, _front, _turn, _ending, _lamp,
			_gauge]:
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", OUTLINE)
		add_child(label)
	_pull.position = Vector2(MARGIN, TURN_TOP)
	_pull.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_numbers.position = Vector2(MARGIN, MARGIN + 34.0)
	_numbers.add_theme_font_size_override("font_size", FONT_SIZE)
	_pushdown_label.position = Vector2(BAR_LEFT + BAR_WIDTH + MARGIN / 2.0, TRACK_TOP - 6.0)
	_pushdown_label.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_pushdown_label.add_theme_color_override("font_color", PUSHDOWN_COLOUR)
	_front.position = Vector2(BAR_LEFT + BAR_WIDTH + MARGIN / 2.0, FRONT_TOP - 6.0)
	_front.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_turn.set_anchors_preset(Control.PRESET_CENTER_TOP)
	_turn.offset_left = -TURN_WIDTH / 2.0
	_turn.offset_right = TURN_WIDTH / 2.0
	_turn.offset_top = TURN_TOP
	_turn.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_turn.clip_text = true
	_turn.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_turn.add_theme_font_size_override("font_size", TURN_FONT_SIZE)
	_turn.add_theme_color_override("font_color", TURN_COLOUR)
	_ending.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	_ending.offset_top = ENDING_TOP
	_ending.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_ending.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_ending.add_theme_font_size_override("font_size", ENDING_FONT_SIZE)
	_lamp.position = Vector2(LAMP_CENTRE.x - LAMP_LABEL_WIDTH / 2.0, LAMP_CENTRE.y + LAMP_RADIUS + 4.0)
	_lamp.size.x = LAMP_LABEL_WIDTH
	_lamp.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_lamp.add_theme_font_size_override("font_size", SMALL_FONT_SIZE)
	_gauge.position = GAUGE_CENTRE + Vector2(-GAUGE_RADIUS, GAUGE_RADIUS * 0.25)
	_gauge.size.x = 2.0 * GAUGE_RADIUS
	_gauge.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_gauge.add_theme_font_size_override("font_size", SMALL_FONT_SIZE - 4)


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
	_taut_within_m = dimensions["ready_taut_within_m"]
	_full_boost_bar = dimensions.get("full_boost_bar", 0.0)


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


# The distance to show in the numbers this frame (the sled's, or the
# pull's once it is over); set before show_snapshot().
func hold_distance(distance_m: float) -> void:
	_held_m = distance_m


func distance_shown() -> float:
	return _held_m


# `title` names the pull ("" none), `pro` whether to show the clutch.
func show_snapshot(s: Dictionary, title: String, pro: bool) -> void:
	_pro = pro
	_pull.text = title
	_omega_rad_s = s["engine_omega_rad_s"]
	_boost = s.get("boost", 0.0)
	_gauge.text = "%.1f bar" % boost_bar() if _full_boost_bar > 0.0 else ""
	# The chain off after the pull: the bar starts again for the next one.
	var chain_off: bool = s.get("stage", "pulling") in ["to_the_parking", "parked"] \
			and not bool(s.get("chain_hooked", true))
	_sled_m = 0.0 if chain_off else float(s["pulled_m"])
	_pushdown = 0.0 if chain_off else float(s["pushdown"])
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
			float(s["tractor_speed_m_s"]) * KMH_PER_M_S, _held_m]
	if pro:
		# The clutch as the run has it: 0 pressed, 100 % in.
		text += "   clutch %3.0f %%" % ((1.0 - float(s["clutch_pedal"])) * 100.0)
	_numbers.text = text
	_turn.text = turn_text(s) if guidance else ""
	_slack_m = s.get("chain_slack_m", 0.0)
	_guide = guidance and s.get("stage", "pulling") in ["taking_up_slack", "waiting_for_the_flag"]
	queue_redraw()


# Why the pull ended (ResultScreen.ending_text()), "" while it is on;
# `full_pull` shows it in green.
func show_ending(text: String, full_pull: bool = false) -> void:
	_ending.text = text
	_ending.add_theme_color_override("font_color",
			FULL_PULL_COLOUR if full_pull else ENDING_COLOUR)
	_ending_left_s = ENDING_SHOWN_S if not text.is_empty() else 0.0
	_ending.modulate.a = 1.0


# The ending's line goes after ENDING_SHOWN_S, fading out over its last
# ENDING_FADE_S: it is not needed on the way back to the bay.
func _process(delta: float) -> void:
	if _ending_left_s <= 0.0:
		return
	_ending_left_s -= delta
	_ending.modulate.a = clampf(_ending_left_s / ENDING_FADE_S, 0.0, 1.0)
	if _ending_left_s <= 0.0:
		_ending.text = ""


func ending_shown() -> String:
	return _ending.text


# What to do next in a whole turn, from the snapshot; "" during the pull.
func turn_text(s: Dictionary) -> String:
	match s.get("stage", "pulling"):
		"to_the_sled":
			var along: float = s["hitch_along_m"]
			if along <= 0.0 or float(s["hitch_to_hook_m"]) \
					> PAST_THE_SLED_CHAINS * _chain_m + PAST_THE_SLED_MARGIN_M:
				return "Drive past the sled, back up in R"
			var to_go := maxf(float(s["hitch_to_hook_m"]) - _chain_m, 0.0)
			var across: float = s["hitch_across_m"]
			var steer := ""
			if absf(across) > _steer_hint_m:
				# Backing up, the tail goes the way the wheel is turned: a
				# hitch left of the line wants the wheel to the right.
				steer = " · steer %s" % ("right" if across > 0.0 else "left")
			if to_go <= 0.0:
				return "Stop: chain goes on" + steer
			return "Back up: %.1f m%s" % [to_go, steer]
		"taking_up_slack":
			return "Creep forward: %.2f m" % maxf(slack_to_go_m(s), 0.0)
		"waiting_for_the_flag":
			if float(s.get("flag_progress", 0.0)) > 0.0:
				return "Hold still..."
			if slack_to_go_m(s) > 0.0:
				return "Creep forward: %.2f m" % slack_to_go_m(s)
			if _pro:
				return "Chain tight: clutch in, gear in, hold"
			return "Chain tight: off the gas, gear in, hold"
		"releasing":
			# Backing off in reverse is what the red flag asks for now: gas
			# in R is fine.
			if int(s["gear"]) < 0:
				return "Back off in R"
			var rolling := float(s["tractor_speed_m_s"]) > STOPPED_M_S
			if rolling or float(s["throttle"]) > THROTTLE_CLOSED:
				return "Red flag: stop!" if rolling else "Red flag: off the gas!"
			return "Shift to R, back off"
		"to_the_parking":
			if _parking == Vector2.INF:
				return ""
			var gap := Vector2(float(s["tractor_x_m"]), float(s["tractor_y_m"])) - _parking
			return "Back to your bay: %.0f m" % gap.length()
		"parked":
			return "Parked" if _parking != Vector2.INF else ""
	return ""


# How far the tractor still has to creep before the slack counts as taken
# up: into the band within the ready distance of tight [m].
func slack_to_go_m(s: Dictionary) -> float:
	return float(s.get("chain_slack_m", 0.0)) - _taut_within_m


# Whether the slack gauge shows, for tests.
func guide_shown() -> bool:
	return _guide


# The boost pressure the gauge shows [bar].
func boost_bar() -> float:
	return _boost * _full_boost_bar


# N for neutral, R for reverse, else the gear's number.
static func gear_name(gear: int) -> String:
	if gear == 0:
		return "N"
	return "R" if gear < 0 else str(gear)


func _draw() -> void:
	# The band behind the turn's line.
	if not _turn.text.is_empty():
		var font := _turn.get_theme_font("font")
		var width := minf(font.get_string_size(_turn.text, HORIZONTAL_ALIGNMENT_LEFT, -1,
				TURN_FONT_SIZE).x, TURN_WIDTH)
		var band := Rect2(_turn.position + Vector2((TURN_WIDTH - width) / 2.0, 0.0),
				Vector2(width, _turn.get_minimum_size().y))
		draw_rect(band.grow(TURN_PADDING), BACK_COLOUR)
	if _guide:
		_draw_guide()
	# The officials' signal.
	if _signal != "off":
		draw_circle(LAMP_CENTRE, LAMP_RADIUS + 5.0, BACK_COLOUR)
		draw_circle(LAMP_CENTRE, LAMP_RADIUS, LAMP_GREEN if _signal == "green" else LAMP_RED)
	if _full_boost_bar > 0.0:
		_draw_gauge()
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


# The boost gauge: the dial, a tick every half bar, full boost marked, the
# boost so far as an arc and a needle.
# The slack gauge: the slack shrinking to the right, the band where it
# counts as taken up, and the chain now.
func _draw_guide() -> void:
	var left := (size.x - GUIDE_WIDTH) / 2.0
	var bar := Rect2(left, GUIDE_TOP, GUIDE_WIDTH, GUIDE_HEIGHT)
	draw_rect(bar.grow(2.0), BACK_COLOUR)
	draw_rect(bar, GUIDE_COLOUR)
	var ok_from := _guide_x(_taut_within_m)
	draw_rect(Rect2(left + ok_from, GUIDE_TOP, _guide_x(0.0) - ok_from, GUIDE_HEIGHT),
			GUIDE_OK_COLOUR)
	var now := left + _guide_x(_slack_m)
	draw_line(Vector2(now, GUIDE_TOP - 4.0), Vector2(now, GUIDE_TOP + GUIDE_HEIGHT + 4.0),
			Color.WHITE, 3.0)


# Where the gauge shows `slack_m`, from its left end [px].
func _guide_x(slack_m: float) -> float:
	var share := (GUIDE_SLACK_M - slack_m) / (GUIDE_SLACK_M + GUIDE_STRETCH_M)
	return GUIDE_WIDTH * clampf(share, 0.0, 1.0)


func _draw_gauge() -> void:
	var top_bar := ceilf(_full_boost_bar * GAUGE_HEADROOM / GAUGE_TICK_BAR) \
			* GAUGE_TICK_BAR
	draw_circle(GAUGE_CENTRE, GAUGE_RADIUS + 4.0, BACK_COLOUR)
	draw_arc(GAUGE_CENTRE, GAUGE_RADIUS - 4.0, GAUGE_FROM_RAD,
			GAUGE_FROM_RAD + GAUGE_SWEEP_RAD, 48, Color(1.0, 1.0, 1.0, 0.25), 4.0)
	var ticks := int(roundf(top_bar / GAUGE_TICK_BAR))
	for i in range(ticks + 1):
		var dir := Vector2.from_angle(_gauge_angle(i * GAUGE_TICK_BAR / top_bar))
		draw_line(GAUGE_CENTRE + dir * (GAUGE_RADIUS - 12.0), GAUGE_CENTRE + dir * GAUGE_RADIUS,
				Color.WHITE, 2.0)
	var full := Vector2.from_angle(_gauge_angle(_full_boost_bar / top_bar))
	draw_line(GAUGE_CENTRE + full * (GAUGE_RADIUS - 14.0), GAUGE_CENTRE + full * GAUGE_RADIUS,
			RATED_COLOUR, 4.0)
	var share := clampf(boost_bar() / top_bar, 0.0, 1.0)
	if share > 0.0:
		draw_arc(GAUGE_CENTRE, GAUGE_RADIUS - 4.0, GAUGE_FROM_RAD, _gauge_angle(share), 48,
				GAUGE_COLOUR, 5.0)
	var needle := Vector2.from_angle(_gauge_angle(share))
	draw_line(GAUGE_CENTRE, GAUGE_CENTRE + needle * (GAUGE_RADIUS - 8.0), Color.WHITE, 3.0)
	draw_circle(GAUGE_CENTRE, 4.0, Color.WHITE)


# Where on the dial a share of its scale is [rad, clockwise on screen].
func _gauge_angle(share: float) -> float:
	return GAUGE_FROM_RAD + GAUGE_SWEEP_RAD * share


func _mark(bar: Rect2, share: float, colour: Color) -> void:
	var x := bar.position.x + bar.size.x * clampf(share, 0.0, 1.0)
	draw_line(Vector2(x, bar.position.y - 4.0), Vector2(x, bar.end.y + 4.0), colour, 4.0)
