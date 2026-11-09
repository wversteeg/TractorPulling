# Which build of the game this is, from res://build_info.cfg:
# scripts/godot_export.sh writes it as it exports, from git. The build
# number counts the commits along develop's first parents, so each merge to
# develop adds one; the commit names the exact source. A game run from the
# project itself (the editor, the tests) has no such file: a development
# build.
class_name BuildInfo
extends RefCounted

const PATH := "res://build_info.cfg"
const SECTION := "build"


# "Build 86 · 8206bca", or "Development build" without the file (or with
# one that does not read) at `path`.
static func describe(path: String = PATH) -> String:
	var file := ConfigFile.new()
	if file.load(path) != OK:
		return "Development build"
	var number := int(file.get_value(SECTION, "number", 0))
	var commit := str(file.get_value(SECTION, "commit", ""))
	if number <= 0:
		return "Development build"
	return "Build %d" % number if commit.is_empty() else "Build %d · %s" % [number, commit]
