# The garage's drawings: an icon for each kind of part, drawn with lines on
# any CanvasItem, so the workshop's pins, the part screens and the pit
# board share them. Parts are named as the garage files name them
# (data/garages/: engine_torque, rear_traction, ...) and a choice by its
# slot (engine, rear_tire, driveline). The menus' buttons have icons here
# too, named for what they do (practice, garage, event, ...).
class_name GarageArt
extends RefCounted

const ORANGE := Color(0.91, 0.59, 0.18)
const ORANGE_LIGHT := Color(0.96, 0.71, 0.36)
const BLUE := Color(0.31, 0.53, 0.85)
const BLUE_LIGHT := Color(0.56, 0.71, 0.93)
const GREEN := Color(0.42, 0.77, 0.42)
const RED := Color(0.88, 0.35, 0.31)
const TEXT := Color(0.93, 0.94, 0.96)
const DIM := Color(0.55, 0.57, 0.64)
const PANEL := Color(0.11, 0.125, 0.157, 0.94)
const PANEL_LIGHT := Color(0.145, 0.16, 0.21)
const LINE := Color(0.2, 0.23, 0.28)

# An icon's line width, as a share of its size.
const STROKE_SHARE := 0.075


# The icon for `part` (or a choice's slot), `size` pixels across, centred
# on `centre`.
static func draw_icon(ci: CanvasItem, part: String, centre: Vector2, size: float,
		colour: Color) -> void:
	var w := maxf(size * STROKE_SHARE, 1.5)
	var s := size / 2.0
	match part:
		"engine_torque":
			_engine(ci, centre, s, w, colour)
		"engine":
			_turbo(ci, centre, s, w, colour)
		"rear_traction", "rear_tire":
			_tire(ci, centre, s, w, colour)
		"ballast_kg":
			_weight(ci, centre, s, w, colour)
		"hitch_height_m":
			_hitch(ci, centre, s, w, colour)
		"load_downshift_gears", "driveline":
			_shift(ci, centre, s, w, colour)
		"rated_rpm":
			_governor(ci, centre, s, w, colour)
		"practice":
			_flag(ci, centre, s, w, colour)
		"garage":
			_wrench(ci, centre, s, w, colour)
		"event":
			_trophy(ci, centre, s, w, colour)
		"continue":
			_play(ci, centre, s, colour)
		"new":
			ci.draw_line(centre + Vector2(-0.6, 0.0) * s, centre + Vector2(0.6, 0.0) * s, colour, w * 1.4)
			ci.draw_line(centre + Vector2(0.0, -0.6) * s, centre + Vector2(0.0, 0.6) * s, colour, w * 1.4)
		"move_up":
			_arrow_up(ci, centre, s, w, colour)
		"restart":
			_restart(ci, centre, s, w, colour)
		_:
			ci.draw_arc(centre, s * 0.8, 0.0, TAU, 24, colour, w)


# The menus' icons.
static func _flag(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	ci.draw_line(p.call(-0.55, -0.85), p.call(-0.55, 0.85), colour, w * 1.3)
	ci.draw_colored_polygon(PackedVector2Array([p.call(-0.55, -0.8), p.call(0.15, -0.65),
			p.call(0.7, -0.75), p.call(0.7, 0.0), p.call(0.15, 0.1), p.call(-0.55, -0.05)]), colour)


static func _wrench(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var head := c + Vector2(0.38, -0.38) * s
	ci.draw_line(c + Vector2(-0.65, 0.65) * s, head, colour, w * 2.2)
	ci.draw_arc(head, s * 0.32, PI * 0.5, PI * 2.0, 20, colour, w * 1.6, true)


static func _trophy(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	ci.draw_colored_polygon(PackedVector2Array([p.call(-0.5, -0.8), p.call(0.5, -0.8),
			p.call(0.42, -0.2), p.call(0.15, 0.1), p.call(-0.15, 0.1), p.call(-0.42, -0.2)]), colour)
	ci.draw_arc(p.call(-0.5, -0.45), s * 0.25, PI * 0.5, PI * 1.5, 12, colour, w, true)
	ci.draw_arc(p.call(0.5, -0.45), s * 0.25, -PI * 0.5, PI * 0.5, 12, colour, w, true)
	ci.draw_line(p.call(0.0, 0.1), p.call(0.0, 0.5), colour, w * 1.5)
	ci.draw_rect(Rect2(p.call(-0.4, 0.5), Vector2(0.8, 0.28) * s), colour)


static func _play(ci: CanvasItem, c: Vector2, s: float, colour: Color) -> void:
	ci.draw_colored_polygon(PackedVector2Array([c + Vector2(-0.45, -0.7) * s,
			c + Vector2(0.7, 0.0) * s, c + Vector2(-0.45, 0.7) * s]), colour)


static func _arrow_up(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	ci.draw_line(c + Vector2(0.0, 0.75) * s, c + Vector2(0.0, -0.55) * s, colour, w * 1.6)
	ci.draw_colored_polygon(PackedVector2Array([c + Vector2(0.0, -0.85) * s,
			c + Vector2(0.55, -0.2) * s, c + Vector2(-0.55, -0.2) * s]), colour)


static func _restart(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	ci.draw_arc(c, s * 0.6, -PI * 0.35, PI * 1.45, 28, colour, w * 1.4, true)
	var tip := c + Vector2.from_angle(-PI * 0.35) * s * 0.6
	ci.draw_colored_polygon(PackedVector2Array([tip + Vector2(0.3, -0.05) * s,
			tip + Vector2(-0.25, -0.3) * s, tip + Vector2(-0.05, 0.3) * s]), colour)


static func _engine(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	ci.draw_polyline(PackedVector2Array([p.call(-0.75, -0.2), p.call(-0.5, -0.2), p.call(-0.5, -0.45),
			p.call(0.3, -0.45), p.call(0.3, -0.2), p.call(0.55, -0.2), p.call(0.7, -0.05),
			p.call(0.85, -0.05), p.call(0.85, 0.45), p.call(0.7, 0.45), p.call(0.55, 0.6),
			p.call(-0.45, 0.6), p.call(-0.75, 0.3), p.call(-0.75, -0.2)]), colour, w, true)
	ci.draw_line(p.call(-0.25, -0.65), p.call(0.05, -0.65), colour, w)
	ci.draw_line(p.call(-0.1, -0.65), p.call(-0.1, -0.45), colour, w)
	ci.draw_circle(p.call(0.0, 0.18), s * 0.17, colour)


static func _turbo(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	ci.draw_arc(c, s * 0.62, -PI / 2.0, PI * 1.25, 28, colour, w, true)
	ci.draw_arc(c, s * 0.32, -PI / 2.0, PI * 1.25, 20, colour, w, true)
	ci.draw_line(c + Vector2(0.0, -0.62) * s, c + Vector2(0.75, -0.62) * s, colour, w)
	ci.draw_line(c + Vector2(0.75, -0.62) * s, c + Vector2(0.75, -0.9) * s, colour, w)
	ci.draw_line(c + Vector2(-0.62, 0.0) * s, c + Vector2(-0.95, 0.0) * s, colour, w)
	ci.draw_circle(c, s * 0.1, colour)


static func _tire(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	ci.draw_arc(c, s * 0.72, 0.0, TAU, 32, colour, w, true)
	ci.draw_arc(c, s * 0.32, 0.0, TAU, 20, colour, w, true)
	for i in 10:
		var a := TAU * i / 10.0
		var out := Vector2(cos(a), sin(a))
		ci.draw_line(c + out * s * 0.72, c + out * s * 0.95, colour, w * 1.6)
	ci.draw_circle(c, s * 0.1, colour)


static func _weight(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	ci.draw_polyline(PackedVector2Array([p.call(-0.55, -0.2), p.call(0.55, -0.2), p.call(0.8, 0.75),
			p.call(-0.8, 0.75), p.call(-0.55, -0.2)]), colour, w, true)
	ci.draw_arc(p.call(0.0, -0.45), s * 0.22, 0.0, TAU, 16, colour, w, true)
	ci.draw_line(p.call(-0.3, 0.32), p.call(0.3, 0.32), colour, w)


static func _hitch(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	ci.draw_polyline(PackedVector2Array([p.call(-0.85, -0.6), p.call(-0.3, -0.6), p.call(-0.3, 0.6),
			p.call(-0.85, 0.6)]), colour, w, true)
	for y: float in [-0.3, 0.0, 0.3]:
		ci.draw_circle(p.call(-0.57, y), s * 0.08, colour)
	ci.draw_line(p.call(-0.3, 0.0), p.call(0.4, 0.0), colour, w * 1.8)
	ci.draw_arc(p.call(0.62, 0.0), s * 0.24, 0.0, TAU, 16, colour, w, true)


static func _shift(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var p := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * s
	for x: float in [-0.55, 0.0, 0.55]:
		ci.draw_arc(p.call(x, -0.7), s * 0.13, 0.0, TAU, 12, colour, w, true)
		ci.draw_line(p.call(x, -0.57), p.call(x, 0.0), colour, w)
	for x: float in [-0.55, 0.0]:
		ci.draw_line(p.call(x, 0.0), p.call(x, 0.57), colour, w)
		ci.draw_arc(p.call(x, 0.7), s * 0.13, 0.0, TAU, 12, colour, w, true)
	ci.draw_line(p.call(-0.55, 0.0), p.call(0.55, 0.0), colour, w)
	ci.draw_line(p.call(0.55, 0.15), p.call(0.55, 0.75), ORANGE, w)
	ci.draw_polyline(PackedVector2Array([p.call(0.35, 0.55), p.call(0.55, 0.78), p.call(0.75, 0.55)]),
			ORANGE, w, true)


static func _governor(ci: CanvasItem, c: Vector2, s: float, w: float, colour: Color) -> void:
	var centre := c + Vector2(0.0, 0.3) * s
	ci.draw_arc(centre, s * 0.8, PI, TAU, 24, colour, w, true)
	ci.draw_line(centre, centre + Vector2(0.55, -0.5) * s, colour, w * 1.3)
	ci.draw_circle(centre, s * 0.13, colour)
	ci.draw_line(centre + Vector2(0.5, -0.66) * s, centre + Vector2(0.62, -0.78) * s, RED, w * 1.3)


# A tire's tread seen from behind, `rect` its face: lugs as chevrons,
# `depth` 0 (worn) to 1 (cut puller tread) setting how big they are.
static func draw_tread(ci: CanvasItem, rect: Rect2, depth: float, colour: Color) -> void:
	var face := StyleBoxFlat.new()
	face.bg_color = Color(0.105, 0.115, 0.135)
	face.set_corner_radius_all(int(rect.size.x * 0.16))
	ci.draw_style_box(face, rect)
	var lugs := 5
	var pitch := rect.size.y * 0.86 / lugs
	var half := rect.size.x * (0.16 + 0.12 * depth)
	var width := 3.0 + 9.0 * depth
	var lug := colour.lerp(Color(0.4, 0.43, 0.49), 0.5)
	for i in lugs:
		var top := rect.position + Vector2(rect.size.x / 2.0 - half / 2.0, rect.size.y * 0.07 + pitch * i)
		ci.draw_polyline(PackedVector2Array([top, top + Vector2(half, pitch * 0.45),
				top + Vector2(0.0, pitch * 0.9)]), lug, width, true)
		if depth > 0.85:
			# Cut lugs: a groove along each.
			ci.draw_line(top + Vector2(width * 0.4, pitch * 0.15),
					top + Vector2(half * 0.75, pitch * 0.42), face.bg_color, 2.0)
