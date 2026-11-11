# A pulling tractor from the side, in colour, for the garage's workshop and
# the pit board: a Sport class farm tractor stripped for pulling, under a
# roll bar, on puller tires, with its wheelie bar, drawbar and weights.
# Drawn to scale in metres, x forward from the rear axle and y up from it,
# fitted into the control; parts can be found on it (anchor()) for the
# workshop's pins, and the pit board shows the setup on it: where the
# weights sit, the hitch's hole and the tires' footprint.
class_name TractorSketch
extends Control

const REAR_RADIUS_M := 0.88
const FRONT_RADIUS_M := 0.5
const WHEELBASE_M := 2.75
# The weights: on the carrier behind the rear axle at their rearmost, on
# the front bracket at their foremost.
const REAR_WEIGHTS_X_M := -1.2
const FRONT_WEIGHTS_X_M := 3.62
# The wheelie bar's tip, behind the rear axle (data/tractors/sport_4500.toml),
# and its skid pad's rear edge behind the tip [m].
const WHEELIE_BAR_TIP_X_M := -1.05
const SKID_BEHIND_TIP_M := 0.14
# A column of weight blocks, as wide as it is drawn [m].
const WEIGHT_COLUMN_M := 0.42
# What the drawing spans: from the rear weights' back edge (two columns on
# the carrier, behind the wheelie bar's skid) to past the front weights,
# and from the ground to the top of the stack [m].
const BOUNDS_LEFT_M := REAR_WEIGHTS_X_M - WEIGHT_COLUMN_M
const BOUNDS_RIGHT_M := 3.98
const BOUNDS_M := Rect2(BOUNDS_LEFT_M, -REAR_RADIUS_M - 0.02, BOUNDS_RIGHT_M - BOUNDS_LEFT_M, 2.98)

const PAINT := Color(0.80, 0.33, 0.13)
const PAINT_DARK := Color(0.62, 0.24, 0.09)
const PAINT_LIGHT := Color(0.93, 0.47, 0.22)
const STEEL := Color(0.22, 0.24, 0.29)
const STEEL_LIGHT := Color(0.36, 0.39, 0.46)
const CHROME := Color(0.72, 0.75, 0.8)
const RUBBER := Color(0.09, 0.095, 0.11)
const RUBBER_LUG := Color(0.17, 0.18, 0.21)
const RIM := Color(0.86, 0.72, 0.25)
const SEAT := Color(0.12, 0.12, 0.14)
const WEIGHT := Color(0.33, 0.36, 0.42)
const WEIGHT_EDGE := Color(0.45, 0.49, 0.56)
const HIGHLIGHT := Color(0.96, 0.71, 0.36)
const HOLE := Color(0.35, 0.38, 0.45)
const HOLE_SET := Color(0.31, 0.53, 0.85)

# The setup to show (the pit board), or none (the workshop):
# `ballast_share` the weights from rearmost (0) to foremost (1) and their
# mass [kg], below 0 none drawn; the hitch's holes from `hitch_low_m` to
# `hitch_top_m` with `hitch_m` set, no ladder unless `hitch_low_m` > 0;
# `footprint` how far the rear tire is let down, 0 (hard) to 1 (soft), below
# 0 none drawn.
var ballast_share := -1.0
var ballast_kg := 0.0
var hitch_low_m := 0.0
var hitch_top_m := 0.5
var hitch_m := 0.5
var footprint := -1.0
# Each part's point on the tractor [m], for anchor().
const ANCHORS_M := {
	"engine_torque": Vector2(1.95, 0.22),
	"engine": Vector2(1.3, 0.22),
	"rear_traction": Vector2(-0.15, 0.62),
	"rear_tire": Vector2(-0.15, 0.62),
	"ballast_kg": Vector2(3.62, 0.16),
	"hitch_height_m": Vector2(-0.5, -0.38),
	"load_downshift_gears": Vector2(0.55, -0.1),
	"driveline": Vector2(0.55, -0.1),
	"rated_rpm": Vector2(2.55, -0.08),
}


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)


# Pixels per metre, and where the rear axle is in the control.
func _scale() -> float:
	return minf(size.x / BOUNDS_M.size.x, size.y / BOUNDS_M.size.y)


func _axle() -> Vector2:
	var k := _scale()
	var used := BOUNDS_M.size * k
	var corner := (size - used) / 2.0
	return corner + Vector2(-BOUNDS_M.position.x, BOUNDS_M.end.y) * k


# A point on the tractor [m] in the control's pixels.
func to_px(m: Vector2) -> Vector2:
	return _axle() + Vector2(m.x, -m.y) * _scale()


# Where part `part` is on the drawing, in the control's pixels (its centre
# if the part is not drawn).
func anchor(part: String) -> Vector2:
	if ANCHORS_M.has(part):
		return to_px(ANCHORS_M[part])
	return size / 2.0


func _draw() -> void:
	var k := _scale()
	var ground_y := to_px(Vector2(0.0, -REAR_RADIUS_M)).y
	draw_line(Vector2(0.0, ground_y), Vector2(size.x, ground_y), Color(0.25, 0.27, 0.33), 2.0)
	draw_rect(Rect2(to_px(Vector2(-1.5, -REAR_RADIUS_M)), Vector2(5.4 * k, 0.07 * k)),
			Color(0, 0, 0, 0.35))
	_draw_wheelie_bar()
	_draw_drawbar()
	_draw_chassis()
	_draw_engine()
	_draw_hood()
	_draw_turbo()
	_draw_front_wheel()
	_draw_cockpit()
	_draw_rear_wheel()
	_draw_fender()
	_draw_weights()
	if hitch_low_m > 0.0:
		_draw_hitch_ladder()
	if footprint >= 0.0:
		_draw_footprint()


func _poly(points_m: Array[Vector2], colour: Color) -> void:
	var px := PackedVector2Array()
	for m in points_m:
		px.append(to_px(m))
	draw_colored_polygon(px, colour)


func _box(from_m: Vector2, to_m: Vector2, colour: Color) -> void:
	_poly([from_m, Vector2(to_m.x, from_m.y), to_m, Vector2(from_m.x, to_m.y)], colour)


func _line(from_m: Vector2, to_m: Vector2, width_m: float, colour: Color) -> void:
	draw_line(to_px(from_m), to_px(to_m), colour, maxf(width_m * _scale(), 1.0), true)


func _circle(centre_m: Vector2, radius_m: float, colour: Color) -> void:
	draw_circle(to_px(centre_m), radius_m * _scale(), colour)


func _draw_wheelie_bar() -> void:
	var tip := Vector2(WHEELIE_BAR_TIP_X_M, -REAR_RADIUS_M + 0.25)
	_line(Vector2(0.15, -0.28), tip, 0.09, STEEL_LIGHT)
	_line(Vector2(-0.2, 0.05), tip + Vector2(0.12, 0.04), 0.06, STEEL)
	# The skid pad.
	_box(tip + Vector2(-SKID_BEHIND_TIP_M, -0.06), tip + Vector2(0.16, 0.03), STEEL)


func _draw_drawbar() -> void:
	var y := hitch_m - REAR_RADIUS_M
	_line(Vector2(0.35, -0.3), Vector2(-0.45, y), 0.12, STEEL_LIGHT)
	draw_arc(to_px(Vector2(-0.5, y)), 0.07 * _scale(), 0.0, TAU, 20, CHROME, 0.035 * _scale(), true)


func _draw_chassis() -> void:
	# The rear axle's housing and the gearbox, then the frame rails forward.
	_poly([Vector2(-0.25, -0.32), Vector2(0.95, -0.32), Vector2(1.05, -0.1), Vector2(1.05, 0.32),
			Vector2(-0.1, 0.32), Vector2(-0.3, 0.1)], STEEL)
	_box(Vector2(0.9, -0.3), Vector2(3.45, -0.12), STEEL)
	_box(Vector2(0.9, -0.12), Vector2(3.45, -0.07), STEEL_LIGHT)
	# The front axle's bracket.
	_poly([Vector2(2.55, -0.3), Vector2(2.95, -0.3), Vector2(2.85, -0.42), Vector2(2.65, -0.42)],
			STEEL_LIGHT)


func _draw_engine() -> void:
	# The block under the hood, with its six cylinders' bumps and the
	# injection pump, then the exhaust manifold, the turbo and the stack.
	_box(Vector2(1.05, -0.12), Vector2(2.75, 0.4), Color(0.30, 0.32, 0.37))
	for i in 6:
		var x := 1.15 + i * 0.26
		_box(Vector2(x, 0.4), Vector2(x + 0.2, 0.48), Color(0.36, 0.38, 0.44))
	_box(Vector2(2.35, -0.1), Vector2(2.7, 0.12), Color(0.24, 0.26, 0.3))
	_line(Vector2(1.45, 0.3), Vector2(2.3, 0.3), 0.07, Color(0.55, 0.38, 0.25))


# The turbo on the engine's side, fed by the exhaust manifold, its stack
# up through the hood.
func _draw_turbo() -> void:
	var turbo := Vector2(1.3, 0.22)
	_line(turbo + Vector2(0.0, 0.1), Vector2(1.3, 2.05), 0.1, CHROME)
	_poly([Vector2(1.23, 2.05), Vector2(1.41, 2.12), Vector2(1.41, 2.0), Vector2(1.23, 1.98)],
			CHROME)
	_circle(turbo, 0.19, Color(0.62, 0.64, 0.68))
	_circle(turbo, 0.12, Color(0.42, 0.44, 0.5))
	draw_arc(to_px(turbo), 0.15 * _scale(), -PI / 2.0, PI, 16, CHROME, 0.02 * _scale(), true)
	_circle(turbo, 0.04, CHROME)


func _draw_hood() -> void:
	# The hood over the engine, open at the side (pullers take the side
	# panels off), the grille, and the fuel tank and dash before the seat.
	_poly([Vector2(1.0, 0.5), Vector2(1.0, 0.95), Vector2(2.9, 0.88), Vector2(3.32, 0.68),
			Vector2(3.32, 0.5)], PAINT)
	_poly([Vector2(1.0, 0.88), Vector2(1.0, 0.95), Vector2(2.9, 0.88), Vector2(3.32, 0.68),
			Vector2(3.32, 0.6)], PAINT_LIGHT)
	_poly([Vector2(2.75, -0.12), Vector2(3.32, -0.12), Vector2(3.32, 0.5), Vector2(2.75, 0.5)],
			PAINT_DARK)
	# The grille's bars.
	for i in 5:
		var y := -0.04 + i * 0.11
		_line(Vector2(3.3, y), Vector2(3.4, y), 0.04, STEEL)
	_box(Vector2(3.32, -0.12), Vector2(3.42, 0.62), STEEL)
	_poly([Vector2(0.3, 0.32), Vector2(1.05, 0.32), Vector2(1.05, 1.02), Vector2(0.42, 1.02)],
			PAINT)
	_poly([Vector2(0.42, 0.95), Vector2(1.05, 0.95), Vector2(1.05, 1.02), Vector2(0.42, 1.02)],
			PAINT_LIGHT)


func _draw_cockpit() -> void:
	# The seat and the driver in a helmet, the steering wheel, and the roll
	# bar behind the seat.
	_poly([Vector2(-0.3, 0.42), Vector2(0.25, 0.42), Vector2(0.25, 0.55), Vector2(-0.3, 0.55)], SEAT)
	_poly([Vector2(-0.38, 0.5), Vector2(-0.26, 0.5), Vector2(-0.2, 1.05), Vector2(-0.32, 1.05)],
			SEAT)
	# The driver: legs to the pedals, body, arms to the wheel, helmet.
	_line(Vector2(0.05, 0.6), Vector2(0.55, 0.55), 0.13, Color(0.2, 0.3, 0.52))
	_line(Vector2(0.55, 0.55), Vector2(0.62, 0.32), 0.11, Color(0.2, 0.3, 0.52))
	_line(Vector2(-0.12, 0.62), Vector2(-0.02, 1.15), 0.2, Color(0.24, 0.36, 0.6))
	_line(Vector2(0.0, 1.05), Vector2(0.38, 0.98), 0.08, Color(0.24, 0.36, 0.6))
	_circle(Vector2(0.02, 1.32), 0.15, Color(0.95, 0.95, 0.97))
	_box(Vector2(0.04, 1.27), Vector2(0.17, 1.35), Color(0.15, 0.2, 0.3))
	_line(Vector2(0.4, 0.92), Vector2(0.5, 1.12), 0.05, SEAT)
	_line(Vector2(0.48, 1.1), Vector2(0.33, 1.17), 0.04, SEAT)
	# The roll bar.
	_line(Vector2(-0.48, 0.3), Vector2(-0.44, 1.78), 0.09, STEEL)
	_line(Vector2(-0.44, 1.75), Vector2(0.25, 1.75), 0.09, STEEL)
	_line(Vector2(0.25, 1.75), Vector2(0.32, 1.02), 0.07, STEEL)


func _draw_wheel(centre: Vector2, radius_m: float, lugs: int, lug_m: float, rim_m: float) -> void:
	_circle(centre, radius_m - lug_m * 0.3, RUBBER)
	# Chevron lugs: on the side they show as blocks round the tread.
	for i in lugs:
		var a := TAU * i / lugs
		var out := Vector2(cos(a), sin(a))
		var side := Vector2(-out.y, out.x)
		var half := TAU * radius_m / lugs * 0.24
		_poly([centre + out * (radius_m - lug_m) - side * half,
				centre + out * radius_m - side * half * 0.8,
				centre + out * radius_m + side * half * 0.8,
				centre + out * (radius_m - lug_m) + side * half], RUBBER_LUG)
	_circle(centre, rim_m, RIM)
	_circle(centre, rim_m * 0.82, Color(0.72, 0.58, 0.18))
	_circle(centre, rim_m * 0.35, STEEL_LIGHT)
	for i in 8:
		var a := TAU * i / 8.0
		_circle(centre + Vector2(cos(a), sin(a)) * rim_m * 0.55, rim_m * 0.06, STEEL)


func _draw_rear_wheel() -> void:
	_draw_wheel(Vector2.ZERO, REAR_RADIUS_M, 26, 0.12, 0.42)


func _draw_front_wheel() -> void:
	var centre := Vector2(WHEELBASE_M, FRONT_RADIUS_M - REAR_RADIUS_M)
	_draw_wheel(centre, FRONT_RADIUS_M, 20, 0.05, 0.26)


func _draw_fender() -> void:
	var outer := PackedVector2Array()
	var steps := 16
	for i in steps + 1:
		var a := PI * (0.12 + 0.72 * i / float(steps))
		outer.append(to_px(Vector2(cos(a), sin(a)) * (REAR_RADIUS_M + 0.08)))
	for i in steps + 1:
		var a := PI * (0.84 - 0.72 * i / float(steps))
		outer.append(to_px(Vector2(cos(a), sin(a)) * (REAR_RADIUS_M + 0.22)))
	draw_colored_polygon(outer, PAINT)


# Weights in stacks of blocks round `x_m`, `kg` of them (a block for each
# 60 kg, in up to two columns of four), on a carrier.
func _weight_stack(x_m: float, kg: float, carrier: bool) -> void:
	var blocks := clampi(int(round(kg / 60.0)), 0, 8)
	var columns := 2 if blocks > 4 else 1
	var base := Vector2(x_m - WEIGHT_COLUMN_M / 2.0 * columns, -0.36 if carrier else -0.2)
	if carrier:
		_box(base + Vector2(-0.04, -0.06), base + Vector2(WEIGHT_COLUMN_M * columns + 0.04, 0.0), STEEL)
		_line(base + Vector2(WEIGHT_COLUMN_M * columns, -0.03), Vector2(-0.3, -0.25), 0.06, STEEL)
	for i in blocks:
		var column := int(i / 4.0) if columns == 2 else 0
		var row := i % 4 if columns == 2 else i
		var x := base.x + column * WEIGHT_COLUMN_M
		var y := base.y + row * 0.11
		_box(Vector2(x, y), Vector2(x + 0.4, y + 0.1), WEIGHT)
		_box(Vector2(x, y + 0.08), Vector2(x + 0.4, y + 0.1), WEIGHT_EDGE)


func _draw_weights() -> void:
	# The front bracket always; the weights where the setup has them.
	_box(Vector2(3.42, -0.28), Vector2(3.86, -0.2), STEEL)
	if ballast_share < 0.0:
		_weight_stack(FRONT_WEIGHTS_X_M, 240.0, false)
		return
	_weight_stack(FRONT_WEIGHTS_X_M, ballast_kg * ballast_share, false)
	_weight_stack(REAR_WEIGHTS_X_M, ballast_kg * (1.0 - ballast_share), true)


func _draw_hitch_ladder() -> void:
	# The pulling hitch's holes, 2 cm apart, the one set in blue.
	var x := -0.55
	var low := hitch_low_m - REAR_RADIUS_M
	var top := hitch_top_m - REAR_RADIUS_M
	_box(Vector2(x - 0.07, low - 0.04), Vector2(x + 0.07, top + 0.04), STEEL_LIGHT)
	var holes := int(round((hitch_top_m - hitch_low_m) / 0.02))
	for i in holes + 1:
		var y := low + i * 0.02
		_circle(Vector2(x, y), 0.007, STEEL)
	_circle(Vector2(x, hitch_m - REAR_RADIUS_M), 0.022, HOLE_SET)


func _draw_footprint() -> void:
	# The rear tire's contact patch: longer as it is let down.
	var half_m := 0.22 + 0.28 * footprint
	var centre := to_px(Vector2(0.0, -REAR_RADIUS_M))
	var k := _scale()
	draw_set_transform(centre, 0.0, Vector2(1.0, 0.18))
	draw_circle(Vector2.ZERO, half_m * k, Color(HIGHLIGHT, 0.55))
	draw_set_transform(Vector2.ZERO)
