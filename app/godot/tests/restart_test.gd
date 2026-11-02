# A pull that stops short of 30 m offers the restart choice (RestartScreen):
# a scripted driver pulls a few metres and lifts off, then lets go of the
# controls; the run waits for the choice, "Restart" puts the sled and the
# tractor back at the line for the pull again; a second short stop counts.
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
	var frame := 0
	while frame < MAX_FRAMES and not run.may_restart():
		await process_frame
		frame += 1
	_expect(run.may_restart(), "stopped short of 30 m: %s" % run.get_result())
	# A scripted driver's pull counts; let go of it and the choice comes.
	_pull.set("driver", Callable())
	await process_frame
	await process_frame
	_expect(restart.visible and not restart.move_back_button.visible,
			"the choice, no moving back in practice")
	var steps_before: float = run.get_snapshot()["time_s"]
	await process_frame
	_expect(run.get_snapshot()["time_s"] == steps_before, "the run waits for the choice")
	restart.restart_requested.emit()
	await process_frame
	var s := run.get_snapshot()
	_expect(not restart.visible and run.get_result().is_empty() and float(s["sled_x_m"]) == 0.0
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
	print("restart test: %d failure(s)" % _failures)
	quit(1 if _failures > 0 else 0)
