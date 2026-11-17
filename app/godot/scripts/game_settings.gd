# The player's settings, kept on the device between launches: the control
# layout (easy or pro) and how they steer (the on-screen slider, or tilting
# the phone), whether the HUD says what to do next in a turn, whether the
# engine is heard, which haptics the phone gives (by Haptics.KEYS), and the
# brake hold for the start: whether the game lets the brakes go itself, at
# what share of full boost, and where its button sits (DriveControls'
# HOLD_PLACES). A missing or unreadable file gives the defaults: easy, with
# the slider, guided, with sound and every haptic, the brakes let go at
# 75 % boost from the button above the right brake (above the gears in the
# easy layout).
class_name GameSettings
extends RefCounted

const SECTION := "controls"
const HUD_SECTION := "hud"
const SOUND_SECTION := "sound"
const HAPTICS_SECTION := "haptics"
const HOLD_SECTION := "brake_hold"
# The share of full boost the brakes may be set to let go at.
const HOLD_BOOST_MIN := 0.5
const HOLD_BOOST_MAX := 0.8

var pro := false
var tilt := false
var guidance := true
var sound := true
# Haptics.KEYS's names: on or off; a name missing is on.
var haptics := {}
var hold_auto_release := true
var hold_release_boost := 0.75
var hold_place := 1


# The settings in the file at `path`, or the defaults.
static func read(path: String) -> GameSettings:
	var settings := GameSettings.new()
	var file := ConfigFile.new()
	if file.load(path) == OK:
		settings.pro = bool(file.get_value(SECTION, "pro", false))
		settings.tilt = bool(file.get_value(SECTION, "tilt", false))
		settings.guidance = bool(file.get_value(HUD_SECTION, "guidance", true))
		settings.sound = bool(file.get_value(SOUND_SECTION, "on", true))
		settings.hold_auto_release = bool(file.get_value(HOLD_SECTION, "auto_release", true))
		settings.hold_release_boost = clampf(float(file.get_value(HOLD_SECTION, "release_boost",
				0.75)), HOLD_BOOST_MIN, HOLD_BOOST_MAX)
		settings.hold_place = int(file.get_value(HOLD_SECTION, "place", 1))
		if file.has_section(HAPTICS_SECTION):
			for key in file.get_section_keys(HAPTICS_SECTION):
				settings.haptics[key] = bool(file.get_value(HAPTICS_SECTION, key, true))
	return settings


# Writes the settings to `path`; an error leaves them for this launch only.
func write(path: String) -> Error:
	var file := ConfigFile.new()
	file.set_value(SECTION, "pro", pro)
	file.set_value(SECTION, "tilt", tilt)
	file.set_value(HUD_SECTION, "guidance", guidance)
	file.set_value(SOUND_SECTION, "on", sound)
	file.set_value(HOLD_SECTION, "auto_release", hold_auto_release)
	file.set_value(HOLD_SECTION, "release_boost", hold_release_boost)
	file.set_value(HOLD_SECTION, "place", hold_place)
	for key: String in haptics:
		file.set_value(HAPTICS_SECTION, key, bool(haptics[key]))
	return file.save(path)
