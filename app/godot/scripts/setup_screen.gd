# Just before a pull (or a practice pull), the pit board: how to set the
# tractor up for today's track. On the left the track's card (its grip,
# how firm it is, the crew chief's tip, and the lane as the crew walked it,
# LaneMap: the hard and weak spots they found, rougher the fewer pulls
# they have seen) with where the sled starts across the lane, from full
# left to full right (the crew's own choice marked); on the right the
# tractor (TractorSketch) with
# the weights drawn where they sit, the hitch's hole and the rear tires'
# footprint, and under it the three settings:
#
# - the weights slide from the carrier behind the rear wheels to the front
#   bracket (more on the rear axle grips harder but lifts the nose sooner);
# - the rear tires' pressure from soft (a bigger footprint for soft tracks)
#   to hard (lugs that bite on firm ones);
# - an adjustable pulling hitch from its lowest hole to its top one
#   (higher puts more of the sled's pull on the rear tires, but lifts the
#   nose sooner).
#
# A line names the controls (picked in the settings). TractorPullEvent
# keeps the setting for the season; the run takes it from there.
class_name SetupScreen
extends Control

signal tuning_changed(ballast_position: float, pressure_bar: float, hitch_m: float)
signal start_changed(y_m: float)
signal pull_requested
signal back_requested

const STAGE := Vector2(1280.0, 720.0)
const BACKGROUND := Color(0.06, 0.07, 0.09, 0.985)
const SLIDER_HEIGHT := 58.0
# The pressure moves in steps of this much [bar].
const PRESSURE_STEP_BAR := 0.05
# The hitch's holes are this far apart [m].
const HITCH_STEP_M := 0.02
# The sled is placed to this much across the lane [m].
const START_STEP_M := 0.1
const START_COLOUR := Color(0.42, 0.77, 0.42, 0.9)
const WEIGHT_COLOUR := Color(0.85, 0.6, 0.2, 0.9)
const TIRE_COLOUR := Color(0.6, 0.6, 0.65, 0.9)
const HITCH_COLOUR := Color(0.35, 0.55, 0.85, 0.9)
# The track card's scales: grip (the surface's grip factor) and firmness
# (the cone index, on a log scale) [kPa].
const GRIP_LOW := 0.7
const GRIP_HIGH := 1.5
const FIRM_LOW_KPA := 500.0
const FIRM_HIGH_KPA := 7000.0

var ballast_slider := TouchSlider.new()
var pressure_slider := TouchSlider.new()
var hitch_slider := TouchSlider.new()
var start_slider := TouchSlider.new()
var pull_button: TouchButton
var back_button: TouchButton
var _stage := Control.new()
var _title: Label
var _track_name: Label
var _grip: Label
var _lane := LaneMap.new()
var _lane_note: Label
var _start_note: Label
var _sent_start := 0.0
# Where the crew would start the sled [m, to the left].
var _crew_y_m := 0.0
# What the crew chief says: the track's tip, the gear the crew's trials
# liked (0 none) and whether the hitch is in its lowest hole.
var _track_tip := ""
var _crew_gear := 0
var _hitch_low := false
var _grip_bar := ColorRect.new()
var _firm: Label
var _firm_bar := ColorRect.new()
var _tip: Label
var _sketch := TractorSketch.new()
var _shares: Label
var _weights: Label
var _pressure: Label
var _hitch: Label
var _hitch_panel: Panel
var _controls: Label
var _sent := Vector3(-1.0, -1.0, -1.0)


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var background := ColorRect.new()
	background.color = BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	_stage.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stage.set_anchors_preset(Control.PRESET_CENTER)
	_stage.offset_left = -STAGE.x / 2.0
	_stage.offset_right = STAGE.x / 2.0
	_stage.offset_top = -STAGE.y / 2.0
	_stage.offset_bottom = STAGE.y / 2.0
	add_child(_stage)

	_panel(_stage, Rect2(0, 0, STAGE.x, 64), Color(0.04, 0.05, 0.07, 0.9), 0)
	_title = _label(_stage, Rect2(28, 12, 860, 40), 26, GarageArt.TEXT)
	back_button = _button(_stage, "Back", Rect2(940, 8, 140, 48), 24)
	pull_button = _button(_stage, "Pull ›", Rect2(1100, 8, 156, 48), 26)
	pull_button.selected = true
	pull_button.tapped.connect(func() -> void: pull_requested.emit())
	back_button.tapped.connect(func() -> void: back_requested.emit())

	var card := _panel(_stage, Rect2(28, 84, 330, 560), GarageArt.PANEL, 14)
	_label(card, Rect2(16, 10, 300, 20), 13, GarageArt.DIM).text = "TODAY'S TRACK"
	_track_name = _label(card, Rect2(16, 28, 300, 28), 19, GarageArt.TEXT)
	_track_name.clip_text = true
	_grip = _scale_row(card, 60, "Grip", _grip_bar, GarageArt.GREEN)
	_firm = _scale_row(card, 98, "Firmness", _firm_bar, Color(0.79, 0.75, 0.42))
	var tip_box := _panel(card, Rect2(16, 140, 298, 86), Color(0.16, 0.13, 0.08), 10)
	_tip = _label(tip_box, Rect2(10, 6, 278, 76), 14, Color(0.91, 0.83, 0.67))
	_tip.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_label(card, Rect2(16, 232, 140, 20), 13, GarageArt.DIM).text = "THE LANE"
	_lane_note = _label(card, Rect2(90, 232, 224, 20), 13, GarageArt.DIM)
	_lane_note.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_lane.position = Vector2(16, 254)
	_lane.size = Vector2(298, 198)
	card.add_child(_lane)
	start_slider.colour = START_COLOUR
	start_slider.label = "start"
	start_slider.position = Vector2(16, 458)
	start_slider.size = Vector2(298, 52)
	start_slider.custom_minimum_size = start_slider.size
	card.add_child(start_slider)
	_start_note = _label(card, Rect2(16, 514, 298, 40), 14, GarageArt.TEXT)
	_start_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART

	var view := _panel(_stage, Rect2(380, 84, 872, 340), GarageArt.PANEL, 14)
	_sketch.position = Vector2(30, 34)
	_sketch.size = Vector2(812, 296)
	view.add_child(_sketch)
	_shares = _label(view, Rect2(18, 10, 836, 22), 15, GarageArt.DIM)
	_controls = _label(view, Rect2(18, 10, 836, 22), 15, GarageArt.DIM)
	_controls.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT

	_weights = _setting(Rect2(380, 440, 280, 204), "WEIGHTS", "ballast_kg", ballast_slider,
			WEIGHT_COLOUR, "rear", "front")
	_pressure = _setting(Rect2(676, 440, 280, 204), "REAR TIRES", "rear_traction",
			pressure_slider, TIRE_COLOUR, "soft", "hard")
	_hitch = _setting(Rect2(972, 440, 280, 204), "HITCH", "hitch_height_m", hitch_slider,
			HITCH_COLOUR, "low", "top")
	_hitch_panel = hitch_slider.get_parent() as Panel


# `title` names the pull to come; `tuning` is TractorPullEvent.get_tuning();
# `pro` and `tilt` the controls that will drive (`tilt` only where the
# phone can steer by tilting); `track` the event's track
# (TractorPullEvent.get_event()["track"]: grip, cone_index_kpa, tip),
# `track_name` its name and `crew` what the crew found walking the lane
# (TractorPullEvent.get_crew_report()).
func show_setup(title: String, tuning: Dictionary, pro: bool, tilt: bool,
		track: Dictionary = {}, track_name: String = "", crew: Dictionary = {}) -> void:
	_title.text = title
	ballast_slider.visible = tuning.get("ballast", false)
	pressure_slider.visible = tuning.get("pressure", false)
	pressure_slider.start_value = tuning.get("min_bar", 0.0)
	pressure_slider.end_value = tuning.get("max_bar", 1.0)
	var hitch: bool = tuning.get("hitch", false)
	hitch_slider.visible = hitch
	_hitch_panel.modulate.a = 1.0 if hitch else 0.45
	hitch_slider.start_value = tuning.get("min_hitch_m", 0.0)
	hitch_slider.end_value = tuning.get("max_hitch_m", 1.0)
	ballast_slider.value = tuning.get("ballast_position", 0.0)
	pressure_slider.value = tuning.get("pressure_bar", 0.0)
	hitch_slider.value = tuning.get("hitch_m", 0.0)
	_sent = Vector3(ballast_slider.value, pressure_slider.value, _hitch_m())
	for slider: TouchSlider in [ballast_slider, pressure_slider, hitch_slider]:
		slider.queue_redraw()
	_show_track(track, track_name)
	_show_crew(crew)
	show_values(tuning)
	_controls.text = "Controls: %s, %s steering" % ["pro" if pro else "easy",
			"tilt" if tilt else "slider"]


# The settings in words and on the tractor, from
# TractorPullEvent.get_tuning().
func show_values(tuning: Dictionary) -> void:
	var ballast: bool = tuning.get("ballast", false)
	var share := float(tuning.get("rear_share", 0.0))
	_weights.text = "%d %% forward" % roundi(float(tuning.get("ballast_position", 0.0)) * 100.0) \
			if ballast else "No weights to move"
	_pressure.text = "%.2f bar" % float(tuning.get("pressure_bar", 0.0)) \
			if tuning.get("pressure", false) else "Fixed pressure"
	_hitch.text = "%.2f m" % float(tuning.get("hitch_m", 0.0)) \
			if tuning.get("hitch", false) else "Fixed hitch"
	var hitch_low: bool = tuning.get("hitch", false) and float(tuning.get("hitch_m", 0.0)) \
			< float(tuning.get("min_hitch_m", 0.0)) + HITCH_STEP_M / 2.0
	if hitch_low != _hitch_low:
		_hitch_low = hitch_low
		_show_tip()
	_shares.text = "At rest: %d %% on the rear axle, %d %% on the front" % [roundi(share * 100.0),
			roundi((1.0 - share) * 100.0)] if share > 0.0 else ""
	_sketch.ballast_share = float(tuning.get("ballast_position", 0.0)) if ballast else -1.0
	_sketch.ballast_kg = float(tuning.get("ballast_kg", 0.0))
	_sketch.hitch_low_m = float(tuning.get("min_hitch_m", 0.0)) if tuning.get("hitch", false) \
			else 0.0
	_sketch.hitch_top_m = float(tuning.get("max_hitch_m", 0.5))
	_sketch.hitch_m = float(tuning.get("hitch_m", 0.5))
	if tuning.get("pressure", false):
		var low := float(tuning.get("min_bar", 0.5))
		var high := float(tuning.get("max_bar", 1.6))
		_sketch.footprint = clampf(inverse_lerp(high, low, float(tuning.get("pressure_bar", 1.0))),
				0.0, 1.0)
	else:
		_sketch.footprint = -1.0
	_sketch.queue_redraw()


func _process(_delta: float) -> void:
	if not visible:
		return
	var bar := snappedf(pressure_slider.value, PRESSURE_STEP_BAR)
	var now := Vector3(ballast_slider.value, bar, _hitch_m())
	if now != _sent:
		_sent = now
		tuning_changed.emit(now.x, now.y, now.z)
	var start := start_y_m()
	if start != _sent_start:
		_sent_start = start
		_lane.show_start(start)
		_start_note.text = _start_words(start)
		start_changed.emit(start)


# Where the start slider puts the sled, to the left of the lane's middle,
# in START_STEP_M steps [m].
func start_y_m() -> float:
	return snappedf(start_slider.value, START_STEP_M)


# The hitch's hole the slider is at, from the top one down; 0 for the top
# hole (and a fixed hitch), so that a drawbar bought later raises it.
func _hitch_m() -> float:
	if not hitch_slider.visible:
		return 0.0
	var top := hitch_slider.end_value
	var hole := clampf(top - snappedf(top - hitch_slider.value, HITCH_STEP_M),
			hitch_slider.start_value, top)
	return 0.0 if hole >= top else hole


func release_sliders() -> void:
	ballast_slider.release()
	pressure_slider.release()
	hitch_slider.release()
	start_slider.release()


func release_buttons() -> void:
	pull_button.release()
	back_button.release()


func _show_track(track: Dictionary, track_name: String) -> void:
	_track_name.text = track_name
	var grip := float(track.get("grip", 1.0))
	var kpa := float(track.get("cone_index_kpa", 1500.0))
	_grip_bar.size.x = 298.0 * clampf(inverse_lerp(GRIP_LOW, GRIP_HIGH, grip), 0.04, 1.0)
	_firm_bar.size.x = 298.0 * clampf(
			inverse_lerp(log(FIRM_LOW_KPA), log(FIRM_HIGH_KPA), log(maxf(kpa, 1.0))), 0.04, 1.0)
	_grip.text = "low" if grip < 0.95 else ("fair" if grip < 1.15 else
			("high" if grip < 1.35 else "very high"))
	_firm.text = "soft" if kpa < 1000.0 else ("firm" if kpa < 3000.0 else "hard")
	_track_tip = String(track.get("tip", ""))
	_show_tip()


# The crew chief's word before the pull: the track's tip, the gear that
# went furthest in the crew's trials with the tractor as it was brought,
# and, with the hitch in its lowest hole, to raise it: the lowest hole
# takes the sled's pull off the rear tires, and on a heavy sled they spin
# out. (The holes above it depend on the weights and the track: no tip.)
func _show_tip() -> void:
	var words: Array[String] = []
	if not _track_tip.is_empty():
		words.append(_track_tip)
	if _crew_gear > 0:
		words.append("Try %s gear." % _ordinal(_crew_gear))
	if _hitch_low:
		words.append("Raise the hitch, or the tires spin.")
	_tip.text = "Crew chief: \"%s\"" % " ".join(words) if not words.is_empty() else \
			"Set the weights, tires and hitch for the track."


# What the crew chief says now.
func tip_text() -> String:
	return _tip.text


static func _ordinal(n: int) -> String:
	if n % 100 >= 11 and n % 100 <= 13:
		return "%dth" % n
	match n % 10:
		1:
			return "%dst" % n
		2:
			return "%dnd" % n
		3:
			return "%drd" % n
	return "%dth" % n


# The lane as the crew found it, and the start slider from full left (its
# left end) to full right, at the player's start; the crew's own line
# marked on it.
func _show_crew(crew: Dictionary) -> void:
	_crew_gear = int(crew.get("gear", 0))
	_show_tip()
	_lane.show_report(crew)
	var max_m := float(crew.get("max_y_m", 0.0))
	start_slider.visible = max_m > 0.0
	start_slider.start_value = max_m
	start_slider.end_value = -max_m
	start_slider.value = float(crew.get("start_y_m", 0.0))
	var suggested := float(crew.get("suggested_y_m", 0.0))
	start_slider.set_band(suggested, suggested, suggested)
	_sent_start = start_y_m()
	_lane.show_start(_sent_start)
	_crew_y_m = suggested
	_start_note.text = _start_words(_sent_start)
	if crew.is_empty():
		_lane_note.text = ""
		return
	var hard := 0
	var weak := 0
	for spot: Dictionary in crew.get("spots", []):
		if spot.get("kind", "") == "hard":
			hard += 1
		else:
			weak += 1
	_lane_note.text = "%d hard · %d weak · %s" % [hard, weak, _seen_words(crew)]


# How well the crew knows the lane, from what it has seen.
func _seen_words(crew: Dictionary) -> String:
	if crew.get("pull_off", false):
		return "well known"
	var seen := int(crew.get("pulls_seen", 0))
	return "a rough guess" if seen == 0 else ("well known" if seen >= 8 else "known better")


# Where the player starts the sled, and where the crew would.
func _start_words(y_m: float) -> String:
	if not start_slider.visible:
		return ""
	return "Start %s; the crew says %s." % [_side_words(y_m), _side_words(_crew_y_m)]


func _side_words(y_m: float) -> String:
	if absf(y_m) < START_STEP_M / 2.0:
		return "in the middle"
	return "%.1f m %s" % [absf(y_m), "left" if y_m > 0.0 else "right"]




# A labelled bar on the track card; returns the label for its word.
func _scale_row(card: Panel, y: float, name_text: String, bar: ColorRect, colour: Color) -> Label:
	_label(card, Rect2(16, y, 150, 22), 15, GarageArt.TEXT).text = name_text
	var word := _label(card, Rect2(160, y, 154, 22), 15, colour)
	word.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	var track := ColorRect.new()
	track.color = GarageArt.LINE
	track.position = Vector2(16, y + 26)
	track.size = Vector2(298, 8)
	card.add_child(track)
	bar.color = colour
	bar.position = track.position
	bar.size = Vector2(150, 8)
	card.add_child(bar)
	return word


# A setting's panel: its heading and icon, its value in words (returned)
# and the slider with its ends named.
func _setting(rect: Rect2, heading: String, icon: String, slider: TouchSlider, colour: Color,
		start: String, end: String) -> Label:
	var panel := _panel(_stage, rect, GarageArt.PANEL, 14)
	_label(panel, Rect2(16, 12, 200, 20), 13, GarageArt.DIM).text = heading
	var art := Control.new()
	art.position = Vector2(rect.size.x - 44, 8)
	art.size = Vector2(30, 30)
	art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	art.draw.connect(func() -> void:
		GarageArt.draw_icon(art, icon, art.size / 2.0, 26.0, colour))
	panel.add_child(art)
	var value := _label(panel, Rect2(16, 38, rect.size.x - 32, 34), 22, GarageArt.TEXT)
	slider.colour = colour
	slider.position = Vector2(16, 86)
	slider.size = Vector2(rect.size.x - 32, SLIDER_HEIGHT)
	slider.custom_minimum_size = slider.size
	panel.add_child(slider)
	var ends := _label(panel, Rect2(16, 152, rect.size.x - 32, 22), 13, GarageArt.DIM)
	ends.text = start
	var right := _label(panel, Rect2(16, 152, rect.size.x - 32, 22), 13, GarageArt.DIM)
	right.text = end
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	return value


func _panel(parent: Control, rect: Rect2, colour: Color, corner: int) -> Panel:
	var panel := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = colour
	style.border_color = GarageArt.LINE
	style.set_border_width_all(1 if corner > 0 else 0)
	style.set_corner_radius_all(corner)
	panel.add_theme_stylebox_override("panel", style)
	panel.position = rect.position
	panel.size = rect.size
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(panel)
	return panel


func _label(parent: Control, rect: Rect2, font_size: int, colour: Color) -> Label:
	var label := Label.new()
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", colour)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label


func _button(parent: Control, text: String, rect: Rect2, font_size: int) -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.font_size = font_size
	button.fires_on_release = true
	button.position = rect.position
	button.size = rect.size
	parent.add_child(button)
	return button
