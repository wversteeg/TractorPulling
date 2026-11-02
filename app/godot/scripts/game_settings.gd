# The player's settings, kept on the device between launches: the control
# layout (easy or pro) and how they steer (the on-screen slider, or tilting
# the phone), and whether the HUD says what to do next in a turn. A missing
# or unreadable file gives the defaults: easy, with the slider, guided.
class_name GameSettings
extends RefCounted

const SECTION := "controls"
const HUD_SECTION := "hud"

var pro := false
var tilt := false
var guidance := true


# The settings in the file at `path`, or the defaults.
static func read(path: String) -> GameSettings:
	var settings := GameSettings.new()
	var file := ConfigFile.new()
	if file.load(path) == OK:
		settings.pro = bool(file.get_value(SECTION, "pro", false))
		settings.tilt = bool(file.get_value(SECTION, "tilt", false))
		settings.guidance = bool(file.get_value(HUD_SECTION, "guidance", true))
	return settings


# Writes the settings to `path`; an error leaves them for this launch only.
func write(path: String) -> Error:
	var file := ConfigFile.new()
	file.set_value(SECTION, "pro", pro)
	file.set_value(SECTION, "tilt", tilt)
	file.set_value(HUD_SECTION, "guidance", guidance)
	return file.save(path)
