# Plays a scripted run through the extension, headless, and prints its
# state hash: the same driver as `tp_pull run ... --easy data/controls/easy.toml
# --stick 0.002 --hash`, a step at a time, so the two must print the same hash
# (scripts/godot_test.sh compares them).
#
#   godot --headless --path app/godot --script res://tests/run_test.gd
extends SceneTree

const STEP_NS := 1_000_000
const STEP_S := 0.001
const SELECT_GEAR_S := 0.5
const LAUNCH_S := 3.0
const GEAR := 6
const STICK := 0.002
const MAX_STEPS := 200_000


func _init() -> void:
	var run := TractorPullRun.new()
	var error := run.setup("res://data/tractors/farm_stock_light.toml",
			"res://data/sleds/light_class.toml", "res://data/tracks/ideal_clay_sand.toml",
			"res://data/controls/easy.toml")
	if not error.is_empty():
		printerr("setup: ", error)
		quit(1)
		return
	var steps := 0
	while not run.is_finished() and steps < MAX_STEPS:
		var t := steps * STEP_S
		if t < SELECT_GEAR_S:
			run.set_controls(0.0, 1.0, 0, 0.0, 0.0, 0.0)
		else:
			run.set_controls(1.0, 1.0, GEAR, STICK if t >= LAUNCH_S else 0.0, 0.0, 0.0)
		run.advance(STEP_NS)
		steps += 1
	if not run.is_finished():
		printerr("the run did not end")
		quit(1)
		return
	var result := run.get_result()
	print("result: ", result.ending, " ", result.distance_m, " m")
	print("state hash ", run.get_state_hash())
	quit(0)
