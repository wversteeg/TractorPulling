# Haptic feedback: short vibrations that tell the player's hands what a
# real tractor's seat, pedals and levers would. Each kind can be switched
# on or off in the settings' Haptics menu (GameSettings.haptics); all of
# them read the run's snapshot each frame (update()) and the pull's result
# once it comes (show_result()).
#
# The phone buzzes through `vibrate` (Input.vibrate_handheld by default:
# a duration in ms and a strength from 0 to 1), which the tests replace to
# see what would be felt. A pattern of several buzzes is queued and played
# out over the next frames.
class_name Haptics
extends RefCounted

enum Kind {
	CLUTCH_BITE,
	WHEELSPIN,
	LUGGING,
	GEAR,
	CHAIN,
	LANDING,
	FULL_PULL,
	CHALK_LINE,
}

# The settings' names for the kinds (GameSettings keeps them by these), and
# what the Haptics menu calls them.
const KEYS := {
	Kind.CLUTCH_BITE: "clutch_bite",
	Kind.WHEELSPIN: "wheelspin",
	Kind.LUGGING: "lugging",
	Kind.GEAR: "gear",
	Kind.CHAIN: "chain",
	Kind.LANDING: "landing",
	Kind.FULL_PULL: "full_pull",
	Kind.CHALK_LINE: "chalk_line",
}
const LABELS := {
	Kind.CLUTCH_BITE: "Clutch bite point",
	Kind.WHEELSPIN: "Wheelspin",
	Kind.LUGGING: "Engine lugging",
	Kind.GEAR: "Gear changes",
	Kind.CHAIN: "Chain tight",
	Kind.LANDING: "Front lands",
	Kind.FULL_PULL: "Full pull",
	Kind.CHALK_LINE: "Chalk line",
}

# The clutch: a tick each time the pedal passes the bite point [ms, 0..1].
const BITE_MS := 15
const BITE_STRENGTH := 0.5
# Wheelspin: a buzz every WHEELSPIN_EVERY_S while the tires slip more than
# WHEELSPIN_FROM, stronger as they slip more, full at WHEELSPIN_FULL.
const WHEELSPIN_FROM := 0.25
const WHEELSPIN_FULL := 0.8
const WHEELSPIN_EVERY_S := 0.12
const WHEELSPIN_MS := 40
# Lugging: a slow, firm pulse while the engine, working (throttle past
# LUGGING_THROTTLE, the clutch in), is pulled below LUGGING_SHARE of its
# rated speed: the cue to shift down or ease off.
const LUGGING_SHARE := 0.75
const LUGGING_THROTTLE := 0.5
const LUGGING_EVERY_S := 0.4
const LUGGING_MS := 60
const LUGGING_STRENGTH := 0.8
# Gears: a crisp tick when one goes in; two short buzzes when the gear
# asked for has not gone in after REFUSED_AFTER_S (refused).
const GEAR_MS := 20
const GEAR_STRENGTH := 0.6
const REFUSED_AFTER_S := 0.3
const REFUSED_MS := 30
const REFUSED_GAP_S := 0.08
# The chain coming tight while the slack is taken up.
const CHAIN_MS := 50
const CHAIN_STRENGTH := 0.7
# The front wheels landing after they were off the ground at least
# LANDED_AFTER_S: a thump.
const LANDED_AFTER_S := 0.2
const LANDING_MS := 80
const LANDING_STRENGTH := 1.0
# Lighter than this, the front axle is off the ground [N].
const FRONT_OFF_N := 1.0
# A full pull: three rising buzzes; out of bounds: one long hard one.
const FULL_PULL_MS := 70
const FULL_PULL_GAP_S := 0.12
const CHALK_MS := 400

# Each kind on (true) or off, by Kind.
var enabled := {}
# Called with (duration_ms: int, strength: float).
var vibrate := func(ms: int, strength: float) -> void: Input.vibrate_handheld(ms, strength)
# The run's sizes (TractorPullRun.get_dimensions()).
var clutch_bite_pedal := 0.5
var rated_omega_rad_s := 250.0
var take_up_tension_n := 1000.0

var _last := {}
var _since_wheelspin_s := 0.0
var _since_lugging_s := 0.0
var _front_off_s := 0.0
var _asked_gear := 0
var _asked_for_s := 0.0
var _refused_told := false
var _result_told := false
# Buzzes still to play: [delay_s, ms, strength], in order.
var _queue: Array = []


func _init() -> void:
	for kind: int in Kind.values():
		enabled[kind] = true


# The settings' dictionary (key: on) into `enabled`; kinds it lacks stay on.
func set_enabled_by_key(by_key: Dictionary) -> void:
	for kind: int in Kind.values():
		enabled[kind] = bool(by_key.get(KEYS[kind], true))


func enabled_by_key() -> Dictionary:
	var out := {}
	for kind: int in Kind.values():
		out[KEYS[kind]] = enabled[kind]
	return out


# A new run: its sizes, and nothing remembered from the last one.
func configure(dimensions: Dictionary) -> void:
	clutch_bite_pedal = dimensions.get("clutch_bite_pedal", clutch_bite_pedal)
	rated_omega_rad_s = dimensions.get("rated_omega_rad_s", rated_omega_rad_s)
	take_up_tension_n = dimensions.get("take_up_tension_n", take_up_tension_n)
	_last = {}
	_queue.clear()
	_result_told = false
	_front_off_s = 0.0
	_asked_for_s = 0.0
	_refused_told = false


# The frame's snapshot, the gear the player asks for, and the frame's time.
func update(s: Dictionary, asked_gear: int, delta: float) -> void:
	_play_queue(delta)
	if s.is_empty():
		return
	if not _last.is_empty():
		_clutch(s)
		_gear(s, asked_gear, delta)
		_chain(s)
	_wheelspin(s, delta)
	_lugging(s, delta)
	_landing(s, delta)
	_last = s


# The pull's result (TractorPullRun.get_result()), told once: a full pull's
# rising buzzes, or the chalk line's long one.
func show_result(result: Dictionary) -> void:
	if result.is_empty() or _result_told:
		return
	_result_told = true
	if result.get("full_pull", false) and enabled[Kind.FULL_PULL]:
		for i in 3:
			_queue.append([FULL_PULL_GAP_S if i > 0 else 0.0, FULL_PULL_MS, 0.5 + 0.25 * i])
	elif String(result.get("ending", "")) == "out_of_bounds" and enabled[Kind.CHALK_LINE]:
		_queue.append([0.0, CHALK_MS, 1.0])


func _buzz(kind: int, ms: int, strength: float) -> void:
	if enabled[kind]:
		vibrate.call(ms, clampf(strength, 0.0, 1.0))


func _clutch(s: Dictionary) -> void:
	var before: float = _last["clutch_pedal"]
	var now: float = s["clutch_pedal"]
	if (before - clutch_bite_pedal) * (now - clutch_bite_pedal) < 0.0:
		_buzz(Kind.CLUTCH_BITE, BITE_MS, BITE_STRENGTH)


func _gear(s: Dictionary, asked_gear: int, delta: float) -> void:
	if int(s["gear"]) != int(_last["gear"]):
		_buzz(Kind.GEAR, GEAR_MS, GEAR_STRENGTH)
	if asked_gear != _asked_gear:
		_asked_gear = asked_gear
		_asked_for_s = 0.0
		_refused_told = false
	if asked_gear == int(s["gear"]):
		_asked_for_s = 0.0
		return
	_asked_for_s += delta
	if _asked_for_s >= REFUSED_AFTER_S and not _refused_told:
		_refused_told = true
		if enabled[Kind.GEAR]:
			_queue.append([0.0, REFUSED_MS, 1.0])
			_queue.append([REFUSED_GAP_S, REFUSED_MS, 1.0])


func _chain(s: Dictionary) -> void:
	var taking_up: bool = s.get("stage", "") in ["taking_up_slack", "waiting_for_the_flag"]
	if taking_up and float(_last["chain_tension_n"]) < take_up_tension_n \
			and float(s["chain_tension_n"]) >= take_up_tension_n:
		_buzz(Kind.CHAIN, CHAIN_MS, CHAIN_STRENGTH)


func _wheelspin(s: Dictionary, delta: float) -> void:
	var slip := absf(float(s["slip_ratio"]))
	if not bool(s["engine_running"]) or slip < WHEELSPIN_FROM:
		_since_wheelspin_s = WHEELSPIN_EVERY_S
		return
	_since_wheelspin_s += delta
	if _since_wheelspin_s >= WHEELSPIN_EVERY_S:
		_since_wheelspin_s = 0.0
		var share := (slip - WHEELSPIN_FROM) / (WHEELSPIN_FULL - WHEELSPIN_FROM)
		_buzz(Kind.WHEELSPIN, WHEELSPIN_MS, lerpf(0.3, 1.0, clampf(share, 0.0, 1.0)))


func _lugging(s: Dictionary, delta: float) -> void:
	var working: bool = bool(s["engine_running"]) and float(s["throttle"]) > LUGGING_THROTTLE \
			and float(s["clutch_pedal"]) < clutch_bite_pedal
	if not working or float(s["engine_omega_rad_s"]) >= LUGGING_SHARE * rated_omega_rad_s:
		_since_lugging_s = LUGGING_EVERY_S
		return
	_since_lugging_s += delta
	if _since_lugging_s >= LUGGING_EVERY_S:
		_since_lugging_s = 0.0
		_buzz(Kind.LUGGING, LUGGING_MS, LUGGING_STRENGTH)


func _landing(s: Dictionary, delta: float) -> void:
	if float(s["front_axle_load_n"]) < FRONT_OFF_N:
		_front_off_s += delta
		return
	if _front_off_s >= LANDED_AFTER_S:
		_buzz(Kind.LANDING, LANDING_MS, LANDING_STRENGTH)
	_front_off_s = 0.0


func _play_queue(delta: float) -> void:
	var time_s := delta
	while not _queue.is_empty():
		var next: Array = _queue[0]
		if float(next[0]) > time_s:
			next[0] = float(next[0]) - time_s
			return
		time_s -= float(next[0])
		_queue.pop_front()
		vibrate.call(int(next[1]), float(next[2]))
