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
# The lens's dark ring past its glass [px], and the glass: a share of the
# lens's radius, a little darker than the body.
const LENS_RING_PX := 3.0
const GLASS_SHARE := 0.55
const GLASS_DARKENING := 0.3
const ICON_COLOUR := Color(0.95, 0.95, 0.95)
const LENS_COLOUR := Color(0.15, 0.15, 0.18)

var _body := StyleBoxFlat.new()


func _draw() -> void:
	draw_key()
	var side := minf(size.x, size.y)
	var centre := size / 2.0 + Vector2(0.0, side * BUMP_SHARE.y / 2.0)
	var body_size := BODY_SHARE * side
	var colour := icon_colour(ICON_COLOUR)
	_body.bg_color = colour
	_body.set_corner_radius_all(int(side * CORNER_SHARE))
	draw_style_box(_body, Rect2(centre - body_size / 2.0, body_size))
	var bump_size := BUMP_SHARE * side
	draw_rect(Rect2(centre + Vector2(-bump_size.x / 2.0, -body_size.y / 2.0 - bump_size.y),
			bump_size), colour)
	draw_circle(centre, side * LENS_SHARE + LENS_RING_PX, LENS_COLOUR)
	draw_circle(centre, side * LENS_SHARE * GLASS_SHARE, colour.darkened(GLASS_DARKENING))
