# The touch controls, played on the pull scene with simulated fingers: the
# hand throttle slid open like a lever and left there, the gear tapped up, a
# button let go when the app loses focus, a thumb steering while the other
# holds the throttle, the steering springing back when the thumb lifts, and
# "again" only on a release over it; then the pro layout, picked on the
# start screen. Checks that the run gets the controls and the tractor pulls
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
const STEER_FRAMES := 60

var _pull: Node
var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
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
	# The start screen, in the easy layout: "Next" starts the pull.
	_expect(start.visible and not controls.visible, "the start screen first")
	_tap(start.next_button)
	await _frames(2)
	_expect(controls.visible and not start.visible, "the pull's controls after Next")
	var throttle := controls.throttle_slider.get_global_rect()
	var steering := controls.steering_slider.get_global_rect()

	# Every control is on the screen, where a thumb can reach it.
	_expect_on_screen([controls.steering_slider, controls.throttle_slider, controls.gear_up,
			controls.gear_down, controls.neutral, controls.camera_button, controls.restart_button,
			controls.menu_button])
	_expect(run.get_layout() == "easy" and not controls.clutch_slider.visible
			and not controls.brake_left_slider.visible, "the easy layout, without clutch or brakes")

	# A touch on no control changes nothing.
	_touch(TAP_FINGER, Vector2(640.0, 300.0), true)
	_touch(TAP_FINGER, Vector2(640.0, 300.0), false)
	_expect(controls.throttle() == 0.0 and controls.gear == 0, "a touch off the controls")

	# The gear: down from neutral stays there, up stops at the top gear.
	_tap(controls.gear_down)
	_expect(controls.gear == 0, "no gear below neutral")
	for i in controls.gear_count + 2:
		_tap(controls.gear_up)
	_expect(controls.gear == controls.gear_count, "the top gear is the last")
	_tap(controls.neutral)
	_expect(controls.gear == 0, "neutral")
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
	_expect(float(s["throttle"]) == 1.0, "the run has the throttle open")
	_expect(float(s["tractor_speed_m_s"]) > 0.5, "the tractor pulls: %s m/s" % s["tractor_speed_m_s"])

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
	_tap(controls.menu_button)
	await _frames(2)
	_expect(start.visible and not controls.visible, "menu: back to the start screen")
	_tap(start.pro_button)
	_expect(start.pro_button.selected and not start.easy_button.selected, "pro picked")
	_tap(start.next_button)
	await _frames(2)
	_expect(run.get_layout() == "pro" and controls.pro, "the pro layout")
	_expect_on_screen([controls.steering_slider, controls.throttle_slider, controls.clutch_slider,
			controls.brake_left_slider, controls.brake_right_slider, controls.gear_up,
			controls.gear_down, controls.neutral, controls.camera_button, controls.restart_button,
			controls.menu_button])
	_expect(controls.clutch() == 1.0, "the clutch starts pressed")

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
	_tap(start.easy_button)
	_tap(start.next_button)
	await _frames(2)
	_expect(run.get_layout() == "easy" and not controls.clutch_slider.visible, "back to easy")
