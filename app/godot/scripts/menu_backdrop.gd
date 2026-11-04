# Behind the menus: a tractor pulling the sled at sunset, in silhouette,
# its front wheels up, black smoke from the stack and dirt flying off the
# rear tires. Drawn in 2D in place of the 3D venue, so the menus read
# clearly. The game's icons are drawn from it too (tools/make_icons.gd).
#
# Drawn to scale in metres, from the reference tractor and sled
# (data/tractors/farm_stock_light.toml, data/sleds/light_class.toml): x
# forward from the tractor's rear axle, y up from the ground.
class_name MenuBackdrop
extends Control

# What to draw: the sky, the ground and the far side of the track (BACK),
# and the tractor, the sled, the smoke and the dirt (FRONT). An adaptive
# phone icon has each on a layer of its own.
enum Layer { BACK = 1, FRONT = 2 }

# The menus' view: this much of the world high, the ground this far up
# from the screen's bottom, centred on the tractor and the sled [m].
const VIEW_HEIGHT_M := 7.2
const GROUND_UP_M := 1.1
const SCENE_MIDDLE_X_M := -1.7

const SKY_TOP := Color(0.09, 0.11, 0.24)
const SKY_MIDDLE := Color(0.50, 0.27, 0.32)
const SKY_LOW := Color(0.98, 0.60, 0.29)
const SUN := Color(1.0, 0.84, 0.50)
const SUN_M := Vector2(1.3, 1.2)
const SUN_RADIUS_M := 0.9
const FAR := Color(0.47, 0.25, 0.27)
const NEAR := Color(0.36, 0.19, 0.23)
const GROUND_TOP := Color(0.26, 0.15, 0.11)
const GROUND_BOTTOM := Color(0.10, 0.06, 0.05)
const SILHOUETTE := Color(0.05, 0.04, 0.06)
const WINDOW := Color(0.62, 0.36, 0.30)
const HUB := Color(0.17, 0.12, 0.13)
const DIRT := Color(0.20, 0.12, 0.08)
const SMOKE := Color(0.06, 0.06, 0.07)

# The tractor, in its own frame from the rear axle [m]: its wheels and
# hitch from the data; the body's outline is the drawing's.
const REAR_RADIUS_M := 0.8
const FRONT_RADIUS_M := 0.5
const WHEELBASE_M := 2.4
const HITCH_M := Vector2(-0.4, 0.5 - REAR_RADIUS_M)
const STACK_TOP_M := Vector2(1.05, 1.95)
# Nose up, about the rear axle: the front wheels half a metre up.
const PITCH_RAD := 0.21
const REAR_LUGS := 22
const FRONT_LUGS := 12
# Wheel turns a second.
const REAR_SPIN := 1.6
const FRONT_SPIN := 2.2

# The sled, behind on its chain: from its wheels' contact [m]. The hook is
# on the sled's drawbar, the chain's length behind the tractor's hitch.
const SLED_X_M := -5.5
const SLED_WHEEL_RADIUS_M := 0.55
const HOOK_M := Vector2(4.0, 0.45)

# The dirt the rear tires throw back and the stack's smoke.
const CLODS := 64
const CLOD_LIFE_S := 1.1
const GRAVITY_M_S2 := 9.81
const PUFFS := 9
const PUFF_LIFE_S := 2.4

# Set before the backdrop draws. `view_m` is the world shown, x from its
# left and y from the ground up, fitted to the control's width; empty, the
# menus' view. `animated` false holds the picture at `time_s`.
var view_m := Rect2()
var layers: int = Layer.BACK | Layer.FRONT
var animated := true
var time_s := 0.0


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_process(animated and is_visible_in_tree())
	visibility_changed.connect(func() -> void: set_process(animated and is_visible_in_tree()))


func _process(delta: float) -> void:
	time_s += delta
	queue_redraw()


func _draw() -> void:
	if size.x <= 0.0 or size.y <= 0.0:
		return
	var world := _world()
	if layers & Layer.BACK:
		_draw_sky(world)
		draw_set_transform_matrix(world)
		_draw_far(world)
	if layers & Layer.FRONT:
		draw_set_transform_matrix(world)
		var tractor := _tractor()
		_draw_sled()
		_draw_chain(tractor)
		_draw_smoke(tractor)
		_draw_tractor(world * tractor)
		draw_set_transform_matrix(world)
		_draw_dirt()
	draw_set_transform_matrix(Transform2D.IDENTITY)


# From the world, metres with y up, to the control's pixels.
func _world() -> Transform2D:
	var px_per_m := size.y / VIEW_HEIGHT_M
	var left_m := SCENE_MIDDLE_X_M - size.x / px_per_m / 2.0
	var top_m := VIEW_HEIGHT_M - GROUND_UP_M
	if view_m.has_area():
		px_per_m = size.x / view_m.size.x
		left_m = view_m.position.x
		top_m = view_m.position.y + size.y / px_per_m
	return Transform2D(Vector2(px_per_m, 0.0), Vector2(0.0, -px_per_m),
			Vector2(-left_m * px_per_m, top_m * px_per_m))


# The tractor's frame in the world: on its rear tires, pitched nose up.
func _tractor() -> Transform2D:
	return Transform2D(PITCH_RAD, Vector2(0.0, REAR_RADIUS_M))


# The sky, the sun going down behind the track and the ground.
func _draw_sky(world: Transform2D) -> void:
	var ground_y := (world * Vector2.ZERO).y
	_gradient(0.0, ground_y * 0.6, SKY_TOP, SKY_MIDDLE)
	_gradient(ground_y * 0.6, ground_y, SKY_MIDDLE, SKY_LOW)
	draw_set_transform_matrix(world)
	for i in 4:
		var glow := SUN
		glow.a = 0.08
		draw_circle(SUN_M, SUN_RADIUS_M * (1.6 + 0.5 * i), glow)
	draw_circle(SUN_M, SUN_RADIUS_M, SUN)
	draw_set_transform_matrix(Transform2D.IDENTITY)
	_gradient(ground_y, size.y, GROUND_TOP, GROUND_BOTTOM)
	# Furrows the tires left in the track.
	var furrow := GROUND_BOTTOM
	furrow.a = 0.5
	var below := size.y - ground_y
	for i in 4:
		var y := ground_y + below * (0.1 + 0.07 * i * i)
		draw_line(Vector2(0.0, y), Vector2(size.x, y), furrow, maxf(1.0, below * 0.012 * (i + 1)))


# The tree line and the floodlights on the far side.
func _draw_far(world: Transform2D) -> void:
	var from_m := (world.affine_inverse() * Vector2.ZERO).x
	var to_m := (world.affine_inverse() * Vector2(size.x, 0.0)).x
	draw_rect(Rect2(from_m, 0.0, to_m - from_m, 0.14), FAR)
	var step := floori(from_m / 0.22)
	while step * 0.22 < to_m:
		draw_circle(Vector2(step * 0.22, 0.16 + 0.06 * _hash(step)), 0.12 + 0.09 * _hash(step + 7),
				FAR)
		step += 1
	for pole_x_m: float in [-7.5, -2.8, 6.8]:
		draw_rect(Rect2(pole_x_m - 0.03, 0.0, 0.06, 2.8), NEAR)
		draw_rect(Rect2(pole_x_m - 0.22, 2.74, 0.44, 0.18), NEAR)


# The sled: its wheels and the operator at the back, the weight box on the
# way forward over the deck, and the pan at the front digging in.
func _draw_sled() -> void:
	var at := Vector2(SLED_X_M, 0.0)
	_polygon(at, [Vector2(-0.7, 0.65), Vector2(3.7, 0.65), Vector2(3.7, 1.0), Vector2(-0.7, 1.0)])
	_polygon(at, [Vector2(2.3, 0.65), Vector2(3.7, 0.65), Vector2(3.5, 0.02), Vector2(2.5, 0.02)])
	_polygon(at, [Vector2(3.6, 0.38), Vector2(4.05, 0.4), Vector2(4.05, 0.5), Vector2(3.6, 0.6)])
	# The weight box, and the weights stacked over the wheels.
	_polygon(at, [Vector2(1.3, 1.0), Vector2(2.8, 1.0), Vector2(2.8, 1.9), Vector2(1.3, 1.9)])
	_polygon(at, [Vector2(-0.5, 1.0), Vector2(0.6, 1.0), Vector2(0.6, 1.45), Vector2(-0.5, 1.45)])
	_polygon(at, [Vector2(-0.4, 1.45), Vector2(0.5, 1.45), Vector2(0.5, 1.75),
			Vector2(-0.4, 1.75)])
	# The operator under his roof at the back.
	_polygon(at, [Vector2(-1.25, 0.9), Vector2(-0.6, 0.9), Vector2(-0.6, 1.0), Vector2(-1.25, 1.0)])
	_polygon(at, [Vector2(-1.2, 1.0), Vector2(-1.12, 1.0), Vector2(-1.12, 2.3), Vector2(-1.2, 2.3)])
	_polygon(at, [Vector2(-1.35, 2.3), Vector2(-0.45, 2.3), Vector2(-0.45, 2.38),
			Vector2(-1.35, 2.38)])
	_polygon(at, [Vector2(-1.0, 1.0), Vector2(-0.7, 1.0), Vector2(-0.72, 1.4), Vector2(-0.98, 1.4)])
	draw_circle(at + Vector2(-0.85, 1.55), 0.13, SILHOUETTE)
	_wheel(at + Vector2(0.0, SLED_WHEEL_RADIUS_M), SLED_WHEEL_RADIUS_M, 0, 0.0, 0.0)
	# Dirt pushed up in front of the pan.
	_polygon(at, [Vector2(3.4, 0.0), Vector2(3.75, 0.3), Vector2(4.15, 0.0)], GROUND_BOTTOM)


func _draw_chain(tractor: Transform2D) -> void:
	var hitch := tractor * HITCH_M
	var hook := Vector2(SLED_X_M, 0.0) + HOOK_M
	var links := 12
	for i in links + 1:
		var t := float(i) / links
		draw_circle(hook.lerp(hitch, t) - Vector2(0.0, 0.06 * sin(t * PI)), 0.04, SILHOUETTE)


# The tractor in its own frame, `frame` from it to the control's pixels.
func _draw_tractor(frame: Transform2D) -> void:
	draw_set_transform_matrix(frame)
	var o := Vector2.ZERO
	# Drawbar, frame and the front axle's bracket, engine and hood, front
	# weights.
	_polygon(o, [Vector2(-0.48, -0.36), Vector2(-0.48, -0.24), Vector2(0.0, -0.08),
			Vector2(0.0, -0.3)])
	_polygon(o, [Vector2(-0.3, -0.38), Vector2(2.85, -0.38), Vector2(2.85, 0.1),
			Vector2(-0.3, 0.1)])
	_polygon(o, [Vector2(2.2, -0.45), Vector2(2.6, -0.45), Vector2(2.6, 0.0), Vector2(2.2, 0.0)])
	_polygon(o, [Vector2(0.5, 0.0), Vector2(0.5, 0.85), Vector2(2.55, 0.85), Vector2(2.75, 0.68),
			Vector2(2.75, 0.0)])
	_polygon(o, [Vector2(2.85, -0.4), Vector2(3.4, -0.4), Vector2(3.4, 0.35), Vector2(2.85, 0.35)])
	# The cab, its window lit by the sky behind, and the driver.
	_polygon(o, [Vector2(-0.55, 0.1), Vector2(-0.48, 1.95), Vector2(0.52, 1.95),
			Vector2(0.6, 0.1)])
	_polygon(o, [Vector2(-0.65, 1.92), Vector2(0.65, 1.92), Vector2(0.65, 2.03),
			Vector2(-0.65, 2.03)])
	_polygon(o, [Vector2(-0.33, 1.0), Vector2(-0.28, 1.8), Vector2(0.4, 1.8), Vector2(0.45, 1.0)],
			WINDOW)
	draw_circle(Vector2(0.05, 1.45), 0.13, SILHOUETTE)
	_polygon(o, [Vector2(-0.2, 1.0), Vector2(-0.15, 1.25), Vector2(0.25, 1.25), Vector2(0.3, 1.0)])
	# The stack.
	_polygon(o, [Vector2(STACK_TOP_M.x - 0.05, 0.8), Vector2(STACK_TOP_M.x - 0.05, STACK_TOP_M.y),
			Vector2(STACK_TOP_M.x + 0.05, STACK_TOP_M.y), Vector2(STACK_TOP_M.x + 0.05, 0.8)])
	_wheel(Vector2(WHEELBASE_M, FRONT_RADIUS_M - REAR_RADIUS_M), FRONT_RADIUS_M, FRONT_LUGS,
			0.03, FRONT_SPIN)
	_wheel(Vector2.ZERO, REAR_RADIUS_M, REAR_LUGS, 0.07, REAR_SPIN)
	# The fender over the rear wheel.
	var fender := PackedVector2Array()
	for i in 13:
		var a := PI * (0.12 + 0.76 * i / 12.0)
		fender.append(Vector2(cos(a), sin(a)) * (REAR_RADIUS_M + 0.1))
	for i in 13:
		var a := PI * (0.88 - 0.76 * i / 12.0)
		fender.append(Vector2(cos(a), sin(a)) * (REAR_RADIUS_M + 0.18))
	draw_colored_polygon(fender, SILHOUETTE)


# A tire with `lugs` lugs `lug_m` high, turning forwards at `spin` turns a
# second, and its rim.
func _wheel(centre: Vector2, radius_m: float, lugs: int, lug_m: float, spin: float) -> void:
	draw_circle(centre, radius_m, SILHOUETTE)
	var turn := -TAU * spin * time_s
	for i in lugs:
		var a := turn + TAU * i / lugs
		var out := Vector2(cos(a), sin(a))
		var side := Vector2(-out.y, out.x)
		var width_m := TAU * radius_m / lugs * 0.3
		draw_colored_polygon(PackedVector2Array([centre + out * (radius_m - 0.01) - side * width_m,
				centre + out * (radius_m + lug_m) - side * width_m * 0.5,
				centre + out * (radius_m + lug_m) + side * width_m * 0.5,
				centre + out * (radius_m - 0.01) + side * width_m]), SILHOUETTE)
	draw_circle(centre, radius_m * 0.38, HUB)
	draw_circle(centre, radius_m * 0.12, SILHOUETTE)


# Clods thrown back and up off the rear tires, falling back to the track.
func _draw_dirt() -> void:
	var origin := Vector2(-0.6, 0.2)
	for i in CLODS:
		var phase := fmod(time_s / CLOD_LIFE_S + i * 0.618, 1.0)
		var t := phase * CLOD_LIFE_S
		var velocity := Vector2(-2.5 - 2.8 * _hash(i), 3.0 + 3.2 * _hash(i + 50))
		var p := origin + velocity * t - Vector2(0.0, GRAVITY_M_S2 * t * t / 2.0)
		if p.y < -0.1:
			continue
		var clod := DIRT
		clod.a = 1.0 - phase * 0.6
		draw_circle(p, 0.04 + 0.06 * _hash(i + 100), clod)


# Black smoke from the stack, blown back over the cab and thinning out.
func _draw_smoke(tractor: Transform2D) -> void:
	var top := tractor * STACK_TOP_M
	for i in PUFFS:
		var phase := fmod(time_s / PUFF_LIFE_S + float(i) / PUFFS, 1.0)
		var p := top + Vector2(-3.2 * phase, 1.0 * phase + 0.4 * phase * phase)
		var puff := SMOKE
		puff.a = 0.75 * (1.0 - phase)
		draw_circle(p, 0.12 + 0.6 * phase, puff)


# A band across the control from `top` to `bottom` [px], `upper` to `lower`.
func _gradient(top: float, bottom: float, upper: Color, lower: Color) -> void:
	draw_polygon(PackedVector2Array([Vector2(0.0, top), Vector2(size.x, top),
			Vector2(size.x, bottom), Vector2(0.0, bottom)]),
			PackedColorArray([upper, upper, lower, lower]))


func _polygon(at: Vector2, points: Array[Vector2], colour: Color = SILHOUETTE) -> void:
	var moved := PackedVector2Array()
	for point in points:
		moved.append(at + point)
	draw_colored_polygon(moved, colour)


# A steady pseudo-random number from 0 to 1 for `i`.
static func _hash(i: int) -> float:
	return fmod(absf(sin(i * 12.9898) * 43758.5453), 1.0)
