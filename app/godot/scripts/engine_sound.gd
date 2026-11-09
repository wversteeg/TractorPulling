# The engine's sound, made as it runs (there are no recordings), after how
# a diesel sounds: each cylinder's firing is a short burst of pressure
# into the exhaust, which rings it at its own pitch, a thump rather than a
# click; no two firings quite alike (strength and timing vary a little, as
# a real engine's cylinders do); the injection's hard knock, a clatter that
# grows with the throttle; all through a muffler, more open at full
# throttle; and the turbo's air and a faint whistle with the boost. It
# speeds up and slows down with the engine, grows louder with the throttle
# and harder as the sled pulls the revs down, and falls silent when the
# engine stops. Soft saturation, never hard clipping, keeps it from
# crackling when it is loudest.
#
# A four-stroke fires cylinders / 2 times a turn, so a six at 2500 rpm
# fires at 125 Hz. The samples come from sample_frames(), which needs no
# audio device (the tests call it); the player pushes them into an
# AudioStreamGenerator each frame. Made sample by sample in GDScript, they
# cost a phone some CPU: so the rate is low, the filters cheap (their
# coefficients set once a frame), and once the sound has faded out (the
# engine off, the menus, the sound switched off) none are made and the
# player pauses.
class_name EngineSound
extends AudioStreamPlayer

# The rate the sound is made at, and how much is kept ready ahead [Hz, s].
const MIX_RATE_HZ := 16000.0
const BUFFER_S := 0.12
# Unknown cylinders (an engine file without them) sound as a four.
const DEFAULT_CYLINDERS := 4
# A firing's burst dies away over this share of its cycle (a time constant).
const BURST_SHARE := 0.45
# The burst's pressure is mostly its own (the same each firing), with this
# share of turbulence in it.
const BURST_ROUGHNESS := 0.35
# Cylinder to cylinder: each firing's strength and the gap before the next
# vary by up to these shares.
const FIRING_STRENGTH_SPREAD := 0.25
const FIRING_TIMING_SPREAD := 0.03
# The exhaust rings at this many times the firing rate, kept within this
# range, and this sharply (Q) [Hz].
const EXHAUST_PITCH_PER_FIRING := 1.5
const EXHAUST_LOW_HZ := 55.0
const EXHAUST_HIGH_HZ := 260.0
const EXHAUST_Q := 2.2
# The exhaust's bark: a higher resonance, its pitch per firing, its range
# and sharpness. A phone's speaker barely plays the low ring; this is what
# it carries [Hz].
const BARK_PITCH_PER_FIRING := 4.0
const BARK_LOW_HZ := 250.0
const BARK_HIGH_HZ := 900.0
const BARK_Q := 3.0
# The mix, as shares of full scale: the exhaust's ring, the burst's own
# low thud under it, the injection's clatter, and the turbo's air and
# whistle (subtle: heard more than noticed).
const EXHAUST_LEVEL := 1.8
const BARK_LEVEL := 1.2
const THUD_LEVEL := 0.35
const CLATTER_LEVEL := 0.07
const TURBO_AIR_LEVEL := 0.025
const TURBO_WHISTLE_LEVEL := 0.015
# The clatter is the burst's first moments, high-passed: its own, faster
# decay (a share of the burst's per sample, as a power).
const CLATTER_SHARPNESS := 4.0
# The muffler: a low-pass, closed at idle and opening with the throttle
# [Hz].
const MUFFLER_IDLE_HZ := 700.0
const MUFFLER_OPEN_HZ := 1800.0
# The turbo whistles at this pitch at full boost, and this share of it
# unboosted [Hz]; its air is noise around that.
const TURBO_FULL_HZ := 2600.0
const TURBO_LOW_SHARE := 0.4
# Loudness: at idle, at full throttle, and the extra when the sled pulls
# the engine down from its rated speed (lugging).
const IDLE_VOLUME := 0.3
const FULL_VOLUME := 0.85
const LUGGING_VOLUME := 0.15
# How quickly the sound follows the engine, as a time constant [s]: smooth
# between frames, no clicks.
const FOLLOW_S := 0.05
# The output's DC blocker (a one-pole high-pass): the thud's pressure
# pulses push one way only, and a speaker should not be held out.
const DC_BLOCK := 0.995
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
# Where the firing cycle and the turbo's whistle are (0 to 1); the gap
# before the next firing (a share of the cycle, varied per firing).
var _fire_phase := 0.0
var _turbo_phase := 0.0
var _next_firing := 1.0
# The burst now: its envelope, and its strength.
var _burst := 0.0
var _clatter := 0.0
# The filters' memories: the exhaust's resonator (two in, two out), the
# muffler, the clatter's high-pass and the turbo's air.
var _ex_x1 := 0.0
var _ex_x2 := 0.0
var _ex_y1 := 0.0
var _ex_y2 := 0.0
var _bk_x1 := 0.0
var _bk_x2 := 0.0
var _bk_y1 := 0.0
var _bk_y2 := 0.0
var _muffled := 0.0
var _last_noise := 0.0
var _air := 0.0
var _dc_in := 0.0
var _dc_out := 0.0
var _random := RandomNumberGenerator.new()
var _playback: AudioStreamGeneratorPlayback = null


func _ready() -> void:
	# The web export plays sounds as samples made ahead, and a generated
	# stream cannot be one (without threads, as the game is built for the
	# web): silent there, see docs/future_improvements.md.
	if OS.has_feature("web"):
		return
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
	# The filters follow the engine once a frame: it hardly changes within
	# one, and the coefficients are what cost.
	var firing_rate_hz := maxf(firing_hz(_target_omega_rad_s, cylinders), 1.0)
	var burst_decay := exp(-1.0 / (BURST_SHARE / firing_rate_hz * MIX_RATE_HZ))
	var clatter_decay := pow(burst_decay, CLATTER_SHARPNESS)
	var exhaust := _resonator(clampf(EXHAUST_PITCH_PER_FIRING * firing_rate_hz,
			EXHAUST_LOW_HZ, EXHAUST_HIGH_HZ), EXHAUST_Q)
	var muffler := 1.0 - exp(-TAU * lerpf(MUFFLER_IDLE_HZ, MUFFLER_OPEN_HZ, _target_throttle)
			/ MIX_RATE_HZ)
	var air := 1.0 - exp(-TAU * TURBO_FULL_HZ / MIX_RATE_HZ)
	var b0: float = exhaust[0]
	var a1: float = exhaust[1]
	var a2: float = exhaust[2]
	var bark_filter := _resonator(clampf(BARK_PITCH_PER_FIRING * firing_rate_hz,
			BARK_LOW_HZ, BARK_HIGH_HZ), BARK_Q)
	var c0: float = bark_filter[0]
	var c1: float = bark_filter[1]
	var c2: float = bark_filter[2]
	for i in count:
		_omega_rad_s += (_target_omega_rad_s - _omega_rad_s) * follow
		_throttle += (_target_throttle - _throttle) * follow
		_boost += (_target_boost - _boost) * follow
		_volume += (target_volume - _volume) * follow
		_fire_phase += firing_hz(_omega_rad_s, cylinders) * dt_s
		if _fire_phase >= _next_firing:
			# A firing: its strength and the gap to the next vary a little.
			_fire_phase -= _next_firing
			_next_firing = 1.0 + _random.randf_range(-FIRING_TIMING_SPREAD, FIRING_TIMING_SPREAD)
			_burst = 1.0 + _random.randf_range(-FIRING_STRENGTH_SPREAD, FIRING_STRENGTH_SPREAD)
			_clatter = _burst
		_burst *= burst_decay
		_clatter *= clatter_decay
		var noise := _random.randf_range(-1.0, 1.0)
		# The burst rings the exhaust (a band-pass resonator) and thuds on
		# its own beneath it.
		var excite := _burst * (1.0 + BURST_ROUGHNESS * noise)
		var ring := b0 * (excite - _ex_x2) - a1 * _ex_y1 - a2 * _ex_y2
		_ex_x2 = _ex_x1
		_ex_x1 = excite
		_ex_y2 = _ex_y1
		_ex_y1 = ring
		var bark := c0 * (excite - _bk_x2) - c1 * _bk_y1 - c2 * _bk_y2
		_bk_x2 = _bk_x1
		_bk_x1 = excite
		_bk_y2 = _bk_y1
		_bk_y1 = bark
		var knock := (noise - _last_noise) * _clatter * (0.3 + 0.7 * _throttle)
		_last_noise = noise
		var engine := EXHAUST_LEVEL * ring + BARK_LEVEL * bark + THUD_LEVEL * _burst * _burst + CLATTER_LEVEL * knock
		_muffled += (engine - _muffled) * muffler
		# The turbo: air rushing (noise smoothed near its pitch) and a
		# faint whistle, both with the boost.
		var whistle_hz := TURBO_FULL_HZ * lerpf(TURBO_LOW_SHARE, 1.0, _boost)
		_turbo_phase = fmod(_turbo_phase + whistle_hz * dt_s, 1.0)
		_air += (noise - _air) * air
		var turbo := _boost * (TURBO_AIR_LEVEL * _air
				+ TURBO_WHISTLE_LEVEL * _boost * sin(TAU * _turbo_phase))
		var mixed := _muffled + turbo
		_dc_out = mixed - _dc_in + DC_BLOCK * _dc_out
		_dc_in = mixed
		var sample := _dc_out * _volume
		# Soft saturation: loud stays round, never clips.
		sample = sample / (1.0 + absf(sample))
		out[i] = Vector2(sample, sample)
	return out


# A band-pass resonator at `pitch_hz`, as sharp as `q` (RBJ's, constant
# 0 dB peak): its b0 (b1 is 0, b2 is -b0) and its a1 and a2, normalised by
# a0.
static func _resonator(pitch_hz: float, q: float) -> PackedFloat64Array:
	var w := TAU * pitch_hz / MIX_RATE_HZ
	var alpha := sin(w) / (2.0 * q)
	var a0 := 1.0 + alpha
	return PackedFloat64Array([alpha / a0, -2.0 * cos(w) / a0, (1.0 - alpha) / a0])


# How loud the sound is now, 0 to 1, for tests.
func volume() -> float:
	return _volume


# Whether samples are made: while the sound is heard or still fading out.
func making() -> bool:
	return target_volume() > 0.0 or _volume > SILENT


# How loud the sound is heading for, from the last snapshot, 0 to 1.
func target_volume() -> float:
	return loudness(_target_running, _target_omega_rad_s, _target_throttle)
