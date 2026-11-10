# A part's levels on one line, as the garage builds it up step by step:
# those bought checked, the one the tractor has ringed, the next one big
# with its price, the rest dimmed. Tires show their tread instead, wider
# and chunkier per level.
class_name LevelLine
extends Control

const TRACK_WIDTH := 6.0
const NODE_RADIUS := 20.0
const NEXT_RADIUS := 30.0
const LABEL_SIZE := 12
const TREAD_SIZE := Vector2(54.0, 92.0)

var labels: PackedStringArray = []
var costs: Array = []
# The level the tractor has (0 the first).
var level := 0
# Draw tread faces (the tires) rather than nodes.
var treads := false
# Whether the part counts on the tractor as fitted (an alternative's build
# that is off is greyed).
var counts := true


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)


func show_levels(level_labels: PackedStringArray, level_costs: Array, at: int,
		tire_treads: bool, counted: bool) -> void:
	labels = level_labels
	costs = level_costs
	level = at
	treads = tire_treads
	counts = counted
	queue_redraw()


func _x(i: int) -> float:
	var n := maxi(labels.size(), 1)
	var margin := 100.0
	if n == 1:
		return size.x / 2.0
	return margin + (size.x - 2.0 * margin) * i / float(n - 1)


func _draw() -> void:
	if labels.is_empty():
		return
	var y := size.y * 0.42
	var dim := 1.0 if counts else 0.5
	draw_line(Vector2(_x(0), y), Vector2(_x(labels.size() - 1), y), GarageArt.LINE, TRACK_WIDTH,
			true)
	if level > 0:
		draw_line(Vector2(_x(0), y), Vector2(_x(level), y), Color(GarageArt.ORANGE, dim),
				TRACK_WIDTH, true)
	for i in labels.size():
		var c := Vector2(_x(i), y)
		if treads:
			_tread(i, c, dim)
		else:
			_node(i, c, dim)
		_labels(i, c)


func _node(i: int, c: Vector2, dim: float) -> void:
	if i < level:
		draw_circle(c, NODE_RADIUS, Color(GarageArt.ORANGE, dim))
		draw_polyline(PackedVector2Array([c + Vector2(-8, 0), c + Vector2(-2, 6), c + Vector2(9, -6)]),
				Color(0.1, 0.07, 0.02), 3.5, true)
	elif i == level:
		draw_circle(c, NODE_RADIUS + 4.0, GarageArt.PANEL)
		draw_arc(c, NODE_RADIUS + 4.0, 0.0, TAU, 32, Color(GarageArt.ORANGE, dim), 5.0, true)
		_centred(str(i + 1), c + Vector2(0, 5), 15, GarageArt.ORANGE_LIGHT)
	elif i == level + 1:
		draw_circle(c, NEXT_RADIUS, Color(0.23, 0.16, 0.08))
		draw_arc(c, NEXT_RADIUS, 0.0, TAU, 40, GarageArt.ORANGE_LIGHT, 4.0, true)
		_centred("NEXT", c + Vector2(0, -2), 12, GarageArt.ORANGE_LIGHT)
		_centred(GarageScreen.euros(int(costs[i])), c + Vector2(0, 13), 11, GarageArt.ORANGE_LIGHT)
	else:
		draw_circle(c, NODE_RADIUS - 2.0, GarageArt.PANEL_LIGHT)
		draw_arc(c, NODE_RADIUS - 2.0, 0.0, TAU, 32, GarageArt.LINE, 2.0, true)
		_lock(c)


func _tread(i: int, c: Vector2, dim: float) -> void:
	var depth := float(i) / maxf(labels.size() - 1.0, 1.0)
	var grow := 0.8 + 0.35 * depth
	var rect := Rect2(c - TREAD_SIZE * grow / 2.0, TREAD_SIZE * grow)
	var fade := 1.0 if i <= level + 1 else 0.45
	modulate_tread(rect, depth, fade * dim)
	if i == level:
		draw_rect(rect.grow(3.0), GarageArt.ORANGE, false, 3.0)
	elif i == level + 1:
		draw_rect(rect.grow(3.0), GarageArt.ORANGE_LIGHT, false, 2.0)
	elif i < level:
		var mark := c + Vector2(0.0, rect.size.y / 2.0 + 12.0)
		draw_polyline(PackedVector2Array([mark + Vector2(-7, 0), mark + Vector2(-2, 5),
				mark + Vector2(8, -5)]), Color(GarageArt.ORANGE, dim), 3.0, true)


func modulate_tread(rect: Rect2, depth: float, alpha: float) -> void:
	GarageArt.draw_tread(self, rect, depth, Color(1.0, 1.0, 1.0, alpha))


func _labels(i: int, c: Vector2) -> void:
	var below := c.y + (TREAD_SIZE.y * 0.62 + 26.0 if treads else NEXT_RADIUS + 22.0)
	var colour := GarageArt.TEXT if i <= level else GarageArt.DIM
	if i == level + 1:
		colour = GarageArt.ORANGE_LIGHT
	var spacing := absf(_x(1) - _x(0)) if labels.size() > 1 else size.x
	var words := _short(labels[i], spacing)
	for line in words.size():
		_centred(words[line], Vector2(c.x, below + line * 15.0), LABEL_SIZE, colour)
	if i > level + 1 and costs.size() > i:
		_centred(GarageScreen.euros(int(costs[i])), Vector2(c.x, below + words.size() * 15.0),
				LABEL_SIZE - 1, GarageArt.DIM)


# `text` broken into at most two lines that fit `width`.
func _short(text: String, width: float) -> PackedStringArray:
	var font := get_theme_default_font()
	var out: PackedStringArray = []
	var line := ""
	for word in text.split(" "):
		var tried := word if line.is_empty() else line + " " + word
		if font.get_string_size(tried, HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE).x > width - 6.0 \
				and not line.is_empty():
			out.append(line)
			line = word
		else:
			line = tried
	out.append(line)
	if out.size() > 2:
		out = PackedStringArray([out[0], out[1] + "…"])
	return out


func _centred(text: String, at: Vector2, font_size: int, colour: Color) -> void:
	var font := get_theme_default_font()
	var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	draw_string(font, Vector2(at.x - width / 2.0, at.y), text, HORIZONTAL_ALIGNMENT_LEFT, -1,
			font_size, colour)


func _lock(c: Vector2) -> void:
	draw_rect(Rect2(c + Vector2(-6, -1), Vector2(12, 9)), GarageArt.DIM, false, 2.0)
	draw_arc(c + Vector2(0, -2), 4.5, PI, TAU, 10, GarageArt.DIM, 2.0, true)
