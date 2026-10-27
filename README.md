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
# run: 89.20 m, engine stalled (36.4 s, top 11.9 km/h)
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
# run: 88.70 m, wheels spun out (33.5 s, top 11.9 km/h)
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
# covered_heavy_clay                875 kPa   1.00      0.21     60.0 m  86.58 m, engine stalled
# dry_hard_clay                    6000 kPa   0.96      0.19     55.0 m  88.60 m, engine stalled
# firm_loam                        1943 kPa   1.00      0.24     60.0 m  89.09 m, engine stalled
# ideal_clay_sand                  1485 kPa   1.00      0.22     60.0 m  89.20 m, engine stalled
# sandy_clay                       2107 kPa   1.00      0.26     60.0 m  89.30 m, engine stalled
# watered_start                     584 kPa   1.00      0.25     60.0 m  88.51 m, engine stalled
```

Pulling tracks are clay mixes, prepared and covered when it rains, so they
differ by a few metres for the same tractor: the moist heavy clay is the
shortest, and the sled operator evens out the rest with the push-down and
the knives on the day.

### Tuning before a pull

The puller sets two things on the tractor before a pull
(`vehicle::Tuning`; parameters only, the simulation is the same):

- **The ballast.** 400 kg of weights moved between the rear wheels and the
  front weight bracket (`[tractor.chassis.ballast]`), so the rear axle
  carries 68 % to 52 % of the weight at rest. More on the rear grips harder
  and pulls further, but in the pull the nose goes light: with all of it
  at the rear the front tires lift and the tractor cannot steer.
- **The rear tires' pressure.** 0.5 to 1.6 bar (`[tire.pressure]`): lower
  deflects the tire more, for a bigger footprint that sinks less and pulls
  better on soft ground, but lets the lugs squirm, which costs grip on firm
  ground; harder than 1.0 bar only shrinks the footprint. The best pressure
  depends on the track:

```bash
for bar in 0.5 1.0 1.6; do
  ./build/release/tools/tp_pull compare-tracks data/tractors/farm_stock_light.toml \
      data/sleds/light_class.toml data/tracks/covered_heavy_clay.toml \
      data/tracks/dry_hard_clay.toml --gear 6 --pressure $bar
done
# 0.5 bar: covered heavy clay 87.33 m, dry hard clay 84.71 m
# 1.0 bar: covered heavy clay 86.58 m, dry hard clay 88.60 m
# 1.6 bar: covered heavy clay 85.51 m, dry hard clay 88.34 m
```

`--ballast B` (0 rearmost, 1 foremost) and `--pressure BAR` tune the
tractor in `pull`, `run` and `compare-tracks`. As built (the ballast
halfway, 1.0 bar) the tractor is unchanged, and the scripted rivals pull
it that way.

### Shifting down under load

Some tractors shift a gear or two down under load, the clutch in: a
powershift step, clutch packs inside the gearbox (`driveline.powershift`,
`load_downshift_gears`; without it the gears change with the clutch pressed
only). The shipped tractor shifts one gear down. In the game, "-" (or the
gamepad's left shoulder) then works with the clutch in; in the easy layout
the clutch assist keeps the clutch in for it. The gear goes in at once:
the wheels keep their speed, the input shaft turns faster than the
engine, and the clutch slips (at most its capacity, for a fraction of a
second) and pulls
the engine up to the lower gear's speed, while the tractor slows through
its tires. A 44 % step is a big one: the tractor slows while the sled rolls
on, so the chain goes slack for a moment and then snatches. Two gears
down, or up, still take the clutch, and so does every shift after the
first: one shift under load a pull (`shifts_per_pull`, 1 if left out).

It is for the end of a pull, when the sled has the engine pulled down
towards the stall. On the light class in 6th (the clutch slipped in over
3 s), one shift to 5th once the engine falls below 90 % of its rated speed
past 40 m:

| track | stays in 6th | shifts to 5th (at) |
|---|---|---|
| ideal clay-sand | 89.20 m, stalled | **91.11 m** (76.6 m) |
| covered heavy clay | 86.58 m, stalled | **87.99 m** (74.7 m) |
| watered start | 88.51 m, stalled | **90.50 m** (75.8 m) |
| dry hard clay | **88.60 m**, stalled | 87.06 m, spun out (75.2 m) |

On dry hard clay the extra torque spins the wheels: there a later shift,
at 60 % of rated speed (87.0 m), gets 89.23 m. A test pins the first row
(`LoadDownshiftRuns`).

The scripted rivals shift too. Once the clutch is fully in, a driver waits
for the engine to come back from the launch's dip and then watches the rev
counter: when the sled pulls it below their `downshift_share` of the rated
speed, they shift one gear down under load (`driver::Plan`). Each rival
draws their own share (`event.field.downshift_share`, 82 to 95 %), so some
shift at the right moment and some a little early. On ideal clay-sand it
takes the reference rival in 6th from 89.20 to 91.11 m (at 90 %), about what
5th gets without a shift; in 4th and 5th the tires, not the engine, end the
pull, so the engine is never pulled down and the driver never shifts.

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
file, € 700 for a win down to € 120 for 10th; a disqualified pull earns
nothing), and the garage spends it, between events:

| part | starts at | up to (the class's limit) |
|---|---|---|
| injection pump | 90 % of the torque, worn | the maker's figures: 84 hp at the PTO (85 hp) |
| rear tires | worn, traction limit 0.86 | wider deep-tread rubber, 0.95 (rubber only, uncut) |
| weights | 100 kg, 3200 kg in all | 400 kg, 3500 kg (3500 kg) |
| drawbar | 0.40 m | 0.50 m (0.5 m, below the rear axle) |
| powershift | none | two gears down under load, once a pull (one make) |

Each level sets its part outright, so the parts never interact; the season
loader checks every level against the class (`garage::validate`). Fully
upgraded, the tractor is at the class's limits like the rivals' reference
tractor, and beyond it: wider tires (a traction limit of 0.95 against
0.93) and a two-gear powershift step against one. Prizes are paid as an
event is scored, once; upgrades are bought between events or before your
first pull of one (`field::LiveSeason::player_may_change_tractor`). The money and the levels carry from season to season. The
whole garage costs € 15,200; a season pays a winner of every event € 7,000.

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
distance and a bar down the track to the full-pull line.

The pull scene (`scenes/pull.tscn`) draws the run in graybox, built from the run's sizes (`get_dimensions()`):
the lane with its chalk lines and the full-pull line, the tractor with
wheels that turn and steer, the sled with its weight box moving forward,
and the chain, darker as it pulls.

You drive on the touch screen, in the easy or the pro layout. Both have a
steering slider under the left thumb, which springs back to straight ahead
when let go; a hand throttle under the right, moved by sliding it like a
lever (a touch alone does not move it) and staying where it is left, as a
tractor's does; and gear buttons (+, - and N) beside it. Both thumbs work at
once. In the easy layout the clutch assist works the clutch. The pro layout
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
Space clutch, Q and E brakes, C camera, R again, M menu) and a gamepad
(right trigger, left trigger for the clutch, left stick, shoulder buttons
for the gear, X neutral, Y camera, Back again) do too.

`scripts/godot_test.sh <godot binary>` plays the scene with a scripted
driver and checks the views follow the run, drives it with simulated
fingers on the touch controls (and tilt steering with a simulated motion
sensor), plays an event's flow (start, the settings, practice, the
event with three rivals, its top ten and the season's, the next event) on a
practice season, then plays two scripted runs
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
