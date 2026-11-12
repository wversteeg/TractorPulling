# Where the track ends: the clay runs out 30 m past the 100 m line, and a
# pile of dirt across the lane from there stops anything that would roll on
# (130 to 140 m on a 100 m track). Past the pile stand the event's trailers:
# the sound trailer with its speakers, and at a club or a big event a
# generator and a trailer with a big LED screen facing the start.
#
# The screen shows another camera during the run: a TV camera beside the
# track that follows the tractor (show_live()), drawn into a small
# SubViewport a few times a second, for a phone's frame. After the run it
# shows the day's standings (show_standings()): text over a dark panel, the
# 3D off.
class_name TrackEnd
extends Node3D

# The pile: from this far past the track's end (where the clay ends), this
# long, this high, and this much wider than the clay strip on each side [m].
const PILE_FROM_M := 30.0
const PILE_LENGTH_M := 10.0
const PILE_HEIGHT_M := 2.6
const PILE_PAST_STRIP_M := 2.0
const PILE_DARKER := 0.15
# The trailers stand this far past the pile's far foot [m].
const TRAILERS_PAST_PILE_M := 8.0
# A trailer: its depth along the track, height and length across it, its
# wheels' radius and width, how far they stand either side of its middle
# (a tandem axle), and its drawbar's length [m].
const TRAILER_SIZE_M := Vector3(2.4, 2.6, 7.0)
const TRAILER_WHEEL_M := 0.4
const TRAILER_WHEEL_WIDTH_M := 0.3
const TRAILER_AXLES_APART_M := 1.2
const DRAWBAR_M := 1.6
const TYRE := Color(0.06, 0.06, 0.06)
const DRAWBAR := Color(0.2, 0.2, 0.2)
# The trailers stand in a row across the track, from this far right of the
# clay strip, this far apart (drawbar to tail), each a step further from
# the pile; the screen trailer left of the middle, this far over [m].
const TRAILERS_OUT_M := 1.0
const TRAILER_GAP_M := 1.5
const TRAILER_STEP_BACK_M := 2.0
const SCREEN_LEFT_SHARE := 0.25
# The screen trailer's base, and how high its mast lifts it [m].
const SCREEN_BASE_HEIGHT_M := 1.2
const SOUND_TRAILER := Color(0.12, 0.13, 0.15)
const GENERATOR := Color(0.85, 0.65, 0.12)
const SCREEN_TRAILER := Color(0.75, 0.76, 0.78)
const SPEAKER := Color(0.05, 0.05, 0.06)
# The screen: its size, how high its foot stands, and its frame [m].
const SCREEN_SIZE_M := Vector2(6.4, 3.6)
const SCREEN_FOOT_M := 4.2
const SCREEN_FRAME_M := 0.25
# What it shows: this many pixels, redrawn this often during the run (a
# second view of the scene costs a phone's frame: kept small and slow) [px,
# Hz].
const SCREEN_PIXELS := Vector2i(384, 216)
const SCREEN_RATE_HZ := 12.0
const SCREEN_GLOW := 1.4
# The TV camera: beside the track on the right, this far out from the
# middle, this high, this far ahead of the tractor, and its field of view
# [m, m, m, deg].
const TV_OUT_M := 13.0
const TV_HEIGHT_M := 3.5
const TV_AHEAD_M := 6.0
const TV_FOV_DEG := 32.0
# The standings: this many lines, the title and the panel's colours.
const STANDINGS_LINES := 7
const STANDINGS_TITLE := "TODAY'S STANDINGS"
const PANEL := Color(0.02, 0.03, 0.08)
const TITLE := Color(1.0, 0.75, 0.15)
const LINE_TEXT := Color(0.93, 0.94, 0.96)

var _screen_view: SubViewport = null
var _tv: Camera3D = null
var _standings: Control = null
var _standings_text: Label = null
var _mode := "none"
var _lines := PackedStringArray()
var _since_s := 0.0
var _lane_m := 12.0
var _pile_at_m := Vector2.ZERO
var _trailers := 0


# Built for a track `length_m` long with a lane `lane_m` wide, the clay in
# `shades` (VenueView.track_colours()), `strip_m` wide; `trailers` of them
# (the sound trailer first, then a generator), and a screen if `screen`.
func build(length_m: float, lane_m: float, strip_m: float, shades: Array[Color],
		trailers: int, screen: bool) -> void:
	for child: Node in get_children():
		child.free()
	_screen_view = null
	_tv = null
	_mode = "none"
	_lane_m = lane_m
	var from_m := length_m + PILE_FROM_M
	_pile_at_m = Vector2(from_m, from_m + PILE_LENGTH_M)
	_add_pile(from_m, strip_m, shades)
	var at_m := from_m + PILE_LENGTH_M + TRAILERS_PAST_PILE_M
	_trailers = 0
	var colours: Array[Color] = [SOUND_TRAILER, GENERATOR]
	# Each trailer's middle: half its length out from where the last one's
	# drawbar ends, with a gap between.
	var y_m := -(strip_m / 2.0 + TRAILERS_OUT_M + TRAILER_SIZE_M.z / 2.0)
	for i in mini(trailers, colours.size()):
		_add_trailer(at_m + TRAILER_STEP_BACK_M * i, y_m, colours[i], i == 0)
		y_m -= TRAILER_SIZE_M.z + DRAWBAR_M + TRAILER_GAP_M
		_trailers += 1
	if screen:
		_add_screen(at_m, strip_m * SCREEN_LEFT_SHARE)


# Where the pile lies along the track, from and to [m].
func pile_at_m() -> Vector2:
	return _pile_at_m


func trailer_count() -> int:
	return _trailers


func has_screen() -> bool:
	return _screen_view != null


# "live", "standings" or "none" (no screen, or nothing shown yet).
func screen_mode() -> String:
	return _mode


func screen_lines() -> PackedStringArray:
	return _lines


# The ridge across the track: a prism of the track's own clay, a little
# darker for being turned over and piled up.
func _add_pile(from_m: float, strip_m: float, shades: Array[Color]) -> void:
	var prism := PrismMesh.new()
	prism.size = Vector3(PILE_LENGTH_M, PILE_HEIGHT_M, strip_m + 2.0 * PILE_PAST_STRIP_M)
	var dirt := StandardMaterial3D.new()
	dirt.albedo_texture = Art.ground_texture(shades[0].darkened(PILE_DARKER),
			shades[1].darkened(PILE_DARKER), 0.08, 17)
	dirt.normal_enabled = true
	dirt.normal_texture = Art.ground_normal(0.1, 18, 6.0)
	dirt.uv1_scale = Vector3(3.0, 1.0, 3.0)
	dirt.roughness = 1.0
	var pile := Art.instance(prism, dirt)
	# The prism's ridge runs along its z: turned to run across the track.
	pile.rotation.y = PI / 2.0
	pile.position = TrackSpace.point(from_m + PILE_LENGTH_M / 2.0, 0.0, PILE_HEIGHT_M / 2.0 - 0.05)
	add_child(VenueView._quiet(pile))
	# Lumps along its foot, so it reads as a heap and not a wedge.
	var lumps: Array[Transform3D] = []
	var y_m := -strip_m / 2.0
	var i := 0
	while y_m < strip_m / 2.0:
		var x_m := from_m + (0.6 if i % 2 == 0 else PILE_LENGTH_M - 0.6)
		var size_m := 0.5 + 0.25 * float(i % 3)
		lumps.append(Transform3D(Basis().scaled(Vector3(size_m * 1.6, size_m, size_m * 1.3)),
				TrackSpace.point(x_m, y_m, 0.1)))
		y_m += 1.7
		i += 1
	var sphere := SphereMesh.new()
	sphere.radial_segments = 8
	sphere.rings = 4
	add_child(VenueView._quiet(Art.many(sphere, lumps, dirt)))


# A box trailer facing across the track, its drawbar to the right; the
# sound trailer has its side open on speakers stacked facing the start.
func _add_trailer(x_m: float, y_m: float, colour: Color, speakers: bool) -> void:
	var trailer := Node3D.new()
	trailer.position = TrackSpace.point(x_m, y_m, 0.0)
	add_child(trailer)
	var body := Art.box(Vector3(TRAILER_SIZE_M.z, TRAILER_SIZE_M.y, TRAILER_SIZE_M.x), colour, 0.6)
	body.position = Vector3(0.0, TRAILER_WHEEL_M * 2.0 + TRAILER_SIZE_M.y / 2.0, 0.0)
	trailer.add_child(VenueView._quiet(body))
	_add_running_gear(trailer)
	if not speakers:
		return
	# Its side towards the start open: speaker cabinets in two stacks.
	for stack: float in [-1.0, 1.0]:
		for level in 3:
			var cabinet := Art.box(Vector3(1.1, 0.75, 0.8), SPEAKER, 0.9)
			cabinet.position = Vector3(stack * 1.8, TRAILER_WHEEL_M * 2.0 + 0.45 + 0.8 * level,
					TRAILER_SIZE_M.x / 2.0 + 0.4)
			trailer.add_child(VenueView._quiet(cabinet))
			var cone := Art.cylinder(0.26, 0.05, Color(0.2, 0.2, 0.22), 16)
			cone.rotation.x = PI / 2.0
			cone.position = cabinet.position + Vector3(0.0, 0.0, 0.42)
			trailer.add_child(VenueView._quiet(cone))
	# The sign between the stacks, under the roof.
	var sign := Art.label("SOUND", 55, Color.WHITE, Color(0.0, 0.0, 0.0, 0.0))
	sign.position = Vector3(0.0, TRAILER_WHEEL_M * 2.0 + TRAILER_SIZE_M.y - 0.4,
			TRAILER_SIZE_M.x / 2.0 + 0.02)
	trailer.add_child(sign)


# A trailer's wheels (two axles under its middle, a wheel at each end) and
# its drawbar to the right, under `trailer` (lying across the track).
func _add_running_gear(trailer: Node3D) -> void:
	for along: float in [-1.0, 1.0]:
		for side: float in [-1.0, 1.0]:
			var wheel := Art.cylinder(TRAILER_WHEEL_M, TRAILER_WHEEL_WIDTH_M, TYRE, 16)
			wheel.rotation.x = PI / 2.0
			wheel.position = Vector3(along * TRAILER_AXLES_APART_M / 2.0, TRAILER_WHEEL_M,
					side * TRAILER_SIZE_M.x / 2.0)
			trailer.add_child(VenueView._quiet(wheel))
	var drawbar := Art.box(Vector3(DRAWBAR_M, 0.12, 0.12), DRAWBAR)
	drawbar.position = Vector3(TRAILER_SIZE_M.z / 2.0 + DRAWBAR_M / 2.0, TRAILER_WHEEL_M * 2.0, 0.0)
	trailer.add_child(VenueView._quiet(drawbar))


# The screen trailer: a mast lifting the screen high over the pile, facing
# the start line; what it shows is drawn into a SubViewport.
func _add_screen(x_m: float, y_m: float) -> void:
	var foot := TrackSpace.point(x_m, y_m, 0.0)
	var trailer := Node3D.new()
	trailer.position = foot
	add_child(trailer)
	var base := Art.box(Vector3(TRAILER_SIZE_M.z, SCREEN_BASE_HEIGHT_M, TRAILER_SIZE_M.x),
			SCREEN_TRAILER, 0.5)
	base.position = Vector3(0.0, TRAILER_WHEEL_M * 2.0 + SCREEN_BASE_HEIGHT_M / 2.0, 0.0)
	trailer.add_child(VenueView._quiet(base))
	_add_running_gear(trailer)
	var deck_m := TRAILER_WHEEL_M * 2.0 + SCREEN_BASE_HEIGHT_M
	var mast := Art.box(Vector3(0.35, SCREEN_FOOT_M - deck_m, 0.35), DRAWBAR, 0.5, 0.5)
	mast.position = foot + Vector3(0.0, (SCREEN_FOOT_M + deck_m) / 2.0, 0.0)
	add_child(VenueView._quiet(mast))
	var middle := foot + Vector3(0.0, SCREEN_FOOT_M + SCREEN_SIZE_M.y / 2.0, 0.0)
	var frame := Art.box(Vector3(SCREEN_SIZE_M.x + 2.0 * SCREEN_FRAME_M,
			SCREEN_SIZE_M.y + 2.0 * SCREEN_FRAME_M, 0.3), Color(0.03, 0.03, 0.04), 0.4)
	frame.position = middle
	add_child(VenueView._quiet(frame))

	_screen_view = SubViewport.new()
	_screen_view.size = SCREEN_PIXELS
	_screen_view.render_target_update_mode = SubViewport.UPDATE_ONCE
	_screen_view.positional_shadow_atlas_size = 0
	add_child(_screen_view)
	_tv = Camera3D.new()
	_tv.fov = TV_FOV_DEG
	_tv.far = 600.0
	_screen_view.add_child(_tv)
	_add_standings_panel()

	var quad := QuadMesh.new()
	quad.size = SCREEN_SIZE_M
	var glow := StandardMaterial3D.new()
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	glow.albedo_texture = _screen_view.get_texture()
	glow.albedo_color = Color(SCREEN_GLOW, SCREEN_GLOW, SCREEN_GLOW)
	var face := Art.instance(quad, glow)
	# A quad faces +z: in TrackSpace towards the start line.
	face.position = middle + Vector3(0.0, 0.0, 0.16)
	add_child(VenueView._quiet(face))
	show_standings(PackedStringArray())


func _add_standings_panel() -> void:
	_standings = ColorRect.new()
	(_standings as ColorRect).color = PANEL
	_standings.size = Vector2(SCREEN_PIXELS)
	_screen_view.add_child(_standings)
	var title := Label.new()
	title.text = STANDINGS_TITLE
	title.position = Vector2(12, 6)
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", TITLE)
	_standings.add_child(title)
	_standings_text = Label.new()
	_standings_text.position = Vector2(12, 40)
	_standings_text.size = Vector2(SCREEN_PIXELS) - Vector2(24, 46)
	_standings_text.add_theme_font_size_override("font_size", 20)
	_standings_text.add_theme_color_override("font_color", LINE_TEXT)
	_standings_text.clip_text = true
	_standings.add_child(_standings_text)


# During the run: the TV camera follows `target` (the tractor, in Godot's
# space) from beside the track, a little ahead of it; the screen redrawn
# SCREEN_RATE_HZ times a second.
func show_live(target: Vector3, delta: float) -> void:
	if _screen_view == null:
		return
	if _mode != "live":
		_mode = "live"
		_standings.visible = false
		_screen_view.disable_3d = false
		_since_s = 1.0 / SCREEN_RATE_HZ
	_since_s += delta
	if _since_s < 1.0 / SCREEN_RATE_HZ:
		return
	_since_s = 0.0
	# Godot's -z is down the track, -x the track's right.
	var along_m := -target.z
	_tv.position = TrackSpace.point(along_m + TV_AHEAD_M, -TV_OUT_M - _lane_m / 2.0, TV_HEIGHT_M)
	_tv.look_at(target + Vector3(0.0, 1.2, 0.0), Vector3.UP)
	_screen_view.render_target_update_mode = SubViewport.UPDATE_ONCE


# After the run: the day's standings, a line per puller from the top
# (EventTexts.standing_rows()), as many as fit; the 3D off.
func show_standings(rows: Array) -> void:
	if _screen_view == null:
		return
	_mode = "standings"
	_lines = PackedStringArray()
	for row: PackedStringArray in rows.slice(0, STANDINGS_LINES):
		_lines.append("%s %s  %s" % [row[0].lpad(3), row[1], row[2]])
	_standings_text.text = "\n".join(_lines) if not _lines.is_empty() else "No pulls yet."
	_standings.visible = true
	_screen_view.disable_3d = true
	_screen_view.render_target_update_mode = SubViewport.UPDATE_ONCE
