# TractorPulling

A tractor pulling game with believable, deterministic semi-sim physics. Android first, other platforms later.

The simulation is a pure C++20 library (`core/`) with no engine or platform dependencies. The Godot 4 front end is in `app/godot/`.

## Status

**Milestone 1, a straight-line pull, is complete.** A light farm-stock tractor
(NTTO standard 3.5 t) pulls a transfer sled on a 100 m track. Each "done when"
criterion is covered by a test:

| Criterion | Test |
|---|---|
| A scripted pull gives plausible curves for RPM, slip and sled load | `Run.PullingGearEndsWithAStall`, `Pull.PullingGearStallsAfterThePushdown` (and `tp_pull run` CSV) |
| Dumping the clutch causes wheelspin or a wheelie | `Vehicle.DumpingTheClutchSpinsTheWheels`, `Vehicle.DumpingTheClutchInALowGearLiftsTheFront` |
| Too much load stalls the engine | `Vehicle.LightClassPullsOnTorqueInItsPullingGears`, `Run.PullingGearEndsWithAStall` |
| Identical inputs give an identical state hash on x86-64 and ARM | every `*RegressionHash` test and `Run.ReplayMatchesRegression`, run in CI on x86-64 and arm64 |

Milestones 2 to 4 are complete too: track conditions and competition (an
event by the NTTO rules and a season), steering and the lane, and phone
readiness (the Android build, the speed budget and `tp::session`, the
interface the Godot front end will use). They are described below.

Try a run:

```bash
cmake --preset release && cmake --build --preset release
./build/release/tools/tp_pull run data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/ideal_clay_sand.toml --gear 6 -o run.csv
# run: 92.61 m, engine stalled (36.9 s, top 11.8 km/h)
```

### Steering

The tractor moves in the ground plane: it slides sideways and turns under
its tires' side grip and the chain. The front wheels steer, and a tractor on
its wheelie bar cannot steer. Hold the wheels at an angle (positive left) to
see it; the CSV gains the sideways position and heading:

```bash
./build/release/tools/tp_pull pull data/tractors/farm_stock_light.toml --gear 4 \
    --resistance-kn 8 --steer-deg 5 -o pull.csv
# ... ended 25.79 m to the left, heading 86.68°
```

Each rear side has its own brake. An open differential drives the two
sides, so braking one side slows it, sends the drive to the other, and
turns the tractor towards the braked side, as farm tractors steer tight
turns. Full pedal holds a side's wheels still where the brake can; in a
high gear it takes more torque than the engine gives and stalls it:

```bash
./build/release/tools/tp_pull pull data/tractors/farm_stock_light.toml --gear 4 \
    --resistance-kn 8 --brake-right 0.3 -o pull.csv
# ... ended -11.37 m to the left, heading -33.39°
```

### The lane

Each track has a lane, 12 m between the chalk lines by the NTTO's minimum.
A tractor tire touching a chalk line before the full-pull line disqualifies
the pull: out of bounds, 0 m. Past the line it may, and the sled may cross
the lines anywhere. The sled follows the tractor as a trailer does: its
wheels keep its tail in line, but its pan, sliding forward, resists sideways
only weakly, so when the driver corrects the tractor's line the sled swings
out wider than the tractor (the dog-tail). Steer a little and the tractor
wanders out of its lane:

```bash
./build/release/tools/tp_pull run data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/ideal_clay_sand.toml --gear 6 --steer-deg 0.5
# run: 0.00 m, out of bounds: a tire touched the chalk line (22.6 s, top 11.9 km/h)
```

A pull is measured down the track. In the run CSV, `sled_distance_m` is how
far down the track the sled is, and `sled_travel_m` its path along its own
heading, a little longer once it has turned. On a disqualified run the CSV
still shows where the sled got to; the result is 0 m.

### Easy and pro controls

The same physics under two control layouts (`tp::controls`). **Pro** has
every control on its own: throttle, clutch pedal, gear, steering and two
brake pedals. **Easy** has the throttle, the gear and one steering control:
its first stretch (60 %) turns the front wheels to full lock, the rest
brakes the rear wheels on the inside of the turn. There is no clutch pedal:
the clutch assist lets the clutch in while the engine holds its speed and
lets it slip when the load pulls the engine below a guard speed (near peak
torque at full throttle; at part throttle where the governor reaches full
fuel, so a touch of throttle creeps the tractor away), so the engine never
stalls; it presses the clutch when the driver
lifts off or changes gear. The layout is data (`data/controls/easy.toml`).
At full throttle the assist pulls about as far as a driver slipping the
clutch in by hand, but the sled stops with the engine running:

```bash
./build/release/tools/tp_pull run data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/ideal_clay_sand.toml --gear 6 \
    --easy data/controls/easy.toml
# run: 92.09 m, stopped (33.7 s, top 11.9 km/h)
```

`--stick S` holds the steering control at S (-1 right to 1 left) from the
launch. The middle 3 % either side is a dead zone, so a thumb resting on a
touch slider steers straight, and the steering follows the control at a
rate, as a driver turns a wheel (both in `easy.toml`): 0.05 is 1.2° of
wheel angle, and leaves the lane 10 s after the launch.

The pro layout's steering has the same dead zone and rate when the game
plays it, from `data/controls/pro.toml`; `--pro data/controls/pro.toml`
gives a scripted pro run the same, where without it the steering is taken
as it is. With `--pro`, `--steer-deg` is the angle the steering control asks
for, before the dead zone (1.2° on the reference tractor, so a smaller angle
steers straight, and the tool warns); `--stick S` holds the control itself.

### Track conditions

Every event's track is different. A track file gives its soil (clay
fraction, moisture, compaction), optionally how that changes along the
track, and the push-down the sled operator sets for the day. The soil sets
how strong the ground is and how well it grips. The tires (Brixius), the
sled's pan and its wheels all work on the ground where they are. Compare
the shipped tracks:

```bash
./build/release/tools/tp_pull compare-tracks data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/*.toml --gear 6
# track                         cone index   grip  fall-off  push-down  result
# covered_heavy_clay                875 kPa   1.00      0.21     78.0 m  90.89 m, engine stalled
# dry_hard_clay                    6000 kPa   0.96      0.19     73.0 m  91.06 m, engine stalled
# firm_loam                        1943 kPa   1.00      0.24     78.0 m  92.61 m, engine stalled
# ideal_clay_sand                  1485 kPa   1.00      0.22     78.0 m  92.61 m, engine stalled
# sandy_clay                       2107 kPa   1.00      0.26     78.0 m  92.71 m, engine stalled
# watered_start                     584 kPa   1.00      0.25     78.0 m  92.26 m, engine stalled
```

Pulling tracks are clay mixes, prepared and covered when it rains, so they
differ by a metre or two for the same tractor: the moist heavy clay is the
shortest, and the sled operator evens out the rest with the push-down and
the knives on the day. On the fast dry clay the operator brings the
push-down 5 m earlier.

### The sled tops out after the push-down

The weight box rides forward from over the rear wheels to over the pan
along the first 88 m, so the drag grows steadily. At the push-down (78 m
on the class sled, set per track and by the operator) the sled's rear
wheels come off the track over 10 m: from 88 m the pan carries all of the
sled and the box is at its stop, so the drag is at its most and stays
there (`Sled.TopsOutShortlyAfterThePushdown`; only the berm the tires
throw still grows a little). Where the operator brings the push-down
earlier (73 m on the dry hard clay) the wheels are off by 83 m, but the
drag still rises until the box reaches its stop at 88 m. A tractor that
can hold that drag goes on to a full pull; one that cannot slows and stops
a few metres on, as momentum runs out. So the operator sets the push-down by the test pulls, and a
step (a knife) changes the distance little until the tractor can hold the
sled, then a lot. In the game the HUD marks the push-down's stretch on the
track bar and shows "PUSH-DOWN" while it comes on; on the sled the rear
wheels lift and its lamp turns from green to red.

### Tuning before a pull

The puller sets two things on the tractor before a pull
(`vehicle::Tuning`; parameters only, the simulation is the same):

- **The ballast.** 400 kg of weights moved between the rear wheels and the
  front weight bracket (`[tractor.chassis.ballast]`), so the rear axle
  carries 68 % to 52 % of the weight at rest. More on the rear grips harder
  and pulls further, but in the pull the nose goes light (the HUD's
  "front" bar): with all of it at the rear a dumped clutch lifts the front
  tires for a moment, and the tractor cannot steer then.
- **The rear tires' pressure.** 0.5 to 1.6 bar (`[tire.pressure]`): lower
  deflects the tire more, for a bigger footprint that sinks less and pulls
  better on soft ground, but lets the lugs squirm, which costs grip, most
  on firm ground; harder than 1.0 bar only shrinks the footprint. The best
  pressure depends on the track (about 0.7 bar on the soft covered heavy
  clay, about 0.9 on dry hard clay, where 0.7 costs nearly 2 m):

```bash
for bar in 0.7 1.0 1.6; do
  ./build/release/tools/tp_pull compare-tracks data/tractors/farm_stock_light.toml \
      data/sleds/light_class.toml data/tracks/covered_heavy_clay.toml \
      data/tracks/dry_hard_clay.toml --gear 6 --pressure $bar
done
# 0.7 bar: covered heavy clay 91.16 m, dry hard clay 89.29 m
# 1.0 bar: covered heavy clay 90.89 m, dry hard clay 91.06 m
# 1.6 bar: covered heavy clay 90.28 m, dry hard clay 90.84 m
```

`--ballast B` (0 rearmost, 1 foremost) and `--pressure BAR` tune the
tractor in `pull`, `run` and `compare-tracks`. As built (the ballast
halfway, 1.0 bar) the tractor is unchanged, and the scripted rivals pull
it that way.

### Why the front stays down in this class

A steady pull takes weight off the front axle by pull × hitch height /
wheelbase. The NTTO's 0.5 m limit on the hitch is there to keep it small:
the reference tractor, weights fully back (10.9 kN on the front axle, 2.4 m
wheelbase), would need about 52 kN of pull to lift its front, twice what
its tires give on the best track (some 24 kN at the end of a pull, when
the front still carries about 6 kN). So in the 3.5 t class the front only
goes light; it lifts for a moment when the clutch is dumped, as the axle's
torque spins the heavy rear wheels up (weights fully back, in 2nd to 6th:
about 0.7° of pitch, the front tires off the ground for some 60 ms).
Wheelies are the physics, not scripted: the same tractor with its drawbar
at 1.0 m pulls one onto its wheelie bar (7.6°), which a test checks
(`Vehicle.AHighHitchPullsAWheelieOntoTheBar`). Classes with higher hitches
will show them.

### Shifting down under load

Some tractors shift a gear or two down under load, the clutch in: a
powershift step, clutch packs inside the gearbox (`driveline.powershift`,
`load_downshift_gears`; without it the gears change with the clutch pressed
only, and under way only up). The shipped tractor shifts one gear down. In
the game, "-" (or the gamepad's left shoulder) then works with the clutch
in; in the easy layout the clutch assist keeps the clutch in for it. The
gear goes in at once: the wheels keep their speed, the input shaft turns
faster than the engine, and the clutch slips (at most its capacity, for a
fraction of a second) and pulls the engine up to the lower gear's speed,
while the tractor slows through its tires. A 44 % step is a big one: the
tractor slows while the sled rolls on, so the chain goes slack for a moment
and then snatches. Up still
takes the clutch. Down, once the tractor moves off, goes only by the
powershift, as far and as often as it allows (`load_downshift_gears`, and
`shifts_per_pull`, 1 if left out): not two gears at once with a one-gear
step, not again after the last, and not with the clutch pressed either
(`driveline::floor_gear`). Standing, any gear may be chosen. A tractor
without a powershift pulls in the gear it moves off in, or higher.

It is for the end of a pull, when the sled has the engine pulled down
towards the stall. It pays where the sled's most drag (see "The sled tops
out after the push-down") lies between what 6th holds before the engine
stalls and what 5th's grip holds: on a sled lighter than shipped (two
knives, the pull-off setting at the shipped event, whose field pulls on
one), not on the sled as shipped (four), where neither gear holds it. In
6th (the clutch slipped in over 3 s), one shift to 5th once the engine
falls below 90 % of its rated speed past 40 m:

| track (two knives) | stays in 6th | shifts to 5th (at) |
|---|---|---|
| ideal clay-sand | 94.94 m, stalled | **96.65 m** (86.1 m) |
| covered heavy clay | **91.86 m**, stalled | 90.83 m, spun out (84.8 m) |
| watered start | 94.69 m, stalled | **96.06 m** (85.9 m) |
| dry hard clay | 97.85 m, stalled | **full pull** (83.1 m) |

On the soft heavy clay the extra torque spins the wheels: there a later
shift, at 60 % of rated speed (90.3 m), gets 92.24 m. A test pins the first
row (`LoadDownshiftRuns`).

The scripted rivals shift too. Once the clutch is fully in, a driver waits
for the engine to come back from the launch's dip and then watches the rev
counter: when the sled pulls it below their `downshift_share` of the rated
speed, they shift one gear down under load (`driver::Plan`). Each rival
draws their own share (`event.field.downshift_share`, 82 to 95 %), so some
shift at the right moment and some a little early. On ideal clay-sand and
the two-knife sled it takes the reference rival in 6th from 94.94 to
96.65 m (at 90 %), about what 5th gets without a shift; in 4th and 5th the
tires, not the engine, end the pull, so the engine is never pulled down and
the driver never shifts.

### From the parking to the sled and back

At an event a puller waits in the parking beside the track, behind the start
line, until it is their turn (`tp::turn`). They drive out, past the sled and
into the lane, and back up to it in reverse (`driveline.gearbox.reverse`:
the shipped tractor's reverse crawls at about 2.5 km/h at rated speed;
reverse goes in only with the tractor standing). Once the tractor stands with
its hitch in front of the sled's hook, within the chain's length and close
to the sled's line, the chain is hooked on. The tractor then creeps forward
to put some force on the chain while the sled is held where it stands
(simplified: it does not move however hard the chain pulls); once the chain
has held 1 kN for a second the official waves the green flag, and the pull
is on, its clock starting at the flag. After the pull the tractor backs off
to slacken the chain, which comes off once it is slack, and drives back to
the parking along the track; the sled drives itself back to the start line
on its own engine. The turn is over when the tractor stands in the parking.
The chalk lines count only in the pull, so driving to the sled and away from
it cannot disqualify the pull, and off the pull any gear may be chosen (the
gearbox's floor is cleared). An engine stalled at the end of the pull, or on
the way, starts again once the clutch is pressed.

A session plays the whole turn when its setup names a parking
(`session::Setup::parking`); without one it is the pull alone, hooked at the
start line, as `tp_pull` and the rivals pull. A test drives a whole turn with
a scripted driver, every stage in order (`Turn.AWholeTurnFromTheParkingAndBack`).

### A class at an event

An event file names the class, the sled and the track, and how a field of
pullers varies around the reference tractor and driver. The first puller
tests the sled; the sled operator makes it lighter or heavier (knives and
push-down), and the next puller tests it again. A test pull between 100 and
110 m moves the full-pull line to 110 m instead. A test puller may keep
their pull unless the sled got heavier, deciding after the 3rd pull and
re-pulling after the 6th. Every full pull, filled up to four with the best
runs from 90 m, goes to the pull-off on a heavier sled. Each driver picks a
gear for the day's track, the best one as often as they read the track
right, and no two pulls are quite the same. A disqualified pull (a tire on
the chalk) counts 0 m and ranks behind every pull that counts; a
disqualified test pull leaves the sled as it is, and the next puller tests
it.

```bash
./build/release/tools/tp_pull event data/events/club_pull.toml [--seed N]
```

### A season

A season file names the class's event, the points per place and the events,
each on its own track. The same pullers pull every event; each event draws a
new running order, and every driver guesses a gear for the day's track. A
disqualified pull scores no points.

```bash
./build/release/tools/tp_pull season data/seasons/club_season.toml [--seed N]
```

### The garage

A career starts on an older tractor, with what the NTTO standard class lets
a puller improve on a stock farm tractor still to buy (`data/garages/`,
`tp::garage`). Every event pays by place (`prize_money_eur` in the season
file, € 1,500 for a win down to € 320 for 10th; a disqualified pull
earns nothing), and the garage spends it, between events:

| part | starts at | up to (the class's limit) |
|---|---|---|
| injection pump | 90 % of the torque, worn | the maker's figures: 84 hp at the PTO (85 hp) |
| rear tires | worn, traction limit 0.86 | wider deep-tread rubber, 0.95 (rubber only, uncut) |
| weights | 100 kg, 3200 kg in all | 400 kg, 3500 kg (3500 kg) |
| drawbar | 0.40 m | 0.50 m in three steps, the last the dearest (0.5 m, below the rear axle) |
| powershift | none | two gears down under load, once a pull (one make) |

Each level sets its part outright, so the parts never interact; the season
loader checks every level against the class (`garage::validate`). Fully
upgraded, the tractor is at the class's limits like the reference tractor
the field is drawn around, and beyond it: wider tires (a traction limit of 0.95 against
0.93) and a two-gear powershift step against one. Prizes are paid as an
event is scored, once; upgrades are bought between events or before your
first pull of one (`field::LiveSeason::player_may_change_tractor`). The
money and the levels carry from season to season. The whole garage costs
€ 17,700; a season of eight events pays € 2,560 at the back of the field and
€ 12,000 to a winner of every event.

### The rivals

The rivals' tractors span what the garage offers (`event.field` in the
event file): their engines and tires from worn to the class's limit, from
3150 kg to the class's weight, drawbars from 0.36 to 0.50 m, and a
powershift step of one gear or none (`hitch_height_m`,
`load_downshift_gears`). The sled operator sets the sled for the field,
counting a step as worth about 8 m (`step_m`: with the drag flat after the
push-down, a small change goes a long way, so a wide field still gets one
full pull or none in the first round). Where the player ends depends on
how they rank in the field. Driving as the reference driver does, in the
best gear:

| the player's tractor | place among ten (the first event, eight fields) |
|---|---|
| new | 6th to 9th |
| every part one level up (€ 4,300) | 4th to 7th |
| two levels up (€ 11,000) | 1st to 5th |
| fully upgraded (€ 17,700) | 1st |

So a career starts near the back, beating a few rivals, climbs with every
level and wins once fully upgraded. The drawbar's last 4 cm matter most
(the weight it puts on the rear tires), so they cost the most. A test
(`RivalDifficulty`) checks every row on three of the eight fields. A player drives less
evenly than the script, and picks the gear themselves.

### Playing a run from a game

A game drives a run through `tp::session`: it creates a session once from a
`Setup` (the tractor, the sled as pulled today, the ground, the rules and the
control layout), then each frame calls `advance(controls, elapsed_ns)` with
the player's controls and the frame's real time, and draws
`blend(previous(), current(), alpha())`. The frame time decides only how many
1 ms steps run, so a run is the same at any frame rate; nothing allocates
after setup. `tp_pull run` plays its runs this way.

The loaders read their files through a function the game supplies
(`tp::data::ReadFile`), so the data can live wherever the game keeps it,
such as Godot's `res://`:

```cpp
const auto tractor = tp::data::load_tractor("res://data/tractors/farm_stock_light.toml",
                                            read_with_godot);
```

### The Godot front end

`app/godot/` is the game: a Godot 4.7 project whose scenes and UI are
GDScript, and a small C++ extension (`app/godot/extension/`) that hands
GDScript a `TractorPullRun`, the session above with the data read from
`res://data`:

```gdscript
var run := TractorPullRun.new()
var error := run.setup("res://data/tractors/farm_stock_light.toml",
        "res://data/sleds/light_class.toml", "res://data/tracks/ideal_clay_sand.toml",
        "res://data/controls/easy.toml")  # or pro.toml; "" pro, steering as given
run.set_controls(throttle, clutch_pedal, gear, steering, brake_left, brake_right)
run.advance(int(delta * 1e9))             # each frame, _process's delta
var snapshot := run.get_snapshot()        # what to draw, SI units, on the track
```

The scene passes `_process`'s `delta`, the frame's time (slowed or stopped
with `Engine.time_scale` for slow motion or a pause); the session alone turns
it into 1 ms steps. Controls held out of range, or not finite, read as held to
their range or at rest.

Build the extension with `cmake --preset godot && cmake --build --preset godot`
(it fetches godot-cpp and copies `data/` into the project), then open
`app/godot` in Godot 4.7 and play it.

The game plays a season (`data/seasons/club_season.toml`, the club class on
ten tracks) against nine scripted rivals, named in `club_pull.toml`'s
`[event.game]`. The season is saved after every event, with your tuning
and your career (`user://season.toml`, `tp::data::SeasonSave`): at the
next launch the start screen asks whether to continue it, at the next
event, or to start a new season. An event left halfway is not saved:
continued, it starts again from its beginning, and the rivals pull as they
did, since the field and every event's draws come from the season's seed.
A finished season is kept for its career, which the next season starts
with. You pull the class's reference tractor as your garage has it (see
"The garage"). The start screen shows the next event and its track.
"Practice" is a free pull on the track; "Garage" spends your prize money;
"Event" goes to the event's board, where the rivals' pulls and the
sled operator's verdicts fill in as they are simulated (the rivals try
their gears from the start screen on). When you are due, you pull: a test
pull, a pull, a re-pull or a pull-off, each on the sled as the operator has
set it, counted by the NTTO rules as for everyone. Before every pull the
setup screen sets the tractor (see "Tuning before a pull": the weights,
shown as the rear axle's share, and the rear tires' pressure, both kept for
the season); it names the controls the settings picked. The gear in the
start screen's top right corner opens the settings: the layout, easy or
pro, and the steering, the slider or tilting the phone, kept on the device
between launches. After a test pull on a
changed sled you keep it or pull again. Then the event's top ten and the
season's (points per place), and the next event. During a pull the HUD
shows the rev counter (stall and rated speeds marked), gear, speed,
distance and a bar down the track to the full-pull line, with the
push-down's stretch in orange ("PUSH-DOWN" beside it once it comes on).

Every pull is your whole turn (see "From the parking to the sled and
back"). You start in your bay in the parking, behind the start line on the
right, with the rivals' tractors in the bays behind yours. Drive out along
the return lane on the right, into the lane past the sled, and back up to
it in reverse ("-" from N puts R in): a white mark on the track shows where
the hitch should stop, the cameras frame the hitch and the hook as you come
close, and the HUD says how far to go and which way to steer. Stand still
there and the chain goes on. Creep forward to take up the slack while the
official on the left past the start line raises the red flag; it turns
green and the pull is on. A second official, down the track from 80 m,
walks ahead of the tractor towards the full-pull line, the green flag up
while the pull is good. The moment the pull is over (a full pull, the sled
stopped, a disqualification) both wave red: stop, close the throttle. A
light on top of the full-pull gantry and a lamp at the top of the HUD
show the same, green GO and red STOP. Then back off in R to release the
chain, and drive back down the return lane to your bay while the sled
drives itself back to the start line. The result comes once you stand in
your bay. The chalk lines count only in the pull.

The pull scene (`scenes/pull.tscn`) draws the run at a club venue, built
from the run's sizes (`get_dimensions()`), in a bright style that stays
cheap on a phone:

- the tractor: hood, grille, cab with the driver, fenders in glossy paint,
  rear tires with tall chevron bars and rounded shoulders on dished rims,
  the wheelie bar and the drawbar at their real places, the front weights
  and rear wheel weights as the ballast is set, soft exhaust smoke that
  turns black under load, dirt thrown by a spinning tire and dust behind;
- the sled: frame and gooseneck, the pan and the berm of loose soil it
  pushes (`Snapshot::berm_m3`, from the simulation), the weight box
  riding forward, the operator under a canopy and the lamp that turns
  from green to red; the chain as links, darker as it pulls;
- the track: ruts with the lugs' prints behind the rear tires, and behind
  the sled the wide band its pan smooths over them (`TrackMarks`), on clay
  whose clods catch the low sun;
- the venue: fences and straw bales, distance boards every 10 m, a
  grandstand and a bank full of people who sway and cheer louder as the
  sled nears the full-pull line, waving flags, the full-pull gantry, a
  scoreboard showing the sled's distance, tents and a barn, farmland, a
  tree line and hills all round, under a deep summer sky with soft
  shadows and a little glow. The cameras shake a little with the work.

The trees, tents, barn and clouds are CC0 models from Kenney's starter kits
(`app/godot/assets/kenney`, credited in its README). Everything else is built
in code: the people (`People`, one low-poly figure, standing or seated) and
the rest from primitives (`Art`). Repeated things (lugs, bolts, the crowd,
the tree line) are MultiMeshes, so a thousand people cost one draw call.

You drive on the touch screen, in the easy or the pro layout. Both have a
steering slider under the left thumb, which springs back to straight ahead
when let go; a hand throttle under the right, moved by sliding it like a
lever (a touch alone does not move it) and staying where it is left, as a
tractor's does; and gear buttons (+, - and N; "-" from N is reverse, R)
beside it. Both thumbs work at once. In the easy layout the clutch assist
works the clutch. The pro layout
adds, on the left above the steering, a clutch slider, pressed at the top
and in at the bottom, which starts pressed and stays where it is left, so
you slip it in and leave it (a gear goes in only with the clutch pressed,
but one: see "Shifting down under load"),
and the left brake beside it; the right brake is on the right beside the
throttle, so each side's brake is under its own thumb. The brakes are off
again when let go. Every control has its own colour and label: GAS green,
CLUTCH blue, BRAKE L and R red, STEER white. The clutch slider marks the
stretch of its travel where the clutch slips, lighter, with a line where
it bites (where the plates first touch); below the stretch it grips fully.
Both come from the tractor's clutch (`clutch::engagement_range`).

With tilt steering (in the settings) the steering slider goes: you steer by
rolling the phone like a steering wheel, its left side down to steer left;
25° either way steers fully. However you hold the phone when you tap Pull
is straight ahead, so it works sitting or lying down. Lying flat, the
steering holds. A device without a motion sensor keeps the slider. Godot
reads the phone's sensors only when the project asks for them
(`input_devices/sensors/enable_gravity` and `enable_accelerometer` in
`project.godot`, off by default since Godot 4.3).
"cam" switches between the chase, side and overhead cameras; in a practice
pull "again" (on lifting the finger on it) starts the same pull again, and
"menu" goes back to the start screen (an event's pull counts: neither is
there). On a desktop the mouse works the same controls, and for
development the keyboard (W throttle, A and D steer, digits for a gear,
minus for reverse, Space clutch, Q and E brakes, C camera, R again, M
menu) and a gamepad
(right trigger, left trigger for the clutch, left stick, shoulder buttons
for the gear, X neutral, Y camera, Back again) do too.

`scripts/godot_test.sh <godot binary>` plays the scene with a scripted
driver and checks the views follow the run, drives it with simulated
fingers on the touch controls (and tilt steering with a simulated motion
sensor), plays an event's flow (start, the settings, practice, the
event with three rivals, its top ten and the season's, the next event) on a
practice season, drives a whole turn headless with a scripted driver (out
of the bay, backing up to the sled, the flag, the pull, back to the bay),
then plays two scripted runs
and checks they end in the same state as `tp_pull`'s (with a renderer,
under Xvfb if there is no display). To look at the scene without a display:

```bash
xvfb-run -a -s "-screen 0 1280x720x24" /path/to/godot --fixed-fps 60 \
    --rendering-method gl_compatibility --resolution 1280x720 --path app/godot \
    --script res://tests/screenshots.gd -- /tmp/shots [pro]   # view_0..2.png
```

### Playing it on a phone or on Windows

Every CI run exports the game (the `export` job) and keeps it for 30 days:
open the run on GitHub's Actions tab and download, under Artifacts,
`tractor-pulling-android` (a debug APK for phones and the emulator) or
`tractor-pulling-windows` (unzip it and run `TractorPulling.exe`). On a
phone, allow your browser or file manager to install unknown apps, then
open the APK; on the emulator, drag the APK onto its window. Every APK is
signed with the same debug key, so a new one installs over the last.

To export locally (Linux), build the extension for each platform, install
Godot's export templates for its version, and run the export script:

```bash
for preset in godot godot-android-arm64 godot-android-x86_64 godot-windows; do
    cmake --preset "$preset" && cmake --build --preset "$preset"
done
ANDROID_HOME=~/Android/Sdk JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 \
    scripts/godot_export.sh /path/to/godot   # build/export: the APK and windows/
```

The Windows extension is cross-compiled with MinGW-w64
(`g++-mingw-w64-x86-64-posix` on Debian and Ubuntu), as Godot builds its
own Windows releases; the preset builds `tp_pull.exe` too. CI runs it on
Windows and checks it gives Linux's results (`scripts/platform_runs.sh`:
the scripted runs' state hashes and a whole event), and starts the
exported game there.

## Requirements

- CMake 3.25+ and Ninja
- GCC 12+ or Clang 16+
- clang-format and clang-tidy (for development)

GoogleTest, Google Benchmark and toml++ are fetched automatically on first
configure; godot-cpp too with the `godot` presets. The game needs Godot 4.7.

## Build and test

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Other presets: `release` (also builds `build/release/bench/core_bench`) and `asan-ubsan`.

### Android

The Android presets cross-compile everything (core, data loading, tests,
benchmarks and the tool) with the Android NDK, for Android 7 (API 24) and
newer, the oldest the current Godot supports. Set `ANDROID_NDK_HOME` to an
NDK, ideally the version CI pins (29.0.14206865, the one Godot uses):

```bash
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/29.0.14206865
cmake --preset android-arm64 && cmake --build --preset android-arm64    # phones
cmake --preset android-x86_64 && cmake --build --preset android-x86_64  # emulator

# Run the tests (and a whole event) on a phone or emulator connected over adb;
# with the event's output from another platform, check it matches line for line
scripts/android_test.sh build/android-arm64 [expected_event.txt]

# Run the benchmarks on a phone and check them against the speed budget
scripts/android_bench.sh build/android-arm64
```

CI builds both and runs the x86_64 tests on an API 24 emulator, so the
regression hashes are checked on Android's own C and C++ libraries too, and
the club event must print exactly what it prints on Linux.

See [CLAUDE.md](CLAUDE.md) for architecture, conventions and the roadmap, [docs/architecture.md](docs/architecture.md) for class and sequence diagrams, [docs/conventions.md](docs/conventions.md) for units and sign conventions, [docs/performance.md](docs/performance.md) for the speed budget, and [docs/future_improvements.md](docs/future_improvements.md) for what is deliberately left for later.

## License

MIT, see [LICENSE](LICENSE).
