# A replay of the pull, as on TV: while the pull is on, record() keeps each
# frame's snapshot (TractorPullRun.get_snapshot()), from a moment before the
# green flag (the boost building) until the sled has stood a moment after
# the pull. After it, start() and next_frame() play the snapshots back at
# the speed they were recorded, and shot() says where a trackside camera
# stands for each: low beside the launch, alongside the tractor down the
# track, head-on at the full-pull line, and behind the sled at the end.
# The replay only draws what was recorded; the run is not stepped again.
# The track's marks are laid again as it plays (TrackMarks).
class_name PullReplay
extends RefCounted

# Kept from before the green flag, and after the pull has ended [s].
const BEFORE_FLAG_S := 2.0
const TRIM_EVERY_S := 1.0
const AFTER_END_S := 3.0
# The stages recorded: standing ready for the flag, and the pull.
const RECORDED_STAGES := ["waiting_for_the_flag", "pulling"]
# The shots (TrackSpace: x down the track, y left, z up) [m]. The launch:
# from this far down the track, low and just outside the right-hand chalk
# line (the start's official stands on the left), until the tractor is
# this far down the track.
const LAUNCH_UNTIL_M := 20.0
const LAUNCH_AHEAD_M := 12.0
const LAUNCH_OUT_M := 1.0
const LAUNCH_UP_M := 0.8
# Alongside: this far behind the tractor, out beside the lane's left
# chalk line and up, until this far before the full-pull line.
const ALONGSIDE_BEHIND_M := 6.0
const ALONGSIDE_OUT_M := 3.0
const ALONGSIDE_UP_M := 2.2
const HEAD_ON_BEFORE_LINE_M := 30.0
# Head-on: this far past the full-pull line, a little left of the middle,
# at eye height; always at least this far ahead of the tractor (a pull may
# go on past the line, which a test pull moves to 110 m).
const HEAD_ON_PAST_LINE_M := 12.0
const HEAD_ON_AHEAD_M := 10.0
const HEAD_ON_LEFT_M := 1.5
const HEAD_ON_UP_M := 1.6
# Once the pull is over: behind the sled and above it.
const END_BEHIND_SLED_M := 12.0
const END_UP_M := 5.0
# How wide each shot frames the scene at the tractor [m]: the launch from
# the rear wheels' width out to the alongside view's, head-on the tractor
# with room round it, the finish the tractor and its chain.
const LAUNCH_FRAME_M := 4.0
const ALONGSIDE_FRAME_M := 12.0
const HEAD_ON_FRAME_M := 9.0
const FINISH_FRAME_M := 7.0
const MIN_FOV_DEG := 8.0
const MAX_FOV_DEG := 60.0
# The finish: over the last this many metres before the tractor stopped,
# from this far past its stopping place, just outside the left chalk line,
# low [m].
const FINISH_WITHIN_M := 6.0
const FINISH_AHEAD_M := 5.0
const FINISH_OUT_M := 1.5
const FINISH_UP_M := 1.0
# The cameras look at the tractor this high [m].
const LOOK_AT_UP_M := 1.0

# Each frame: its time since the recording began, and its snapshot.
var _times: Array[float] = []
var _frames: Array[Dictionary] = []
var _recording_s := 0.0
var _ended_s := -1.0
var _has_pull := false
var _stop_m := INF
var _playing := false
var _play_s := 0.0
var _index := 0


func clear() -> void:
	_times.clear()
	_frames.clear()
	_recording_s = 0.0
	_ended_s = -1.0
	_has_pull = false
	_stop_m = INF
	_playing = false


# Keeps the frame's snapshot `s` (shown `delta` after the last): the wait
# for the flag (its last BEFORE_FLAG_S), the pull, and AFTER_END_S after it.
func record(s: Dictionary, delta: float) -> void:
	if s.is_empty() or _playing:
		return
	var stage: String = s.get("stage", "")
	if stage in RECORDED_STAGES:
		if _ended_s >= 0.0:
			# Pulled again (restarted): the last pull's frames go.
			clear()
		if stage == "pulling" and not _has_pull:
			# The flag: the wait's last BEFORE_FLAG_S, up to the last frame
			# before it.
			_trim_before_flag(true)
		_add(s, delta)
		if stage == "waiting_for_the_flag":
			_trim_before_flag()
		return
	if _ended_s >= AFTER_END_S:
		return
	if not has_pull():
		# Backed off before the flag, or not there yet: nothing to keep.
		clear()
		return
	_ended_s = maxf(_ended_s, 0.0) + delta
	_add(s, delta)


func _add(s: Dictionary, delta: float) -> void:
	if s.get("stage", "") == "pulling":
		_has_pull = true
		_stop_m = float(s.get("tractor_x_m", 0.0))
	_recording_s += delta
	_times.append(_recording_s)
	_frames.append(s)


# Only BEFORE_FLAG_S before the latest frame stays of the wait: trimmed a
# second at a time while waiting, rather than every frame, and exactly at
# the flag (`exactly`).
func _trim_before_flag(exactly: bool = false) -> void:
	var from_s := _recording_s - BEFORE_FLAG_S
	if _times.is_empty() or _times[0] >= from_s - (0.0 if exactly else TRIM_EVERY_S):
		return
	var drop := 0
	while drop < _times.size() and _times[drop] < from_s:
		drop += 1
	if drop > 0:
		_times = _times.slice(drop)
		_frames = _frames.slice(drop)


# Whether a pull was recorded: one that got the green flag.
func has_pull() -> bool:
	return _has_pull


func start() -> void:
	_playing = has_pull()
	_play_s = _times[0] if _playing else 0.0
	_index = 0


func playing() -> bool:
	return _playing


func stop() -> void:
	_playing = false


# The snapshot `delta` further into the replay, {} once it is over.
func next_frame(delta: float) -> Dictionary:
	if not _playing:
		return {}
	_play_s += delta
	while _index + 1 < _times.size() and _times[_index + 1] <= _play_s:
		_index += 1
	if _play_s > _times[-1]:
		_playing = false
		return {}
	return _frames[_index]


# The frames still to come while it plays, none once it is over or stopped.
func frames_left() -> Array[Dictionary]:
	if not _playing:
		return []
	return _frames.slice(_index + 1)


# How long the replay lasts [s].
func length_s() -> float:
	return 0.0 if _times.is_empty() else _times[-1] - _times[0]


# Where the trackside camera stands for snapshot `s` and what it looks at
# (TrackSpace), on a track with its full-pull line at `full_pull_m` and
# `lane_m` between the chalk lines: {"from": Vector3, "at": Vector3,
# "name": String}.
# `stop_m` is where the tractor stood when the pull ended (stop_m()): the
# last metres before it, and after, are the finish shot. Each shot has the
# zoom ("fov", degrees) that frames the tractor as wide as it says: the
# launch tight on the rear wheels, to see them bite or spin.
static func shot(s: Dictionary, full_pull_m: float, lane_m: float,
		stop_m: float = INF) -> Dictionary:
	var tractor_m := float(s.get("tractor_x_m", 0.0))
	var at := Vector3(tractor_m, float(s.get("tractor_y_m", 0.0)), LOOK_AT_UP_M)
	var shot := {}
	var stage: String = s.get("stage", "pulling")
	if stage in RECORDED_STAGES and tractor_m < LAUNCH_UNTIL_M \
			and tractor_m < stop_m - FINISH_WITHIN_M:
		var from := launch_from(lane_m)
		# Tight on the wheels as they bite, wider as the tractor comes on.
		var frame_m := lerpf(LAUNCH_FRAME_M, ALONGSIDE_FRAME_M,
				clampf(tractor_m / LAUNCH_UNTIL_M, 0.0, 1.0))
		shot = {"name": "launch", "from": from, "frame_m": frame_m}
	elif is_finite(stop_m) and tractor_m >= stop_m - FINISH_WITHIN_M:
		# On the left, as alongside and head-on are: the cut keeps the
		# tractor going the same way across the screen.
		shot = {"name": "finish", "from": Vector3(stop_m + FINISH_AHEAD_M,
				lane_m / 2.0 + FINISH_OUT_M, FINISH_UP_M), "frame_m": FINISH_FRAME_M}
	elif stage not in RECORDED_STAGES:
		# The stop not known (a replay with no pull): from behind the sled.
		var sled_m := float(s.get("sled_x_m", 0.0))
		shot = {"name": "end", "from": Vector3(sled_m - END_BEHIND_SLED_M, 0.0, END_UP_M),
				"frame_m": FINISH_FRAME_M}
	elif tractor_m < full_pull_m - HEAD_ON_BEFORE_LINE_M:
		shot = {"name": "alongside", "from": Vector3(tractor_m - ALONGSIDE_BEHIND_M,
				lane_m / 2.0 + ALONGSIDE_OUT_M, ALONGSIDE_UP_M), "frame_m": ALONGSIDE_FRAME_M}
	else:
		var ahead_m := maxf(full_pull_m + HEAD_ON_PAST_LINE_M, tractor_m + HEAD_ON_AHEAD_M)
		shot = {"name": "head_on", "from": Vector3(ahead_m, HEAD_ON_LEFT_M, HEAD_ON_UP_M),
				"frame_m": HEAD_ON_FRAME_M}
	shot["at"] = at
	var from_at: Vector3 = shot["from"]
	shot["fov"] = fov_framing(float(shot["frame_m"]), from_at.distance_to(at))
	return shot


# The zoom [deg] that shows `frame_m` across at `distance_m`, within the
# camera's range.
static func fov_framing(frame_m: float, distance_m: float) -> float:
	var fov := rad_to_deg(2.0 * atan(frame_m / 2.0 / maxf(distance_m, 0.1)))
	return clampf(fov, MIN_FOV_DEG, MAX_FOV_DEG)


# Where the tractor stood when the pull ended [m down the track]; INF until
# a pull was recorded.
func stop_m() -> float:
	return _stop_m


static func launch_from(lane_m: float) -> Vector3:
	return Vector3(LAUNCH_AHEAD_M, -(lane_m / 2.0 + LAUNCH_OUT_M), LAUNCH_UP_M)
