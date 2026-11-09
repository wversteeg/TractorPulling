# The engine's sound, made as it runs (there are no recordings): a diesel's
# firing pulses at the engine's speed, the crankshaft's lower rumble under
# them, a rough edge of noise that grows with the throttle, and the turbo's
# whistle with the boost. It speeds up and slows down with the engine,
# grows louder with the throttle and harder as the sled pulls the revs
# down, and falls silent when the engine stops.
#
# A four-stroke fires cylinders / 2 times a turn, so a six at 2500 rpm
# fires at 125 Hz. The samples come from sample_frames(), which needs no
# audio device (the tests call it); the player pushes them into an
# AudioStreamGenerator each frame. Made sample by sample in GDScript, they
# cost a phone some CPU: so the rate is low, and once the sound has faded
# out (the engine off, the menus, the sound switched off) none are made and
# the player pauses.
class_name EngineSound
extends AudioStreamPlayer

# The rate the sound is made at, and how much is kept ready ahead [Hz, s].
const MIX_RATE_HZ := 16000.0
const BUFFER_S := 0.12
# Unknown cylinders (an engine file without them) sound as a four.
const DEFAULT_CYLINDERS := 4
# How each firing pulse dies away within its cycle: larger, sharper.
const PULSE_SHARPNESS := 7.0
# The mix: the pulses, the half-order rumble (once every two turns), the
# noise and the turbo, as shares of full scale.
const PULSE_LEVEL := 0.55
const RUMBLE_LEVEL := 0.25
const NOISE_LEVEL := 0.35
const TURBO_LEVEL := 0.06
# The turbo whistles at this pitch at full boost, and this share of it
# unboosted [Hz].
const TURBO_FULL_HZ := 3200.0
const TURBO_LOW_SHARE := 0.35
# Loudness: at idle, at full throttle, and the extra when the sled pulls
# the engine down from its rated speed (lugging).
const IDLE_VOLUME := 0.25
const FULL_VOLUME := 0.8
const LUGGING_VOLUME := 0.2
# How quickly the sound follows the engine, as a time constant [s]: smooth
# between frames, no clicks.
const FOLLOW_S := 0.05
# How quickly the noise wanders: a low-passed noise is the rough edge, not
# a hiss (a share a sample).
const NOISE_SMOOTHING := 0.24
# Quieter than this, heading for silence, the sound is silent [0..1].
const SILENT := 1e-4

# Off (the settings' sound off): silent, whatever the engine does.
var enabled := true
var cylinders := DEFAULT_CYLINDERS
var idle_omega_rad_s := 80.0
var rated_omega_rad_s := 250.0

# What the engine is doing now, and what the sound has caught up with.
var _target_omega_rad_s := 0.0
var _target_throttle := 0.0
var _target_boost := 0.0
var _target_running := false
var _omega_rad_s := 0.0
var _throttle := 0.0
var _boost := 0.0
var _volume := 0.0
# Where each part of the sound is in its cycle (0 to 1), and the noise.
var _fire_phase := 0.0
var _rumble_phase := 0.0
var _turbo_phase := 0.0
var _noise := 0.0
var _random := RandomNumberGenerator.new()
var _playback: AudioStreamGeneratorPlayback = null


func _ready() -> void:
	var generator := AudioStreamGenerator.new()
	generator.mix_rate = MIX_RATE_HZ
	generator.buffer_length = BUFFER_S
	stream = generator
	_random.seed = 1
	play()
	_playback = get_stream_playback() as AudioStreamGeneratorPlayback


# The engine of this run: its cylinders (0 when not known) and its idle and
# rated speeds (TractorPullRun.get_dimensions()).
func configure(dimensions: Dictionary) -> void:
	var count: int = dimensions.get("engine_cylinders", 0)
	cylinders = count if count > 0 else DEFAULT_CYLINDERS
	idle_omega_rad_s = dimensions.get("idle_omega_rad_s", idle_omega_rad_s)
	rated_omega_rad_s = dimensions.get("rated_omega_rad_s", rated_omega_rad_s)


# The engine as the snapshot has it; the sound follows over the next
# samples.
func show_snapshot(s: Dictionary) -> void:
	_target_running = enabled and bool(s.get("engine_running", false))
	_target_omega_rad_s = float(s.get("engine_omega_rad_s", 0.0))
	_target_throttle = float(s.get("throttle", 0.0))
	_target_boost = float(s.get("boost", 0.0))


func _process(_delta: float) -> void:
	if _playback == null:
		return
	stream_paused = not making()
	if stream_paused:
		return
	var frames := _playback.get_frames_available()
	if frames > 0:
		_playback.push_buffer(sample_frames(frames))


# The firing frequency of an engine with `count` cylinders at
# `omega_rad_s` [Hz].
static func firing_hz(omega_rad_s: float, count: int) -> float:
	return omega_rad_s / TAU * float(count) / 2.0


# How loud the engine is, 0 to 1: louder with the throttle, and harder as
# the load pulls it below its rated speed; silent stopped.
func loudness(running: bool, omega_rad_s: float, throttle: float) -> float:
	if not running:
		return 0.0
	var lugging := clampf((rated_omega_rad_s - omega_rad_s)
			/ maxf(rated_omega_rad_s - idle_omega_rad_s, 1.0), 0.0, 1.0)
	return clampf(lerpf(IDLE_VOLUME, FULL_VOLUME, throttle) + LUGGING_VOLUME * lugging * throttle,
			0.0, 1.0)


# The next `count` stereo frames of the sound, both channels the same.
func sample_frames(count: int) -> PackedVector2Array:
	var out := PackedVector2Array()
	out.resize(count)
	var dt_s := 1.0 / MIX_RATE_HZ
	var follow := 1.0 - exp(-dt_s / FOLLOW_S)
	var target_volume := loudness(_target_running, _target_omega_rad_s, _target_throttle)
	for i in count:
		_omega_rad_s += (_target_omega_rad_s - _omega_rad_s) * follow
		_throttle += (_target_throttle - _throttle) * follow
		_boost += (_target_boost - _boost) * follow
		_volume += (target_volume - _volume) * follow
		_fire_phase = fmod(_fire_phase + firing_hz(_omega_rad_s, cylinders) * dt_s, 1.0)
		# Once every two turns: the four-stroke cycle.
		_rumble_phase = fmod(_rumble_phase + _omega_rad_s / TAU / 2.0 * dt_s, 1.0)
		var turbo_hz := TURBO_FULL_HZ * lerpf(TURBO_LOW_SHARE, 1.0, _boost)
		_turbo_phase = fmod(_turbo_phase + turbo_hz * dt_s, 1.0)
		_noise += (_random.randf_range(-1.0, 1.0) - _noise) * NOISE_SMOOTHING
		# Each firing a sharp knock that dies away before the next.
		var pulse := exp(-PULSE_SHARPNESS * _fire_phase) * 2.0 - 0.3
		var sample := PULSE_LEVEL * pulse \
				+ RUMBLE_LEVEL * sin(TAU * _rumble_phase) \
				+ NOISE_LEVEL * (0.3 + 0.7 * _throttle) * _noise * (0.5 + 0.5 * pulse) \
				+ TURBO_LEVEL * _boost * sin(TAU * _turbo_phase)
		sample = clampf(sample * _volume, -1.0, 1.0)
		out[i] = Vector2(sample, sample)
	return out


# How loud the sound is now, 0 to 1, for tests.
func volume() -> float:
	return _volume


# Whether samples are made: while the sound is heard or still fading out.
func making() -> bool:
	return target_volume() > 0.0 or _volume > SILENT


# How loud the sound is heading for, from the last snapshot, 0 to 1.
func target_volume() -> float:
	return loudness(_target_running, _target_omega_rad_s, _target_throttle)
