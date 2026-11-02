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
# Of the smaller side: how far from the middle the chevron's tip and the
# sign sit, each in its half.
const TIP_FROM_MIDDLE_SHARE := 0.3
const SIGN_FROM_MIDDLE_SHARE := 0.1
const ICON_COLOUR := Color(0.95, 0.95, 0.95)

# Shifts up ("+"), else down ("-").
@export var up := true


func _draw() -> void:
	draw_key()
	var side := minf(size.x, size.y)
	var centre := size / 2.0
	var direction := -1.0 if up else 1.0
	# The chevron in the half it points to, the sign in the other.
	var tip := centre + Vector2(0.0, direction * side * TIP_FROM_MIDDLE_SHARE)
	var wing := Vector2(CHEVRON_SHARE.x * side / 2.0, -direction * CHEVRON_SHARE.y * side)
	var stroke := side * CHEVRON_STROKE_SHARE
	var colour := icon_colour(ICON_COLOUR)
	draw_polyline(PackedVector2Array([tip + Vector2(-wing.x, wing.y), tip,
			tip + Vector2(wing.x, wing.y)]), colour, stroke, true)
	var sign_at := centre - Vector2(0.0, direction * side * SIGN_FROM_MIDDLE_SHARE)
	var half := side * SIGN_SHARE / 2.0
	var line := side * SIGN_STROKE_SHARE
	draw_line(sign_at - Vector2(half, 0.0), sign_at + Vector2(half, 0.0), colour, line)
	if up:
		draw_line(sign_at - Vector2(0.0, half), sign_at + Vector2(0.0, half), colour, line)

