# How the event and the season read: the rows of the boards and the lines
# about a pull, from TractorPullEvent's log entries and standings.
class_name EventTexts
extends RefCounted

const KINDS := {
	"test": "test pull",
	"competition": "pull",
	"repull": "re-pull",
	"pull_off": "pull-off",
}


# What kind of pull the player pulls next, from TractorPullEvent.get_event().
static func pull_name(kind: String, pull_off_round: int) -> String:
	if kind == "pull_off":
		return "Pull-off %d" % pull_off_round
	var text := String(KINDS.get(kind, kind))
	return text.left(1).to_upper() + text.substr(1)


# Where a pull got: past the line in force before it, a full pull (the
# event judges it: a test pull that moves the line to 110 m is one too).
static func reach(entry: Dictionary) -> String:
	if entry["disqualified"]:
		return "out of bounds"
	if entry["full_pull"]:
		return "FULL PULL"
	return "%.2f m" % float(entry["reached_m"])


# What the sled operator did after a test pull; "" for nothing.
static func verdict(entry: Dictionary) -> String:
	var steps: int = entry.get("steps", 0)
	match entry.get("change", "none"):
		"lighter":
			return "sled lighter, %d step%s" % [steps, "" if steps == 1 else "s"]
		"heavier":
			return "sled heavier, %d step%s" % [steps, "" if steps == 1 else "s"]
		"line_extended":
			return "line to %.0f m" % float(entry["full_pull_m"])
	return ""


# A row of the event board for a log entry: who, what, how far, and the
# operator's verdict or the decision.
static func log_row(entry: Dictionary) -> PackedStringArray:
	if entry.has("deciding"):
		var keeps: bool = entry["keep"]
		return PackedStringArray([entry["name"], "decides",
				"keeps the test pull" if keeps else "pulls again", ""])
	var kind: String = entry["kind"]
	var what := pull_name(kind, entry["pull_off_round"]).to_lower()
	return PackedStringArray([entry["name"], what, reach(entry), verdict(entry)])


# A row of the event's standings: place, who, and the result that counts.
static func standing_row(line: Dictionary) -> PackedStringArray:
	var result: String
	if line["disqualified"]:
		result = "out of bounds"
	elif int(line["pull_off_rounds"]) > 0:
		result = "full pull, pull-off %.2f m" % float(line["pull_off_distance_m"])
	elif line["full_pull"]:
		result = "full pull"
	else:
		result = "%.2f m" % float(line["distance_m"])
	return PackedStringArray(["%d." % int(line["place"]), line["name"], result])


# A row of the championship: place, who, points and wins.
static func season_row(line: Dictionary) -> PackedStringArray:
	var wins: int = line["wins"]
	return PackedStringArray(["%d." % int(line["place"]), line["name"],
			"%d points" % int(line["points"]),
			"" if wins == 0 else "%d win%s" % [wins, "" if wins == 1 else "s"]])
