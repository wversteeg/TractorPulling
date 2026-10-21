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
        "res://data/controls/easy.toml")  # "" for the pro layout
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
`app/godot` in Godot 4.7 and play it. The pull scene (`scenes/pull.tscn`)
draws the run in graybox, built from the run's sizes (`get_dimensions()`):
the lane with its chalk lines and the full-pull line, the tractor with
wheels that turn and steer, the sled with its weight box moving forward,
and the chain, darker as it pulls.

You drive with the easy layout on the touch screen: a steering slider under
the left thumb, which springs back to straight ahead when let go; a hand
throttle under the right, moved by sliding it like a lever (a touch alone
does not move it) and staying where it is left, as a tractor's does; and
gear buttons (+, - and N) beside it. Both thumbs work at once.
The clutch assist works the clutch. "cam" switches between the chase, side
and overhead cameras, "again" (on lifting the finger on it) pulls again. On a desktop the mouse works the
same controls, and for development the keyboard (W throttle, A and D steer,
digits for a gear, C camera, R again) and a gamepad (right trigger, left stick,
shoulder buttons for the gear, X neutral, Y camera, Back again) do too.

`scripts/godot_test.sh <godot binary>` plays the scene with a scripted
driver and checks the views follow the run, drives it with simulated
fingers on the touch controls, then plays two scripted runs
and checks they end in the same state as `tp_pull`'s (with a renderer,
under Xvfb if there is no display). To look at the scene without a display:

```bash
xvfb-run -a -s "-screen 0 1280x720x24" /path/to/godot --fixed-fps 60 \
    --rendering-method gl_compatibility --resolution 1280x720 --path app/godot \
    --script res://tests/screenshots.gd -- /tmp/shots   # view_0..2.png
```

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
