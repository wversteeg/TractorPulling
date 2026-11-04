# Requirements

What the game must do, one requirement per line, each with the tests that
check it. Every change is validated against this list:

- **Before a change:** find the requirements it touches. A change that
  breaks one either changes the requirement here, in the same PR and with
  the owner's agreement, or is wrong.
- **With a change:** a new behaviour adds its requirement here, with the
  test that checks it. A requirement no automated test can check (a look,
  a feel) says how it is checked instead.
- **IDs are stable:** a dropped requirement keeps its ID, marked
  *withdrawn*, so old references still resolve.

Status: **met** (a test checks it), **manual** (checked by playing or on
screenshots), **open** (wanted, not yet built: the next work).

C++ tests are named `Suite.Test` (`tests/`), Godot tests by their script
(`app/godot/tests/`, run by `scripts/godot_test.sh`).

## Goals and platform

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| G-1 | The simulation runs at a fixed 1 kHz step (1 ms), whatever the frame rate; the game hands it real time and it alone decides the steps. | met | `FixedStepClock.*`, `Session.GivesTheSameRunHoweverTheTimeIsSplitIntoFrames` |
| G-2 | Identical inputs give an identical state hash on every supported platform (x86-64, ARM, Windows, Android). | met | `*.MatchesRegressionHash`, `*.ReplayMatchesRegression`, CI's ARM, Android and Windows jobs, `scripts/platform_runs.sh` |
| G-3 | The game's runs through the Godot extension end in the same state hashes as `tp_pull run --hash`. | met | `run_test.gd` via `scripts/godot_test.sh` |
| G-4 | A simulation step takes at most 50 µs on a mid-range phone, and never allocates on the heap. | met (CI limits; phone not yet measured) | `bench/speed_budget.toml`, `scripts/check_speed_budget.py`, `NoAllocation.*` |
| G-5 | Android first (API 24 and up); Windows as a desktop build. | met | CI's Android emulator tests and `export` job, `windows game` job |
| G-6 | All tractor, engine, tire, sled, track, class, event and season numbers come from `data/`, validated on load with a message naming the file and field. | met | `*Loader.*` |

## Physics

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| P-1 | The engine follows its torque curve with inertia, turbo lag, a governor (droop to high idle) and stalls under too much load. | met | `Engine.*` |
| P-2 | The clutch slips with a friction model and stalls the engine when dumped under heavy load; slipped in, it launches. | met | `Clutch.*`, `ClutchWithEngine.*` |
| P-3 | The gearbox, final drive and inertias set road speed by gear; a low gear pulls what a high one cannot. | met | `Driveline.*`, `DrivelineChain.*` |
| P-4 | Rear tires: load-dependent traction with slip (Brixius mobility), softer soil grips less and rolls harder; side grip with a relaxation length. | met | `Tire.*`, `SideGrip.*`, `Soil.*` |
| P-5 | Weight transfer: pulling unloads the front axle; the axle torque's reaction lifts the nose; a wheelie bar catches the tractor. | met | `Chassis.*`, `Vehicle.SteadyPullUnloadsTheFrontByItsMoments` |
| P-6 | The sled's weight box moves forward as it travels, the push-down takes the wheels off, so the drag grows with distance and tops out. | met | `Sled.*` |
| P-7 | Too much load stalls the engine in a pulling gear; a low gear spins out. | met | `Run.PullingGearEndsWithAStall`, `Run.LowGearEndsSpinningOut` |
| P-8 | Dumping the clutch spins the wheels or lifts the front. | met | `Vehicle.DumpingTheClutchSpinsTheWheels`, `Pull.DumpingTheClutchSnapsTheChain` |
| P-9 | Reverse moves at about 9 km/h at full throttle (less with less gas), and changes direction only standing. | met | `VehicleReversing.*` |
| P-10 | A hard launch lifts the front wheels for the first metres, as real pullers do (a launch wheelie), with the weights to the rear. | **open** (next PR) | to add: a launch test on the reference tractor |
| P-11 | When the sled gets heavy near the end of a pull the front comes up, more with the weights to the rear; weights forward keep it down. | **open** (next PR) | to add: a late-pull front-load test per ballast position |

## Competition rules (after the NTTO's)

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| R-1 | The first pull of an event, and the first after every sled change, is a test pull; the sled operator lightens or makes heavier by it, weighing who pulled it. | met | `Event.FirstPullIsATestPull`, `Event.EasyTestPullMakesTheSledHeavier`, `Event.ShortTestPullMakesTheSledLighter`, `Event.OperatorLearnsWhatAStepIsWorth` |
| R-2 | A test pull stopping between 100 m and 110 m moves the full-pull line to 110 m and leaves the sled; it counts its own distance (e.g. 104 m), not a full pull, and the board says so. | met | `Event.TestPullJustPastTheLineExtendsIt`, `flow_test.gd` |
| R-3 | A test puller keeps the test pull or pulls again, deciding after the 3rd pull and re-pulling after the 6th. | met | `Event.TestPullBeatenByLaterTestPullsIsRepulled`, `Event.DecisionIsDueByTheEndOfTheRound` |
| R-4 | Every full pull goes to the pull-off; the places (4) are filled up with the best pulls from 20 m short of the line in force: from 80 m on the 100 m line, from 90 m on the 110 m line. | met | `Event.QualifyingFromTwentyMetresShortOfTheLine`, `Event.QualifyingFollowsTheExtendedLine` |
| R-5 | The pull-off is on a heavier sled; its full-pull line is 120 m (per class, `pull_off_full_pull_m`). Those who reach it pull off again, on a heavier sled still. | met | `Event.PullOffFullPullIsAtThePullOffLine`, `Event.PullOffsRepeatUntilOneIsStrongest`, `Event.PullOffSledMatchesTheStrongestFullPull` |
| R-6 | A pull past the line stops at the maximum distance (130 m); a counting pull stops at the line. | met | `PullRules.OnlyTestPullsAndPullOffsRunPastTheLine` |
| R-7 | The distance is measured down the track from the start line to the front of the sled; the sled's front stands on the start line at the start. | met | `Model.StartLineIsAtTheSledsFront`, `Session.StartsAtTheStartLine`, `Run.ATurningPullIsMeasuredDownTheTrack` |
| R-8 | Standings: pull-off rounds first, then the pull-off distance, then the main round's distance; disqualified pulls last; equal results share a place. | met | `Event.EqualResultsShareAPlace`, `Event.DisqualifiedPullRanksLast` |
| R-9 | A puller stopped short of the restart line (30 m) may restart once, or move back 6 places and pull later. | met | `Turn.AShortPullMayBeRestarted`, `Event.ADeferredPullerPullsLater`, `restart_test.gd` |
| R-10 | Points per place over a season; prize money per place; disqualified scores nothing. | met | `Championship.*`, `Garage.PaysByPlace` |

## The lane

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| L-1 | A tractor tire (front or rear) on a chalk line before the full-pull line disqualifies the pull: 0 m. The sled may cross. | met | `Lane.*`, `Run.LeavingTheLaneIsOutOfBounds` |
| L-2 | Driving to the sled and away crosses the chalk lines freely. | met | `Turn.AWholeTurnFromTheParkingAndBack` |

## Rivals and career

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| K-1 | The game's events have 15 pullers: the player and 14 named rivals. | met | `EventLoader.LoadsTheShippedEvent`, `RivalDifficulty.*` |
| K-2 | No two pulls are the same: grip, clutch timing and the sled pan's drag (the spot on the track) vary per pull, so the strongest two or three rivals fight for the win rather than one winning every event. | met (measured with `tp_pull season`) | `Driver.EveryPullDiffersALittle`, `RivalDifficulty.*` |
| K-3 | A new tractor finishes near the back of its class's first level; a fully upgraded one wins at its last level; each stage of the garage climbs. | met | `RivalDifficulty.*` |
| K-4 | Winning a level's championship opens the level above, after at least two seasons on a level. | met | `Career.*` |
| K-5 | Upgrades only change parameters within the class's rules. | met | `Garage.*`, `ClassRules.*` |

## Controls

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| C-1 | Two layouts: easy (the clutch assist works the clutch and brakes) and pro (clutch and two brakes on their own). | met | `ClutchAssist.*`, `Pro.*`, `touch_test.gd` |
| C-2 | Steering by an on-screen slider or by tilting the phone. | met | `touch_test.gd` |
| C-3 | In the pull each brake pedal brakes its own side; off the pull they are latched together, and the two brake sliders move as one. | met | `Session.TheBrakesWorkTogetherOutsideThePull`, `touch_test.gd` |
| C-4 | The gear "−" stops at the lowest gear the gearbox may go to under way, and reaches N and R standing. | met | `touch_test.gd`, `turn_test.gd` |

## Game and screens

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| U-1 | The event board lists every pull and the operator's verdict; the main round's best four pulls (the pull-off places) and the best of a pull-off round under way are in bold, so the distance to beat stands out. | met | `flow_test.gd` (bold rows), manual on screen |
| U-2 | The standings show each pull-off round's distance, not only the last. | met | `Event.PullOffsRepeatUntilOneIsStrongest`, `flow_test.gd` |
| U-3 | The full-pull banner and scoreboard stand over the class's own line (100 m), and stay there when the line in force moves to 110 m or to the pull-off line; the chalk line shows the line in force. | met | `scene_test.gd` |
| U-4 | Behind the menus a drawn tractor-pulling scene (MenuBackdrop) in place of the 3D venue, which is then not drawn. | manual | screenshots |
| U-5 | The game's icon is drawn from the menus' scene (`tools/make_icons.gd`), for the project and Android's launcher. | manual | `export` job builds the APK with it |
| U-6 | For testing (to remove before a release): "+ € 1000" in the settings adds money to the bank. | manual | `docs/future_improvements.md` |
