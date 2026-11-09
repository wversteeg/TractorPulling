# The player's settings, kept on the device between launches: the control
# layout (easy or pro) and how they steer (the on-screen slider, or tilting
# the phone), whether the HUD says what to do next in a turn, and whether
# the engine is heard. A missing or unreadable file gives the defaults:
# easy, with the slider, guided, with sound.
class_name GameSettings
extends RefCounted

const SECTION := "controls"
const HUD_SECTION := "hud"
const SOUND_SECTION := "sound"

var pro := false
var tilt := false
var guidance := true
var sound := true


# The settings in the file at `path`, or the defaults.
static func read(path: String) -> GameSettings:
	var settings := GameSettings.new()
	var file := ConfigFile.new()
	if file.load(path) == OK:
		settings.pro = bool(file.get_value(SECTION, "pro", false))
		settings.tilt = bool(file.get_value(SECTION, "tilt", false))
		settings.guidance = bool(file.get_value(HUD_SECTION, "guidance", true))
		settings.sound = bool(file.get_value(SOUND_SECTION, "on", true))
	return settings


# Writes the settings to `path`; an error leaves them for this launch only.
func write(path: String) -> Error:
	var file := ConfigFile.new()
	file.set_value(SECTION, "pro", pro)
	file.set_value(SECTION, "tilt", tilt)
	file.set_value(HUD_SECTION, "guidance", guidance)
	file.set_value(SOUND_SECTION, "on", sound)
	return file.save(path)
