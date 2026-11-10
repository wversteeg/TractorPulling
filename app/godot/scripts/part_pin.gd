# One of the workshop's pins on the tractor: the part's icon in a ring that
# fills as the part is built up, its name and where it stands below, and a
# € badge when its next level is affordable. A choice's pin is a full blue
# ring. Tapped, it opens the part's screen.
class_name PartPin
extends TouchButton

const RING_RADIUS := 34.0
const RING_WIDTH := 6.0
const ICON_SIZE := 36.0
const NAME_SIZE := 15
const NOTE_SIZE := 13
const BADGE_RADIUS := 11.0

# The part as the garage files name it (or a choice's slot), its name and
# a short line under it.
var part := ""
var title := ""
var note := ""
# How far the part is built (0 to 1); `choice` draws the full blue ring.
var share := 0.0
var choice := false
var complete := false
var affordable := false


func _init() -> void:
	fires_on_release = true
	custom_minimum_size = Vector2(170.0, 120.0)
	size = custom_minimum_size


# Where the ring's centre is, in the pin's own pixels: the line from the
# tractor ends there.
func ring_centre() -> Vector2:
	return Vector2(size.x / 2.0, RING_RADIUS + 4.0)


func _draw() -> void:
	var c := ring_centre()
	var held := is_held()
	draw_circle(c, RING_RADIUS, GarageArt.PANEL_LIGHT if held else GarageArt.PANEL)
	var colour := GarageArt.ORANGE
	if choice:
		colour = GarageArt.BLUE
	elif complete:
		colour = GarageArt.GREEN
	draw_arc(c, RING_RADIUS, 0.0, TAU, 48, GarageArt.LINE, RING_WIDTH, true)
	var sweep := TAU if choice or complete else TAU * clampf(share, 0.0, 1.0)
	if sweep > 0.0:
		draw_arc(c, RING_RADIUS, -PI / 2.0, -PI / 2.0 + sweep, 48, colour, RING_WIDTH, true)
	var icon_colour := GarageArt.GREEN if complete else GarageArt.TEXT
	GarageArt.draw_icon(self, part, c, ICON_SIZE, icon_colour)
	if affordable and not complete:
		var badge := c + Vector2(RING_RADIUS * 0.72, -RING_RADIUS * 0.72)
		draw_circle(badge, BADGE_RADIUS, GarageArt.ORANGE)
		_text("€", badge + Vector2(0.0, 5.0), 14, Color(0.1, 0.07, 0.02))
	_text(title, Vector2(size.x / 2.0, c.y + RING_RADIUS + 22.0), NAME_SIZE, GarageArt.TEXT)
	var note_colour := GarageArt.DIM
	if choice:
		note_colour = GarageArt.BLUE_LIGHT
	elif complete:
		note_colour = GarageArt.GREEN
	_text(note, Vector2(size.x / 2.0, c.y + RING_RADIUS + 40.0), NOTE_SIZE, note_colour)


# `text` centred on `at` (its baseline).
func _text(text: String, at: Vector2, font_size: int, colour: Color) -> void:
	var font := get_theme_default_font()
	var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	draw_string(font, Vector2(at.x - width / 2.0, at.y), text, HORIZONTAL_ALIGNMENT_LEFT, -1,
			font_size, colour)
