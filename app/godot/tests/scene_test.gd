# The pull scene: the main scene with its scripted driver for
# 10 s at a fixed 60 fps, then checks that what is drawn is the run. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/scene_test.gd
extends SceneTree

# How far ahead of the sled's start the tractor may stand at the start,
# past the chain's length (its hitch behind the rear axle) [m].
const MARKS_START_SLACK_M := 3.0
const FRAMES := 600
const TOLERANCE_M := 1e-4

var _pull: Node
var _frames := 0
var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("autoplay", true)
	root.add_child(_pull)


func _process(_delta: float) -> bool:
	_frames += 1
	if _frames < FRAMES:
		return false
	_check()
	print("scene test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
	return true


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _check() -> void:
	var run: TractorPullRun = _pull.get("run")
	var s := run.get_snapshot()
	var dimensions := run.get_dimensions()
	_expect(not s.is_empty(), "the run is set up")
	if s.is_empty():
		return
	_expect(float(s["time_s"]) > 9.0, "10 s of frames ran about 10 s of the run: %s" % s["time_s"])
	_expect(float(s["sled_x_m"]) > 1.0, "the sled has moved: %s m" % s["sled_x_m"])

	var tractor: Node3D = _pull.get("tractor_view")
	var sled: Node3D = _pull.get("sled_view")
	var at_tractor := TrackSpace.point(s["tractor_x_m"], s["tractor_y_m"], 0.0)
	var at_sled := TrackSpace.point(s["sled_x_m"], s["sled_y_m"], 0.0)
	_expect(tractor.position.distance_to(at_tractor) < TOLERANCE_M,
			"the tractor is drawn where the run has it")
	_expect(sled.position.distance_to(at_sled) < TOLERANCE_M,
			"the sled is drawn where the run has it")
	_expect(tractor.position.z < sled.position.z, "the tractor is ahead of the sled (-z)")
	_expect(is_equal_approx(tractor.rotation.y, float(s["tractor_heading_rad"])),
			"the tractor's heading")

	# The chain spans the hitch and the hook: about its length while it pulls.
	var hitch: Vector3 = tractor.call("hitch_position")
	var hook: Vector3 = sled.call("hook_position")
	var chain_m: float = dimensions["chain_length_m"]
	_expect(absf(hitch.distance_to(hook) - chain_m) < 0.1 * chain_m,
			"hitch to hook is the chain's length: %.2f m" % hitch.distance_to(hook))

	# The venue's banner stands at the full-pull line, and follows it when a
	# test pull moves it to 110 m; the rest of the venue is not rebuilt.
	var venue: VenueView = _pull.get("venue_view")
	_expect(venue.banner_at_m() == float(dimensions["full_pull_line_m"]),
			"the banner at the full-pull line: %s m" % venue.banner_at_m())
	var parts := venue.get_child_count()
	var first_part := venue.get_child(0).get_instance_id()
	var moved := dimensions.duplicate()
	moved["full_pull_line_m"] = 110.0
	venue.build(moved)
	_expect(venue.banner_at_m() == 110.0 and venue.get_child_count() == parts
			and venue.get_child(0).get_instance_id() == first_part,
			"the banner moves with the line, the venue stays: %s m" % venue.banner_at_m())
	venue.build(dimensions)
	# Another event's track, a wider lane: the venue is built again, with
	# its banner at the line and a working scoreboard.
	var wider := dimensions.duplicate()
	wider["lane_width_m"] = float(dimensions["lane_width_m"]) + 2.0
	venue.build(wider)
	venue.show_distance(12.5)
	var board: Label3D = venue.get("_scoreboard")
	_expect(venue.banner_at_m() == float(dimensions["full_pull_line_m"])
			and venue.get_child_count() == parts
			and venue.get_child(0).get_instance_id() != first_part
			and board != null and board.text.strip_edges() == "12.50 m",
			"a wider lane builds the venue again: %d parts, board %s" % [
			venue.get_child_count(), board.text if board != null else "none"])
	venue.build(dimensions)

	# The sizes the views draw the parts from are there and positive.
	for key: String in ["wheelie_bar_behind_rear_axle_m", "wheelie_bar_tip_height_m",
			"sled_wheel_width_m"]:
		_expect(float(dimensions.get(key, 0.0)) > 0.0, "%s: %s" % [key, dimensions.get(key)])
	_expect(float(dimensions.get("ballast_mass_kg", -1.0)) >= 0.0
			and float(dimensions.get("ballast_position", -1.0)) >= 0.0,
			"the weights: %s kg at %s" % [dimensions.get("ballast_mass_kg"),
			dimensions.get("ballast_position")])

	# The marks the pull left: a rut a tire, a point every 0.2 m of the
	# way, and the band the pan smooths behind the sled.
	var marks: TrackMarks = _pull.get("track_marks")
	var counts := marks.point_counts()
	# From where the tractor started (a few metres ahead of the sled's
	# start) to where it is now: a point every STEP_M, give or take the
	# start and the way it weaves.
	var travel_m := float(s["tractor_x_m"]) - float(dimensions["chain_length_m"]) \
			- MARKS_START_SLACK_M
	var rut_points := int(travel_m / TrackMarks.STEP_M)
	var most := int(float(s["tractor_x_m"]) / TrackMarks.STEP_M) + 2
	_expect(counts.x >= rut_points and counts.x <= most and counts.y == counts.x
			and counts.z > 0, "the marks: %s points, %d to %d a rut" % [counts, rut_points, most])

	# The weights drawn for their mass at each end: 45 kg suitcase weights,
	# 70 kg wheel weights a hub, rounded up so a little weight still shows.
	var view: TractorView = _pull.get("tractor_view")
	for case: Array in [[400.0, 1.0, Vector2i(9, 0)], [400.0, 0.0, Vector2i(0, 3)],
			[100.0, 0.5, Vector2i(2, 1)], [0.0, 0.5, Vector2i(0, 0)],
			# Whole numbers of weights that come out a hair over in doubles.
			[900.0, 0.55, Vector2i(11, 3)], [750.0, 0.44, Vector2i(8, 3)]]:
		var weighted := dimensions.duplicate()
		weighted["ballast_mass_kg"] = case[0]
		weighted["ballast_position"] = case[1]
		view.build(weighted)
		_expect(view.weight_counts() == case[2], "%s kg at %s: %s weights, not %s" % [
				case[0], case[1], view.weight_counts(), case[2]])
	view.build(dimensions)

	var cameras: Node = _pull.get("cameras")
	var camera: Camera3D = cameras.call("current_camera")
	_expect(camera != null and camera.is_current(), "a camera is current")
	_expect(camera.global_position.distance_to(tractor.global_position) < 50.0,
			"the camera follows the tractor")
