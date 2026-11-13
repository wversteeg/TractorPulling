# Where the pull takes place, around the track TrackView draws: a summer
# sky, grass, the prepared clay strip (the lane only, from just behind the
# sled: grass beside it, where the tractors drive back), the parking behind
# the start line on the right (laid out per event: PARKINGS), a fence with straw bales along each
# side, a covered grandstand full of people on the left and a standing crowd
# behind the fence on the right, distance boards every 10 m, a banner over
# the full-pull line and a scoreboard showing how far the sled has got;
# beyond them food tents, a barn, trees and flags. Built from the run's
# sizes: again only when the track's length or lane width changes (another
# event's track). The banner and scoreboard are built over the class's own
# full-pull line ("banner_line_m", 100 m), and stay there when the line in
# force moves (to 110 m after a test pull past 100 m, 120 m in a pull-off):
# the chalk line on the track shows that one. Only the scoreboard changes
# during a pull.
#
# Each event dresses it its own way (set_look(), from the season file):
# a small village pull has no grandstand, a thin crowd standing along the
# fences and only a signal light on a pole at the full-pull line; a club
# event the covered grandstand and the banner; a big event a longer, taller
# grandstand, a crowd three deep and flames and fireworks for a full pull
# (celebrate()). The time of day sets the sky, the sun and the floodlights:
# a few dim ones along the track at sunset, and in the evening many bright
# ones along the track, over the parking and at the track's end (light(),
# FLOODLIGHTS); the track's soil sets its colour (track_colours()). Past the
# end a pile of dirt across the track, and the event's trailers and pop-up
# tents behind it: at a club or big event a trailer with a big screen
# showing the run and the standings (TrackEnd).
#
# The people, trees, tents, barn, flags and clouds are Kenney's CC0 models
# (assets/kenney); the rest is built here. Copies of a thing are one
# MultiMesh each, and the scenery casts no shadows, for a phone's frame.
class_name VenueView
extends Node3D

const GRASS_DARK := Color(0.13, 0.25, 0.08)
const GRASS_LIGHT := Color(0.3, 0.43, 0.15)
const STRAW := Color(0.86, 0.72, 0.38)
const WOOD := Color(0.52, 0.36, 0.22)
const STEEL := Color(0.55, 0.57, 0.6)
const ROOF := Color(0.72, 0.12, 0.1)
const BOARD := Color(0.96, 0.96, 0.92)
const BANNER := Color(0.1, 0.32, 0.65)
const SHIRTS: Array[Color] = [Color(0.9, 0.2, 0.15), Color(0.2, 0.45, 0.85), Color(0.95, 0.8, 0.2),
		Color(0.25, 0.65, 0.3), Color(0.95, 0.95, 0.95), Color(0.15, 0.15, 0.2),
		Color(0.95, 0.5, 0.15), Color(0.6, 0.3, 0.7)]

# The grass field, and the clay strip's margin past the lane [m].
const FIELD_M := Vector2(1100.0, 1100.0)
# The hills round the field, and the tree line in front of them [m].
const HILLS_FROM_M := 420.0
const HILLS_TO_M := 560.0
const HILL_HEIGHT_M := 38.0
const HILL_SEGMENTS := 96
const HILLS := Color(0.3, 0.44, 0.24)
const FAR_HILLS := Color(0.42, 0.52, 0.5)
const TREE_LINE_M := 300.0
const TREE_LINE_TREES := 160
const COPSE_TREES := 9
# The fields between the venue and the tree line: their crops, with the
# rows they are sown in [m], and how many fields.
const CROPS: Array[Color] = [Color(0.82, 0.68, 0.32), Color(0.2, 0.36, 0.12),
		Color(0.4, 0.27, 0.16), Color(0.55, 0.6, 0.25)]
const CROP_ROW_M := 0.75
const FIELDS := 18
# The fields: lifted over the grass [m], and tries to find room for them
# all (how far past the lane's edges they stay clear is below, past the
# return lane).
const FIELD_LIFT_M := 0.15
const FIELD_TRIES := 20
const GRASS_Y_M := -0.06
const RUN_UP_M := 40.0
const OVERRUN_M := 30.0
# On the right, between the lane and the bales, room to drive back to the
# parking after a pull, and for the parking behind the start line: the
# fence, the bales and everything past them stand this much further out
# there [m].
const RETURN_LANE_M := 6.0
# How far past the lane's edges the fields stay clear: the stands, banks
# and tents are inside, on the right past the return lane [m].
const FIELDS_CLEAR_OF_LANE_M := 30.0 + RETURN_LANE_M
# The parkings, one per event in turn (set_parking_layout): the bays'
# rear axles right of the lane's right-hand chalk line ("right"), down the
# track from the start line ("xs", behind it: the player's bay first, the
# rivals' after it), which way the tractors face there ("heading", 0 down
# the track, turned left towards the lane) and the ground ("gravel", or
# "grass" with stakes at the bays' corners) [m, rad]. The player's tractor
# leaves from its bay and comes back to it (TractorPullRun.set_parking).
const PARKINGS: Array[Dictionary] = [
	{"right": 8.0, "heading": PI / 2.0, "xs": [-12.0, -16.5, -21.0, -25.5], "ground": "gravel"},
	{"right": 7.5, "heading": PI / 3.0, "xs": [-13.0, -18.0, -23.0, -28.0], "ground": "grass"},
	{"right": 10.0, "heading": PI / 2.0, "xs": [-16.0, -20.5, -25.0, -29.5], "ground": "grass"},
]
const RIVAL_BAYS := 3
# A bay: as deep as a tractor is long, as wide as one with room to climb
# down [m].
const BAY_SIZE_M := Vector2(5.0, 4.0)
# A bay starts this far behind the tractor's rear axle: room for the rear
# tires [m].
const BAY_BEHIND_REAR_AXLE_M := 1.0
# The parking's ground: behind the start line from here to the run-up's
# end, out from the lane's right-hand chalk line between these [m].
const PARKING_FRONT_X_M := -3.0
const PARKING_FROM_LANE_M := Vector2(1.0, 15.0)
const GRAVEL_DARK := Color(0.42, 0.4, 0.37)
const GRAVEL_LIGHT := Color(0.66, 0.63, 0.58)
# The clay strip: from this far behind the start line (just behind the
# sled), this far past each chalk line [m].
const CLAY_BEHIND_START_M := 5.0
# The clay is laid in pieces about this long, each lit by the floodlights
# near it, and its texture repeats every this many metres [m].
const CLAY_PIECE_M := 20.0
const CLAY_TILE_M := 6.0
const CLAY_PAST_LANE_M := 1.5
# The signal light's lamps on the full-pull gantry.
const LAMP_RADIUS_M := 0.22
const LAMP_RED := Color(1.0, 0.12, 0.08)
const LAMP_GREEN := Color(0.15, 1.0, 0.3)
const LAMP_GLOW := 3.0
# The fence and the straw bales inside it, from the lane's edge [m].
const FENCE_OUT_M := 6.5
const BALES_OUT_M := 5.5
const POST_EVERY_M := 2.5
const BALE_SIZE_M := Vector3(1.2, 0.5, 0.45)
# The grandstand: from the fence back, rows rising like steps [m].
const STAND_FROM_M := 9.5
const ROW_DEPTH_M := 0.9
const ROW_RISE_M := 0.5
const SEAT_EVERY_M := 0.85
# People are built at their size (People); each varies a little.
const PERSON_SCALE := 1.0
# How each size of event fills the venue: the grandstand's rows (none at a
# small pull) and length along the track, how full its seats are, the
# standing crowd's rows on the right and how full, whether the left fence
# has a standing crowd too (where there is no grandstand), whether a banner
# spans the full-pull line, a show for a full pull, the stand's sign, how
# many trailers stand past the track's end, whether one is a big screen
# and how many pop-up tents stand beside them (TrackEnd).
const SIZES := {
	"small": {"stand_rows": 0, "stand_along_m": Vector2(5.0, 95.0), "seat_fill": 0.0,
			"standing_rows": 1, "standing_fill": 0.45, "left_crowd": true, "banner": false,
			"show": false, "sign": "", "trailers": 1, "screen": false, "tents": 2},
	"club": {"stand_rows": 4, "stand_along_m": Vector2(5.0, 95.0), "seat_fill": 0.75,
			"standing_rows": 2, "standing_fill": 0.8, "left_crowd": false, "banner": true,
			"show": false, "sign": "TRACTOR PULLING  ·  CLUB PULL", "trailers": 2, "screen": true,
			"tents": 3},
	"big": {"stand_rows": 7, "stand_along_m": Vector2(-5.0, 110.0), "seat_fill": 0.95,
			"standing_rows": 3, "standing_fill": 0.95, "left_crowd": false, "banner": true,
			"show": true, "sign": "TRACTOR PULLING  ·  GRAND PRIX", "trailers": 2, "screen": true,
			"tents": 4},
}
# The track's colour: sand's and clay's, mixed by the soil's clay, and a
# wet track darker, by this much per unit of moisture.
const SAND := Color(0.72, 0.57, 0.33)
const CLAY := Color(0.5, 0.3, 0.19)
const WET_DARKENING := 1.6
const TRACK_LIGHTER := 0.25
# The floodlights at each time of day: masts this far apart down both
# sides (none in the day), their spotlights' energy and reach, and whether
# the parking gets its own [m, energy, m]. At sunset a few come on, dim,
# as the light goes; in the evening many, bright.
const FLOODLIGHTS := {
	"sunset": {"every_m": 45.0, "energy": 2.5, "range_m": 50.0, "parking": false},
	"evening": {"every_m": 20.0, "energy": 7.0, "range_m": 60.0, "parking": true},
}
const FLOODLIGHT_HEIGHT_M := 14.0
# A floodlight's beam: this wide from its middle to its edge, and fading
# this slowly towards the edge (Godot's spot_angle_attenuation, 1 linear):
# wide and even enough, aimed at the track, to light the grass beside it
# and the crowd at the fence too [deg, -].
const FLOODLIGHT_SPREAD_DEG := 70.0
const FLOODLIGHT_EDGE_FADE := 0.5
const FLOODLIGHT := Color(1.0, 0.93, 0.8)
# One more pair of masts this far past the track's end (over the run-off),
# another beside the pile's crest (TrackEnd), and over the parking at these
# distances behind the start line, beyond its far edge [m].
const END_LIGHTS_PAST_M := 25.0
const PARKING_LIGHTS_X_M: Array[float] = [-10.0, -24.0, -38.0]
const PARKING_LIGHTS_OUT_M := 2.0
# A lower lamp post beside each rival's bay, this far inside the parking's
# far edge (behind the parked tractor), this high [m].
const RIVAL_LIGHT_IN_M := 1.0
const RIVAL_LIGHT_HEIGHT_M := 7.0
# The fields stay this far clear past the track's end: the pile and the
# trailers behind it [m].
const END_CLEAR_M := 40.0
# The sky and the sun at each time of day (light()) [deg for the sun's
# pitch and yaw].
const TIMES := {
	"day": {"sky_top": Color(0.13, 0.33, 0.72), "horizon": Color(0.66, 0.77, 0.9),
			"ground_horizon": Color(0.6, 0.68, 0.6), "ground": Color(0.25, 0.32, 0.2),
			"ambient": 0.55, "fog": Color(0.7, 0.78, 0.88), "exposure": 1.05,
			"sun_pitch_deg": -48.0, "sun_yaw_deg": 35.0, "sun": Color(1.0, 0.97, 0.92),
			"sun_energy": 1.2},
	"sunset": {"sky_top": Color(0.2, 0.25, 0.5), "horizon": Color(0.98, 0.58, 0.32),
			"ground_horizon": Color(0.55, 0.4, 0.3), "ground": Color(0.2, 0.18, 0.14),
			"ambient": 0.42, "fog": Color(0.92, 0.62, 0.45), "exposure": 1.1,
			"sun_pitch_deg": -9.0, "sun_yaw_deg": 70.0, "sun": Color(1.0, 0.6, 0.32),
			"sun_energy": 1.05},
	"evening": {"sky_top": Color(0.01, 0.02, 0.06), "horizon": Color(0.1, 0.11, 0.2),
			"ground_horizon": Color(0.05, 0.06, 0.07), "ground": Color(0.02, 0.03, 0.02),
			"ambient": 0.18, "fog": Color(0.07, 0.08, 0.13), "exposure": 1.15,
			"sun_pitch_deg": -35.0, "sun_yaw_deg": -40.0, "sun": Color(0.55, 0.65, 1.0),
			"sun_energy": 0.12},
}
# A full pull's show at a big event: flames from the banner's poles and
# fireworks over the line, for this long [s].
const SHOW_S := 3.0
const FIREWORKS: Array[Color] = [Color(1.0, 0.3, 0.2), Color(0.3, 0.6, 1.0),
		Color(1.0, 0.85, 0.3), Color(0.4, 1.0, 0.5)]
# The bank the standing crowd is on [m].
const BANK_HEIGHT_M := 0.9
const BANK_DEPTH_M := 3.5
# Kenney's scenery tiles stand on a plate of their own; sunk this share of
# their size into the grass, the plate is gone.
const TILE_PLATE_SHARE := 0.09
const STANDING_EVERY_M := 1.1
const BOARD_EVERY_M := 10.0
const CROWD_SHADER := preload("res://shaders/crowd.gdshader")
const FLAG_SHADER := preload("res://shaders/flag.gdshader")
const FLAGS: Array[Color] = [Color(0.85, 0.12, 0.1), Color(0.95, 0.95, 0.95),
		Color(0.1, 0.3, 0.7), Color(0.95, 0.6, 0.1)]
const FLAG_SIZE_M := Vector2(1.6, 1.0)
const FLAG_POLE_M := 2.4
# The officials' flags beside the lane, waving: a yellow TEST flag down
# the track from the start line for a test pull, where the driver sees it
# from the cab, and a black-and-white chequered flag at the full-pull line,
# or in a pull-off where the best of the round so far stopped. Their poles'
# height, the cloth's size, how far out from the lane's left edge and how
# far down the track the TEST flag stands [m]; the chequers along it.
const OFFICIAL_FLAG_POLE_M := 3.2
const OFFICIAL_FLAG_SIZE_M := Vector2(1.6, 1.05)
const OFFICIAL_FLAG_OUT_M := 2.5
const TEST_FLAG_AT_M := 15.0
const TEST_FLAG := Color(1.0, 0.85, 0.1)
const CHEQUERS := 6.0
# How excited the crowd is before a pull, and from where along the track
# it starts to cheer (a share of the full-pull line).
const CALM := 0.15
const CHEER_FROM := 0.75

var _scoreboard: Label3D
# The signal light on the full-pull gantry: its red and green lamps.
var _red_lamp := StandardMaterial3D.new()
var _green_lamp := StandardMaterial3D.new()
var _signal := ""
var _crowd_material: ShaderMaterial
# The track's length and lane width the venue was built for [m].
var _sizes := Vector2.ZERO
var _full_pull_m := -1.0
# The banner and the scoreboard, which follow the full-pull line.
var _line: Node3D
# The fields round the venue, in track space: each one's centre (x along,
# y across) and the circle it turns in (z) [m].
var _fields: Array[Vector3] = []
var _random := RandomNumberGenerator.new()
var _test_flag: Node3D = null
var _best_flag: Node3D = null
# What the flags show (show_flags), kept for a rebuild.
var _test_pull := false
var _best_m := -1.0
var _lane_m := 12.0
# The event's look (set_look()) and the one the venue was built for.
var _look := {"size": "club", "time": "day", "clay_fraction": 0.4, "moisture": 0.15}
var _built_look := {}
# A full pull's show: the flames and fireworks, made with the banner.
var _show: Array[CPUParticles3D] = []
# The parking laid out now (PARKINGS), and the node that draws it.
var _parking_layout := 0
var _parking: Node3D = null
var _parking_built := -1
var _track_end: TrackEnd = null
# Where the floodlight masts stand (x down the track, y to the left) [m].
var _masts: Array[Vector2] = []
var _parking_masts: Array[Vector2] = []
var _rival_lights: Array[Vector2] = []


func build(d: Dictionary) -> void:
	var length_m: float = d["track_length_m"]
	var lane_m: float = d["lane_width_m"]
	var full_pull_m: float = d.get("banner_line_m", d["full_pull_line_m"])
	var sizes := Vector2(length_m, lane_m)
	if sizes != _sizes or _look != _built_look:
		for child: Node in get_children():
			child.free()
		_line = null
		_show.clear()
		_masts.clear()
		_track_end = TrackEnd.new()
		_sizes = sizes
		_built_look = _look.duplicate()
		_full_pull_m = -1.0
		# The same venue at every launch.
		_random.seed = 2026
		_add_ground(length_m, lane_m)
		for side: float in [-1.0, 1.0]:
			_add_fence(length_m, lane_m, side)
			_add_boards(length_m, lane_m, side)
		if int(_size()["stand_rows"]) > 0:
			_add_grandstand(lane_m)
		_add_standing_crowd(length_m, lane_m, -1.0)
		if _size()["left_crowd"]:
			_add_standing_crowd(length_m, lane_m, 1.0)
		if FLOODLIGHTS.has(_look["time"]):
			_add_floodlights(length_m, lane_m, FLOODLIGHTS[_look["time"]])
		add_child(_track_end)
		_track_end.build(length_m, lane_m, lane_m + 2.0 * CLAY_PAST_LANE_M,
				track_colours(float(_look["clay_fraction"]), float(_look["moisture"])),
				int(_size()["trailers"]), bool(_size()["screen"]),
				int(_size()["tents"]))
		_add_scenery(length_m, lane_m)
		_add_farmland(length_m, lane_m)
		_add_horizon(length_m)
		_parking = null
		_parking_built = -1
	if _parking_built != _parking_layout:
		if _parking != null:
			_parking.free()
		_parking = Node3D.new()
		add_child(_parking)
		_add_parking(_parking, lane_m)
		_parking_masts.clear()
		_rival_lights.clear()
		var lights: Dictionary = FLOODLIGHTS.get(_look["time"], {})
		if lights.get("parking", false):
			_add_parking_lights(_parking, lane_m, lights)
		_parking_built = _parking_layout
	_lane_m = lane_m
	# A rebuild freed the flags: up again as they were.
	show_flags(_test_pull, _best_m)
	if full_pull_m != _full_pull_m:
		if _line != null:
			_line.free()
		_line = Node3D.new()
		add_child(_line)
		if _size()["banner"]:
			_add_banner(_line, full_pull_m, lane_m)
		else:
			_add_signal_pole(_line, full_pull_m, lane_m)
		if _size()["show"]:
			_add_show(_line, full_pull_m, lane_m)
		_add_scoreboard(_line, full_pull_m, lane_m)
		_full_pull_m = full_pull_m


# The event's look for the next build: TractorPullEvent.get_event()'s
# "size" and "time", and the track's soil ("track": "clay_fraction",
# "moisture", from TractorPullRun.get_dimensions()). Missing values keep a
# club event in the day.
func set_look(event: Dictionary) -> void:
	var track: Dictionary = event.get("track", {})
	_look = {
		"size": event.get("size", "club") if SIZES.has(event.get("size", "")) else "club",
		"time": event.get("time", "day") if event.get("time", "") in ["day", "sunset", "evening"]
				else "day",
		"clay_fraction": float(track.get("clay_fraction", 0.4)),
		"moisture": float(track.get("moisture", 0.15)),
	}


func look() -> Dictionary:
	return _look


func _size() -> Dictionary:
	return SIZES[_look["size"]]


# The track's two shades (dark, light) for `clay_fraction` and `moisture`:
# sandy tracks pale and yellow, clay ones red-brown, wet ones darker.
static func track_colours(clay_fraction: float, moisture: float) -> Array[Color]:
	var dry := SAND.lerp(CLAY, clampf(clay_fraction, 0.0, 1.0))
	var dark := dry.darkened(clampf(moisture * WET_DARKENING, 0.0, 0.6))
	return [dark, dark.lightened(TRACK_LIGHTER)]


# A full pull: at a big event the flames go up from the banner's poles and
# fireworks burst over the line; elsewhere nothing.
func celebrate() -> void:
	for part in _show:
		part.restart()
		part.emitting = true


# Whether the last full pull set off the show (for the tests).
func show_running() -> bool:
	return _show.any(func(part: CPUParticles3D) -> bool: return part.emitting)


# The parking for the next build (an event's: PARKINGS, in turn).
func set_parking_layout(index: int) -> void:
	_parking_layout = posmod(index, PARKINGS.size())


func parking_layout() -> Dictionary:
	return PARKINGS[_parking_layout]


# The bays: x down the track, right of the lane's right-hand chalk line,
# and the heading the tractors face [m, m, rad]; the player's first.
func bays() -> Array[Vector3]:
	var layout := parking_layout()
	var out: Array[Vector3] = []
	for x_m: float in layout["xs"]:
		out.append(Vector3(x_m, layout["right"], layout["heading"]))
	return out


# The rivals' bays (bays() after the player's) [m, m, rad].
func rival_bays() -> Array[Vector3]:
	var all := bays()
	var out: Array[Vector3] = []
	for i in RIVAL_BAYS:
		out.append(all[1 + i])
	return out


# Where the banner over the full-pull line stands, along the track [m].
func banner_at_m() -> float:
	return _full_pull_m


func fields() -> Array[Vector3]:
	return _fields


# How far the sled has got, on the board by the full-pull line; and the
# crowd gets louder as it nears the line.
func show_distance(sled_m: float) -> void:
	if _scoreboard != null:
		_scoreboard.text = "%6.2f m" % maxf(sled_m, 0.0)
	if _crowd_material != null and _full_pull_m > 0.0:
		var near := clampf((sled_m / _full_pull_m - CHEER_FROM) / (1.0 - CHEER_FROM), 0.0, 1.0)
		_crowd_material.set_shader_parameter("excitement", lerpf(CALM, 1.0, near))


# How excited the crowd is, 0 to 1.
func excitement() -> float:
	if _crowd_material == null:
		return 0.0
	return float(_crowd_material.get_shader_parameter("excitement"))


# The sky, its light and a little haze: added by the scene once. A summer
# afternoon: a deep blue sky paling to the horizon, the sun's disk, a soft
# glow on bright things, and haze that hides the field's far edge.
static func environment() -> WorldEnvironment:
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color(0.13, 0.33, 0.72)
	sky_material.sky_horizon_color = Color(0.66, 0.77, 0.9)
	sky_material.sky_curve = 0.12
	sky_material.ground_horizon_color = Color(0.6, 0.68, 0.6)
	sky_material.ground_bottom_color = Color(0.25, 0.32, 0.2)
	sky_material.sun_angle_max = 12.0
	sky_material.sun_curve = 0.08
	var sky := Sky.new()
	sky.sky_material = sky_material
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.55
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 1.05
	env.glow_enabled = true
	env.glow_intensity = 0.35
	env.glow_bloom = 0.04
	env.glow_hdr_threshold = 1.1
	env.adjustment_enabled = true
	env.adjustment_saturation = 1.18
	env.adjustment_contrast = 1.06
	env.fog_enabled = true
	env.fog_light_color = Color(0.7, 0.78, 0.88)
	env.fog_density = 0.0009
	env.fog_sky_affect = 0.0
	var world := WorldEnvironment.new()
	world.environment = env
	return world


# The light for the time of day ("day", "sunset" or "evening") on the
# scene's sky (environment()) and sun: a high afternoon sun; a low orange
# sun under a red horizon, the first floodlights on; or night falling, the
# sky deep blue and a faint cold light left, the floodlights lighting the
# track (_add_floodlights()).
static func light(world: WorldEnvironment, sun: DirectionalLight3D, time: String) -> void:
	var look: Dictionary = TIMES.get(time, TIMES["day"])
	var env := world.environment
	var sky := env.sky.sky_material as ProceduralSkyMaterial
	sky.sky_top_color = look["sky_top"]
	sky.sky_horizon_color = look["horizon"]
	sky.ground_horizon_color = look["ground_horizon"]
	sky.ground_bottom_color = look["ground"]
	env.ambient_light_energy = look["ambient"]
	env.fog_light_color = look["fog"]
	env.tonemap_exposure = look["exposure"]
	sun.rotation = Vector3(deg_to_rad(look["sun_pitch_deg"]), deg_to_rad(look["sun_yaw_deg"]), 0.0)
	sun.light_color = look["sun"]
	sun.light_energy = look["sun_energy"]


func _add_ground(length_m: float, lane_m: float) -> void:
	var grass_material := StandardMaterial3D.new()
	grass_material.albedo_texture = Art.ground_texture(GRASS_DARK, GRASS_LIGHT, 0.02, 7)
	grass_material.normal_enabled = true
	grass_material.normal_texture = Art.ground_normal(0.08, 8, 6.0)
	grass_material.normal_scale = 0.6
	grass_material.uv1_scale = Vector3(FIELD_M.x / 12.0, FIELD_M.y / 12.0, 1.0)
	grass_material.roughness = 1.0
	var grass := PlaneMesh.new()
	grass.size = FIELD_M
	var field := Art.instance(grass, grass_material)
	field.position = TrackSpace.point(length_m / 2.0, 0.0, GRASS_Y_M)
	_quiet(field)
	add_child(field)

	var strip_length_m := CLAY_BEHIND_START_M + length_m + OVERRUN_M
	var clay_material := StandardMaterial3D.new()
	var shades := track_colours(float(_look["clay_fraction"]), float(_look["moisture"]))
	clay_material.albedo_texture = Art.ground_texture(shades[0], shades[1], 0.05, 11)
	var strip_width_m := lane_m + 2.0 * CLAY_PAST_LANE_M
	# Clods catch the low sun: a fine normal map over the colour.
	clay_material.normal_enabled = true
	clay_material.normal_texture = Art.ground_normal(0.06, 12, 5.0)
	clay_material.normal_scale = 0.8
	clay_material.roughness = 0.95
	# In pieces down the track: a renderer lights each mesh with only its
	# nearest few lights, so one long strip would miss most floodlights.
	# Each piece carries on the texture where the last one stopped.
	var end_m := length_m + OVERRUN_M
	var pieces := ceili(strip_length_m / CLAY_PIECE_M)
	var piece_m := strip_length_m / pieces
	var clay := PlaneMesh.new()
	clay.size = Vector2(strip_width_m, piece_m)
	for i in pieces:
		var surface := clay_material.duplicate() as StandardMaterial3D
		surface.uv1_scale = Vector3(strip_width_m / CLAY_TILE_M, piece_m / CLAY_TILE_M, 1.0)
		surface.uv1_offset = Vector3(0.0, i * piece_m / CLAY_TILE_M, 0.0)
		var strip := Art.instance(clay, surface)
		strip.position = TrackSpace.point(end_m - (i + 0.5) * piece_m, 0.0, -0.03)
		_quiet(strip)
		add_child(strip)


# The parking behind the start line on the right: gravel with white lines
# between the bays, or the field's grass with a stake at each bay's corner.
func _add_parking(parent: Node3D, lane_m: float) -> void:
	var layout := parking_layout()
	var edge_m := lane_m / 2.0
	if layout["ground"] == "gravel":
		var gravel_material := StandardMaterial3D.new()
		gravel_material.albedo_texture = Art.ground_texture(GRAVEL_DARK, GRAVEL_LIGHT, 0.2, 5)
		gravel_material.normal_enabled = true
		gravel_material.normal_texture = Art.ground_normal(0.3, 6, 3.0)
		gravel_material.roughness = 1.0
		var size := Vector2(PARKING_FROM_LANE_M.y - PARKING_FROM_LANE_M.x,
				PARKING_FRONT_X_M + RUN_UP_M)
		gravel_material.uv1_scale = Vector3(size.x / 4.0, size.y / 4.0, 1.0)
		var plane := PlaneMesh.new()
		plane.size = size
		var gravel := Art.instance(plane, gravel_material)
		gravel.position = TrackSpace.point(PARKING_FRONT_X_M - size.y / 2.0,
				-(edge_m + (PARKING_FROM_LANE_M.x + PARKING_FROM_LANE_M.y) / 2.0), -0.035)
		parent.add_child(_quiet(gravel))
	# Each bay's sides: along the tractor's heading, half a bay either side
	# of its middle, which is half a bay ahead of the bay's back.
	var heading: float = layout["heading"]
	var ahead := Vector2(cos(heading), sin(heading))
	var left := Vector2(-ahead.y, ahead.x)
	var lines: Array[Transform3D] = []
	var stakes: Array[Transform3D] = []
	for bay in bays():
		var rear := Vector2(bay.x, -(edge_m + bay.y))
		var middle := rear + ahead * (BAY_SIZE_M.x / 2.0 - BAY_BEHIND_REAR_AXLE_M)
		for side: float in [-1.0, 1.0]:
			var line_at := middle + left * side * BAY_SIZE_M.y / 2.0
			lines.append(Transform3D(Basis(Vector3.UP, heading),
					TrackSpace.point(line_at.x, line_at.y, -0.02)))
			for end: float in [-1.0, 1.0]:
				var stake_at := line_at + ahead * end * BAY_SIZE_M.x / 2.0
				stakes.append(Transform3D(Basis(), TrackSpace.point(stake_at.x, stake_at.y, 0.25)))
	if layout["ground"] == "gravel":
		var line := BoxMesh.new()
		line.size = Vector3(0.12, 0.02, BAY_SIZE_M.x)
		parent.add_child(_quiet(Art.many(line, lines, Art.material(Color(0.95, 0.95, 0.9), 0.9))))
	else:
		var stake := BoxMesh.new()
		stake.size = Vector3(0.06, 0.5, 0.06)
		parent.add_child(_quiet(Art.many(stake, stakes, Art.material(Color(0.95, 0.5, 0.1), 0.8))))


# How much further out the venue stands beside the lane on `side` (-1 right,
# 1 left): the return lane is on the right.
static func past_lane_m(side: float) -> float:
	return RETURN_LANE_M if side < 0.0 else 0.0


func _add_fence(length_m: float, lane_m: float, side: float) -> void:
	# On the right the parking is open to the lane: the fence starts past it.
	var from_m := -RUN_UP_M if side > 0.0 else PARKING_FRONT_X_M
	var to_m := length_m + OVERRUN_M
	var posts: Array[Transform3D] = []
	var x_m := from_m
	while x_m <= to_m:
		posts.append(Transform3D(Basis(), TrackSpace.point(x_m,
				side * (lane_m / 2.0 + FENCE_OUT_M + past_lane_m(side)), 0.55)))
		x_m += POST_EVERY_M
	var post_mesh := BoxMesh.new()
	post_mesh.size = Vector3(0.1, 1.1, 0.1)
	add_child(_quiet(Art.many(post_mesh, posts, Art.material(WOOD))))
	for height_m: float in [0.45, 0.95]:
		var rail := Art.box(Vector3(0.06, 0.12, to_m - from_m), BOARD)
		rail.position = TrackSpace.point((from_m + to_m) / 2.0,
				side * (lane_m / 2.0 + FENCE_OUT_M + past_lane_m(side)), height_m)
		add_child(_quiet(rail))

	# Straw bales two high inside the fence, a few knocked askew.
	var bales: Array[Transform3D] = []
	x_m = from_m + 1.0
	while x_m <= to_m:
		for layer in 2:
			var yaw := _random.randf_range(-0.06, 0.06)
			var at := TrackSpace.point(x_m + (0.6 if layer == 1 else 0.0),
					side * (lane_m / 2.0 + BALES_OUT_M + past_lane_m(side)),
					BALE_SIZE_M.y * (layer + 0.5))
			bales.append(Transform3D(Basis(Vector3.UP, yaw), at))
		x_m += BALE_SIZE_M.z * 2.6 + 0.05
	var bale_mesh := BoxMesh.new()
	bale_mesh.size = Vector3(BALE_SIZE_M.z, BALE_SIZE_M.y, BALE_SIZE_M.x)
	add_child(_quiet(Art.many(bale_mesh, bales, Art.material(STRAW, 1.0))))


# A white board on a post every 10 m, its distance facing the lane.
func _add_boards(length_m: float, lane_m: float, side: float) -> void:
	var x_m := BOARD_EVERY_M
	while x_m <= length_m + 0.5 * BOARD_EVERY_M:
		var at := TrackSpace.point(x_m,
				side * (lane_m / 2.0 + BALES_OUT_M + past_lane_m(side) - 1.2), 0.0)
		var post := Art.box(Vector3(0.08, 1.4, 0.08), STEEL)
		post.position = at + Vector3(0.0, 0.7, 0.0)
		add_child(_quiet(post))
		var board := Art.box(Vector3(1.1, 0.7, 0.05), BOARD)
		board.position = at + Vector3(0.0, 1.6, 0.0)
		# Facing back down the track, where the cameras are.
		add_child(_quiet(board))
		var number := Art.label("%d" % int(x_m), 64, Color(0.1, 0.1, 0.1), BOARD)
		number.position = at + Vector3(0.0, 1.6, 0.03)
		add_child(number)
		x_m += BOARD_EVERY_M


func _add_grandstand(lane_m: float) -> void:
	var stand_rows: int = _size()["stand_rows"]
	var stand_along_m: Vector2 = _size()["stand_along_m"]
	var seat_fill: float = _size()["seat_fill"]
	var base_y_m := lane_m / 2.0 + STAND_FROM_M
	var along_m := stand_along_m.y - stand_along_m.x
	var middle_m := (stand_along_m.x + stand_along_m.y) / 2.0
	var seats: Array[Transform3D] = []
	var colours: Array[Color] = []
	for row in stand_rows:
		var y_m := base_y_m + row * ROW_DEPTH_M
		var z_m := (row + 1) * ROW_RISE_M
		var step := Art.box(Vector3(ROW_DEPTH_M, z_m, along_m), Color(0.75, 0.75, 0.72))
		step.position = TrackSpace.point(middle_m, y_m, z_m / 2.0)
		add_child(_quiet(step))
		var bench := Art.box(Vector3(0.35, 0.08, along_m), WOOD)
		bench.position = TrackSpace.point(middle_m, y_m + 0.2, z_m + 0.4)
		add_child(_quiet(bench))
		var x_m := stand_along_m.x + 0.5
		while x_m < stand_along_m.y - 0.5:
			if _random.randf() < seat_fill:
				# Facing the track (the model faces +z: turned to +x, the
				# track's right), a little turned towards the action.
				var basis := Basis(Vector3.UP, PI / 2.0 + _random.randf_range(-0.35, 0.35))
				basis = basis.scaled(Vector3.ONE * PERSON_SCALE * _random.randf_range(0.9, 1.05))
				# The hips on the bench, the feet in front of it.
				seats.append(Transform3D(basis, TrackSpace.point(x_m + _random.randf_range(-0.1, 0.1),
						y_m - 0.15, z_m)))
				colours.append(_shirt())
			x_m += SEAT_EVERY_M
	add_child(_crowd(seats, colours, true))

	# A roof on posts over the stand, and flags along its front.
	var back_y_m := base_y_m + stand_rows * ROW_DEPTH_M + 0.3
	var roof_z_m := stand_rows * ROW_RISE_M + 3.2
	var roof := Art.box(Vector3(stand_rows * ROW_DEPTH_M + 1.6, 0.15, along_m + 1.0), ROOF)
	roof.position = TrackSpace.point(middle_m, base_y_m + stand_rows * ROW_DEPTH_M / 2.0, roof_z_m)
	roof.rotation.z = -0.12
	add_child(_quiet(roof))
	var back := Art.box(Vector3(0.15, roof_z_m, along_m), Color(0.85, 0.85, 0.82))
	back.position = TrackSpace.point(middle_m, back_y_m, roof_z_m / 2.0)
	add_child(_quiet(back))
	var x_m := stand_along_m.x
	while x_m <= stand_along_m.y + 0.1:
		var pole := Art.cylinder(0.08, roof_z_m, STEEL, 8)
		pole.position = TrackSpace.point(x_m, base_y_m - 0.5, roof_z_m / 2.0)
		add_child(_quiet(pole))
		_add_flag(TrackSpace.point(x_m, base_y_m - 0.5, roof_z_m),
				FLAGS[int(x_m) % FLAGS.size()], x_m)
		x_m += along_m / 6.0
	var sign := Art.label(_size()["sign"], 180, Color.WHITE, ROOF.darkened(0.4))
	sign.position = TrackSpace.point(middle_m, base_y_m - 1.0, roof_z_m - 0.5)
	sign.rotation.y = PI / 2.0
	add_child(sign)


# A pole on the grandstand's roof with a flag waving from its top.
func _add_flag(foot: Vector3, colour: Color, phase: float) -> void:
	var pole := Art.cylinder(0.04, FLAG_POLE_M, STEEL, 6, 0.4, 0.6)
	pole.position = foot + Vector3(0.0, FLAG_POLE_M / 2.0, 0.0)
	add_child(_quiet(pole))
	var cloth := PlaneMesh.new()
	cloth.size = FLAG_SIZE_M
	cloth.subdivide_width = 10
	cloth.orientation = PlaneMesh.FACE_Z
	var material := ShaderMaterial.new()
	material.shader = FLAG_SHADER
	material.set_shader_parameter("colour", colour)
	material.set_shader_parameter("phase", phase)
	var flag := Art.instance(cloth, material)
	# Flying from the pole along the track (Godot's -z is down the track,
	# the plane spans x: turned so its pole edge is at the pole).
	flag.rotation.y = PI / 2.0
	flag.position = foot + Vector3(0.0, FLAG_POLE_M - FLAG_SIZE_M.y / 2.0, -FLAG_SIZE_M.x / 2.0)
	add_child(_quiet(flag))


# People standing on a grass bank behind the fence on the right (`side`
# -1), so they see over it, as many rows deep as the event's size says; at
# a small pull a thin row behind the left fence too (`side` 1).
func _add_standing_crowd(length_m: float, lane_m: float, side: float) -> void:
	var standing_rows: int = _size()["standing_rows"] if side < 0.0 else 1
	var fill: float = _size()["standing_fill"]
	var bank_from_m := lane_m / 2.0 + FENCE_OUT_M + (RETURN_LANE_M if side < 0.0 else 0.0) + 0.4
	var bank := Art.box(Vector3(BANK_DEPTH_M, BANK_HEIGHT_M, length_m + 30.0), GRASS_LIGHT, 1.0)
	bank.position = TrackSpace.point(length_m / 2.0, side * (bank_from_m + BANK_DEPTH_M / 2.0),
			BANK_HEIGHT_M / 2.0 - 0.05)
	add_child(_quiet(bank))
	var people: Array[Transform3D] = []
	var colours: Array[Color] = []
	for row in standing_rows:
		var x_m := -10.0
		while x_m < length_m + 15.0:
			if _random.randf() < fill:
				var basis := Basis(Vector3.UP, side * PI / 2.0 + _random.randf_range(-0.5, 0.5))
				basis = basis.scaled(Vector3.ONE * PERSON_SCALE * _random.randf_range(0.85, 1.05))
				var y_m := side * (bank_from_m + 0.4 + row * 1.0 + _random.randf_range(0.0, 0.4))
				people.append(Transform3D(basis, TrackSpace.point(
						x_m + _random.randf_range(-0.3, 0.3), y_m, BANK_HEIGHT_M - 0.05)))
				colours.append(_shirt())
			x_m += STANDING_EVERY_M
	add_child(_crowd(people, colours, false))


# A crowd in one MultiMesh: each person's shirt and skin ride in its
# custom data (crowd.gdshader).
func _crowd(transforms: Array[Transform3D], shirts: Array[Color],
		seated: bool) -> MultiMeshInstance3D:
	# One moving crowd material for both crowds, so one call stirs them.
	if _crowd_material == null:
		_crowd_material = ShaderMaterial.new()
		_crowd_material.shader = CROWD_SHADER
		_crowd_material.set_shader_parameter("excitement", CALM)
	var custom: Array[Color] = []
	# White per copy: the vertex colour must reach the shader unchanged,
	# and a MultiMesh without colours can zero it.
	var white: Array[Color] = []
	for shirt in shirts:
		custom.append(Color(shirt.r, shirt.g, shirt.b, _random.randf()))
		white.append(Color.WHITE)
	var person := People.seated() if seated else People.standing()
	var crowd := Art.many(person, transforms, _crowd_material, white, custom)
	return _quiet(crowd) as MultiMeshInstance3D


func _shirt() -> Color:
	var colour: Color = SHIRTS[_random.randi() % SHIRTS.size()]
	return colour.lerp(Color.WHITE, _random.randf_range(0.0, 0.25))


# A banner over the full-pull line on two poles.
func _add_banner(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var half_m := lane_m / 2.0 + 1.5
	var height_m := 5.5
	for side: float in [-1.0, 1.0]:
		var pole := Art.cylinder(0.12, height_m, STEEL, 10, 0.4, 0.6)
		pole.position = TrackSpace.point(full_pull_m, side * half_m, height_m / 2.0)
		parent.add_child(pole)
	var banner := Art.box(Vector3(2.0 * half_m, 1.0, 0.08), BANNER)
	banner.position = TrackSpace.point(full_pull_m, 0.0, height_m - 0.5)
	parent.add_child(_quiet(banner))
	var text := Art.label("FULL PULL", 120, Color.WHITE, BANNER.darkened(0.5))
	text.position = TrackSpace.point(full_pull_m - 0.06, 0.0, height_m - 0.5)
	parent.add_child(text)
	# The signal light on top, facing down the track.
	_add_lamps(parent, full_pull_m, 0.0, height_m)


# At a small pull no banner: the signal light on a single pole beside the
# full-pull line, on the left, facing down the track.
func _add_signal_pole(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var height_m := 3.2
	var side_m := lane_m / 2.0 + 1.5
	var pole := Art.cylinder(0.08, height_m, STEEL, 8, 0.4, 0.6)
	pole.position = TrackSpace.point(full_pull_m, side_m, height_m / 2.0)
	parent.add_child(_quiet(pole))
	_add_lamps(parent, full_pull_m, side_m, height_m)


# The signal light's housing and lamps on top of a pole or the banner, at
# `side_m` across the track: red above green, as the officials' flags show
# (show_signal()).
func _add_lamps(parent: Node3D, full_pull_m: float, side_m: float, height_m: float) -> void:
	var housing := Art.box(Vector3(0.7, 1.3, 0.35), Color(0.08, 0.08, 0.09))
	housing.position = TrackSpace.point(full_pull_m, side_m, height_m + 0.65)
	parent.add_child(_quiet(housing))
	for lamp: Array in [[_red_lamp, 0.95], [_green_lamp, 0.35]]:
		var sphere := SphereMesh.new()
		sphere.radius = LAMP_RADIUS_M
		sphere.height = 2.0 * LAMP_RADIUS_M
		var bulb := Art.instance(sphere, lamp[0])
		bulb.position = TrackSpace.point(full_pull_m - 0.18, side_m, height_m + float(lamp[1]))
		parent.add_child(_quiet(bulb))
	_signal = "?"
	show_signal("off")


# A big event's show for a full pull (celebrate()): a jet of flame from
# beside each of the banner's poles and fireworks bursting high over the
# line, all idle until then.
func _add_show(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var half_m := lane_m / 2.0 + 2.5
	for side: float in [-1.0, 1.0]:
		var flame := CPUParticles3D.new()
		flame.emitting = false
		flame.one_shot = true
		flame.amount = 60
		flame.lifetime = 0.9
		flame.explosiveness = 0.0
		flame.direction = Vector3.UP
		flame.spread = 8.0
		flame.gravity = Vector3(0.0, 2.0, 0.0)
		flame.initial_velocity_min = 7.0
		flame.initial_velocity_max = 10.0
		flame.scale_amount_min = 0.5
		flame.scale_amount_max = 0.9
		flame.mesh = _glow_mesh(0.35, Color(1.0, 0.55, 0.1))
		flame.color_ramp = _fade(Color(1.0, 0.9, 0.4), Color(0.9, 0.2, 0.05, 0.0))
		flame.position = TrackSpace.point(full_pull_m, side * half_m, 0.3)
		parent.add_child(flame)
		_show.append(flame)
	for i in FIREWORKS.size():
		var burst := CPUParticles3D.new()
		burst.emitting = false
		burst.one_shot = true
		burst.amount = 70
		burst.lifetime = 1.6
		burst.explosiveness = 1.0
		burst.direction = Vector3.UP
		burst.spread = 180.0
		burst.gravity = Vector3(0.0, -3.0, 0.0)
		burst.initial_velocity_min = 7.0
		burst.initial_velocity_max = 9.0
		burst.mesh = _glow_mesh(0.2, FIREWORKS[i])
		burst.color_ramp = _fade(FIREWORKS[i], Color(FIREWORKS[i], 0.0))
		# Staggered: each bursts a moment after the last.
		burst.preprocess = 0.0
		burst.speed_scale = 1.0
		burst.position = TrackSpace.point(full_pull_m + 6.0 + 4.0 * i, (i - 1.5) * 6.0,
				22.0 + 3.0 * (i % 2))
		parent.add_child(burst)
		_show.append(burst)


# A small glowing sphere for a particle.
static func _glow_mesh(radius_m: float, colour: Color) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius_m
	mesh.height = 2.0 * radius_m
	mesh.radial_segments = 8
	mesh.rings = 4
	var glow := StandardMaterial3D.new()
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	glow.vertex_color_use_as_albedo = true
	glow.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	glow.albedo_color = colour
	mesh.material = glow
	return mesh


static func _fade(from: Color, to: Color) -> Gradient:
	var ramp := Gradient.new()
	ramp.set_color(0, from)
	ramp.set_color(1, to)
	return ramp


# Floodlight masts down both sides, `lights` (FLOODLIGHTS) apart, a pair
# past the track's end over the run-off, and a pair either side of the pile
# lighting the berm.
func _add_floodlights(length_m: float, lane_m: float, lights: Dictionary) -> void:
	var xs: Array[float] = []
	var x_m := -10.0
	while x_m <= length_m + 20.0:
		xs.append(x_m)
		x_m += float(lights["every_m"])
	xs.append(length_m + END_LIGHTS_PAST_M)
	for along_m in xs:
		for side: float in [-1.0, 1.0]:
			var out_m := lane_m / 2.0 + FENCE_OUT_M + 1.0 + past_lane_m(side)
			_add_mast(self, Vector2(along_m, side * out_m),
					Vector2(along_m + 6.0, -side * 1.0), side * 0.3, lights)
			_masts.append(Vector2(along_m, side * out_m))
	var crest_m := length_m + TrackEnd.PILE_FROM_M + TrackEnd.PILE_LENGTH_M / 2.0
	for side: float in [-1.0, 1.0]:
		var out_m := lane_m / 2.0 + FENCE_OUT_M + 1.0 + past_lane_m(side)
		_add_mast(self, Vector2(crest_m, side * out_m), Vector2(crest_m, 0.0), side * 0.3,
				lights)
		_masts.append(Vector2(crest_m, side * out_m))


# Masts beyond the parking's far edge, lighting the bays, and a lower lamp
# post behind each rival's bay, aimed at the tractor parked in it.
func _add_parking_lights(parent: Node3D, lane_m: float, lights: Dictionary) -> void:
	var out_m := lane_m / 2.0 + PARKING_FROM_LANE_M.y + PARKING_LIGHTS_OUT_M
	for x_m: float in PARKING_LIGHTS_X_M:
		var middle := Vector2(x_m, -(lane_m / 2.0 + (PARKING_FROM_LANE_M.x
				+ PARKING_FROM_LANE_M.y) / 2.0))
		_add_mast(parent, Vector2(x_m, -out_m), middle, -0.3, lights)
		_parking_masts.append(Vector2(x_m, -out_m))
	var heading: float = parking_layout()["heading"]
	var ahead := Vector2(cos(heading), sin(heading))
	var post_out_m := lane_m / 2.0 + PARKING_FROM_LANE_M.y - RIVAL_LIGHT_IN_M
	for bay in rival_bays():
		var rear := Vector2(bay.x, -(lane_m / 2.0 + bay.y))
		var tractor := rear + ahead * (BAY_SIZE_M.x / 2.0 - BAY_BEHIND_REAR_AXLE_M)
		_add_mast(parent, Vector2(bay.x, -post_out_m), tractor, -0.3, lights,
				RIVAL_LIGHT_HEIGHT_M)
		_rival_lights.append(Vector2(bay.x, -post_out_m))


# A floodlight mast at `at` (down the track, to the left) with its lamp
# head `inward` metres towards the track, glowing, and a spotlight aimed at
# `aim` on the ground (no shadows, for a phone), as bright and far as
# `lights` (FLOODLIGHTS) says, its lamp `height_m` up [m].
func _add_mast(parent: Node3D, at: Vector2, aim: Vector2, inward: float,
		lights: Dictionary, height_m := FLOODLIGHT_HEIGHT_M) -> void:
	var mast := Art.cylinder(0.15, height_m, STEEL, 8, 0.4, 0.6)
	mast.position = TrackSpace.point(at.x, at.y, height_m / 2.0)
	parent.add_child(_quiet(mast))
	var head := Art.box(Vector3(1.6, 0.6, 0.4), Color(0.15, 0.15, 0.16))
	head.position = TrackSpace.point(at.x, at.y - inward, height_m)
	parent.add_child(_quiet(head))
	var face := MeshInstance3D.new()
	var plate := BoxMesh.new()
	plate.size = Vector3(1.4, 0.45, 0.05)
	face.mesh = plate
	var lit := StandardMaterial3D.new()
	lit.albedo_color = FLOODLIGHT
	lit.emission_enabled = true
	lit.emission = FLOODLIGHT
	lit.emission_energy_multiplier = 4.0
	face.material_override = lit
	face.position = TrackSpace.point(at.x, at.y - 1.8 * inward, height_m)
	parent.add_child(_quiet(face))
	var spot := SpotLight3D.new()
	spot.light_color = FLOODLIGHT
	spot.light_energy = float(lights["energy"])
	spot.spot_range = float(lights["range_m"])
	spot.spot_angle = FLOODLIGHT_SPREAD_DEG
	spot.spot_angle_attenuation = FLOODLIGHT_EDGE_FADE
	spot.shadow_enabled = false
	spot.position = TrackSpace.point(at.x, at.y - 2.0 * inward, height_m)
	parent.add_child(spot)
	spot.look_at(TrackSpace.point(aim.x, aim.y, 0.0), Vector3.UP)


# Where the floodlight masts stand, along the track and over the parking
# (x down the track, y to the left) [m]: none in the day, a few along the
# track at sunset, many in the evening and over the parking too, with a
# lamp post behind each rival's bay (rival_lights()).
func floodlight_masts() -> Array[Vector2]:
	return _masts + _parking_masts + _rival_lights


# The lamp posts behind the rivals' bays, one each, in the evening
# (x down the track, y to the left) [m].
func rival_lights() -> Array[Vector2]:
	return _rival_lights


# The track's end: the pile and the trailers (TrackEnd); null before the
# first build().
func track_end() -> TrackEnd:
	return _track_end


# The big screen during the run: the TV camera on `target`.
func show_screen_live(target: Vector3, delta: float) -> void:
	if _track_end != null:
		_track_end.show_live(target, delta)


# The big screen after the run: the day's standings
# (EventTexts.standing_rows()).
func show_screen_standings(rows: Array) -> void:
	if _track_end != null:
		_track_end.show_standings(rows)


# The signal light: "green" while the pull is good, "red" to stop, "off".
func show_signal(signal_name: String) -> void:
	if signal_name == _signal:
		return
	_signal = signal_name
	for pair: Array in [[_red_lamp, LAMP_RED, "red"], [_green_lamp, LAMP_GREEN, "green"]]:
		var lamp: StandardMaterial3D = pair[0]
		var lit: bool = signal_name == pair[2]
		lamp.albedo_color = pair[1] if lit else Color(pair[1]).darkened(0.8)
		lamp.emission_enabled = lit
		lamp.emission = pair[1]
		lamp.emission_energy_multiplier = LAMP_GLOW


# The signal light's state now (show_signal()), for tests.
func signal_shown() -> String:
	return _signal


# The scoreboard on the right of the full-pull line, facing the track.
func _add_scoreboard(parent: Node3D, full_pull_m: float, lane_m: float) -> void:
	var at := TrackSpace.point(full_pull_m - 8.0,
			-(lane_m / 2.0 + FENCE_OUT_M + RETURN_LANE_M + 4.0), 0.0)
	for side: float in [-1.0, 1.0]:
		var leg := Art.box(Vector3(0.2, 4.0, 0.2), STEEL)
		leg.position = at + Vector3(side * 2.0, 2.0, 0.0)
		parent.add_child(_quiet(leg))
	var panel := Art.box(Vector3(5.0, 2.0, 0.3), Color(0.08, 0.08, 0.1))
	panel.position = at + Vector3(0.0, 5.0, 0.0)
	panel.rotation.y = -0.5
	parent.add_child(panel)
	_scoreboard = Art.label("  0.00 m", 160, Color(1.0, 0.75, 0.15), Color(0.0, 0.0, 0.0, 0.0))
	_scoreboard.position = at + Vector3(0.0, 5.0, 0.0) + Basis(Vector3.UP, -0.5) * Vector3(0.0, 0.0, 0.17)
	_scoreboard.rotation.y = -0.5
	parent.add_child(_scoreboard)


# Tents behind the standing crowd, a barn at the far end, trees around, a
# few clouds.
func _add_scenery(length_m: float, lane_m: float) -> void:
	var right_m := -(lane_m / 2.0 + FENCE_OUT_M + RETURN_LANE_M + 9.0)
	for x_m: float in [15.0, 45.0, 75.0]:
		_place(KenneyModels.TENTS, TrackSpace.point(x_m, right_m - 4.0, 0.0), 1.0, PI / 2.0)
	_place(KenneyModels.BARN, TrackSpace.point(length_m + 55.0, 18.0, 0.0), 14.0, 0.3)
	# Copses at the corners and single trees along the sides: pines and
	# broadleaves, one MultiMesh each.
	var pines: Array[Vector3] = []
	var broadleaves: Array[Vector3] = []
	for x_m: float in [-60.0, 160.0]:
		for y_m: float in [-55.0, -35.0, 35.0, 55.0]:
			for i in COPSE_TREES:
				var spot := TrackSpace.point(x_m + _random.randf_range(-9.0, 9.0),
						y_m + _random.randf_range(-7.0, 7.0), 0.0)
				(pines if _random.randf() < 0.6 else broadleaves).append(spot)
	for i in 18:
		var x_m := _random.randf_range(-40.0, length_m + 60.0)
		var left := i % 2 == 0
		var y_m := _random.randf_range(32.0, 70.0) * (1.0 if left else -1.0)
		(broadleaves if i % 3 else pines).append(TrackSpace.point(x_m, y_m, 0.0))
	add_child(Trees.many(Trees.pine(), pines, _random))
	add_child(Trees.many(Trees.broadleaf(), broadleaves, _random))
	for i in 7:
		var cloud := KenneyModels.scene(KenneyModels.CLOUD)
		cloud.scale = Vector3(30.0, 8.0, 18.0) * _random.randf_range(0.7, 1.3)
		cloud.position = TrackSpace.point(_random.randf_range(-80.0, 250.0),
				_random.randf_range(-160.0, 160.0), _random.randf_range(60.0, 90.0))
		_quiet_all(cloud)
		add_child(cloud)


# Fields of wheat, maize, ploughed land and grass ley round the venue,
# each a flat patch with its rows, so the land around reads as farmland.
func _add_farmland(length_m: float, lane_m: float) -> void:
	var materials: Array[StandardMaterial3D] = []
	var rows := _rows_texture()
	for crop in CROPS:
		var m := StandardMaterial3D.new()
		m.albedo_texture = rows
		m.albedo_color = crop
		m.roughness = 1.0
		materials.append(m)
	var centre := TrackSpace.point(length_m / 2.0, 0.0, 0.0)
	# The strip from the run-up to the overrun, the stands, banks and tents
	# beside it, in track space: no field reaches into it, since a field
	# lies above the clay and would cover the lane.
	var half_m := lane_m / 2.0 + FIELDS_CLEAR_OF_LANE_M
	var keep_clear := Rect2(-RUN_UP_M, -half_m, RUN_UP_M + length_m + OVERRUN_M + END_CLEAR_M,
			2.0 * half_m)
	# Each field's circle (its centre and half its diagonal), so the next
	# can be kept off it: fields that overlapped would fight for depth.
	var taken: Array[Vector4] = []
	_fields.clear()
	for i in FIELDS * FIELD_TRIES:
		if taken.size() >= FIELDS:
			break
		var angle := _random.randf_range(0.0, TAU)
		var reach := _random.randf_range(110.0, 250.0)
		var size := Vector2(_random.randf_range(60.0, 110.0), _random.randf_range(50.0, 90.0))
		var at := centre + Vector3(cos(angle), 0.0, sin(angle)) * reach
		var radius := size.length() / 2.0
		# The field's centre in track space (TrackSpace.point inverted).
		var track_at := Vector2(-at.z, -at.x)
		var nearest := Vector2(clampf(track_at.x, keep_clear.position.x, keep_clear.end.x),
				clampf(track_at.y, keep_clear.position.y, keep_clear.end.y))
		if nearest.distance_to(track_at) < radius:
			continue
		var clear := true
		for other in taken:
			if Vector2(at.x, at.z).distance_to(Vector2(other.x, other.y)) < radius + other.z:
				clear = false
				break
		if not clear:
			continue
		taken.append(Vector4(at.x, at.z, radius, 0.0))
		_fields.append(Vector3(track_at.x, track_at.y, radius))
		var patch := PlaneMesh.new()
		patch.size = size
		# Its own copy: the rows tile to the field's size.
		var surface := materials[taken.size() % CROPS.size()].duplicate() as StandardMaterial3D
		surface.uv1_scale = Vector3(size.x / (CROP_ROW_M * 16.0), size.y / 8.0, 1.0)
		var field := Art.instance(patch, surface)
		# Lifted clear of the grass, far enough for a phone's depth buffer at
		# this distance.
		field.position = at + Vector3(0.0, GRASS_Y_M + FIELD_LIFT_M, 0.0)
		field.rotation.y = _random.randf_range(0.0, TAU)
		add_child(_quiet(field))


# Sixteen rows across, a little uneven along: shared by every crop.
static func _rows_texture() -> ImageTexture:
	var size := Vector2i(64, 32)
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGB8)
	for y in size.y:
		for x in size.x:
			var row := 0.78 + 0.22 * maxf(0.0, sin(TAU * 16.0 * (float(x) + 0.5) / size.x))
			var speck := 0.95 + 0.05 * sin(float(x * 7 + y * 13))
			image.set_pixel(x, y, Color(row * speck, row * speck, row * speck))
	return ImageTexture.create_from_image(image)


# Hills all round, beyond a tree line, so the field has no edge: one mesh
# of rolling ridges (its height a few sines of the angle, so it is the
# same every launch), shaded from green near to a hazy blue-green far, and
# the trees as one MultiMesh.
func _add_horizon(length_m: float) -> void:
	var centre := TrackSpace.point(length_m / 2.0, 0.0, 0.0)
	# Three rings of points: the near foot, the ridge, and the far slope;
	# each segment two quads between them.
	var vertices := PackedVector3Array()
	var colours := PackedColorArray()
	for i in HILL_SEGMENTS + 1:
		var angle := TAU * float(i) / HILL_SEGMENTS
		var out := Vector3(cos(angle), 0.0, sin(angle))
		var h := _hill_height(angle)
		vertices.append(centre + out * HILLS_FROM_M + Vector3(0.0, -2.0, 0.0))
		colours.append(HILLS)
		vertices.append(centre + out * lerpf(HILLS_FROM_M, HILLS_TO_M, 0.55) + Vector3(0.0, h, 0.0))
		colours.append(HILLS.lerp(FAR_HILLS, 0.5))
		vertices.append(centre + out * HILLS_TO_M + Vector3(0.0, h * 0.6, 0.0))
		colours.append(FAR_HILLS)
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in HILL_SEGMENTS:
		for ring in 2:
			var a := i * 3 + ring
			var b := (i + 1) * 3 + ring
			for index: int in [a, b, a + 1, b, b + 1, a + 1]:
				tool.set_color(colours[index])
				tool.add_vertex(vertices[index])
	tool.generate_normals()
	var hills_material := StandardMaterial3D.new()
	hills_material.vertex_color_use_as_albedo = true
	hills_material.vertex_color_is_srgb = true
	hills_material.roughness = 1.0
	hills_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	add_child(_quiet(Art.instance(tool.commit(), hills_material)))

	var pines: Array[Vector3] = []
	var broadleaves: Array[Vector3] = []
	for i in TREE_LINE_TREES:
		var angle := TAU * (float(i) + _random.randf_range(-0.3, 0.3)) / TREE_LINE_TREES
		var reach := TREE_LINE_M + _random.randf_range(-25.0, 25.0)
		var at := centre + Vector3(cos(angle), 0.0, sin(angle)) * reach
		(pines if _random.randf() < 0.5 else broadleaves).append(at)
	add_child(Trees.many(Trees.pine(), pines, _random, 1.0, 1.6))
	add_child(Trees.many(Trees.broadleaf(), broadleaves, _random, 1.0, 1.6))


# The ridge's height at `angle` round the field [m].
static func _hill_height(angle: float) -> float:
	var rolling := 0.55 + 0.25 * sin(3.0 * angle + 0.7) + 0.15 * sin(7.0 * angle + 2.1) \
			+ 0.05 * sin(13.0 * angle)
	return HILL_HEIGHT_M * rolling


func _place(path: String, at: Vector3, scale_by: float, yaw: float) -> void:
	var model := KenneyModels.scene(path)
	model.scale = Vector3.ONE * scale_by
	var tile_m := 10.0 if path == KenneyModels.TENTS else 1.0
	model.position = at - Vector3(0.0, TILE_PLATE_SHARE * tile_m * scale_by, 0.0)
	model.rotation.y = yaw
	_quiet_all(model)
	add_child(model)


# Scenery casts no shadows: only the tractor and the sled do.
static func _quiet(node: GeometryInstance3D) -> GeometryInstance3D:
	node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return node


static func _quiet_all(root: Node) -> void:
	for node in root.find_children("*", "GeometryInstance3D", true, false):
		_quiet(node as GeometryInstance3D)


# The test flag at the start line for a test pull, and the chequered flag
# at `best_m` down the track in a pull-off (none below 0).
func show_flags(test_pull: bool, best_m: float) -> void:
	_test_pull = test_pull
	_best_m = best_m
	if not is_instance_valid(_test_flag):
		_test_flag = _official_flag(TEST_FLAG, 0.0, "TEST")
	if not is_instance_valid(_best_flag):
		_best_flag = _official_flag(Color.WHITE, CHEQUERS, "")
	var side_m := _lane_m / 2.0 + OFFICIAL_FLAG_OUT_M
	_test_flag.visible = test_pull
	_test_flag.position = TrackSpace.point(TEST_FLAG_AT_M, side_m, 0.0)
	_best_flag.visible = best_m >= 0.0
	_best_flag.position = TrackSpace.point(maxf(best_m, 0.0), side_m, 0.0)


func test_flag_shown() -> bool:
	return is_instance_valid(_test_flag) and _test_flag.visible


# Where the chequered flag stands along the track [m]; -1 when not shown.
func best_flag_at_m() -> float:
	if not is_instance_valid(_best_flag) or not _best_flag.visible:
		return -1.0
	return -_best_flag.position.z


# A pole with a waving cloth flying from its top down the track (in
# `colour`, or chequered with `chequers` squares along it), and `text` on it
# facing the lane.
func _official_flag(colour: Color, chequers: float, text: String) -> Node3D:
	var flag := Node3D.new()
	var pole := Art.cylinder(0.035, OFFICIAL_FLAG_POLE_M, STEEL, 8, 0.4, 0.6)
	pole.position = Vector3(0.0, OFFICIAL_FLAG_POLE_M / 2.0, 0.0)
	flag.add_child(_quiet(pole))
	var cloth := PlaneMesh.new()
	cloth.size = OFFICIAL_FLAG_SIZE_M
	cloth.subdivide_width = 12
	cloth.subdivide_depth = 4
	cloth.orientation = PlaneMesh.FACE_Z
	var material := ShaderMaterial.new()
	material.shader = FLAG_SHADER
	material.set_shader_parameter("colour", colour)
	material.set_shader_parameter("chequers", chequers)
	var face := Art.instance(cloth, material)
	# Turned to face the lane; its pole edge at the pole.
	face.rotation.y = PI / 2.0
	face.position = Vector3(0.0, OFFICIAL_FLAG_POLE_M - OFFICIAL_FLAG_SIZE_M.y / 2.0,
			-OFFICIAL_FLAG_SIZE_M.x / 2.0)
	flag.add_child(_quiet(face))
	if not text.is_empty():
		var label := Art.label(text, 56, Color.BLACK, colour)
		label.position = face.position + Vector3(0.12, 0.0, 0.25)
		label.rotation.y = PI / 2.0
		flag.add_child(label)
	add_child(flag)
	return flag
