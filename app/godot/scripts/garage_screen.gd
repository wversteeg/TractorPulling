# The garage, from the start screen, in three views:
#
# - The workshop: the tractor itself is the menu (TractorSketch). Each part
#   has a pin on it (PartPin): its icon in a ring that fills as the part is
#   built up, and a € badge when its next level is affordable. A side panel
#   gives the tractor's power and how much of the garage is bought.
# - A part's screen: every level on one line (LevelLine), the next one in a
#   card with its price and the button that buys it, and for the engine a
#   dyno chart of what the next level adds (DynoChart).
# - A choice's screen: the alternatives side by side, the fitted one picked
#   out; an owned one fits for nothing, one not owned shows its price.
#
# The game asks the extension (TractorPullEvent.get_garage()), which holds
# the money and the class's rules. During an event, once the player has
# pulled, the tractor stays as it is: the prices show, their buttons greyed.
class_name GarageScreen
extends Control

signal buy_requested(index: int)
# Alternative `alternative` of choice `choice`: to buy (and fit) or to fit.
signal choice_requested(choice: int, alternative: int)
signal done

enum View { WORKSHOP, UPGRADE, CHOICE }

const STAGE := Vector2(1280.0, 720.0)
const BACKGROUND := Color(0.06, 0.07, 0.09, 0.985)
const TOP_BAR_HEIGHT := 64.0
const TITLE_SIZE := 26
const MONEY_SIZE := 24
const LINE_SIZE := 15
const BIG_SIZE := 28
const CORNER := 14
# Where each part's pin sits on the workshop's stage (its top-left), round
# the tractor; a second pin for the same part goes beside the first.
const PIN_AT := {
	"rear_traction": Vector2(225, 185),
	"load_downshift_gears": Vector2(390, 112),
	"engine": Vector2(555, 82),
	"engine_torque": Vector2(720, 112),
	"rated_rpm": Vector2(875, 170),
	"ballast_kg": Vector2(865, 560),
	"hitch_height_m": Vector2(40, 420),
	"rear_tire": Vector2(40, 250),
	"driveline": Vector2(380, 560),
}
const SKETCH_RECT := Rect2(150, 250, 830, 340)
# What each part does, for its screen when there is no dyno to show.
const ABOUT := {
	"engine_torque": "More fuel and air: the engine's torque all through its range.",
	"rated_rpm": "The governor: full fuel up to higher revs, so the same gear pulls faster.",
	"rear_traction": "Grip: from worn farm tires to cut puller tires that bite as they spin.",
	"ballast_kg": "Weights up to the class's limit. Where they sit you set before each pull.",
	"hitch_height_m": "The pulling hitch's top hole: higher puts more of the sled's pull on the rear tires, and lifts the front sooner.",
	"load_downshift_gears": "Shift down under load, the clutch in: it saves a pull that bogs at the end.",
}

# For the engine's dyno charts; without it the part screens show text only.
var event: TractorPullEvent
var view := View.WORKSHOP
# The upgrade or choice the view shows.
var view_index := 0

var done_button: TouchButton
var back_button: TouchButton
var buy_button: TouchButton
# The workshop's pins, in the garage's order.
var upgrade_pins: Array[PartPin] = []
var choice_pins: Array[PartPin] = []
# The choice screen's buttons, one per alternative.
var alternative_buttons: Array[TouchButton] = []

var _garage: Dictionary = {}
var _message := ""
var _stage := Control.new()
var _workshop := Control.new()
var _part := Control.new()
var _choice := Control.new()
var _title: Label
var _subtitle: Label
var _money: Label
var _status: Label
var _icon := Control.new()
var _icon_part := ""
var _sketch := TractorSketch.new()
var _power: Label
var _bought: Label
var _bought_bar := ColorRect.new()
var _dyno := DynoChart.new()
var _dyno_legend: Label
var _about: Label
var _now: Label
var _next_heading: Label
var _next_title: Label
var _next_note: Label
var _next_icon := Control.new()
var _line := LevelLine.new()


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var background := ColorRect.new()
	background.color = BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	_stage.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stage.set_anchors_preset(Control.PRESET_CENTER)
	_stage.offset_left = -STAGE.x / 2.0
	_stage.offset_right = STAGE.x / 2.0
	_stage.offset_top = -STAGE.y / 2.0
	_stage.offset_bottom = STAGE.y / 2.0
	add_child(_stage)
	for control: Control in [_workshop, _part, _choice]:
		control.mouse_filter = Control.MOUSE_FILTER_IGNORE
		control.size = STAGE
		_stage.add_child(control)
	_build_top_bar()
	_build_workshop()
	_build_part()
	_status = _label(_stage, Rect2(28, 676, 1000, 30), LINE_SIZE, GarageArt.DIM)


# `garage` is TractorPullEvent.get_garage(); `message` what the last
# purchase said, if anything. The view stays where it is.
func show_garage(garage: Dictionary, message: String = "") -> void:
	_garage = garage
	_message = message
	_money.text = euros(int(garage.get("money_eur", 0)))
	_show_view()


func show_workshop() -> void:
	view = View.WORKSHOP
	_message = ""
	_show_view()


func open_upgrade(index: int) -> void:
	view = View.UPGRADE
	view_index = index
	_message = ""
	_show_view()


func open_choice(index: int) -> void:
	view = View.CHOICE
	view_index = index
	_message = ""
	_show_view()


# Lets go of any finger on the buttons, as when the screen is hidden.
func release_buttons() -> void:
	for button in find_children("*", "TouchButton", true, false):
		(button as TouchButton).release()


# Whole euros, thousands apart: "€ 1,250".
static func euros(amount: int) -> String:
	var digits := str(absi(amount))
	var grouped := ""
	while digits.length() > 3:
		grouped = "," + digits.right(3) + grouped
		digits = digits.left(digits.length() - 3)
	return "%s€ %s%s" % ["-" if amount < 0 else "", digits, grouped]


func _show_view() -> void:
	_workshop.visible = view == View.WORKSHOP
	_part.visible = view == View.UPGRADE
	_choice.visible = view == View.CHOICE
	back_button.visible = view != View.WORKSHOP
	done_button.visible = view == View.WORKSHOP
	_title.position.x = 28.0 if view == View.WORKSHOP else 128.0
	match view:
		View.WORKSHOP:
			_show_workshop()
		View.UPGRADE:
			_show_upgrade()
		View.CHOICE:
			_show_choice()
	_icon.queue_redraw()
	if not _message.is_empty():
		_status.text = _message
	elif not _garage.get("can_buy", false):
		_status.text = "The tractor stays as it is until this event is over."
	elif _garage.get("fully_upgraded", false):
		_status.text = "Fully built: as far as the class allows."
	else:
		_status.text = "Events pay by place. Every part stays within the class's rules."


# ---------------------------------------------------------------- building


func _build_top_bar() -> void:
	var bar := _panel(_stage, Rect2(0, 0, STAGE.x, TOP_BAR_HEIGHT), Color(0.04, 0.05, 0.07, 0.9), 0)
	back_button = _button(_stage, "‹", Rect2(24, 10, 46, 44), 30)
	back_button.tapped.connect(show_workshop)
	_icon.position = Vector2(80, 14)
	_icon.size = Vector2(36, 36)
	_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon.draw.connect(func() -> void:
		if not _icon_part.is_empty():
			GarageArt.draw_icon(_icon, _icon_part, _icon.size / 2.0, 32.0, GarageArt.ORANGE))
	_stage.add_child(_icon)
	_title = _label(_stage, Rect2(128, 12, 520, 40), TITLE_SIZE, GarageArt.TEXT)
	_subtitle = _label(_stage, Rect2(128, 40, 700, 22), 13, GarageArt.DIM)
	_money = _label(_stage, Rect2(880, 12, 220, 40), MONEY_SIZE, GarageArt.ORANGE_LIGHT)
	_money.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	done_button = _button(_stage, "Done", Rect2(1120, 8, 136, 48), 24)
	done_button.tapped.connect(func() -> void: done.emit())
	bar.z_index = -1


func _build_workshop() -> void:
	_workshop.draw.connect(_draw_leaders)
	_sketch.position = SKETCH_RECT.position
	_sketch.size = SKETCH_RECT.size
	_workshop.add_child(_sketch)
	var side := _panel(_workshop, Rect2(1030, 84, 222, 250), GarageArt.PANEL, CORNER)
	_label(side, Rect2(16, 12, 190, 20), 13, GarageArt.DIM).text = "YOUR TRACTOR"
	_power = _label(side, Rect2(16, 30, 200, 48), 36, GarageArt.TEXT)
	_label(side, Rect2(16, 98, 190, 20), 13, GarageArt.DIM).text = "GARAGE BOUGHT"
	_bought = _label(side, Rect2(16, 116, 200, 40), 26, GarageArt.TEXT)
	var track := ColorRect.new()
	track.color = GarageArt.LINE
	track.position = Vector2(16, 164)
	track.size = Vector2(190, 8)
	side.add_child(track)
	_bought_bar.color = GarageArt.ORANGE
	_bought_bar.position = track.position
	_bought_bar.size = Vector2(0, 8)
	side.add_child(_bought_bar)
	var hint := _label(side, Rect2(16, 182, 194, 60), 13, GarageArt.DIM)
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	hint.text = "Tap a part to see its levels. € on a part: its next level is affordable."


func _build_part() -> void:
	var left := _panel(_part, Rect2(28, 84, 610, 300), GarageArt.PANEL, CORNER)
	_dyno_legend = _label(left, Rect2(16, 10, 580, 22), 14, GarageArt.DIM)
	_dyno.position = Vector2(14, 38)
	_dyno.size = Vector2(582, 250)
	left.add_child(_dyno)
	_about = _label(left, Rect2(20, 44, 570, 120), 20, GarageArt.TEXT)
	_about.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_now = _label(left, Rect2(20, 200, 570, 80), 24, GarageArt.ORANGE_LIGHT)
	_now.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var right := _panel(_part, Rect2(660, 84, 592, 300), GarageArt.PANEL, CORNER)
	_next_heading = _label(right, Rect2(18, 12, 400, 20), 13, GarageArt.DIM)
	_next_icon.position = Vector2(18, 44)
	_next_icon.size = Vector2(110, 110)
	_next_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_next_icon.draw.connect(func() -> void:
		var box := StyleBoxFlat.new()
		box.bg_color = Color(0.2, 0.14, 0.07)
		box.border_color = GarageArt.ORANGE
		box.set_border_width_all(2)
		box.set_corner_radius_all(16)
		_next_icon.draw_style_box(box, Rect2(Vector2.ZERO, _next_icon.size))
		GarageArt.draw_icon(_next_icon, _icon_part, _next_icon.size / 2.0, 70.0,
				GarageArt.ORANGE_LIGHT))
	right.add_child(_next_icon)
	_next_title = _label(right, Rect2(146, 44, 430, 76), BIG_SIZE, GarageArt.TEXT)
	_next_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_next_note = _label(right, Rect2(146, 124, 430, 40), 15, GarageArt.DIM)
	_next_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	buy_button = _button(right, "Buy", Rect2(18, 196, 260, 70), 26)
	buy_button.selected = true
	buy_button.tapped.connect(func() -> void: buy_requested.emit(view_index))
	var line_panel := _panel(_part, Rect2(28, 404, 1224, 262), GarageArt.PANEL, CORNER)
	_label(line_panel, Rect2(16, 10, 400, 20), 13, GarageArt.DIM).text = "STEP BY STEP"
	_line.position = Vector2(0, 30)
	_line.size = Vector2(1224, 226)
	line_panel.add_child(_line)


# ---------------------------------------------------------------- views


func _show_workshop() -> void:
	_title.text = "WORKSHOP"
	_subtitle.text = ""
	_icon_part = ""
	for pin: PartPin in upgrade_pins + choice_pins:
		pin.queue_free()
	upgrade_pins.clear()
	choice_pins.clear()
	var used := {}
	var money := int(_garage.get("money_eur", 0))
	var can_buy: bool = _garage.get("can_buy", false)
	var upgrades: Array = _garage.get("upgrades", [])
	for i in upgrades.size():
		var line: Dictionary = upgrades[i]
		var pin := _pin(String(line.get("part", "")), used)
		var levels := int(line["levels"])
		var level := int(line["level"])
		var at_top := level >= levels - 1
		pin.title = String(line["name"])
		pin.share = float(level) / maxf(levels - 1.0, 1.0)
		pin.complete = at_top
		pin.affordable = can_buy and not at_top and int(line["next_cost_eur"]) <= money
		var owner := String(line.get("for", ""))
		if not line.get("counts", true):
			pin.note = "for the %s" % owner
		elif at_top:
			pin.note = "%s · complete" % _short(String(line["label"]))
		else:
			pin.note = "%d / %d · next %s" % [level + 1, levels, euros(int(line["next_cost_eur"]))]
		var index := i
		pin.tapped.connect(func() -> void: open_upgrade(index))
		upgrade_pins.append(pin)
	var choices: Array = _garage.get("choices", [])
	for c in choices.size():
		var choice: Dictionary = choices[c]
		var pin := _pin(String(choice.get("slot", "")), used)
		var alternatives: Array = choice["alternatives"]
		var fitted: Dictionary = alternatives[int(choice["fitted"])]
		pin.title = String(choice["name"])
		pin.choice = true
		var owned := 0
		for alternative: Dictionary in alternatives:
			owned += 1 if alternative["owned"] else 0
			if can_buy and not alternative["owned"] and int(alternative["cost_eur"]) <= money:
				pin.affordable = true
		pin.note = "%s · %d of %d owned" % [_short(String(fitted["label"])), owned,
				alternatives.size()]
		var index := c
		pin.tapped.connect(func() -> void: open_choice(index))
		choice_pins.append(pin)
	_power.text = "%d hp" % roundi(float(_garage.get("power_hp", 0.0)))
	var share := _bought_share(upgrades, choices)
	_bought.text = "%d %%" % roundi(share * 100.0)
	_bought_bar.size.x = 190.0 * share
	_workshop.queue_redraw()


func _show_upgrade() -> void:
	var upgrades: Array = _garage.get("upgrades", [])
	if view_index >= upgrades.size():
		show_workshop()
		return
	var line: Dictionary = upgrades[view_index]
	var part := String(line.get("part", ""))
	var levels := int(line["levels"])
	var level := int(line["level"])
	var at_top := level >= levels - 1
	var counts: bool = line.get("counts", true)
	_icon_part = part
	_title.text = String(line["name"]).to_upper()
	var owner := String(line.get("for", ""))
	_subtitle.text = "for the %s%s" % [owner, "" if counts else " (not fitted: kept)"] \
			if not owner.is_empty() else "Level %d of %d" % [level + 1, levels]
	var labels: PackedStringArray = line.get("labels", PackedStringArray())
	_line.show_levels(labels, line.get("costs", []), level, part == "rear_traction", counts)
	# The engine on the dyno; any other part described.
	var dyno := {}
	if event != null and part in ["engine_torque", "rated_rpm"]:
		dyno = event.get_upgrade_dyno(view_index)
	_dyno.visible = not dyno.is_empty()
	_about.visible = dyno.is_empty()
	_now.visible = dyno.is_empty()
	if dyno.is_empty():
		_dyno_legend.text = "WHAT IT DOES"
		_about.text = ABOUT.get(part, "")
		_now.text = "Now: %s" % line["label"]
	else:
		var now: Dictionary = dyno["now"]
		var curves: Array[Dictionary] = [{"points": now["points"], "colour": GarageArt.TEXT}]
		var legend := "DYNO   now %d hp" % roundi(float(now["hp"]))
		if dyno.has("next"):
			var next: Dictionary = dyno["next"]
			curves.append({"points": next["points"], "colour": GarageArt.ORANGE_LIGHT,
					"dashed": true})
			legend += "   - - next %d hp (%+d)" % [roundi(float(next["hp"])),
					roundi(float(next["hp"]) - float(now["hp"]))]
		_dyno_legend.text = legend
		_dyno.show_curves(curves)
	var money := int(_garage.get("money_eur", 0))
	var can_buy: bool = _garage.get("can_buy", false)
	if at_top:
		_next_heading.text = "COMPLETE"
		_next_title.text = String(line["label"])
		_next_note.text = "This part is as far as the class allows."
		buy_button.visible = false
	else:
		var cost := int(line["next_cost_eur"])
		_next_heading.text = "NEXT: LEVEL %d OF %d" % [level + 2, levels]
		_next_title.text = String(line["next_label"])
		buy_button.visible = true
		buy_button.text = "Buy %s" % euros(cost)
		buy_button.disabled = not can_buy or cost > money
		if not can_buy:
			_next_note.text = "Not during an event."
		elif cost > money:
			_next_note.text = "%s more needed." % euros(cost - money)
		else:
			_next_note.text = "%s left after." % euros(money - cost)
	_next_icon.queue_redraw()


func _show_choice() -> void:
	var choices: Array = _garage.get("choices", [])
	if view_index >= choices.size():
		show_workshop()
		return
	var choice: Dictionary = choices[view_index]
	var slot := String(choice.get("slot", ""))
	_icon_part = slot
	_title.text = String(choice["name"]).to_upper()
	_subtitle.text = "Parts you own swap for free between events"
	for child in _choice.get_children():
		child.queue_free()
	alternative_buttons.clear()
	var alternatives: Array = choice["alternatives"]
	var dynos: Array = []
	if event != null and slot == "engine":
		dynos = event.get_choice_dynos(view_index)
	var n := alternatives.size()
	var gap := 24.0
	var width := (STAGE.x - 56.0 - gap * (n - 1)) / n
	var money := int(_garage.get("money_eur", 0))
	var can_buy: bool = _garage.get("can_buy", false)
	for a in n:
		var alternative: Dictionary = alternatives[a]
		var fitted := int(choice["fitted"]) == a
		var owned: bool = alternative["owned"]
		var rect := Rect2(28 + a * (width + gap), 92, width, 560)
		var card := _panel(_choice, rect, GarageArt.PANEL, 18)
		if fitted:
			var style: StyleBoxFlat = card.get_theme_stylebox("panel")
			style.border_color = GarageArt.BLUE
			style.set_border_width_all(3)
		var tag := _label(card, Rect2(width - 150, 16, 132, 26), 14,
				GarageArt.BLUE_LIGHT if fitted else (GarageArt.GREEN if owned else GarageArt.ORANGE_LIGHT))
		tag.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		tag.text = "FITTED" if fitted else ("OWNED" if owned else euros(int(alternative["cost_eur"])))
		var art := Control.new()
		art.position = Vector2(18, 20)
		art.size = Vector2(110, 110)
		art.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var art_size := 70.0 + 14.0 * a
		var colour := GarageArt.BLUE_LIGHT if fitted else GarageArt.ORANGE_LIGHT
		art.draw.connect(func() -> void:
			GarageArt.draw_icon(art, slot, art.size / 2.0, art_size, colour))
		card.add_child(art)
		var heading := _label(card, Rect2(140, 44, width - 160, 80), 26, GarageArt.TEXT)
		heading.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		heading.text = String(alternative["label"])
		if a < dynos.size() and not (dynos[a] as Dictionary).is_empty():
			var chart := DynoChart.new()
			chart.position = Vector2(16, 150)
			chart.size = Vector2(width - 32, 220)
			card.add_child(chart)
			var curves: Array[Dictionary] = []
			for other in dynos.size():
				if other != a and not (dynos[other] as Dictionary).is_empty():
					curves.append({"points": dynos[other]["points"], "colour": GarageArt.DIM,
							"dashed": true})
			curves.append({"points": dynos[a]["points"], "colour": colour})
			chart.show_curves(curves)
			var hp := _label(card, Rect2(16, 380, width - 32, 30), 18, colour)
			hp.text = "%d hp with your build" % roundi(float(dynos[a]["hp"]))
		var button := _button(card, "", Rect2(16, 470, width - 32, 70), 24)
		if fitted:
			button.text = "Fitted"
			button.selected = true
		elif owned:
			button.text = "Fit · free"
			button.disabled = not can_buy
		else:
			button.text = "Buy %s" % euros(int(alternative["cost_eur"]))
			button.disabled = not can_buy or int(alternative["cost_eur"]) > money
		var at := Vector2i(view_index, a)
		button.tapped.connect(func() -> void: choice_requested.emit(at.x, at.y))
		alternative_buttons.append(button)


# ---------------------------------------------------------------- helpers


# A pin for `part` at its place round the tractor, beside one already
# there for the same part.
func _pin(part: String, used: Dictionary) -> PartPin:
	var pin := PartPin.new()
	pin.part = part
	var at: Vector2 = PIN_AT.get(part, Vector2(560, 600))
	var again := int(used.get(part, 0))
	used[part] = again + 1
	pin.position = at + Vector2(175.0 * again, 0.0)
	_workshop.add_child(pin)
	return pin


# The dashed lines from each pin to its part on the tractor.
func _draw_leaders() -> void:
	for pin: PartPin in upgrade_pins + choice_pins:
		var from := pin.position + pin.ring_centre()
		var to := _sketch.position + _sketch.anchor(pin.part)
		var direction := (to - from).normalized()
		from += direction * (PartPin.RING_RADIUS + 4.0)
		_workshop.draw_dashed_line(from, to, Color(GarageArt.ORANGE, 0.75), 2.0, 7.0)
		_workshop.draw_circle(to, 4.0, GarageArt.ORANGE)


# The share of the garage's money bought: every upgrade's levels and every
# alternative, by what they cost.
static func _bought_share(upgrades: Array, choices: Array) -> float:
	var total := 0.0
	var spent := 0.0
	for line: Dictionary in upgrades:
		var costs: Array = line.get("costs", [])
		for l in costs.size():
			total += float(costs[l])
			if l <= int(line["level"]):
				spent += float(costs[l])
	for choice: Dictionary in choices:
		for alternative: Dictionary in choice["alternatives"]:
			total += float(alternative["cost_eur"])
			if alternative["owned"]:
				spent += float(alternative["cost_eur"])
	return spent / total if total > 0.0 else 1.0


# A label's first part, before a comma, for a pin's line.
static func _short(text: String) -> String:
	return text.get_slice(",", 0)


func _panel(parent: Control, rect: Rect2, colour: Color, corner: int) -> Panel:
	var panel := Panel.new()
	var style := StyleBoxFlat.new()
	style.bg_color = colour
	style.border_color = GarageArt.LINE
	style.set_border_width_all(1 if corner > 0 else 0)
	style.set_corner_radius_all(corner)
	panel.add_theme_stylebox_override("panel", style)
	panel.position = rect.position
	panel.size = rect.size
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(panel)
	return panel


func _label(parent: Control, rect: Rect2, font_size: int, colour: Color) -> Label:
	var label := Label.new()
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", colour)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	label.clip_text = false
	parent.add_child(label)
	return label


func _button(parent: Control, text: String, rect: Rect2, font_size: int) -> TouchButton:
	var button := TouchButton.new()
	button.text = text
	button.font_size = font_size
	button.fires_on_release = true
	button.position = rect.position
	button.size = rect.size
	parent.add_child(button)
	return button
