# How a class's tractors are painted, so the classes look apart at a
# glance: the Standard class's farm tractor in red with a white cab roof,
# the Sport 2.5 t in blue with a white top, the Sport 4.5 t in black and
# gold. The player's tractor wears its class's scheme; the rivals keep the
# class's trim (the top, the rims) in their own paint and numbers.
# The shapes (a cab or a roll bar) come from the tractor's data, not from
# here.
class_name TractorLook
extends RefCounted

# The player's start number, and the rivals' after it.
const PLAYER_NUMBER := "7"
const RIVAL_NUMBERS: Array[String] = ["12", "23", "31", "44"]
# The rivals' paints and their drivers' shirts; those too close to the
# class's own paint are skipped, so no rival looks like the player. No
# green: with the Sport 4.5 t's gold trim it would pass for a maker's
# colours.
const RIVAL_PAINTS: Array[Color] = [Color(0.9, 0.45, 0.1), Color(0.4, 0.16, 0.52),
		Color(0.08, 0.45, 0.45), Color(0.16, 0.3, 0.62), Color(0.72, 0.1, 0.08)]
const RIVAL_SHIRTS: Array[Color] = [Color(0.85, 0.8, 0.2), Color(0.8, 0.8, 0.82),
		Color(0.2, 0.2, 0.22), Color(0.3, 0.55, 0.3)]
# Closer than this to the class's paint (RGB distance), a rival's paint
# would pass for the player's.
const TOO_CLOSE := 0.3
# The darker shade of a paint: louvres and shadowed panels.
const DARKER := 0.4

# The look of a farm tractor (a cab) and of a pulling tractor (a roll bar)
# when the class is not known, as on a scripted run.
const FARM_CLASS := "Standard 3.5 t"
const PULLING_CLASS := "Sport 2.5 t"

var name := ""
# The body: the hood or bonnet, the fenders; its darker shade.
var paint := Color(0.72, 0.1, 0.08)
var paint_dark := Color(0.45, 0.06, 0.05)
# The trim: a pulling bonnet's top, a farm cab's roof and the hood stripe.
var trim := Color(0.95, 0.95, 0.93)
# The rims; a pulling tractor's rim discs and the ring round them.
var rim := Color(0.93, 0.88, 0.72)
var disc := Color(0.25, 0.26, 0.28)
var ring := Color(0.95, 0.95, 0.93)
var number := PLAYER_NUMBER
var driver_shirt := Color(0.16, 0.26, 0.52)


# The schemes by the class's name, as the career names it.
static func schemes() -> Dictionary:
	var standard := TractorLook.new()
	standard.name = "Standard 3.5 t"
	var sport_2500 := TractorLook.new()
	sport_2500.name = "Sport 2.5 t"
	sport_2500.paint = Color(0.08, 0.3, 0.72)
	sport_2500.paint_dark = sport_2500.paint.darkened(DARKER)
	sport_2500.rim = Color(0.92, 0.92, 0.94)
	sport_2500.driver_shirt = Color(0.88, 0.88, 0.9)
	var sport_4500 := TractorLook.new()
	sport_4500.name = "Sport 4.5 t"
	sport_4500.paint = Color(0.07, 0.07, 0.08)
	sport_4500.paint_dark = Color(0.2, 0.2, 0.22)
	sport_4500.trim = Color(0.92, 0.7, 0.15)
	sport_4500.rim = Color(0.92, 0.7, 0.15)
	sport_4500.disc = Color(0.08, 0.08, 0.09)
	sport_4500.ring = sport_4500.trim
	sport_4500.driver_shirt = Color(0.8, 0.12, 0.1)
	return {standard.name: standard, sport_2500.name: sport_2500, sport_4500.name: sport_4500}


# The class's look; for a class without its own (or none named), a farm or
# pulling tractor's by `protection` ("cab" or "roll_bar").
static func for_class(class_label: String, protection: String) -> TractorLook:
	var all := schemes()
	if all.has(class_label):
		return all[class_label]
	return all[PULLING_CLASS if protection == "roll_bar" else FARM_CLASS]


# The `i`th rival's look in this class: the class's trim in the rival's
# own paint, shirt and number.
func rival(i: int) -> TractorLook:
	var paints: Array[Color] = []
	for colour: Color in RIVAL_PAINTS:
		if _distance(colour, paint) >= TOO_CLOSE:
			paints.append(colour)
	var other := TractorLook.new()
	other.name = name
	other.paint = paints[i % paints.size()]
	other.paint_dark = other.paint.darkened(DARKER)
	other.trim = trim
	other.rim = rim
	other.disc = disc
	other.ring = ring
	other.number = RIVAL_NUMBERS[i % RIVAL_NUMBERS.size()]
	other.driver_shirt = RIVAL_SHIRTS[i % RIVAL_SHIRTS.size()]
	return other


static func _distance(a: Color, b: Color) -> float:
	return Vector3(a.r - b.r, a.g - b.g, a.b - b.b).length()
