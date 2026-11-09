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
| G-2 | Identical inputs give an identical state hash on every supported platform (x86-64, ARM, Windows, Android, WebAssembly). | met | `*.MatchesRegressionHash`, `*.ReplayMatchesRegression`, CI's ARM, Android, Windows and web jobs, `scripts/platform_runs.sh` |
| G-3 | The game's runs through the Godot extension end in the same state hashes as `tp_pull run --hash`. | met | `run_test.gd` via `scripts/godot_test.sh` |
| G-4 | A simulation step takes at most 50 µs on a mid-range phone, and never allocates on the heap. | met (CI limits; phone not yet measured) | `bench/speed_budget.toml`, `scripts/check_speed_budget.py`, `NoAllocation.*` |
| G-5 | Android first (API 24 and up); Windows as a desktop build; a web build that plays in a phone's or desktop's browser (Safari on iPhones included), served as plain files with no special headers. | partly met (the web game started in Chrome only; Safari not yet tried) | CI's Android emulator tests and `export` job (the web game started in headless Chrome, `scripts/web_game_test.sh`), `windows game` job, `pages` job (the web game published on GitHub Pages) |
| G-6 | All tractor, engine, tire, sled, track, class, event and season numbers come from `data/`, validated on load with a message naming the file and field. | met | `*Loader.*` |
| G-7 | The simulation, the data loaders and the Godot extension build for the web (WebAssembly, Emscripten at Godot's version, no threads); a file with a TOML syntax error gives a message naming the file and line there too, with no exception thrown (Godot's web export cannot catch one). | met | CI's `web` job (the tests under Node) and `export` job (the extension loaded in the web game), `SyntaxError.EveryLoaderNamesTheFileAndLine` |

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
| P-10 | From the Sport 2.5 t class up, the built tractors' fronts come up as real pullers' do: on the launch, down again while the sled is light, and up again as the sled gets heavy and the gear goes down under load. Without a powershift step the engine gives out first, unless the weights are all the way back: then the front comes up by itself. | met | `WheelieRuns.ASportTractorLiftsOnTheLaunchAndAgainAsTheSledGetsHeavy`, `WheelieRuns.WithoutAPowershiftOnlyTheWeightsAllBackLiftItAtTheEnd` |
| P-11 | The weights decide how high: towards the back the front comes up, forward it stays down (less grip at the rear: it spins out sooner). The track's grip decides too: on a track that grips less the front comes up on the launch only and the wheels spin out at the end. | met | `WheelieRuns.TheWeightsForwardKeepTheFrontDown`, `WheelieRuns.OnALowGripTrackItLiftsOnlyOnTheLaunch` |
| P-12 | The standard class's farm tractor keeps its front down in a pull on every track (its weights halfway, the clutch slipped in); only a dumped clutch in a low gear throws it up. | met | `WheelieRuns.TheStandardTractorKeepsItsFrontDown`, `Vehicle.DumpingTheClutchInALowGearLiftsTheFront` |
| P-13 | Tracks differ in grip by how they were prepared (`grip` per track, a scale on the soil's grip): from firm loam, where front ends go high, to the covered heavy clay, where tractors spin out. On every track the reference pulls past 80 m. | met | `Track.GripScaleScalesTheSurfaceGripOnly`, `TrackLoader.ReadsTheGrip`, `ShippedTracks.*` |
| P-14 | With the drive out (the clutch pressed, or in neutral) a standing tractor that the chain pulls back rolls back on it: after the pull the stretched chain eases and the front comes down, so backing off starts from the ground, not from a wheelie. In gear with the clutch in, the engine holds it; the brakes hold it to a creep. | met | `VehicleReversing.TheChainPullsATractorWithTheDriveOutBack`, `VehicleReversing.AfterRollingBackItDrivesForwardsAgain`, `Driveline.RollingBackInGearTurnsTheTrainBackwards`, `WheelieRuns.PressingTheClutchAfterThePullLetsTheFrontDown` |
| P-15 | The chain is steel, not a rubber band: a tractor that creeps into the held sled and then presses the clutch eases back by the chain's stretch alone (a couple of centimetres), slowly, without bouncing off it. | met | `Turn.AJoltIntoTheHeldChainDoesNotThrowTheTractorBack` |

## Competition rules (after the NTTO's)

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| R-1 | The first pull of an event, and the first after every sled change, is a test pull; the sled operator lightens or makes heavier by it, weighing who pulled it. | met | `Event.FirstPullIsATestPull`, `Event.EasyTestPullMakesTheSledHeavier`, `Event.ShortTestPullMakesTheSledLighter`, `Event.OperatorLearnsWhatAStepIsWorth` |
| R-11 | The sled operator sets the sled for the class as a whole: a test pull is judged by what the field's average tractor would reach on it (`sled_target_m`), not its strongest, so one tractor far stronger than the rest (a built-up player's) does not get the sled set for it alone, too heavy for everyone else. | met | `Event.OneFarStrongerTractorDoesNotSetTheSled`, `Event.TheOperatorWeighsATestPullByItsTractor` |
| R-12 | The pull's rules hold until the sled stops, not only to the full-pull line: past a full pull (or a disqualification) the sled rolls on, and with it the gearbox goes down only by its powershift and each brake pedal brakes its own side. Once the sled stands they are off. | met | `Turn.ASledStillRollingAfterThePullStopsBeforeItIsHeld`, `Session.TheBrakesWorkTogetherOutsideThePull` |
| R-2 | A test pull stopping between 100 m and 110 m moves the full-pull line to 110 m and leaves the sled; it counts its own distance (e.g. 104 m), not a full pull, and the board says so. | met | `Event.TestPullJustPastTheLineExtendsIt`, `flow_test.gd` |
| R-3 | A test puller keeps the test pull or pulls again, deciding after the 3rd pull and re-pulling after the 6th. | met | `Event.TestPullBeatenByLaterTestPullsIsRepulled`, `Event.DecisionIsDueByTheEndOfTheRound` |
| R-4 | Every full pull goes to the pull-off; the places (4) are filled up with the best pulls from 20 m short of the line in force: from 80 m on the 100 m line, from 90 m on the 110 m line. | met | `Event.QualifyingFromTwentyMetresShortOfTheLine`, `Event.QualifyingFollowsTheExtendedLine` |
| R-5 | The pull-off is on a heavier sled; its full-pull line is 120 m (per class, `pull_off_full_pull_m`). Those who reach it pull off again, on a sled heavier still by as many steps as the operator changes at once. | met | `Event.PullOffFullPullIsAtThePullOffLine`, `Event.PullOffsRepeatUntilOneIsStrongest`, `Event.PullOffSledMatchesTheStrongestFullPull`, `Event.ASecondPullOffTakesTheMostStepsAtOnce` |
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
| L-3 | Backing up to the sled, before the pull or after it, the rear tires stop against the sled's fenders: the tractor never drives through the sled with the chain on. | met | `Turn.TheFendersStopATractorBackingIntoTheSled`, `Turn.TheFendersStopATractorBackingOffAfterThePull` |

## Rivals and career

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| K-1 | The game's events have 15 pullers: the player and 14 named rivals. | met | `EventLoader.LoadsTheShippedEvent`, `RivalDifficulty.*` |
| K-2 | No two of a rival's pulls are the same: grip, clutch timing and the sled pan's drag (the spot on the track) vary per pull, so the strongest two or three rivals fight for the win rather than one winning every event. The player's pull runs on the event's sled as it is (its draw is taken and dropped, so the rivals' draws do not depend on the player); varying it too is in `docs/future_improvements.md`. | met (measured with `tp_pull season`) | `Driver.EveryPullDiffersALittle`, `RivalDifficulty.*`, `LiveEvent.IsPlayEventWithThePlayerPullingAsScripted` |
| K-3 | A new tractor finishes near the back of its class's first level; a fully upgraded one wins at its last level; each stage of the garage climbs. | met | `RivalDifficulty.*` |
| K-4 | Finishing a level's championship first or second opens the level above, after at least two seasons on a level. | met | `Career.*`, `CareerLoader.LoadsTheShippedCareer` |
| K-5 | Upgrades only change parameters within the class's rules. | met | `Garage.*`, `ClassRules.*` |

## Controls

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| C-1 | Two layouts: easy (the clutch assist works the clutch and brakes) and pro (clutch and two brakes on their own). | met | `ClutchAssist.*`, `Pro.*`, `touch_test.gd` |
| C-2 | Steering by an on-screen slider or by tilting the phone. The touch controls take any finger the platform reports, also the negative touch indexes iPhone Safari's identifiers can arrive as in the web build. | met | `touch_test.gd` (the steering thumb a negative index) |
| C-3 | In the pull each brake pedal brakes its own side; off the pull they are latched together, and the two brake sliders move as one. The pull lasts until the sled stops: past a full pull, while the sled rolls on, the brakes are still each side's (R-12). | met | `Session.TheBrakesWorkTogetherOutsideThePull`, `touch_test.gd` |
| C-4 | The gear "−" stops at the lowest gear the gearbox may go to under way, and reaches N and R standing. | met | `touch_test.gd`, `turn_test.gd` |

## Game and screens

| ID | Requirement | Status | Checked by |
|----|-------------|--------|------------|
| U-1 | The event board lists every pull and the operator's verdict; the main round's best four pulls (the pull-off places) and the best of a pull-off round under way are in bold, so the distance to beat stands out. | met | `flow_test.gd` (bold rows), manual on screen |
| U-2 | The standings show each pull-off round's distance, not only the last. | met | `Event.PullOffsRepeatUntilOneIsStrongest`, `flow_test.gd` |
| U-3 | The full-pull banner and scoreboard stand over the class's own line (100 m), and stay there when the line in force moves to 110 m or to the pull-off line; the chalk line shows the line in force. | met | `scene_test.gd` |
| U-4 | Behind the menus a drawn tractor-pulling scene (MenuBackdrop) in place of the 3D venue, which is then not drawn. | manual | screenshots |
| U-5 | The game's icon is drawn from the menus' scene (`tools/make_icons.gd`), for the project, Android's launcher and the web game's home-screen icons. | manual | `export` job builds the APK and the web game with it |
| U-6 | For testing (to remove before a release): "+ € 1000" in the settings adds money to the bank. | manual | `docs/future_improvements.md` |
| U-7 | The event's results and the season's standings list every puller, not a top ten; the list scrolls with a finger, the player's row highlighted. | met | `flow_test.gd` (a fifteen-puller field on both boards; the board scrolling) |
| U-8 | Taking up the slack, the turn's line says how far the tractor still has to creep ("Creep forward: 0.85 m"), and a gauge under it shows the slack shrinking towards the band where it counts as taken up (green: within `turn::kReadyTautWithin_m`, 10 cm, of tight). In the band the slack is taken up without a jolt on the chain. | met | `Turn.TheSlackIsTakenUpInTheReadyBand`, `turn_test.gd` (the distance told, the gauge shown while taking up the slack), manual on screen |
| U-9 | The event's results show the points each place scores in the championship (none for a disqualified pull); they add up to what the season's standings gain. | met | `SeasonPoints.AStandingScoresItsPlaceUnlessDisqualified`, `flow_test.gd` |
| U-10 | The sled's weight box shows as many weights as its mass for the pull (one per 250 kg): the operator's lighter steps that take weight out, and a heavier box, show. | met | `scene_test.gd` |
| U-11 | The sled has two transport wheels beside the pan: the turn starts with the sled on them (the pan lifted, as it drove back), the operator lifts them before the pull (the pan on the track), and they come down to lift the pan once the pull is over and the box runs back. | met | `turn_test.gd`, `scene_test.gd` |
| U-12 | The engine is heard on the track: its firing pulses follow its speed (cylinders / 2 a turn, `cylinders` in the engine's file), louder with the throttle and as the load pulls it down, the turbo with the boost; silent when it stops and behind the menus. The settings turn it off (kept on the device). | met (the sound itself manual) | `EngineLoader.TheCylindersAreOptional`, `Engine.*`, `scene_test.gd`, `touch_test.gd` (the setting) |
| U-13 | The phone vibrates in the pull for what hands and seat would feel: a tick as the clutch passes its bite point, buzzes while the tires spin (stronger with more slip), a slow pulse while the engine lugs, a tick for a gear going in and two buzzes for one refused, the chain coming tight, the front landing after a wheelie, three rising buzzes for a full pull and a long one for the chalk line. The settings' Haptics menu switches each on or off (kept on the device). | met (the feel itself manual, on a phone) | `touch_test.gd` |
