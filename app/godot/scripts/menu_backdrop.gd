# Behind the menus: a tractor pulling the sled at sunset, in silhouette,
# its front wheels up, black smoke from the stack and dirt flying off the
# rear tires. Drawn in 2D in place of the busy venue, so the menus read
# clearly. Laid out in the 1280×720 base layout and scaled to the screen's
# height; the sky and the ground run on to either side of a wider screen.
class_name MenuBackdrop
extends Control

const BASE_SIZE := Vector2(1280.0, 720.0)
const HORIZON_Y := 610.0

const SKY_TOP := Color(0.09, 0.11, 0.24)
const SKY_MIDDLE := Color(0.50, 0.27, 0.32)
const SKY_LOW := Color(0.98, 0.60, 0.29)
const SKY_MIDDLE_Y := 380.0
const SUN := Color(1.0, 0.84, 0.50)
const SUN_CENTRE := Vector2(470.0, HORIZON_Y - 55.0)
const SUN_RADIUS := 80.0
const FAR := Color(0.47, 0.25, 0.27)
const NEAR := Color(0.36, 0.19, 0.23)
const GROUND_TOP := Color(0.26, 0.15, 0.11)
const GROUND_BOTTOM := Color(0.10, 0.06, 0.05)
const SILHOUETTE := Color(0.05, 0.04, 0.06)
const WINDOW := Color(0.62, 0.36, 0.30)
const HUB := Color(0.17, 0.12, 0.13)
const DIRT := Color(0.20, 0.12, 0.08)
const SMOKE := Color(0.06, 0.06, 0.07)

# The tractor, in its own frame: x forward, y up, from the rear axle.
# Drawn at TRACTOR_SCALE.
const TRACTOR_SCALE := 0.72
const REAR_AXLE := Vector2(900.0, HORIZON_Y - REAR_RADIUS * TRACTOR_SCALE)
const REAR_RADIUS := 128.0
const FRONT_RADIUS := 52.0
const WHEELBASE := 330.0
const PITCH_RAD := 0.13
const LUGS := 18
const LUG_HEIGHT := 14.0
const HITCH := Vector2(-150.0, 38.0)
const STACK_TOP := Vector2(158.0, 330.0)

# The sled, behind on its chain: drawn standing on SLED_GROUND_Y, moved
# down to the horizon.
const SLED_GROUND_Y := 560.0
const SLED_SCALE := 0.8
const SLED_AT := Vector2(70.0, HORIZON_Y)
const HOOK := Vector2(560.0, 500.0)

# The dirt the rear tires throw back and the stack's smoke.
const CLODS := 42
const CLOD_LIFE_S := 1.1
const CLOD_GRAVITY := 900.0
const PUFFS := 9
const PUFF_LIFE_S := 2.4

var _time_s := 0.0


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_process(is_visible_in_tree())
	visibility_changed.connect(func() -> void: set_process(is_visible_in_tree()))


func _process(delta: float) -> void:
	_time_s += delta
	queue_redraw()


func _draw() -> void:
	if size.y <= 0.0:
		return
	draw_set_transform_matrix(_base_transform())
	# Everything below is in the base layout; `from` and `to` span the screen.
	var left := (size.x * BASE_SIZE.y / size.y - BASE_SIZE.x) / 2.0
	var from := -left
	var to := BASE_SIZE.x + left
	_draw_sky(from, to)
	_draw_far(from, to)
	_draw_ground(from, to)
	_draw_sled()
	var tractor := Transform2D(-PITCH_RAD, Vector2(TRACTOR_SCALE, TRACTOR_SCALE), 0.0, REAR_AXLE)
	_draw_chain(tractor)
	_draw_smoke(tractor)
	_draw_tractor(tractor)
	_draw_dirt()


func _draw_sky(from: float, to: float) -> void:
	_gradient(from, to, 0.0, SKY_MIDDLE_Y, SKY_TOP, SKY_MIDDLE)
	_gradient(from, to, SKY_MIDDLE_Y, HORIZON_Y, SKY_MIDDLE, SKY_LOW)
	for i in 4:
		var glow := SUN
		glow.a = 0.08
		draw_circle(SUN_CENTRE, SUN_RADIUS * (1.6 + 0.5 * i), glow)
	draw_circle(SUN_CENTRE, SUN_RADIUS, SUN)


# The stands, the tree line and the floodlights along the horizon.
func _draw_far(from: float, to: float) -> void:
	draw_rect(Rect2(from, HORIZON_Y - 14.0, to - from, 14.0), FAR)
	var x := from
	var step := 0
	while x < to:
		draw_circle(Vector2(x, HORIZON_Y - 16.0 - 6.0 * _hash(step)), 12.0 + 9.0 * _hash(step + 7),
				FAR)
		x += 22.0
		step += 1
	# The grandstand on the far side, the crowd's heads along its top.
	var stand_top := HORIZON_Y - 70.0
	draw_colored_polygon(PackedVector2Array([Vector2(470.0, HORIZON_Y), Vector2(500.0, stand_top),
			Vector2(840.0, stand_top), Vector2(870.0, HORIZON_Y)]), NEAR)
	draw_rect(Rect2(480.0, stand_top - 22.0, 380.0, 8.0), NEAR)
	for i in 31:
		draw_circle(Vector2(505.0 + i * 11.0, stand_top - 3.0 + 2.0 * sin(i * 2.3)), 4.5, NEAR)
	for pole_x: float in [20.0, 660.0, 1250.0]:
		draw_rect(Rect2(pole_x - 3.0, HORIZON_Y - 280.0, 6.0, 280.0), NEAR)
		draw_rect(Rect2(pole_x - 22.0, HORIZON_Y - 292.0, 44.0, 18.0), NEAR)


func _draw_ground(from: float, to: float) -> void:
	_gradient(from, to, HORIZON_Y, BASE_SIZE.y, GROUND_TOP, GROUND_BOTTOM)
	# Furrows the tires left in the track.
	var furrow := GROUND_BOTTOM
	furrow.a = 0.5
	for i in 4:
		var y := HORIZON_Y + 18.0 + i * i * 12.0
		draw_line(Vector2(from, y), Vector2(to, y), furrow, 2.0 + i)


# The sled: its wheels at the back, the weight box run forward over the
# deck, and the pan at the front digging in.
func _draw_sled() -> void:
	draw_set_transform_matrix(_base_transform() * _sled_transform())
	var deck := PackedVector2Array([Vector2(110.0, 520.0), Vector2(120.0, 470.0),
			Vector2(500.0, 470.0), Vector2(540.0, 495.0), Vector2(570.0, SLED_GROUND_Y),
			Vector2(430.0, SLED_GROUND_Y), Vector2(420.0, 520.0)])
	draw_colored_polygon(deck, SILHOUETTE)
	draw_circle(Vector2(165.0, SLED_GROUND_Y - 42.0), 42.0, SILHOUETTE)
	draw_circle(Vector2(165.0, SLED_GROUND_Y - 42.0), 16.0, HUB)
	# The weight box and the stacked weights over the wheels.
	draw_rect(Rect2(330.0, 400.0, 130.0, 72.0), SILHOUETTE)
	draw_rect(Rect2(140.0, 430.0, 110.0, 42.0), SILHOUETTE)
	draw_rect(Rect2(150.0, 404.0, 90.0, 28.0), SILHOUETTE)
	# The operator's cage and mast.
	draw_rect(Rect2(270.0, 380.0, 8.0, 92.0), SILHOUETTE)
	draw_rect(Rect2(266.0, 374.0, 50.0, 8.0), SILHOUETTE)
	# Dirt pushed up in front of the pan.
	draw_colored_polygon(PackedVector2Array([Vector2(560.0, SLED_GROUND_Y),
			Vector2(590.0, 538.0), Vector2(625.0, SLED_GROUND_Y)]), GROUND_BOTTOM)
	draw_set_transform_matrix(_base_transform())


func _draw_chain(tractor: Transform2D) -> void:
	var hitch := tractor * _up(HITCH)
	var hook := _sled_transform() * HOOK
	var links := 22
	for i in links + 1:
		var t := float(i) / links
		var p := hook.lerp(hitch, t) + Vector2(0.0, 10.0 * sin(t * PI))
		draw_circle(p, 3.5, SILHOUETTE)


func _draw_tractor(tractor: Transform2D) -> void:
	draw_set_transform_matrix(_base_transform() * tractor)
	# Drawbar, frame, engine and hood, the front weights.
	_polygon([Vector2(-160.0, 30.0), Vector2(-160.0, 46.0), Vector2(0.0, 60.0),
			Vector2(0.0, 40.0)])
	_polygon([Vector2(-60.0, 50.0), Vector2(400.0, 50.0), Vector2(400.0, 100.0),
			Vector2(-60.0, 100.0)])
	_polygon([Vector2(40.0, 95.0), Vector2(40.0, 205.0), Vector2(310.0, 196.0),
			Vector2(345.0, 175.0), Vector2(350.0, 95.0)])
	_polygon([Vector2(380.0, 40.0), Vector2(450.0, 40.0), Vector2(450.0, 140.0),
			Vector2(380.0, 140.0)])
	# The cab, its window lit by the sky behind.
	_polygon([Vector2(-75.0, 90.0), Vector2(-62.0, 300.0), Vector2(70.0, 300.0),
			Vector2(80.0, 90.0)])
	_polygon([Vector2(-85.0, 296.0), Vector2(80.0, 296.0), Vector2(80.0, 312.0),
			Vector2(-85.0, 312.0)])
	_polygon([Vector2(-40.0, 190.0), Vector2(-32.0, 280.0), Vector2(55.0, 280.0),
			Vector2(60.0, 190.0)], WINDOW)
	# The driver's head and shoulders in the window.
	draw_circle(_up(Vector2(5.0, 240.0)), 15.0, SILHOUETTE)
	_polygon([Vector2(-25.0, 190.0), Vector2(-18.0, 218.0), Vector2(28.0, 218.0),
			Vector2(35.0, 190.0)])
	# The stack.
	_polygon([Vector2(STACK_TOP.x - 8.0, 195.0), Vector2(STACK_TOP.x - 8.0, STACK_TOP.y),
			Vector2(STACK_TOP.x + 8.0, STACK_TOP.y), Vector2(STACK_TOP.x + 8.0, 195.0)])
	# The fender over the rear wheel.
	var fender := PackedVector2Array()
	for i in 13:
		var a := PI * (0.15 + 0.7 * i / 12.0)
		fender.append(_up(Vector2(cos(a), sin(a)) * (REAR_RADIUS + 14.0)))
	for i in 13:
		var a := PI * (0.85 - 0.7 * i / 12.0)
		fender.append(_up(Vector2(cos(a), sin(a)) * (REAR_RADIUS + 26.0)))
	draw_colored_polygon(fender, SILHOUETTE)
	_wheel(Vector2(WHEELBASE, FRONT_RADIUS - REAR_RADIUS), FRONT_RADIUS, 10, 6.0, 0.9)
	_wheel(Vector2.ZERO, REAR_RADIUS, LUGS, LUG_HEIGHT, 1.6)
	draw_set_transform_matrix(_base_transform())


# A tire with its lugs, turning at `spin` revolutions a second, and the rim.
func _wheel(centre: Vector2, radius: float, lugs: int, lug: float, spin: float) -> void:
	var p := _up(centre)
	draw_circle(p, radius, SILHOUETTE)
	var turn := TAU * spin * _time_s
	for i in lugs:
		var a := turn + TAU * i / lugs
		var out := Vector2(cos(a), sin(a))
		var side := Vector2(-out.y, out.x)
		draw_colored_polygon(PackedVector2Array([p + out * (radius - 2.0) - side * lug,
				p + out * (radius + lug) - side * lug * 0.4,
				p + out * (radius + lug) + side * lug * 0.4,
				p + out * (radius - 2.0) + side * lug]), SILHOUETTE)
	draw_circle(p, radius * 0.38, HUB)
	draw_circle(p, radius * 0.12, SILHOUETTE)


# Clods thrown back and up off the rear tires, falling back to the track.
func _draw_dirt() -> void:
	var origin := REAR_AXLE + Vector2(-0.7, 0.55) * REAR_RADIUS * TRACTOR_SCALE
	for i in CLODS:
		var phase := fmod(_time_s / CLOD_LIFE_S + i * 0.618, 1.0)
		var t := phase * CLOD_LIFE_S
		var velocity := Vector2(-260.0 - 240.0 * _hash(i), -330.0 - 260.0 * _hash(i + 50))
		var p := origin + velocity * t + Vector2(0.0, CLOD_GRAVITY * t * t / 2.0)
		if p.y > HORIZON_Y + 20.0:
			continue
		var clod := DIRT
		clod.a = 1.0 - phase * 0.6
		draw_circle(p, 3.0 + 5.0 * _hash(i + 100), clod)


# Black smoke from the stack, blown back over the cab and thinning out.
func _draw_smoke(tractor: Transform2D) -> void:
	var top := tractor * _up(STACK_TOP)
	for i in PUFFS:
		var phase := fmod(_time_s / PUFF_LIFE_S + float(i) / PUFFS, 1.0)
		var p := top + Vector2(-380.0 * phase, -120.0 * phase - 40.0 * phase * phase)
		var puff := SMOKE
		puff.a = 0.75 * (1.0 - phase)
		draw_circle(p, 14.0 + 70.0 * phase, puff)


func _gradient(from: float, to: float, top: float, bottom: float, upper: Color,
		lower: Color) -> void:
	draw_polygon(PackedVector2Array([Vector2(from, top), Vector2(to, top), Vector2(to, bottom),
			Vector2(from, bottom)]), PackedColorArray([upper, upper, lower, lower]))


func _polygon(points: Array[Vector2], colour: Color = SILHOUETTE) -> void:
	var flipped := PackedVector2Array()
	for point in points:
		flipped.append(_up(point))
	draw_colored_polygon(flipped, colour)


# The sled's drawing, from SLED_GROUND_Y to the horizon at SLED_AT.
func _sled_transform() -> Transform2D:
	return Transform2D(0.0, Vector2(SLED_SCALE, SLED_SCALE), 0.0,
			SLED_AT - Vector2(0.0, SLED_GROUND_Y) * SLED_SCALE)


# The base layout's transform, as _draw() sets it.
func _base_transform() -> Transform2D:
	var zoom := size.y / BASE_SIZE.y
	var left := (size.x / zoom - BASE_SIZE.x) / 2.0
	return Transform2D(0.0, Vector2(zoom, zoom), 0.0, Vector2(left * zoom, 0.0))


# From the tractor's frame, y up, to the screen's, y down.
static func _up(point: Vector2) -> Vector2:
	return Vector2(point.x, -point.y)


# A steady pseudo-random number from 0 to 1 for `i`.
static func _hash(i: int) -> float:
	return fmod(absf(sin(i * 12.9898) * 43758.5453), 1.0)
