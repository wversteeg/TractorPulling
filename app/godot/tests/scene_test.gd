# The pull scene: the main scene with its scripted driver for
# 10 s at a fixed 60 fps, then checks that what is drawn is the run. Exits
# non-zero if a check fails (scripts/godot_test.sh runs it).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/scene_test.gd
extends SceneTree

# The most a mark's step comes to: STEP_M and up to a frame's travel at
# the pull's speed (60 frames a second, a few metres a second) [m].
const MARK_STEP_MOST_M := 0.27
# The venue finds room for at least this many fields round it.
const FIELDS_AT_LEAST := 8
# Past the lane's edge, the venue's outermost things: the tents beyond the
# return lane, whose far side is about 29 m out (the stands' back rows and
# the banks are nearer) [m]. No field may reach inside it.
const VENUE_PAST_LANE_M := 24.0 + VenueView.RETURN_LANE_M
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
	_expect(float(s["pulled_m"]) > 1.0, "the sled has moved: %s m" % s["pulled_m"])

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

	_check_engine_sound()
	# The box holds a weight for every 250 kg of its mass for the pull, as
	# many as fit; this pull's box shows its own.
	_expect(SledView.weight_count(6000.0) == 24 and SledView.weight_count(1500.0) == 6
			and SledView.weight_count(0.0) == 0 and SledView.weight_count(1.0e5) == 36,
			"the weights follow the box's mass")
	var sled_view: SledView = _pull.get("sled_view")
	_expect(sled_view.weights_shown() == SledView.weight_count(float(dimensions["box_mass_kg"])),
			"this pull's box: %d weights for %.0f kg" % [sled_view.weights_shown(),
			float(dimensions["box_mass_kg"])])
	# The transport wheels: up for the pull, down once the turn is over and
	# the box runs back; a pull from the line has them up.
	_expect(SledView.transport_wanted({"stage": "pulling"}) == 0.0
			and SledView.transport_wanted({"stage": "taking_up_slack"}) == 0.0
			and SledView.transport_wanted({"stage": "to_the_parking", "chain_hooked": false}) == 1.0
			and SledView.transport_wanted({"stage": "parked", "chain_hooked": true}) == 0.0,
			"the transport wheels down only to drive back")
	_expect(sled_view.transport == 0.0, "pulling: the transport wheels are up")

	# The chain spans the hitch and the hook: about its length while it pulls.
	var hitch: Vector3 = tractor.call("hitch_position")
	var hook: Vector3 = sled.call("hook_position")
	var chain_m: float = dimensions["chain_length_m"]
	_expect(absf(hitch.distance_to(hook) - chain_m) < 0.1 * chain_m,
			"hitch to hook is the chain's length: %.2f m" % hitch.distance_to(hook))

	# The venue's banner stands at the class's full-pull line, and stays
	# there when a test pull moves the line in force to 110 m; the rest of
	# the venue is not rebuilt.
	var venue: VenueView = _pull.get("venue_view")
	_expect(venue.banner_at_m() == float(dimensions["full_pull_line_m"]),
			"the banner at the full-pull line: %s m" % venue.banner_at_m())
	var parts := venue.get_child_count()
	var first_part := venue.get_child(0).get_instance_id()
	var moved := dimensions.duplicate()
	moved["banner_line_m"] = dimensions["full_pull_line_m"]
	moved["full_pull_line_m"] = 110.0
	venue.build(moved)
	_expect(venue.banner_at_m() == float(dimensions["full_pull_line_m"])
			and venue.get_child_count() == parts
			and venue.get_child(0).get_instance_id() == first_part,
			"the banner stays at 100 m when the line moves, the venue too: %s m"
			% venue.banner_at_m())
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
	# The fields round the venue lie above the clay, so none may reach the
	# strip, from the run-up to the overrun, or the stands, banks and tents
	# beside it.
	var length_m: float = dimensions["track_length_m"]
	var clear_half_m := float(dimensions["lane_width_m"]) / 2.0 + VENUE_PAST_LANE_M
	var strip := Rect2(-VenueView.RUN_UP_M, -clear_half_m,
			VenueView.RUN_UP_M + length_m + VenueView.OVERRUN_M, 2.0 * clear_half_m)
	var on_strip := 0
	for field in venue.fields():
		var centre := Vector2(field.x, field.y)
		var nearest := Vector2(clampf(centre.x, strip.position.x, strip.end.x),
				clampf(centre.y, strip.position.y, strip.end.y))
		if nearest.distance_to(centre) < field.z:
			on_strip += 1
	_expect(venue.fields().size() >= FIELDS_AT_LEAST and on_strip == 0,
			"the fields stay off the strip: %d of %d on it" % [on_strip, venue.fields().size()])

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
	# Every mark gets a point each time its end has moved STEP_M on, which
	# is a little more than STEP_M (it moves in frames): about the sled's
	# distance in STEP_M to MARK_STEP_MOST_M steps. Half the step would
	# give twice the points.
	var moved_m := float(s["pulled_m"])
	var fewest := int(moved_m / MARK_STEP_MOST_M) - 2
	var most := int(moved_m / TrackMarks.STEP_M) + 2
	_expect(counts.x >= fewest and counts.x <= most and counts.y == counts.x
			and counts.z >= fewest and counts.z <= most,
			"the marks: %s points, %d to %d a mark" % [counts, fewest, most])

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
	# The rev counter's red zone starts at the engine's governed limit:
	# 2400 of a dial to 1.25 × 2200 rpm is the last two of nine ticks; a
	# governor at 0.75 of the rated speed would redden four (0.6 of the dial).
	_expect(view.rev_red_ticks == _red_ticks(dimensions), "the reference engine's red zone: %d ticks"
			% view.rev_red_ticks)
	var low_governor := dimensions.duplicate()
	low_governor["high_idle_omega_rad_s"] = float(dimensions["rated_omega_rad_s"]) * 0.75
	var other := TractorView.new()
	other.build(low_governor)
	_expect(other.rev_red_ticks == 4, "a lower governor reddens more: %d ticks" % other.rev_red_ticks)
	other.free()

	var cameras: Node = _pull.get("cameras")
	var camera: Camera3D = cameras.call("current_camera")
	_expect(camera != null and camera.is_current(), "a camera is current")
	_expect(camera.global_position.distance_to(tractor.global_position) < 50.0,
			"the camera follows the tractor")
	_check_cameras(cameras as CameraRig, tractor as TractorView, _pull.get("sled_view"))


# The rev counter's ticks at or past the engine's high idle.
static func _red_ticks(d: Dictionary) -> int:
	var red_from := float(d["high_idle_omega_rad_s"]) / (float(d["rated_omega_rad_s"])
			* TractorView.REV_COUNTER_HEADROOM)
	var count := 0
	for t in TractorView.DIAL_TICKS:
		if t / float(TractorView.DIAL_TICKS - 1) >= red_from - 1e-6:
			count += 1
	return count


# The engine's sound: a six at 2500 rpm fires at 125 Hz; louder at full
# throttle than at idle, and silent once the engine stops. Made without an
# audio device (sample_frames()). Silent, it makes no samples at all.
func _check_engine_sound() -> void:
	var pulling: EngineSound = _pull.get("engine_sound")
	_expect(pulling.target_volume() > 0.0,
			"the engine is heard in the pull: %.2f" % pulling.target_volume())
	_expect(absf(EngineSound.firing_hz(2500.0 * TAU / 60.0, 6) - 125.0) < 1e-9,
			"a six at 2500 rpm fires at 125 Hz")
	var sound := EngineSound.new()
	sound.configure({"engine_cylinders": 6, "idle_omega_rad_s": 84.0, "rated_omega_rad_s": 262.0})
	# The RMS level of a quarter second; the mean (no DC offset) and the
	# share of samples past the saturation's knee kept for the checks below.
	var mean := [0.0]
	var bent := [0.0]
	var level := func(snapshot: Dictionary) -> float:
		sound.show_snapshot(snapshot)
		sound.sample_frames(int(EngineSound.MIX_RATE_HZ))  # a second to settle
		sound.peak_before_saturation()  # from here on
		var frames := sound.sample_frames(int(EngineSound.MIX_RATE_HZ / 4.0))
		var sum := 0.0
		var total := 0.0
		var over := 0
		for frame in frames:
			sum += frame.x * frame.x
			total += frame.x
			over += 1 if absf(frame.x) > EngineSound.SATURATION_KNEE else 0
		mean[0] = total / frames.size()
		bent[0] = float(over) / frames.size()
		return sqrt(sum / frames.size())
	var idle: float = level.call({"engine_running": true, "engine_omega_rad_s": 84.0,
			"throttle": 0.0, "boost": 0.0})
	var full: float = level.call({"engine_running": true, "engine_omega_rad_s": 262.0,
			"throttle": 1.0, "boost": 1.0})
	# Loudest, lugging at full throttle: the mix itself stays near the
	# saturation's knee (only its peaks bend), and centred.
	level.call({"engine_running": true, "engine_omega_rad_s": 160.0, "throttle": 1.0,
			"boost": 0.8})
	var mixed_peak := sound.peak_before_saturation()
	_expect(mixed_peak <= 0.85 and bent[0] < 0.02 and absf(mean[0]) < 0.01,
			"the loudest mix near the knee and centred: peak %.2f, %.1f %% bent, mean %.4f" % [
			mixed_peak, 100.0 * bent[0], mean[0]])
	# The saturation: untouched below the knee, a soft bend above it short
	# of full scale at any level the mix reaches, never past it.
	_expect(EngineSound.saturated(0.5) == 0.5 and EngineSound.saturated(-0.5) == -0.5
			and EngineSound.saturated(0.7) < 0.7 and EngineSound.saturated(0.7) > 0.65
			and EngineSound.saturated(2.0) < 1.0 and EngineSound.saturated(-2.0) > -1.0
			and EngineSound.saturated(50.0) <= 1.0 and EngineSound.saturated(-50.0) >= -1.0,
			"saturation linear below the knee, soft above")
	var stopped: float = level.call({"engine_running": false, "engine_omega_rad_s": 0.0,
			"throttle": 0.0, "boost": 0.0})
	_expect(idle > 0.01 and full > 2.0 * idle and stopped < 1e-3,
			"the engine's sound: idle %.3f, full %.3f, stopped %.4f" % [idle, full, stopped])
	_expect(not sound.making(), "silent: no samples made")
	_check_web_loops()
	sound.show_snapshot({"engine_running": true, "engine_omega_rad_s": 84.0})
	_expect(sound.making(), "running: the samples made again")
	sound.free()


# The web's way, tried here: the engine's sound made ahead as two seamless
# loops for this engine, idling and pulling; their pitch follows the
# engine's speed, the throttle blends from one to the other, and silent
# they stop.
func _check_web_loops() -> void:
	var sound := EngineSound.new()
	sound.loops = true
	_pull.add_child(sound)
	sound.configure({"engine_cylinders": 6, "idle_omega_rad_s": 84.0, "rated_omega_rad_s": 262.0})
	var loop: AudioStreamWAV = sound.load_loop.stream
	_expect(loop != null and loop.loop_mode == AudioStreamWAV.LOOP_FORWARD
			and loop.loop_end > int(EngineSound.MIX_RATE_HZ * 0.4), "a loop made for the web")
	# Seamless: the step from the loop's end to its start is no bigger than
	# its steps elsewhere.
	var data := loop.data
	var last := data.decode_s16((loop.loop_end - 1) * 2)
	var seam := absi(data.decode_s16(0) - last)
	var biggest := 0
	for i in range(1, loop.loop_end):
		biggest = maxi(biggest, absi(data.decode_s16(i * 2) - data.decode_s16((i - 1) * 2)))
	_expect(seam <= biggest, "the loop seamless: %d at the seam, %d at most elsewhere" % [seam,
			biggest])
	sound.show_snapshot({"engine_running": true, "engine_omega_rad_s": 262.0, "throttle": 1.0,
			"boost": 1.0})
	for i in 30:
		sound._process(1.0 / 60.0)
	_expect(sound.load_loop.playing and sound.load_loop.volume_db > sound.idle_loop.volume_db
			and absf(sound.load_loop.pitch_scale - 1.0) < 0.05,
			"pulling: the pulling loop at its own pitch, louder: %.1f dB, %.1f dB, x%.2f" % [
			sound.load_loop.volume_db, sound.idle_loop.volume_db, sound.load_loop.pitch_scale])
	# Lugging, louder than the loop was made: never above its own level.
	sound.show_snapshot({"engine_running": true, "engine_omega_rad_s": 160.0, "throttle": 1.0,
			"boost": 0.8})
	for i in 30:
		sound._process(1.0 / 60.0)
	_expect(sound.load_loop.volume_db <= 0.0, "lugging: no gain past the loop's own level: %.1f dB"
			% sound.load_loop.volume_db)
	# A second sound for the same engine uses the loops made for the first.
	var again := EngineSound.new()
	again.loops = true
	_pull.add_child(again)
	again.configure({"engine_cylinders": 6, "idle_omega_rad_s": 84.0, "rated_omega_rad_s": 262.0})
	_expect(again.load_loop.stream == sound.load_loop.stream, "the loops made once per engine")
	again.queue_free()
	sound.show_snapshot({"engine_running": false})
	for i in 60:
		sound._process(1.0 / 60.0)
	_expect(not sound.load_loop.playing and not sound.idle_loop.playing, "silent: the loops stop")
	sound.queue_free()


# The close camera sees the hook and the tractor from a little behind and
# over the sled's canopy; the cab camera rides on the tractor; next() goes
# round every view and back.
func _check_cameras(cameras: CameraRig, tractor: TractorView, sled: SledView) -> void:
	var close: Camera3D = cameras.get("_cameras")[CameraRig.View.CLOSE]
	var mount := sled.camera_mount()
	_expect(close.is_position_in_frustum(sled.hook_position())
			and close.is_position_in_frustum(tractor.global_position),
			"the close camera sees the hook and the tractor")
	# Down the track is -z (TrackSpace): behind is +z.
	var behind_m := (close.global_position - mount).z
	var over_m := close.global_position.y - mount.y
	_expect(behind_m > 0.0 and behind_m < CameraRig.CLOSE_PAST_SLED_M + 2.0 and over_m > 0.0,
			"the close camera a little behind and over the sled's canopy: %.1f m, %.1f m"
			% [behind_m, over_m])
	# The tractor turned: the camera turns with it, still as far back.
	var target := tractor.global_position
	var heading_rad := 0.3
	cameras.follow(target, 0.0, false, heading_rad, mount)
	var back := TrackSpace.point(-cos(heading_rad), -sin(heading_rad), 0.0)
	_expect(close.is_position_in_frustum(target) and (close.global_position - target).dot(back) > 0.0,
			"turned, the close camera behind the tractor along its heading")
	# On the dog-tail: the tractor turned and the sled off to one side; the
	# camera still sees the sled's box, the hook and the whole tractor, its
	# roof (where the driver's eyes are) too.
	var aside := TrackSpace.point(0.0, -1.5, 0.0)
	var box: Node3D = sled.get("_box")
	var box_top := box.global_position + aside + Vector3.UP * SledView.BOX_SIZE_M.y / 2.0
	cameras.follow(target, 0.0, false, heading_rad, mount + aside)
	_expect(close.is_position_in_frustum(box_top)
			and close.is_position_in_frustum(sled.hook_position() + aside)
			and close.is_position_in_frustum(target)
			and close.is_position_in_frustum(cameras.cab.global_position),
			"on the dog-tail, the close camera sees the sled's box, the hook and the whole tractor")
	var chase: Camera3D = cameras.get("_cameras")[CameraRig.View.CHASE]
	_expect((chase.global_position - target).dot(back) > -CameraRig.CHASE_OFFSET.x * 0.9,
			"the chase camera turns with the tractor too")
	# Off the chain, it stands behind the tractor.
	cameras.follow(target, 0.0, false, 0.0)
	_expect(close.is_position_in_frustum(target)
			and (close.global_position - target).z > CameraRig.CLOSE_UNHOOKED_BACK_M - 1e-3,
			"off the chain, the close camera behind the tractor")
	_expect(cameras.cab != null and tractor.is_ancestor_of(cameras.cab),
			"the cab camera rides on the tractor")
	# Looking ahead the driver sees the dashboard's dials and the wheel's rim.
	var rev: Node3D = tractor.get("_rev_needle")
	var boost: Node3D = tractor.get("_boost_needle")
	var steering_wheel: Node3D = tractor.get("_steering_wheel")
	_expect(cameras.cab.is_position_in_frustum(rev.global_position)
			and cameras.cab.is_position_in_frustum(boost.global_position)
			and cameras.cab.is_position_in_frustum(steering_wheel.global_transform
				* Vector3(0.0, 0.0, -TractorView.WHEEL_RADIUS_M)),
			"the cab camera sees the dials and the steering wheel")
	# The needles sweep clockwise (rotation.z falls) as the engine speeds up
	# and the boost builds; the wheel turns with the front wheels, left
	# (positive) one way.
	tractor.show_cockpit({"steering_rad": 0.0, "engine_omega_rad_s": 80.0, "boost": 0.1})
	var slow_rev := rev.rotation.z
	var low_boost := boost.rotation.z
	tractor.show_cockpit({"steering_rad": 0.1, "engine_omega_rad_s": 220.0, "boost": 0.8})
	_expect(rev.rotation.z < slow_rev and boost.rotation.z < low_boost,
			"the needles rise with the engine's speed and the boost")
	_expect(is_equal_approx(steering_wheel.rotation.y, 0.1 * TractorView.STEERING_RATIO),
			"the steering wheel turns with the front wheels")
	# In reverse it looks back over the seat: the backs of both rear wheels
	# (the tractor's own reference) and the chain leaving the hitch in view.
	cameras.set_reversing(true)
	cameras.follow(target, 0.0, false, 0.0)
	var r: float = tractor.get("_rear_radius_m")
	var basis := tractor.global_transform.basis
	var wheel_backs := true
	for wheel: Node3D in [tractor.get("_rear_left"), tractor.get("_rear_right")]:
		wheel_backs = wheel_backs and cameras.cab.is_position_in_frustum(
				wheel.global_position + basis * TrackSpace.point(-0.8 * r, 0.0, 0.7 * r))
	var chain := tractor.hitch_position() + basis * TrackSpace.point(-1.0, 0.0, 0.0)
	_expect(wheel_backs and cameras.cab.is_position_in_frustum(chain),
			"in reverse the cab camera sees the backs of the rear wheels and the chain at the hitch")
	cameras.set_reversing(false)
	cameras.follow(target, 0.0, false, 0.0)
	var seen: Array[Camera3D] = []
	for i in CameraRig.View.size():
		var current := cameras.current_camera()
		_expect(current.is_current(), "view %d is current" % cameras.view)
		seen.append(current)
		cameras.next()
	_expect(cameras.view == CameraRig.View.CHASE and seen.has(cameras.cab),
			"next() goes round every view, the cab too")
	_check_hitch_camera(cameras)


# The hitch from the side while it meets the hook, back to the view picked
# after; a tap dismisses it to the view picked; never from the cab. In
# reverse the cab camera looks back, and ahead again after.
func _check_hitch_camera(cameras: CameraRig) -> void:
	var chain_m := 1.15
	var near := {"stage": "to_the_sled", "hitch_to_hook_m": 2.0, "hitch_along_m": 2.0,
			"hitch_across_m": 0.3, "gear": -1}
	_expect(_pull.framing_hitch(near, chain_m), "the last metre to the hook: framed")
	near["gear"] = 1
	_expect(not _pull.framing_hitch(near, chain_m), "driving forwards in front of it: not framed")
	near["gear"] = -1
	near["hitch_to_hook_m"] = 4.0
	_expect(not _pull.framing_hitch(near, chain_m), "further out: the view behind to steer by")
	near["hitch_to_hook_m"] = 2.0
	near["gear"] = 3
	near["hitch_across_m"] = 4.0
	_expect(not _pull.framing_hitch(near, chain_m), "passing beside the sled: not framed")
	near["hitch_along_m"] = -1.0
	near["gear"] = -1
	_expect(not _pull.framing_hitch(near, chain_m), "behind the hook: not framed")
	_expect(_pull.framing_hitch({"stage": "releasing", "gear": -1}, chain_m)
			and not _pull.framing_hitch({"stage": "releasing", "gear": 2}, chain_m),
			"backing off in R after the pull: framed")

	# The following cameras look back while the tractor drives backwards off
	# the chain, keep looking back while it barely moves, and never on it.
	var off := {"chain_hooked": false, "tractor_speed_m_s": -1.0}
	_expect(_pull.driving_back(off, false), "backing off the chain: looking back")
	off["tractor_speed_m_s"] = -0.1
	_expect(_pull.driving_back(off, true) and not _pull.driving_back(off, false),
			"barely moving: as before")
	off["tractor_speed_m_s"] = 1.0
	_expect(not _pull.driving_back(off, true), "forwards: looking ahead")
	_expect(not _pull.driving_back({"chain_hooked": true, "tractor_speed_m_s": -1.0}, true),
			"on the chain (releasing): never")

	# The sled's lamp stays green from the slack taken up through the pull,
	# though the chain eases while the tractor waits for the flag.
	_expect(SledView.lamp_colour({"stage": "waiting_for_the_flag", "chain_tension_n": 200.0},
			1000.0) == SledView.LAMP_GREEN
			and SledView.lamp_colour({"stage": "pulling", "chain_tension_n": 0.0}, 1000.0)
			== SledView.LAMP_GREEN
			and SledView.lamp_colour({"stage": "taking_up_slack", "chain_tension_n": 200.0},
			1000.0) == SledView.LAMP_DARK, "the lamp green from the slack taken up")

	var picked := cameras.view
	cameras.show_hitch(true, 3.0, 0.0)
	_expect(cameras.showing_hitch() and cameras.current_camera().is_current(), "the hitch shown")
	cameras.next()
	_expect(not cameras.showing_hitch() and cameras.view == picked, "a tap: back to the view picked")
	cameras.show_hitch(false)
	cameras.show_hitch(true, 3.0, 0.0)
	_expect(cameras.showing_hitch(), "wanted again: shown again")
	cameras.show_hitch(false)
	_expect(not cameras.showing_hitch() and cameras.view == picked, "off: the view picked")
	var ahead := cameras.cab.position
	cameras.set_reversing(true)
	cameras.follow(Vector3.ZERO, 0.0, false)
	_expect(is_equal_approx(cameras.cab.rotation.y, PI), "reversing: the cab looks back")
	cameras.set_reversing(false)
	cameras.follow(Vector3.ZERO, 0.0, false)
	_expect(cameras.cab.position.is_equal_approx(ahead) and is_zero_approx(cameras.cab.rotation.y),
			"the cab looks ahead again")
