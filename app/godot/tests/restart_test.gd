# A pull that stops short of 30 m offers the restart choice (RestartScreen):
# a scripted driver pulls a few metres and lifts off, then lets go of the
# controls; the run waits for the choice, "Restart" puts the sled and the
# tractor back at the line for the pull again; a second short stop counts.
# A fresh short pull then takes "Finish": declined, it counts and ends.
# Run: godot --headless --fixed-fps 30 --path app/godot --script res://tests/restart_test.gd
extends SceneTree

const SETTINGS_PATH := "user://restart_test_settings.cfg"
const MAX_FRAMES := 900
const LIFT_AT_S := 7.0

var _pull: Node
var _failures := 0
var _time_s := 0.0


func _initialize() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SETTINGS_PATH))
	var scene: PackedScene = load("res://scenes/pull.tscn")
	_pull = scene.instantiate()
	_pull.set("settings_path", SETTINGS_PATH)
	_pull.set("career_path", "res://tests/data/practice_career.toml")
	_pull.set("save_path", "")
	_pull.set("whole_turn", false)
	root.add_child(_pull)
	_play.call_deferred()


func _expect(ok: bool, what: String) -> void:
	if not ok:
		printerr("FAILED: ", what)
		_failures += 1


# Neutral, rev up, slip the clutch in over 3 s in 6th, lift off and brake.
func _drive(_s: Dictionary) -> Array:
	_time_s += 1.0 / 30.0
	if _time_s < 0.5:
		return [0.0, 1.0, 0, 0.0, 0.0, 0.0]
	if _time_s >= LIFT_AT_S:
		return [0.0, 1.0, 6, 0.0, 1.0, 1.0]
	return [1.0, clampf(1.0 - (_time_s - 3.0) / 3.0, 0.0, 1.0), 6, 0.0, 0.0, 0.0]


func _play() -> void:
	await process_frame
	await process_frame
	_pull.set("pro", true)
	_pull.set("driver", _drive)
	_pull.call("_start_practice")
	var run: TractorPullRun = _pull.get("run")
	var restart: RestartScreen = _pull.get("restart_screen")
	# The phone's vibrations, caught instead of felt.
	var haptics: Haptics = _pull.get("haptics")
	var felt: Array = []
	haptics.vibrate = func(ms: int, strength: float) -> void: felt.append([ms, strength])
	var result_waited := true
	var frame := 0
	while frame < MAX_FRAMES and not run.may_restart():
		await process_frame
		frame += 1
		# In the pull the result is not told yet.
		if run.get_snapshot().get("stage", "") == "pulling":
			result_waited = result_waited and haptics.wants_result()
	_expect(result_waited, "no result told while the pull is on")
	_expect(run.may_restart(), "stopped short of 30 m: %s" % run.get_result())
	# A scripted driver's pull counts; let go of it and the choice comes.
	_pull.set("driver", Callable())
	await process_frame
	await process_frame
	_expect(restart.visible and not restart.move_back_button.visible,
			"the choice, no moving back in practice")
	var steps_before: float = run.get_snapshot()["time_s"]
	felt.clear()
	for i in 20:
		await process_frame
	_expect(run.get_snapshot()["time_s"] == steps_before, "the run waits for the choice")
	_expect(felt.is_empty(), "nothing felt behind the choice: %s" % [felt])
	restart.restart_requested.emit()
	await process_frame
	_expect(haptics.wants_result(), "restarted: the new pull's result to be told")
	var s := run.get_snapshot()
	_expect(not restart.visible and run.get_result().is_empty() and float(s["pulled_m"]) == 0.0
			and int(s["gear"]) == 0, "restarted at the line in neutral: %s" % s)
	# Pull and stop again: one restart a pull, so no choice now.
	_time_s = 0.0
	_pull.set("driver", _drive)
	frame = 0
	while frame < MAX_FRAMES and run.get_result().is_empty():
		await process_frame
		frame += 1
	_pull.set("driver", Callable())
	await process_frame
	await process_frame
	_expect(not restart.visible and not run.may_restart() and not run.get_result().is_empty(),
			"stopped short again: one restart only, the pull counts")
	# A fresh pull stopped short, and "Finish": the restart is declined
	# (TractorPullRun.decline_restart), the choice gone, and the pull ends
	# with its result shown.
	_pull.call("_start_practice")
	run = _pull.get("run")
	_time_s = 0.0
	_pull.set("driver", _drive)
	frame = 0
	while frame < MAX_FRAMES and not run.may_restart():
		await process_frame
		frame += 1
	_pull.set("driver", Callable())
	await process_frame
	await process_frame
	_expect(restart.visible, "the choice again on a fresh pull")
	restart.finish_requested.emit()
	await process_frame
	_expect(not restart.visible and not run.may_restart() and not run.get_result().is_empty(),
			"finished: declined, the short pull counts")
	frame = 0
	while frame < MAX_FRAMES and _pull.get("screen") != _pull.Screen.RESULT:
		await process_frame
		frame += 1
	_expect(_pull.get("screen") == _pull.Screen.RESULT, "the result shown after finishing")
	print("restart test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
