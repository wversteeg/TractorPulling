# The tractor models a dealer sells (the garage's dealer: data/garages/*,
# by their `style`), as TractorView draws them. For now they only look
# different: each class's tractor pulls the same whatever its model (a
# later version may give them their own engines or weights).
#
# A style is a set of choices on the class's tractor:
# - body: "farm" (a farm tractor's square hood) or "pulling" (a pulling
#   tractor's bonnet over a frame out to a bumper);
# - hood: the farm hood's shape, "square", "round" (an older one, rounded
#   all over) or "sloped" (a modern one, falling to the nose, lamp strips
#   in the grille); the pulling bonnet's, "round" or "square";
# - hood_height: the hood's or bonnet's height, a share of the usual;
# - cab: "cab" (closed, glass all round), "canopy" (a two-post frame with a
#   roof), "cage" (four posts and a roof, no glass), "roll_bar" or "open"
#   (a two-post frame alone);
# - cab_height: the cab's or frame's height, a share of the usual;
# - roof: a cab's or cage's roof, "flat" or "rounded";
# - stack: the exhaust, "single", "twin" (one each side of the hood),
#   "side" (up the cab's front corner) or "tall";
# - fenders: "round" (an arc of panels), "flat" (a flat top over the wheel,
#   a modern tractor's) or "wide" (an arc wider than the tire);
# - beacon, roof_lamps, grille_lamps, chrome: extras.
class_name TractorModels
extends RefCounted

const FARM := "farm"
const PULLING := "pulling"

# Every style, by its id.
const STYLES := {
	# Farm tractors: the Standard 3.5 t.
	"farm_classic": {"body": FARM, "hood": "square", "hood_height": 1.0, "cab": "cab",
			"cab_height": 1.0, "roof": "flat", "stack": "single", "fenders": "round",
			"beacon": true, "roof_lamps": true, "grille_lamps": false, "chrome": false},
	"farm_vintage": {"body": FARM, "hood": "round", "hood_height": 0.95, "cab": "open",
			"cab_height": 0.95, "roof": "flat", "stack": "tall", "fenders": "round",
			"beacon": false, "roof_lamps": false, "grille_lamps": true, "chrome": true},
	"farm_low": {"body": FARM, "hood": "square", "hood_height": 0.76, "cab": "cab",
			"cab_height": 0.82, "roof": "flat", "stack": "side", "fenders": "flat",
			"beacon": false, "roof_lamps": true, "grille_lamps": false, "chrome": false},
	"farm_high": {"body": FARM, "hood": "square", "hood_height": 1.18, "cab": "cab",
			"cab_height": 1.12, "roof": "rounded", "stack": "twin", "fenders": "wide",
			"beacon": true, "roof_lamps": true, "grille_lamps": false, "chrome": true},
	"farm_modern": {"body": FARM, "hood": "sloped", "hood_height": 1.05, "cab": "cab",
			"cab_height": 1.05, "roof": "rounded", "stack": "side", "fenders": "flat",
			"beacon": true, "roof_lamps": true, "grille_lamps": true, "chrome": false},
	# Pulling tractors: the Sport classes.
	"pull_classic": {"body": PULLING, "hood": "round", "hood_height": 1.0, "cab": "roll_bar",
			"cab_height": 1.0, "roof": "flat", "stack": "single", "fenders": "round",
			"beacon": false, "roof_lamps": false, "grille_lamps": true, "chrome": true},
	"pull_low": {"body": PULLING, "hood": "round", "hood_height": 0.84, "cab": "roll_bar",
			"cab_height": 0.9, "roof": "flat", "stack": "tall", "fenders": "round",
			"beacon": false, "roof_lamps": false, "grille_lamps": true, "chrome": true},
	"pull_high": {"body": PULLING, "hood": "square", "hood_height": 1.16, "cab": "roll_bar",
			"cab_height": 1.08, "roof": "flat", "stack": "twin", "fenders": "wide",
			"beacon": false, "roof_lamps": false, "grille_lamps": true, "chrome": true},
	"pull_cage": {"body": PULLING, "hood": "round", "hood_height": 1.0, "cab": "cage",
			"cab_height": 1.0, "roof": "rounded", "stack": "single", "fenders": "wide",
			"beacon": false, "roof_lamps": true, "grille_lamps": true, "chrome": false},
	"pull_farm": {"body": FARM, "hood": "square", "hood_height": 1.0, "cab": "roll_bar",
			"cab_height": 1.0, "roof": "flat", "stack": "tall", "fenders": "round",
			"beacon": false, "roof_lamps": false, "grille_lamps": false, "chrome": true},
	"pull_cab": {"body": PULLING, "hood": "square", "hood_height": 1.05, "cab": "cab",
			"cab_height": 0.95, "roof": "rounded", "stack": "twin", "fenders": "flat",
			"beacon": true, "roof_lamps": true, "grille_lamps": true, "chrome": false},
}


# The style `id`, or the class's usual one when there is none by that name:
# a farm tractor behind a cab, a pulling tractor behind a roll bar
# (`protection`, from the tractor's data).
static func style(id: String, protection: String) -> Dictionary:
	if STYLES.has(id):
		return STYLES[id]
	return STYLES["pull_classic" if protection == "roll_bar" else "farm_classic"]
