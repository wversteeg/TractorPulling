# Over the replay (PullReplay): "REPLAY" and the pull's distance at the top
# left, and "Back" at the top right, back to the result.
class_name ReplayOverlay
extends Control

signal back_requested

const MARGIN := 24.0
const TITLE_SIZE := 34
const LINE_SIZE := 26
const BACK_SIZE := Vector2(180.0, 76.0)

var back_button := TouchButton.new()
var _title := Label.new()
var _distance := Label.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for label: Label in [_title, _distance]:
		label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", 6)
		add_child(label)
	_title.text = "REPLAY"
	_title.add_theme_font_size_override("font_size", TITLE_SIZE)
	_title.add_theme_color_override("font_color", GarageArt.ORANGE)
	_title.position = Vector2(MARGIN, MARGIN)
	_distance.add_theme_font_size_override("font_size", LINE_SIZE)
	_distance.position = Vector2(MARGIN, MARGIN + TITLE_SIZE + 12.0)
	back_button.text = "Back"
	back_button.custom_minimum_size = BACK_SIZE
	back_button.size = BACK_SIZE
	back_button.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	back_button.position = Vector2(-BACK_SIZE.x - MARGIN, MARGIN)
	back_button.tapped.connect(func() -> void: back_requested.emit())
	add_child(back_button)


# The pull's distance in the frame shown [m].
func show_distance(distance_m: float) -> void:
	_distance.text = "%.2f m" % distance_m


func release_buttons() -> void:
	back_button.release()
