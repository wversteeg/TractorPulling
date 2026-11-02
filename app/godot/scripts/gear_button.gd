# The settings button: a touch button drawn as a gear (a toothed ring), so
# it reads the same in every language and needs no image file.
class_name GearButton
extends TouchButton

const TEETH := 8
# Of the button's half size: the teeth's tips, the ring's outer edge and
# the hole.
const TIP_SHARE := 0.8
const RING_SHARE := 0.6
const HOLE_SHARE := 0.27
# Of each tooth's pitch, the share its tip covers.
const TOOTH_SHARE := 0.45
# The tooth's root is this much wider than its tip: sloped flanks.
const ROOT_SPREAD := 1.4
const GEAR_COLOUR := Color(0.95, 0.95, 0.95)
const HOLE_COLOUR := Color(0.15, 0.15, 0.18)


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), HELD_COLOUR if is_held() else COLOUR)
	var centre := size / 2.0
	var half := minf(size.x, size.y) / 2.0
	var tip := half * TIP_SHARE
	var ring := half * RING_SHARE
	var points := PackedVector2Array()
	var pitch := TAU / TEETH
	for i in TEETH:
		var middle := i * pitch
		var half_tooth := pitch * TOOTH_SHARE / 2.0
		# Each tooth: up its flank to the tip, across it, and back down to the
		# ring, then along the ring to the next one.
		for corner: Array in [[middle - half_tooth * ROOT_SPREAD, ring], [middle - half_tooth, tip],
				[middle + half_tooth, tip], [middle + half_tooth * ROOT_SPREAD, ring]]:
			points.append(centre + Vector2.from_angle(float(corner[0])) * float(corner[1]))
	draw_colored_polygon(points, GEAR_COLOUR)
	draw_circle(centre, half * HOLE_SHARE, HOLE_COLOUR)
