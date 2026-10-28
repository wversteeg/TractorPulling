# The touch controls, played on the pull scene with simulated fingers: the
# hand throttle slid open like a lever and left there, the gear tapped up, a
# button let go when the app loses focus, a thumb steering while the other
# holds the throttle, the steering springing back when the thumb lifts, and
# "again" only on a release over it; then the pro layout, picked in the
# settings (the gear on the start screen) and kept in their file, with its
# clutch and left brake on the left and the right brake on the right and the
# clutch's slipping range marked; then tilt steering, with a simulated
# motion sensor. All in practice pulls, where "again" and "menu" work.
# Checks that the run gets the controls and the tractor pulls
# and steers. Exits non-zero if a check fails (scripts/godot_test.sh runs
# it).
#
#   godot --fixed-fps 60 --path app/godot --script res://tests/touch_test.gd
extends SceneTree

const THROTTLE_FINGER := 0
const TAP_FINGER := 1
const STEERING_FINGER := 2
const STUCK_FINGER := 3
const PULL_FRAMES := 300
# Long enough for the easy layout's assist to press the clutch, change
# gear and let it out again.
const SHIFT_FRAMES := 60
const STEER_FRAMES := 60
const SETTINGS_PATH := "user://touch_test_settings.cfg"
# The reference clutch slips between these pedals (data/clutches).
const CLUTCH_FULL_GRIP := 0.3
const CLUTCH_BITE := 0.7
const G_M_S2 := 9.81

var _pull: Node
var _failures := 0
# The simulated sensor's gravity, in the screen's plane.
var _gravity := Vector3.ZERO


func _initialize() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SETTINGS_PATH))
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("settings_path", SETTINGS_PATH)
	# The reference tractor as it is, with its powershift step (the club
	# season's garage starts the player without it).
	_pull.set("season_path", "res://tests/data/practice_season.toml")
	# A new season, whatever this machine has saved, and nothing saved.
	_pull.set("save_path", "")
	# A pull from the start line: the whole turn is turn_test.gd's.
	_pull.set("whole_turn", false)
	root.add_child(_pull)
	_play.call_deferred()


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


func _touch(finger: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = finger
	event.position = at
	event.pressed = pressed
	root.push_input(event, true)


func _drag(finger: int, at: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = finger
	event.position = at
	root.push_input(event, true)


func _tap(control: Control) -> void:
	var at := control.get_global_rect().get_center()
	_touch(TAP_FINGER, at, true)
	_touch(TAP_FINGER, at, false)


func _frames(count: int) -> void:
	for i in count:
		await process_frame


func _play() -> void:
	await _frames(2)
	var controls: DriveControls = _pull.get("controls")
	var run: TractorPullRun = _pull.get("run")
	var start: StartScreen = _pull.get("start_screen")
	var setup: SetupScreen = _pull.get("setup_screen")
	# The start screen; "Practice", then the setup screen in the easy layout,
	# and "Pull" starts the pull.
	_expect(start.visible and not controls.visible, "the start screen first")
	_tap(start.practice_button)
	await _frames(2)
	_expect(setup.visible, "the setup screen")
	_tap(setup.pull_button)
	await _frames(2)
	_expect(controls.visible and not setup.visible, "the pull's controls after Pull")
	var throttle := controls.throttle_slider.get_global_rect()
	var steering := controls.steering_slider.get_global_rect()

	# Every control is on the screen, where a thumb can reach it.
	_expect_on_screen([controls.steering_slider, controls.throttle_slider, controls.gear_up,
			controls.gear_down, controls.gear_readout, controls.camera_button,
			controls.restart_button,
			controls.menu_button])
	_expect(run.get_layout() == "easy" and not controls.clutch_slider.visible
			and not controls.brake_left_slider.visible, "the easy layout, without clutch or brakes")

	# A touch on no control changes nothing.
	_touch(TAP_FINGER, Vector2(640.0, 300.0), true)
	_touch(TAP_FINGER, Vector2(640.0, 300.0), false)
	_expect(controls.throttle() == 0.0 and controls.gear == 0, "a touch off the controls")

	# The gear: down from neutral is reverse, and no further; up from it
	# neutral again, and up stops at the top gear.
	_tap(controls.gear_down)
	_expect(controls.gear == -1, "reverse below neutral: %d" % controls.gear)
	_tap(controls.gear_down)
	_expect(controls.gear == -1, "nothing below reverse")
	_tap(controls.gear_up)
	_expect(controls.gear == 0, "up from reverse: neutral")
	for i in controls.gear_count + 2:
		_tap(controls.gear_up)
	_expect(controls.gear == controls.gear_count, "the top gear is the last")
	# The readout between the buttons shows the gear in and the one asked
	# for; it is not a button.
	_tap(controls.gear_readout)
	_expect(controls.gear == controls.gear_count, "the readout is not a button")
	controls.show_gear(0)
	_expect(controls.gear_readout.text == "N › %d" % controls.gear_count
			and controls.gear_readout.selected, "asked for, waiting: %s" % controls.gear_readout.text)
	for i in controls.gear_count:
		_tap(controls.gear_down)
	_expect(controls.gear == 0, "down to neutral")
	controls.show_gear(0)
	_expect(controls.gear_readout.text == "N" and not controls.gear_readout.selected,
			"in neutral: %s" % controls.gear_readout.text)
	for i in 6:
		_tap(controls.gear_up)
	_expect(controls.gear == 6, "six taps up: 6th gear")

	# A gear button pressed when the app loses focus lets go, so the next
	# finger still works it.
	_touch(STUCK_FINGER, controls.gear_up.get_global_rect().get_center(), true)
	controls.gear_up.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
	_tap(controls.gear_down)
	_tap(controls.gear_down)
	_expect(controls.gear == 5, "the button works after focus loss: %d" % controls.gear)
	_tap(controls.gear_up)

	# The hand throttle is moved like a lever: a touch in its middle does not
	# open it; sliding from the bottom past the top does, and it stays open
	# when the finger lifts.
	_touch(THROTTLE_FINGER, throttle.get_center(), true)
	_touch(THROTTLE_FINGER, throttle.get_center(), false)
	_expect(controls.throttle() == 0.0, "a touch alone leaves the throttle: %s" % controls.throttle())
	var low := Vector2(throttle.get_center().x, throttle.end.y - 1.0)
	var top := Vector2(throttle.get_center().x, throttle.position.y - 10.0)
	_touch(THROTTLE_FINGER, low, true)
	_drag(THROTTLE_FINGER, throttle.get_center())
	_expect(absf(controls.throttle() - 0.5) < 0.01, "half the slide, half open: %s"
			% controls.throttle())
	_drag(THROTTLE_FINGER, top)
	_touch(THROTTLE_FINGER, top, false)
	_expect(controls.throttle() == 1.0, "the throttle open: %s" % controls.throttle())

	await _frames(PULL_FRAMES)
	var s := run.get_snapshot()
	_expect(s["gear"] == 6, "the run is in 6th: %s" % s["gear"])
	_expect(controls.gear_readout.text == "6" and not controls.gear_readout.selected,
			"the readout shows 6th: %s" % controls.gear_readout.text)
	_expect(float(s["throttle"]) == 1.0, "the run has the throttle open")
	_expect(float(s["tractor_speed_m_s"]) > 0.5, "the tractor pulls: %s m/s" % s["tractor_speed_m_s"])

	# Under way the gearbox goes down only by its powershift: this tractor's
	# (the practice garage's first level) steps one gear down, once. A second
	# "-" is not taken, by the controls or by the run.
	_expect(int(s["lowest_gear"]) == 5, "under way, 5th at the lowest: %s" % s["lowest_gear"])
	_tap(controls.gear_down)
	await _frames(3)
	_expect(controls.gear == 5 and int(run.get_snapshot()["gear"]) == 5,
			"one gear down under load: %d" % int(run.get_snapshot()["gear"]))
	_tap(controls.gear_down)
	await _frames(3)
	_expect(controls.gear == 5 and int(run.get_snapshot()["gear"]) == 5
			and int(run.get_snapshot()["lowest_gear"]) == 5,
			"no second gear down: %d" % int(run.get_snapshot()["gear"]))
	# Out to neutral (the gamepad's X) and "+" again: back into the lowest
	# gear there is, not stuck in neutral below it.
	controls.set_gear(0)
	await _frames(3)
	_expect(controls.gear == 0, "neutral under way: %d" % controls.gear)
	_tap(controls.gear_up)
	await _frames(SHIFT_FRAMES)
	_expect(controls.gear == 5 and int(run.get_snapshot()["gear"]) == 5,
			"up from neutral, back in 5th: %d" % int(run.get_snapshot()["gear"]))

	# Steering with one thumb while the other holds the throttle: full left at
	# the slider's left end, the wheels turning left at the steering rate.
	_touch(THROTTLE_FINGER, throttle.position + Vector2(1.0, 1.0), true)
	_drag(THROTTLE_FINGER, top)
	var left_end := Vector2(steering.position.x - 10.0, steering.get_center().y)
	_touch(STEERING_FINGER, steering.get_center(), true)
	_expect(absf(controls.steering()) < 0.01, "the slider's middle steers straight")
	_drag(STEERING_FINGER, left_end)
	_expect(controls.steering() == 1.0, "the left end steers left: %s" % controls.steering())
	_expect(controls.throttle_slider.is_held(), "the throttle is still held")
	await _frames(STEER_FRAMES)
	s = run.get_snapshot()
	_expect(float(s["steering_rad"]) > 0.0, "the front wheels turn left: %s" % s["steering_rad"])
	_expect(float(s["throttle"]) == 1.0, "the throttle stays open while steering")

	# The thumb lifts: the slider springs back to straight ahead.
	_touch(STEERING_FINGER, left_end, false)
	_expect(controls.steering() == 0.0, "the steering springs back")
	_expect(not controls.steering_slider.is_held(), "the slider is let go")

	# Closing the throttle: dragged to the bottom.
	var bottom := Vector2(throttle.get_center().x, throttle.end.y + 10.0)
	_drag(THROTTLE_FINGER, bottom)
	_touch(THROTTLE_FINGER, bottom, false)
	_expect(controls.throttle() == 0.0, "the throttle closed: %s" % controls.throttle())

	# Again fires only when the finger lifts on it: a touch that slides off
	# leaves the run going.
	var time_s := float(run.get_snapshot()["time_s"])
	var again := controls.restart_button.get_global_rect()
	_touch(TAP_FINGER, again.get_center(), true)
	_touch(TAP_FINGER, again.get_center() + Vector2(0.0, again.size.y), false)
	await _frames(2)
	# The run may have ended (off the lane), but it was not started again.
	_expect(time_s > 1.0 and float(run.get_snapshot()["time_s"]) >= time_s,
			"a touch slid off again does nothing")

	# Again: back to the start, in neutral with the throttle closed.
	_tap(controls.restart_button)
	await _frames(2)
	s = run.get_snapshot()
	_expect(float(s["time_s"]) < 0.1, "a new run: %s s" % s["time_s"])
	_expect(controls.gear == 0 and controls.throttle() == 0.0, "the controls are reset")

	await _play_pro(controls, run)
	await _play_tilt(controls, run)

	print("touch test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)


# Every control in `list` is on the screen, where a thumb can reach it, and
# no two overlap.
func _expect_on_screen(list: Array) -> void:
	var screen := root.get_visible_rect()
	for i in list.size():
		var rect: Rect2 = (list[i] as Control).get_global_rect()
		_expect(rect.has_area() and screen.encloses(rect), "on the screen: %s" % rect)
		for j in range(i + 1, list.size()):
			_expect(not rect.intersects((list[j] as Control).get_global_rect()),
					"controls %d and %d overlap" % [i, j])


# Slides `slider` from just inside one end to past the other: 0 to 1
# (`up`) or 1 to 0.
func _slide(finger: int, slider: TouchSlider, up: bool) -> void:
	var rect := slider.get_global_rect()
	var x := rect.get_center().x
	var low := Vector2(x, rect.end.y - 1.0)
	var high := Vector2(x, rect.position.y + 1.0)
	var past := Vector2(x, rect.position.y - 10.0) if up else Vector2(x, rect.end.y + 10.0)
	_touch(finger, low if up else high, true)
	_drag(finger, past)
	_touch(finger, past, false)


# The pro layout: the layout button switches and starts again; the clutch
# starts pressed and is slipped in with a slide; the brakes brake while held.
func _play_pro(controls: DriveControls, run: TractorPullRun) -> void:
	var start: StartScreen = _pull.get("start_screen")
	var setup: SetupScreen = _pull.get("setup_screen")
	var settings: SettingsScreen = _pull.get("settings_screen")
	_tap(controls.menu_button)
	await _frames(2)
	_expect(start.visible and not controls.visible, "menu: back to the start screen")
	# The gear in the top right corner opens the settings.
	var corner := root.get_visible_rect().end
	var gear := start.settings_button.get_global_rect()
	_expect(gear.end.x > corner.x * 0.9 and gear.position.y < corner.y * 0.1,
			"the gear in the top right: %s" % gear)
	_tap(start.settings_button)
	await _frames(2)
	_expect(settings.visible and not start.visible and settings.easy_button.selected
			and settings.slider_button.selected, "the settings: easy, the slider")
	_tap(settings.pro_button)
	_expect(settings.pro_button.selected and not settings.easy_button.selected, "pro picked")
	_expect(GameSettings.read(SETTINGS_PATH).pro, "pro kept in the settings file")
	_tap(settings.done_button)
	await _frames(2)
	_expect(start.visible and not settings.visible, "done: back to the start screen")
	_tap(start.practice_button)
	await _frames(2)
	_tap(setup.pull_button)
	await _frames(2)
	_expect(run.get_layout() == "pro" and controls.pro, "the pro layout")
	_expect_on_screen([controls.steering_slider, controls.throttle_slider, controls.clutch_slider,
			controls.brake_left_slider, controls.brake_right_slider, controls.gear_up,
			controls.gear_down, controls.gear_readout, controls.camera_button,
			controls.restart_button,
			controls.menu_button])
	_expect(controls.clutch() == 1.0, "the clutch starts pressed")
	# The clutch slider marks where the clutch slips, its line at the bite,
	# where the thumb's middle is for those pedals.
	var clutch := controls.clutch_slider
	_expect(clutch.has_band and is_equal_approx(clutch.band_from, CLUTCH_FULL_GRIP)
			and is_equal_approx(clutch.band_to, CLUTCH_BITE)
			and is_equal_approx(clutch.mark, CLUTCH_BITE), "the clutch's range marked")
	_expect(clutch.position_of(CLUTCH_FULL_GRIP) < clutch.position_of(CLUTCH_BITE)
			and clutch.position_of(CLUTCH_BITE) < clutch.size.y, "the bite above the full grip")
	# Each side's pedals under its own thumb: the clutch and the left brake
	# with the steering on the left, the right brake with the throttle.
	var middle := root.get_visible_rect().get_center().x
	for left: Control in [controls.clutch_slider, controls.brake_left_slider]:
		_expect(left.get_global_rect().end.x < middle, "on the left: %s" % left.label)
	_expect(controls.brake_right_slider.get_global_rect().position.x > middle,
			"the right brake on the right")
	_expect(controls.clutch_slider.colour != controls.throttle_slider.colour
			and controls.brake_left_slider.colour != controls.clutch_slider.colour,
			"the controls look different")

	for i in 6:
		_tap(controls.gear_up)
	_slide(THROTTLE_FINGER, controls.throttle_slider, true)
	await _frames(PULL_FRAMES / 3)
	var s := run.get_snapshot()
	_expect(s["gear"] == 6 and float(s["clutch_pedal"]) == 1.0, "in 6th, the clutch pressed")
	_expect(float(s["tractor_speed_m_s"]) < 0.01, "standing on the pressed clutch")

	# The clutch slid in: down from pressed to released, and left there.
	_slide(TAP_FINGER, controls.clutch_slider, false)
	_expect(controls.clutch() == 0.0, "the clutch released: %s" % controls.clutch())
	await _frames(PULL_FRAMES)
	s = run.get_snapshot()
	_expect(float(s["clutch_pedal"]) == 0.0, "the run has the clutch in")
	_expect(float(s["tractor_speed_m_s"]) > 0.5, "the tractor pulls: %s m/s" % s["tractor_speed_m_s"])

	# One gear down with the clutch in: the gearbox's powershift step.
	_tap(controls.gear_down)
	await _frames(2)
	s = run.get_snapshot()
	_expect(s["gear"] == 5 and float(s["clutch_pedal"]) == 0.0, "5th with the clutch in: %s"
			% s["gear"])
	# Once a pull, and under way never down with the clutch either: a
	# second "-" is not taken.
	_tap(controls.gear_down)
	await _frames(2)
	s = run.get_snapshot()
	_expect(s["gear"] == 5 and controls.gear == 5,
			"a second gear down is refused: %s" % s["gear"])

	# A brake brakes while held and comes off when let go.
	var brake := controls.brake_left_slider.get_global_rect()
	_touch(STEERING_FINGER, Vector2(brake.get_center().x, brake.end.y - 1.0), true)
	_drag(STEERING_FINGER, Vector2(brake.get_center().x, brake.position.y - 10.0))
	_expect(controls.brake_left() == 1.0 and controls.brake_right() == 0.0, "the left brake on")
	_touch(STEERING_FINGER, brake.position, false)
	_expect(controls.brake_left() == 0.0, "the left brake off when let go")

	# Back to easy.
	_tap(controls.menu_button)
	await _frames(2)
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.easy_button)
	_tap(settings.done_button)
	await _frames(2)
	_tap(start.practice_button)
	await _frames(2)
	_tap(setup.pull_button)
	await _frames(2)
	_expect(run.get_layout() == "easy" and not controls.clutch_slider.visible, "back to easy")


# Gravity for a phone rolled `degrees` with its left side down, in the
# screen's plane as Android reports it (up the screen when held upright).
func _tilted(degrees: float) -> Vector3:
	var rad := deg_to_rad(degrees)
	return Vector3(sin(rad), cos(rad), 0.0) * G_M_S2


# Tilt steering, with a simulated sensor: picked in the settings, it hides
# the slider; the phone's angle at Pull is straight ahead (here held
# rolled 20° already), and rolling it further to the left steers left.
func _play_tilt(controls: DriveControls, run: TractorPullRun) -> void:
	var start: StartScreen = _pull.get("start_screen")
	var setup: SetupScreen = _pull.get("setup_screen")
	var settings: SettingsScreen = _pull.get("settings_screen")
	controls.tilt_steering.gravity = func() -> Vector3: return _gravity
	_gravity = _tilted(20.0)
	_tap(controls.menu_button)
	await _frames(2)
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.tilt_button)
	_expect(settings.tilt_button.selected and GameSettings.read(SETTINGS_PATH).tilt,
			"tilt picked and kept")
	_tap(settings.done_button)
	await _frames(2)
	_tap(start.practice_button)
	await _frames(2)
	_tap(setup.pull_button)
	await _frames(2)
	_expect(controls.tilt and not controls.steering_slider.visible, "tilt: no steering slider")
	_expect(absf(controls.steering()) < 1e-6, "held as at Pull: straight ahead")
	for i in 6:
		_tap(controls.gear_up)
	_slide(THROTTLE_FINGER, controls.throttle_slider, true)
	await _frames(PULL_FRAMES)
	# Rolled 10° more to the left: 10° of the 25° that steer fully.
	_gravity = _tilted(30.0)
	_expect(absf(controls.steering() - 10.0 / TiltSteering.FULL_TILT_DEG) < 1e-6,
			"10° more steers left: %s" % controls.steering())
	await _frames(STEER_FRAMES)
	_expect(float(run.get_snapshot()["steering_rad"]) > 0.0, "the front wheels turn left: %s"
			% run.get_snapshot()["steering_rad"])
	# Lying flat the roll means nothing: the last steering holds.
	_gravity = Vector3(0.0, 0.0, G_M_S2)
	_expect(absf(controls.steering() - 10.0 / TiltSteering.FULL_TILT_DEG) < 1e-6,
			"flat: the steering holds")
	# Past the full tilt to the right: full right.
	_gravity = _tilted(-20.0)
	_expect(controls.steering() == -1.0, "rolled right past full: %s" % controls.steering())

	# Lying flat at Pull, the first angle it is lifted to is straight ahead.
	_gravity = Vector3(0.0, 0.0, G_M_S2)
	_tap(controls.restart_button)
	await _frames(2)
	_expect(absf(controls.steering()) < 1e-6, "flat at Pull: straight ahead")
	_gravity = _tilted(20.0)
	_expect(absf(controls.steering()) < 1e-6, "lifted to 20°: still straight ahead: %s"
			% controls.steering())
	_gravity = _tilted(30.0)
	_expect(absf(controls.steering() - 10.0 / TiltSteering.FULL_TILT_DEG) < 1e-6,
			"then 10° more steers left: %s" % controls.steering())

	# Back to the slider.
	_tap(controls.menu_button)
	await _frames(2)
	_tap(start.settings_button)
	await _frames(2)
	_tap(settings.slider_button)
	_tap(settings.done_button)
	await _frames(2)
	_tap(start.practice_button)
	await _frames(2)
	_tap(setup.pull_button)
	await _frames(2)
	_expect(not controls.tilt and controls.steering_slider.visible, "back to the slider")
