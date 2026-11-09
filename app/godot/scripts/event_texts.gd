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
# event judges it). A test pull that moves the line to 110 m is not one:
# it counts its distance.
static func reach(entry: Dictionary) -> String:
	if entry["disqualified"]:
		return "out of bounds"
	if entry["full_pull"]:
		return "FULL PULL"
	return "%.2f m" % float(entry["reached_m"])


# What the sled operator did after a test pull, or that the puller moved
# back to pull again later; "" for nothing.
static func verdict(entry: Dictionary) -> String:
	var moved: int = entry.get("moved_back", 0)
	if moved > 0:
		return "moves back %d place%s, pulls again later" % [moved, "" if moved == 1 else "s"]
	var steps: int = entry.get("steps", 0)
	match entry.get("change", "none"):
		"lighter":
			return "sled lighter, %d step%s" % [steps, "" if steps == 1 else "s"]
		"heavier":
			return "sled heavier, %d step%s" % [steps, "" if steps == 1 else "s"]
		"line_extended":
			return "line to %.0f m" % float(entry["full_pull_m"])
		"trouble":
			return "sled kept: tractor trouble?"
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
	if int(entry.get("moved_back", 0)) > 0:
		return PackedStringArray([entry["name"], what, "stopped short", verdict(entry)])
	return PackedStringArray([entry["name"], what, reach(entry), verdict(entry)])


# A row of the event's standings: place, who, the result that counts (the
# main round's, then each pull-off's) and the points the place scores in
# the championship.
static func standing_row(line: Dictionary) -> PackedStringArray:
	var result: String
	if line["disqualified"]:
		result = "out of bounds"
	elif line["full_pull"]:
		result = "full pull"
	else:
		result = "%.2f m" % float(line["distance_m"])
	var rounds: PackedFloat64Array = line.get("pull_off_distances_m", PackedFloat64Array())
	for i in rounds.size():
		result += ", pull-off %s%.2f m" % ["" if rounds.size() == 1 else "%d: " % (i + 1),
				rounds[i]]
	return PackedStringArray(["%d." % int(line["place"]), line["name"], result,
			points_text(int(line.get("points", 0)))])


# "1st", "2nd", "3rd", "11th", "22nd".
static func ordinal(n: int) -> String:
	var suffix := "th"
	if n % 100 < 11 or n % 100 > 13:
		match n % 10:
			1:
				suffix = "st"
			2:
				suffix = "nd"
			3:
				suffix = "rd"
	return "%d%s" % [n, suffix]


# "1 point", "17 points".
static func points_text(points: int) -> String:
	return "%d point%s" % [points, "" if points == 1 else "s"]


# The rows of the event's log (one per entry, as log_row()) to show in
# bold: each puller's pull that counts in the main round (their last pull,
# or the test pull they kept), the best `places` of them (the pull-off's
# places, TractorPullEvent.get_event()); and the best pull of the pull-off
# round under way, the one to beat.
static func leading_rows(log: Array, places: int) -> PackedInt32Array:
	var counting := {}
	var pull_off_round := 0
	var best_pull_off := -1
	for i in log.size():
		var entry: Dictionary = log[i]
		if entry.has("deciding") or int(entry.get("moved_back", 0)) > 0:
			continue
		if entry["kind"] == "pull_off":
			var round: int = entry["pull_off_round"]
			if round != pull_off_round:
				pull_off_round = round
				best_pull_off = -1
			if best_pull_off < 0 or _distance(entry) > _distance(log[best_pull_off]):
				best_pull_off = i
			continue
		counting[int(entry["puller"])] = i
	var rows: Array = counting.values()
	rows.sort_custom(func(a: int, b: int) -> bool:
		return _distance(log[a]) > _distance(log[b]))
	var out := PackedInt32Array(rows.slice(0, places))
	if best_pull_off >= 0:
		out.append(best_pull_off)
	return out


static func _distance(entry: Dictionary) -> float:
	return 0.0 if entry["disqualified"] else float(entry["reached_m"])


# The event's results, a row for every puller in the standings
# (TractorPullEvent.get_standings()), best first.
static func standing_rows(standings: Array) -> Array[PackedStringArray]:
	var rows: Array[PackedStringArray] = []
	for line: Dictionary in standings:
		rows.append(standing_row(line))
	return rows


# The championship, a row for every puller (get_season_standings()).
static func season_rows(standings: Array) -> Array[PackedStringArray]:
	var rows: Array[PackedStringArray] = []
	for line: Dictionary in standings:
		rows.append(season_row(line))
	return rows


# Which of those rows are the player's (puller 0), to highlight.
static func player_rows(standings: Array) -> PackedInt32Array:
	var rows := PackedInt32Array()
	for i in standings.size():
		if int(standings[i]["puller"]) == 0:
			rows.append(i)
	return rows


# A row of the championship: place, who, points and wins.
static func season_row(line: Dictionary) -> PackedStringArray:
	var wins: int = line["wins"]
	return PackedStringArray(["%d." % int(line["place"]), line["name"],
			points_text(int(line["points"])),
			"" if wins == 0 else "%d win%s" % [wins, "" if wins == 1 else "s"]])
