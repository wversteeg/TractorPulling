# The track, big: opened from the pit board by tapping its lane. The lane
# as the crew walked it, drawn lengthwise across the screen (LaneMap: the
# start on the left, the full-pull line on the right, the driver's left at
# the top) with a distance mark every 10 m, and beside it the start, the
# same as the pit board's; under it the ground along the track as it was
# prepared (its grip and how firm it is, from the start to the full-pull
# line) and what the crew found: how sure it is, the strongest spots by
# where they lie, and what each colour on the lane means.
class_name TrackScreen
extends Control

signal done_requested

const STAGE := Vector2(1280.0, 720.0)
const BACKGROUND := Color(0.06, 0.07, 0.09, 0.985)
# The profile's scales: grip (the surface's grip factor) and firmness (the
# cone index, on a log scale) [kPa], as the pit board's track card.
const GRIP_LOW := 0.7
const GRIP_HIGH := 1.5
const FIRM_LOW_KPA := 500.0
const FIRM_HIGH_KPA := 7000.0
const GRIP_COLOUR := Color(0.42, 0.77, 0.42)
const FIRM_COLOUR := Color(0.79, 0.75, 0.42)
const WATER_COLOUR := Color(0.36, 0.55, 0.82)
const PROFILE_WIDTH := 3.0
# The water's scale: dry to this much water per kg of dry soil.
const WATER_HIGH := 0.3
# The ground differs from the start's by more than these shares.
const SAME_GRIP := 0.05
const SAME_FIRMNESS := 0.2
# The ground's stretches told in words, at most.
const ALONG_STRETCHES := 2
# The spots listed, strongest first, and the strength above which a spot
# is called strong, below which mild.
const LISTED_SPOTS := 5
const STRONG := 0.2
const MILD := 0.1
# A spot this close to the lane's middle lies in the middle [m].
const MIDDLE_M := 0.5

var start_slider := TouchSlider.new()
var done_button: TouchButton
var lane := LaneMap.new()
var _stage := Control.new()
var _title: Label
var _lane_note: Label
var _facts: Label
var _crew: Label
var _spots: Label
var _profile_art := Control.new()
# TractorPullEvent.get_event()["track"]["profile"]: the ground every few
# metres along the track.
var _profile: Array = []
var _full_pull_m := 100.0


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

	SetupScreen.make_panel(_stage, Rect2(0, 0, STAGE.x, 64), Color(0.04, 0.05, 0.07, 0.9), 0)
	_title = SetupScreen.make_label(_stage, Rect2(28, 12, 1040, 40), 26, GarageArt.TEXT)
	_title.clip_text = true
	done_button = SetupScreen.make_button(_stage, "Done", Rect2(1100, 8, 156, 48), 26)
	done_button.selected = true
	done_button.tapped.connect(func() -> void: done_requested.emit())

	var lane_panel := SetupScreen.make_panel(_stage, Rect2(28, 78, 1224, 318), GarageArt.PANEL,
			14)
	SetupScreen.make_label(lane_panel, Rect2(16, 8, 400, 20), 13, GarageArt.DIM).text = \
			"THE LANE  ·  START ›  FULL PULL"
	_lane_note = SetupScreen.make_label(lane_panel, Rect2(420, 8, 690, 20), 13, GarageArt.DIM)
	_lane_note.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	lane.lengthwise = true
	lane.marks = true
	lane.position = Vector2(16, 32)
	lane.size = Vector2(1100, 270)
	lane_panel.add_child(lane)
	SetupScreen.make_label(lane_panel, Rect2(1126, 8, 84, 20), 13, GarageArt.DIM).text = "left"
	SetupScreen.make_label(lane_panel, Rect2(1126, 294, 84, 20), 13, GarageArt.DIM).text = "right"
	start_slider.colour = SetupScreen.START_COLOUR
	start_slider.label = "start"
	start_slider.vertical = true
	start_slider.position = Vector2(1132, 32)
	start_slider.size = Vector2(76, 262)
	start_slider.custom_minimum_size = start_slider.size
	lane_panel.add_child(start_slider)

	var ground := SetupScreen.make_panel(_stage, Rect2(28, 410, 600, 296), GarageArt.PANEL, 14)
	SetupScreen.make_label(ground, Rect2(16, 10, 300, 20), 13, GarageArt.DIM).text = \
			"THE GROUND ALONG THE TRACK"
	var keys := [[GRIP_COLOUR, "— grip"], [FIRM_COLOUR, "— firmness"], [WATER_COLOUR, "— water"]]
	for i in keys.size():
		var key := SetupScreen.make_label(ground, Rect2(290 + i * 98, 10, 98, 20), 13, keys[i][0])
		key.text = keys[i][1]
		key.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_profile_art.position = Vector2(16, 38)
	_profile_art.size = Vector2(568, 186)
	_profile_art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_profile_art.draw.connect(_draw_profile)
	ground.add_child(_profile_art)
	_facts = SetupScreen.make_label(ground, Rect2(16, 232, 568, 56), 15, GarageArt.TEXT)
	_facts.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART

	var found := SetupScreen.make_panel(_stage, Rect2(644, 410, 608, 296), GarageArt.PANEL, 14)
	SetupScreen.make_label(found, Rect2(16, 10, 300, 20), 13, GarageArt.DIM).text = \
			"WHAT THE CREW FOUND"
	_crew = SetupScreen.make_label(found, Rect2(16, 34, 576, 44), 15, GarageArt.TEXT)
	_crew.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_spots = SetupScreen.make_label(found, Rect2(16, 82, 576, 150), 15, GarageArt.TEXT)
	var legend := Control.new()
	legend.position = Vector2(16, 238)
	legend.size = Vector2(576, 50)
	legend.mouse_filter = Control.MOUSE_FILTER_IGNORE
	legend.draw.connect(func() -> void: _draw_legend(legend))
	found.add_child(legend)


# `track` is TractorPullEvent.get_event()["track"], `track_name` the
# event's name, `crew` TractorPullEvent.get_crew_report() and `start` the
# pit board's start slider, whose ends and place this one takes.
func show_track(track: Dictionary, track_name: String, crew: Dictionary,
		start: TouchSlider) -> void:
	_title.text = "The track  ·  %s" % track_name
	_full_pull_m = maxf(float(crew.get("full_pull_m", 100.0)), 1.0)
	_profile = track.get("profile", [])
	_profile_art.queue_redraw()
	lane.show_report(crew)
	# Upright beside the lane, whose left is at the top: the pit board's
	# slider runs from full left to full right, this one from full right
	# at the bottom up to full left.
	start_slider.visible = start.visible
	start_slider.start_value = start.end_value
	start_slider.end_value = start.start_value
	start_slider.value = start.value
	var suggested := float(crew.get("suggested_y_m", 0.0))
	start_slider.set_band(suggested, suggested, suggested)
	start_slider.queue_redraw()
	_facts.text = _facts_words(track)
	_crew.text = _crew_words(crew)
	_spots.text = _spot_lines(crew.get("spots", []))
	var hard := 0
	for spot: Dictionary in crew.get("spots", []):
		if spot.get("kind", "") == "hard":
			hard += 1
	var spots: int = crew.get("spots", []).size()
	_lane_note.text = "%d m long  ·  %d m between the chalk lines  ·  %d hard, %d weak spots" % [
			roundi(_full_pull_m), roundi(float(crew.get("lane_width_m", 12.0))), hard,
			spots - hard]


func show_start(y_m: float) -> void:
	lane.show_start(y_m)


func release_buttons() -> void:
	done_button.release()


func release_sliders() -> void:
	start_slider.release()


# What the track is made of and how it was prepared, in words.
static func _facts_words(track: Dictionary) -> String:
	var words := "%d %% clay, %d %% sand, %d %% water at the start." % [
			roundi(float(track.get("clay_fraction", 0.0)) * 100.0),
			roundi((1.0 - float(track.get("clay_fraction", 0.0))) * 100.0),
			roundi(float(track.get("moisture", 0.0)) * 100.0)]
	return words + " " + _along_words(track.get("profile", []))


# How the ground changes down the lane: each stretch that differs from the
# start's (a patch, or from somewhere on to the end), and how, by its most
# different point; the first ALONG_STRETCHES of them.
static func _along_words(profile: Array) -> String:
	if profile.size() < 2:
		return ""
	var first: Dictionary = profile[0]
	var stretches: Array[String] = []
	var count := 0
	var i := 0
	while i < profile.size():
		if not _differs(profile[i], first):
			i += 1
			continue
		var from := i
		var most := i
		while i < profile.size() and _differs(profile[i], first):
			if _difference(profile[i], first) > _difference(profile[most], first):
				most = i
			i += 1
		count += 1
		if count > ALONG_STRETCHES:
			continue
		var how := _change_words(first, profile[most])
		var from_m := roundi(float(profile[from].get("at_m", 0.0)))
		if i >= profile.size():
			stretches.append("from about %d m on: %s" % [from_m, how])
		else:
			stretches.append("a patch from about %d to %d m: %s" % [from_m,
					roundi(float(profile[i - 1].get("at_m", 0.0))), how])
	if stretches.is_empty():
		return "The same all the way down."
	var words := "; ".join(stretches)
	if count > ALONG_STRETCHES:
		words += "; and more further on"
	return words.left(1).to_upper() + words.substr(1) + "."


# How far `a` is from `b`: grip and firmness as shares, together.
static func _difference(a: Dictionary, b: Dictionary) -> float:
	var grip := float(a.get("grip", 1.0)) / maxf(float(b.get("grip", 1.0)), 0.01)
	var firm := float(a.get("cone_index_kpa", 1.0)) / maxf(float(b.get("cone_index_kpa", 1.0)), 1.0)
	return absf(grip - 1.0) + absf(firm - 1.0)


static func _differs(a: Dictionary, b: Dictionary) -> bool:
	var grip := float(a.get("grip", 1.0)) / maxf(float(b.get("grip", 1.0)), 0.01)
	var firm := float(a.get("cone_index_kpa", 1.0)) / maxf(float(b.get("cone_index_kpa", 1.0)), 1.0)
	return absf(grip - 1.0) > SAME_GRIP or absf(firm - 1.0) > SAME_FIRMNESS


# `to` against `from`, in words: firmer or softer, more or less grip.
static func _change_words(from: Dictionary, to: Dictionary) -> String:
	var words: Array[String] = []
	var firm := float(to.get("cone_index_kpa", 1.0)) / maxf(float(from.get("cone_index_kpa", 1.0)),
			1.0)
	if absf(firm - 1.0) > SAME_FIRMNESS:
		words.append("firmer" if firm > 1.0 else "softer")
	var grip := float(to.get("grip", 1.0)) / maxf(float(from.get("grip", 1.0)), 0.01)
	if absf(grip - 1.0) > SAME_GRIP:
		words.append("more grip" if grip > 1.0 else "less grip")
	return " and ".join(words)


# How sure the crew is of where the spots lie.
static func _crew_words(crew: Dictionary) -> String:
	if crew.is_empty():
		return "The crew has not walked the lane."
	var seen := int(crew.get("pulls_seen", 0))
	var pulls := "The pull-off: the crew has seen every pull." if crew.get("pull_off", false) \
			else ("No pulls seen yet: a rough guess." if seen == 0 else ("Seen %d pull%s." % [seen,
			"" if seen == 1 else "s"]))
	return "%s Each spot may be up to %.1f m off along the lane and %.1f m across." % [pulls,
			float(crew.get("along_error_m", 0.0)), float(crew.get("across_error_m", 0.0))]


# The strongest spots, one a line: what, where along and across, how
# strong.
static func _spot_lines(spots: Array) -> String:
	if spots.is_empty():
		return "No spots found: an even lane."
	var sorted := spots.duplicate()
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("strength", 0.0)) > float(b.get("strength", 0.0)))
	var lines: Array[String] = []
	for spot: Dictionary in sorted.slice(0, LISTED_SPOTS):
		lines.append("%s  at %d m, %s  ·  %s" % [
				"Hard spot" if spot.get("kind", "") == "hard" else "Weak spot",
				roundi(float(spot.get("at_m", 0.0))), _across_words(float(spot.get("y_m", 0.0))),
				_strength_words(float(spot.get("strength", 0.0)))])
	if sorted.size() > LISTED_SPOTS:
		lines.append("and %d milder ones" % (sorted.size() - LISTED_SPOTS))
	return "\n".join(lines)


static func _across_words(y_m: float) -> String:
	if absf(y_m) < MIDDLE_M:
		return "in the middle"
	return "%.1f m %s" % [absf(y_m), "left" if y_m > 0.0 else "right"]


static func _strength_words(strength: float) -> String:
	return "strong" if strength >= STRONG else ("mild" if strength < MILD else "medium")


# The grip and the firmness from the start line to the full-pull line,
# each on its own scale, with a faint line every 10 m.
func _draw_profile() -> void:
	var art := _profile_art
	var area := Rect2(Vector2.ZERO, art.size)
	art.draw_rect(area, Color(0.08, 0.09, 0.11))
	var font := art.get_theme_default_font()
	var bottom := area.size.y - 18.0
	var x_m := 0.0
	while x_m <= _full_pull_m + 0.01:
		var x := x_m / _full_pull_m * area.size.x
		art.draw_line(Vector2(x, 0.0), Vector2(x, bottom), GarageArt.LINE, 1.0)
		if x_m > 0.0 and x_m < _full_pull_m - 1.0:
			art.draw_string(font, Vector2(x - 14.0, area.size.y - 3.0), "%d m" % roundi(x_m),
					HORIZONTAL_ALIGNMENT_LEFT, -1, 12, GarageArt.DIM)
		x_m += LaneMap.MARK_EVERY_M
	if _profile.size() < 2:
		return
	var grip := PackedVector2Array()
	var firm := PackedVector2Array()
	var water := PackedVector2Array()
	for point: Dictionary in _profile:
		var x := float(point.get("at_m", 0.0)) / _full_pull_m * area.size.x
		var g := clampf(inverse_lerp(GRIP_LOW, GRIP_HIGH, float(point.get("grip", 1.0))), 0.0, 1.0)
		var kpa := maxf(float(point.get("cone_index_kpa", 1.0)), 1.0)
		var f := clampf(inverse_lerp(log(FIRM_LOW_KPA), log(FIRM_HIGH_KPA), log(kpa)), 0.0, 1.0)
		var w := clampf(float(point.get("moisture", 0.0)) / WATER_HIGH, 0.0, 1.0)
		grip.append(Vector2(x, bottom - g * (bottom - 8.0)))
		firm.append(Vector2(x, bottom - f * (bottom - 8.0)))
		water.append(Vector2(x, bottom - w * (bottom - 8.0)))
	art.draw_polyline(water, WATER_COLOUR, PROFILE_WIDTH - 1.0, true)
	art.draw_polyline(firm, FIRM_COLOUR, PROFILE_WIDTH, true)
	art.draw_polyline(grip, GRIP_COLOUR, PROFILE_WIDTH, true)
	art.draw_string(font, Vector2(4.0, 14.0), "high", HORIZONTAL_ALIGNMENT_LEFT, -1, 12,
			GarageArt.DIM)
	art.draw_string(font, Vector2(4.0, bottom - 4.0), "low", HORIZONTAL_ALIGNMENT_LEFT, -1, 12,
			GarageArt.DIM)


# What the lane's colours and marks mean: one swatch and word each.
func _draw_legend(legend: Control) -> void:
	var font := legend.get_theme_default_font()
	var keys := [
		[LaneMap.HARD, "hard spot: the sled drags"],
		[LaneMap.WEAK, "weak spot: less grip"],
		[Color(LaneMap.HARD, LaneMap.BLUR_ALPHA * 2.0), "haze: where it may be"],
		[Color(0.35, 0.35, 0.35), "your rear tires"],
		[LaneMap.SUGGESTION, "the crew's start"],
		[Color(1.0, 1.0, 1.0, 0.35), "the sled's pan"],
	]
	for i in keys.size():
		var at := Vector2(float(i % 3) * 192.0, floorf(i / 3.0) * 24.0)
		legend.draw_rect(Rect2(at + Vector2(0.0, 4.0), Vector2(14.0, 14.0)), keys[i][0])
		legend.draw_string(font, at + Vector2(20.0, 16.0), keys[i][1], HORIZONTAL_ALIGNMENT_LEFT,
				-1, 13, GarageArt.DIM)
