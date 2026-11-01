# A gear button: a rounded key with a chevron pointing the way it shifts
# (up for "+", down for "-") above the sign, like a sequential shifter's.
class_name ShiftButton
extends TouchButton

# Of the button's smaller side: the chevron's width, height and stroke,
# and the sign's size and stroke.
const CHEVRON_SHARE := Vector2(0.42, 0.18)
const CHEVRON_STROKE_SHARE := 0.07
const SIGN_SHARE := 0.2
const SIGN_STROKE_SHARE := 0.06
const CORNER_SHARE := 0.18
const BORDER := 3
const ICON_COLOUR := Color(0.95, 0.95, 0.95)
const BORDER_COLOUR := Color(1.0, 1.0, 1.0, 0.35)

# Shifts up ("+"), else down ("-").
@export var up := true


func _draw() -> void:
	draw_back(self, HELD_COLOUR if is_held() else COLOUR)
	var side := minf(size.x, size.y)
	var centre := size / 2.0
	var direction := -1.0 if up else 1.0
	# The chevron in the half it points to, the sign in the other.
	var tip := centre + Vector2(0.0, direction * side * 0.3)
	var wing := Vector2(CHEVRON_SHARE.x * side / 2.0, -direction * CHEVRON_SHARE.y * side)
	var stroke := side * CHEVRON_STROKE_SHARE
	draw_polyline(PackedVector2Array([tip + Vector2(-wing.x, wing.y), tip,
			tip + Vector2(wing.x, wing.y)]), ICON_COLOUR, stroke, true)
	var sign_at := centre - Vector2(0.0, direction * side * 0.1)
	var half := side * SIGN_SHARE / 2.0
	var line := side * SIGN_STROKE_SHARE
	draw_line(sign_at - Vector2(half, 0.0), sign_at + Vector2(half, 0.0), ICON_COLOUR, line)
	if up:
		draw_line(sign_at - Vector2(0.0, half), sign_at + Vector2(0.0, half), ICON_COLOUR, line)


# The rounded key behind an icon button, with a thin light rim.
static func draw_back(button: Control, colour: Color) -> void:
	var back := StyleBoxFlat.new()
	back.bg_color = colour
	back.set_corner_radius_all(int(minf(button.size.x, button.size.y) * CORNER_SHARE))
	back.set_border_width_all(BORDER)
	back.border_color = BORDER_COLOUR
	button.draw_style_box(back, Rect2(Vector2.ZERO, button.size))
