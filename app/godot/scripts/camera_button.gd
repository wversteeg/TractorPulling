# The camera button: a touch button drawn as a camera (a body with a lens
# and a viewfinder bump), so it reads at a glance and needs no image file.
class_name CameraButton
extends TouchButton

# Of the button's smaller side: the body's width and height, the lens's
# radius and the viewfinder bump's width and height.
const BODY_SHARE := Vector2(0.72, 0.46)
const LENS_SHARE := 0.15
const BUMP_SHARE := Vector2(0.22, 0.09)
const CORNER_SHARE := 0.08
const ICON_COLOUR := Color(0.95, 0.95, 0.95)
const LENS_COLOUR := Color(0.15, 0.15, 0.18)


func _draw() -> void:
	ShiftButton.draw_back(self, HELD_COLOUR if is_held() else COLOUR)
	var side := minf(size.x, size.y)
	var centre := size / 2.0 + Vector2(0.0, side * BUMP_SHARE.y / 2.0)
	var body_size := BODY_SHARE * side
	var body := StyleBoxFlat.new()
	body.bg_color = ICON_COLOUR
	body.set_corner_radius_all(int(side * CORNER_SHARE))
	draw_style_box(body, Rect2(centre - body_size / 2.0, body_size))
	var bump_size := BUMP_SHARE * side
	draw_rect(Rect2(centre + Vector2(-bump_size.x / 2.0, -body_size.y / 2.0 - bump_size.y),
			bump_size), ICON_COLOUR)
	draw_circle(centre, side * LENS_SHARE + 3.0, LENS_COLOUR)
	draw_circle(centre, side * LENS_SHARE * 0.55, ICON_COLOUR.darkened(0.3))
