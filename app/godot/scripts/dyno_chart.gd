# The engine on the dynamometer: power against engine speed at full
# throttle, for one or more builds (TractorPullEvent.get_upgrade_dyno(),
# get_choice_dynos()), so a part's screen shows what it adds before it is
# bought. Each curve is {points (rpm, kW), colour, dashed}.
class_name DynoChart
extends Control

const MARGIN := Vector2(46.0, 24.0)
const LABEL_SIZE := 12
const GRID := Color(0.17, 0.19, 0.24)
const DASH := 9.0

var curves: Array[Dictionary] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)


func show_curves(to_show: Array[Dictionary]) -> void:
	curves = to_show
	queue_redraw()


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.07, 0.08, 0.1, 0.6))
	if curves.is_empty():
		return
	var rpm_low := INF
	var rpm_high := -INF
	var kw_high := 1.0
	for curve in curves:
		for p: Vector2 in curve.get("points", PackedVector2Array()):
			rpm_low = minf(rpm_low, p.x)
			rpm_high = maxf(rpm_high, p.x)
			kw_high = maxf(kw_high, p.y)
	if rpm_high <= rpm_low:
		return
	kw_high = ceilf(kw_high / 50.0) * 50.0
	var area := Rect2(Vector2(MARGIN.x, 8.0), size - MARGIN - Vector2(12.0, 8.0))
	var to_px := func(p: Vector2) -> Vector2:
		return Vector2(area.position.x + (p.x - rpm_low) / (rpm_high - rpm_low) * area.size.x,
				area.end.y - p.y / kw_high * area.size.y)
	var font := get_theme_default_font()
	for i in 5:
		var y := area.position.y + area.size.y * i / 4.0
		draw_line(Vector2(area.position.x, y), Vector2(area.end.x, y), GRID, 1.0)
		var kw := kw_high * (4 - i) / 4.0
		draw_string(font, Vector2(4.0, y + 4.0), "%d kW" % roundi(kw), HORIZONTAL_ALIGNMENT_LEFT,
				-1, LABEL_SIZE, GarageArt.DIM)
	var step := 500.0
	var rpm := ceilf(rpm_low / step) * step
	while rpm <= rpm_high:
		var x: float = to_px.call(Vector2(rpm, 0.0)).x
		draw_line(Vector2(x, area.position.y), Vector2(x, area.end.y), GRID, 1.0)
		draw_string(font, Vector2(x - 14.0, area.end.y + 16.0), "%d" % roundi(rpm),
				HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE, GarageArt.DIM)
		rpm += step
	draw_string(font, Vector2(area.end.x - 30.0, area.end.y + 16.0), "rpm",
			HORIZONTAL_ALIGNMENT_LEFT, -1, LABEL_SIZE, GarageArt.DIM)
	for curve in curves:
		var px := PackedVector2Array()
		for p: Vector2 in curve.get("points", PackedVector2Array()):
			px.append(to_px.call(p))
		var colour: Color = curve.get("colour", GarageArt.TEXT)
		if curve.get("dashed", false):
			for i in px.size() - 1:
				draw_dashed_line(px[i], px[i + 1], colour, 2.5, DASH, true, true)
		else:
			draw_polyline(px, colour, 3.0, true)
