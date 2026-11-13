# Future improvements

Ideas, known simplifications and things put out of scope along the way, so
they are not lost. Each entry says what it is and why it was deferred. When
something is consciously left out of a step, add it here; when it gets built,
move it out (the PR and the code are its record).

The roadmap in [CLAUDE.md](../CLAUDE.md) ("Later") stays the source of truth
for the big features; they are repeated at the end for completeness.

## Track and soil

- **Prepared top layer over a base.** Tracks are built as a base with a
  worked top layer. One soil layer is modelled for now; a top layer would
  improve the feel (grip that changes once the tires dig through it).
- **Wear during an event.** Each pull leaves ruts and loosens the surface,
  and the track dries out or is watered between classes. Needs a track state
  that persists between runs.
- **Lane-to-lane differences.** Lanes are prepared and worn differently;
  one lane per track for now.
- **Left and right differences.** The two halves of a lane can grip
  differently (a wetter side, a softer edge), which makes a tractor drift
  on its own. The hard and weak spots now lie somewhere across the lane
  (one under one rear side spins that side), but the soil itself is one
  surface across the width. Equal sides stay the default. Deferred to be
  built together with more track changes.
- **The lane wears.** Every pull leaves its tracks: ruts, a groove from the
  knives, firmer and looser lines. A crew sees more with every pull (the
  crew's report sharpens), but the spots themselves stay as laid; the lane
  could change with the pulls on it, and a line others have pulled could
  grip differently.
- **Weak spots only bite near the grip limit.** In most pulls the engine
  against the sled's drag limits the tractor, not the tires' grip, so a
  short patch with a few per cent less grip costs next to nothing (the
  crew's line across the lane gains tenths of a metre on the tracks with
  hard spots, about nothing on those with weak spots only). They matter in
  a launch with wheelspin and in the heavier classes; stronger or longer
  weak spots would make the line count more.
- **The pan's width.** Where the sled meets the spots its pan is taken as
  2 m wide for every sled (sled::kPanHalfWidth_m): the sleds' data give the
  pan's area, not its width.
- **Scripted pullers do not steer.** The rivals' crews choose where their
  sled starts, and the drivers then pull straight; one that drifts off its
  line does not correct.
- **Automatic test pull.** The sled operator sets the push-down from a test
  pull on the day's track. A reference tractor could run that test pull and
  choose the push-down for a target distance.
- **Tires digging in.** Under sustained wheelspin a tire digs down and its
  grip changes over time. Modelled without memory: the soil's fall-off past
  the grip peak, and a motion resistance that grows with slip (Brixius). A
  rut that deepens while the tire keeps spinning would need a state.
- **Tire deflection that follows the load.** Brixius' mobility number uses
  the tire's working deflection as a fixed size; a deflection that follows
  the load (and tire pressure as a tuning setting) would make light and
  heavy setups differ more.

## Sled

- **Knife penetration.** Knives cut at their set depth up to a soil
  strength of 2 MPa and shallower beyond it; how deep a knife actually
  reaches could follow from the load on the pan and the blade's shape.
- **Where thrown soil lands.** The rear tires' thrown soil lands in front
  of the pan at once, a fixed share of it. Where it falls (it lies on the
  track until the pan reaches it, or flies straight at the sled when the
  tires spin hard) and how much the pan's width catches could follow from
  the tractor's track width and wheel speed.
- **Drag after the push-down.** Once the push-down is fully engaged the
  drag grows only with the berm and ploughing; on hard tracks the sled
  stays touchier than on soft ones.
- **The box's travel and the push-down are set apart.** The weight box
  reaches its stop after a fixed 88 m of the sled's travel, the push-down
  wherever the track and the operator put it. Set later than 78 m, the box
  is at its stop before the push-down is fully on; set earlier, the box
  still moves after it. A real sled operator sets both, and a sled whose
  box gearing follows the push-down setting would keep the drag's top
  where the push-down ends.
- **Tracks matter less now that the drag tops out.** With the drag flat
  from 88 m, the six shipped tracks end within about 2 m of each other for
  the reference tractor (firm loam and ideal clay-sand give the same
  distance). Heavier classes and more along-track variation (see "Later")
  should spread them again; worth checking when milestone 6 adds classes.
- **The pan is drawn longer than it drags.** The sled shows its pan
  from just behind the front stop plate back to the middle of the sled,
  as real ones are, and the track's smoothed band starts at its back edge;
  the simulation has the pan's contact at one point, 5.5 m ahead of the
  wheels, where it feels the hard spots. A pan whose load spreads over its
  length would feel a spot as it slides over it, and the spots would line
  up with the smoothed band.
- **The push-down is drawn larger than it is.** The game lifts the sled's
  rear wheels 15 cm with the push-down fully set, so that it shows from
  the tractor; a real sled lifts them a few centimetres.
- **Speed-dependent pan resistance.** The pan's drag does not depend on speed.
  Right for the stock classes, which crawl because they lack the power to run
  fast; faster classes may need the pan to plough harder with speed. A
  fully built Sport 2.5 t tractor already pulls its sled at about 17 km/h
  in 7th; left out until pulls in the heavier classes look wrong.
- **Sled pitch.** The sled shares its weight between wheels and pan
  quasi-statically; it does not pitch or bounce.

## Tractor

- **The rear tires never leave the ground.** The rear axle is the pitch
  pivot and stays at the tire's radius, so a tractor that stands up onto
  its wheelie bar keeps its rear tires down: the bar takes only part of
  their load. A real tractor pulled too high (too soft tires, a grippy
  track) lifts its rear tires off the ground on the bar and spins out; in
  the game riding the bar costs the load it carries and the pad's drag
  (a steel skid, ETPC FS 2026 1.2.4), and the weights fully back are
  rarely wrong. A tractor stays up on its bar only while the sled's pull
  times the hitch's height outweighs its weight times the centre of
  gravity's distance ahead of the rear axle: the Sport 2.5 t with its
  weights back rides its bar for seconds (about 17°), the Sport 4.5 t
  (82 % on the rear axle) only pops up at the launch (about 15°). Real
  4.5 t pullers likely carry more on the rear; a tractor balance from
  measurements would settle it.
- **A tire that bites too hard does not bog the engine.** Pullers keep
  some wheelspin on a grippy track: a spinning tire works as a slip
  clutch, taking the sled's sudden jumps in drag (the knives biting, the
  push-down coming on, a soft spot) while the engine stays in its power
  and boost band; a tire that bites hands each jump to the engine, which
  drops out of boost and stalls. On most tracks the drag rises smoothly, so
  more grip never costs: a too-soft tire loses only by its lugs squirming.
  The dry, hard tracks now have a few mild hard spots where the drag jumps
  (track::HardSpots), and there a tire that slips a little more lets the
  engine dip less at each spot; but the spots are short and the tractor's
  momentum carries it through, so it does not yet decide the best
  pressure. Longer or stronger spots, or soft patches where the pan digs
  in, could make it count. The nose does not come up from the
  grip itself: the pull at the hitch is what the sled asks, so more grip
  only lets the tractor hold a heavier drag before it spins.
- **No roll, so no sideways load transfer.** Side forces do not shift load
  between the left and right rear tires; both sides carry half the rear load.
- **No differential lock.** The open differential is the only kind. Pulling
  tractors may lock theirs (rules permitting): both sides would then turn
  together, the brakes would steer much less, and the side on better
  ground would pull more.
- **Brakes hold by Newton steps.** A brake that would carry its side
  through rest holds it with the torque found by up to eight steps: Newton
  steps (one more solve per held side), then, once a single held side is
  bracketed, Illinois false position, which a kink in the step's response
  (the axle coming to rest) cannot throw off. Both sides held at once only
  get Newton steps. In gear a held step costs about 550 ns against 355 ns
  for a plain one (x86-64 release). Brakes have no fade or pedal feel.
- **Combined slip, simply.** A rear tire's side grip is cut by its traction
  (a friction ellipse against full bite) and by its slip (1 - |s|), and the
  side force does not take grip from traction in turn. A proper combined-slip
  model would let both trade off along the sliding direction.
- **Front tires' rolling drag along the body.** Their rolling resistance acts
  along the body, not along the steered wheels (a few degrees off).
- **The sled sideways, simply.** The pan's drag (friction and knives) acts
  against its sliding direction for the sideways part, while the forward
  resistance keeps its full value; the knives, sitting in their grooves,
  probably hold the sled harder sideways than plain friction does; the berm
  only drags along the track. The weight box is a point mass for the yaw
  inertia.
- **Tires on the chalk, simply.** A tire is out when its outer edge, taken
  at the axle and as wide as its section, reaches the line; the contact
  patch's own length and the tire's bulge are left out.
- **Steering is instant.** The front wheels take the asked angle at once; a
  steering rate (the driver turning the wheel) belongs in the input mapping.
- **Rear tire compliance and sinkage.** The rear axle stays at the tire
  radius; a springy rear tire would soften launches and let the tractor
  squat, and a tire digging its own hole, which tilts a real tractor back
  and helps the front up at the end of a pull, does not pitch the body.
- **Centre of gravity moving with pitch.** When the body pitches, the centre
  of gravity's shift along the track does not feed back into the longitudinal
  motion (second order).
- **Driver mass and ballast.** Class weight includes the driver, and tractors
  are ballasted to the class limit; neither is modelled separately yet.
- **Class rules not checked.** Engine layout (cylinders, turbo), rubber tires
  only and the wheelie bar's strength are rules the simulation does not
  model, so the class check skips them.
- **Heavier classes.** Each class needs its own tractor, tire and sled data;
  in stronger classes wheelspin matters more than engine torque.

## Gearbox

- **The powershift step's clutch packs.** A gear shifted down under load
  goes in at once: the axle and the wheels keep their speed, the input
  shaft stands in for the packs' side and takes the new gear's speed, the
  engine giving that momentum, and the main clutch's slip stands in for
  the packs'. Since the input shaft really stays with the engine, that
  bookkeeping adds a little energy (some 0.2 kJ for 6th to 5th: the input
  shaft gains about 0.95 kJ, the engine gives up about 0.8 kJ), against the
  clutch's several kJ of slip. A real step slips its own packs over a
  set time, with the torque handed from one to the other, at their own
  capacity rather than the main clutch's.
- **What a big step does.** The pulling gears 5th to 7th are about 10 %
  apart, but 5th to 4th asks the engine to turn 58 % faster; that energy
  comes from the tractor's motion, through the slipping clutch and the
  tires, which brake. The tractor slows while the sled rolls on: the chain
  goes slack for a moment and then snatches. That is the physics of such a
  big step, not of the model; the shipped gearbox has eight gears, a real
  pulling tractor's powershift more and finer ones, which would be worth
  data of their own.
- **Which gear starts the sled is a cliff.** As the clutch comes in, a
  gear too high pulls the engine below its turbo's boost and it stalls
  within metres; one gear lower pulls nearly as far as any. The scripted
  driver lets the clutch in over a fixed time, and a longer slip does not
  move the cliff. Real drivers start a gear higher with a slipping clutch
  and more revs; a clutch or launch model with more to it (a slipping
  clutch held at a set engine speed) would let technique move it.
- **The governor gives speed, not metres.** With the governor at 2500 rpm
  the tractor pulls faster in the same gear, but in the scripted pulls the
  tires' grip, not the engine, ends the pull, so it barely changes the
  place. Its worth to a player is the higher gear and the speed; a sled
  whose drag grows with speed, or a scoring that counts time, would give
  it more.
- **The rivals' parts are drawn one by one.** Each rival draws each part
  of its tractor on its own, so few have every part at the top: the field
  is tuned by its ranges to sit a step above the garage, not by drawing
  whole careers. Drawing a rival's career level and its parts around it
  would make the field's spread easier to read.
- **Up under load.** Only shifts down, a gear or two
  (`load_downshift_gears`, the garage's powershift), once a pull
  (`shifts_per_pull`); a powershift that also shifts up under load, or a
  second shift a pull as an upgrade, are left out.
- **When a pull is under way.** Past `vehicle::kUnderWay_m_s` (0.3 m/s) the
  gearbox goes no lower than its floor, the same for every class. If classes
  ever set it differently, it belongs in the class rules in `data/`.
- **The rivals shift by the rev counter alone.** A scripted driver shifts
  once the engine falls below their share of its rated speed; one who also
  reads the sled's distance, the wheelspin or the chain would shift later on
  a track where the tires spin first (dry hard clay).

## The turn

- **The held sled never moves.** Before the flag the sled stands still
  however hard the chain pulls; a real one is held by its operator and its
  pan, and a tractor pulling too hard could drag it.
- **The chain eases a braked tractor back.** With the drive out a
  stretched chain pulls the tractor back physically, but one held on its
  brakes (the easy layout holds it so) only creeps, so the jolt that ends
  the creep could stay in the chain. Waiting for the flag with the drive
  out, the turn moves the standing tractor back to the chain's length at
  5 cm/s (`turn::kChainEasesBack_m_s`), its brakes on or not; the flag waits
  until it has.
- **In the pull the drawbar never pulls back.** There the drawbar is taken
  as the sled's drag, which holds the tractor but never rolls it back
  (`vehicle::travel_direction`); only off the pull does the chain alone
  pull a tractor with the drive out back. A tractor standing on a
  stretched chain in the pull, the clutch pressed, stays put until the
  pull ends.
- **No collisions.** Apart from the sled's fenders while the chain is on
  (backing up to it, and backing off after the pull), the tractor drives
  through the sled, the fence and the parked tractors; nothing stops it
  but the driver.
- **The ground beside the track.** Off the lane, in the parking and on the
  way back, the tractor rolls on the track's own soil; grass or a gravel
  lane would grip and roll differently.
- **The sled drives back by the clock.** It goes straight back at a steady
  2 m/s (`sled::kDriveBack_m_s`), without an engine or physics of its own,
  and through anything in its way.
- **The official's flag.** Green after the tractor has stood ready for 2 s,
  the chain tight; there is no false start (moving before the flag only
  lowers it again) and no time limit on the way to the sled or for the
  flag.
- **The red flag is a signal only.** After the pull the officials wave red
  and the driver should close the throttle, but nothing counts against one
  who does not; the result is the sled's, at its stop.
- **One reverse gear, and an instant restart.** A real gearbox may have
  several reverse gears or a shuttle; a stalled engine starts again the
  moment the clutch is pressed.

## Tuning

- **Tire pressure and the rolling radius.** A softer tire rolls on a
  slightly smaller radius, which lowers the gearing and the rear axle; the
  pressure only changes the deflection and the traction limit
  (tire::Pressure). Only the rear tires' pressure is set; the front tires
  and the pressure's effect on side grip stay as built.
- **The ballast as a point.** The weights move only along the tractor, at
  the centre of gravity's height and on its centre line; real front
  weights sit higher and further out, which changes the centre of
  gravity's height and the pitch inertia more. Wheel weights are not a
  separate setting.
- **The setup screen.** The weights and the pressure are kept for the
  season, not per track, and the screen shows the rear axle's share at
  rest, not what it becomes in the pull. The controls are picked in the
  settings only.
- **Rivals don't tune.** The scripted pullers pull as built; a driver who
  reads the track could set the pressure and the weights for it, as they
  pick a gear.

- **Rivals don't upgrade.** The field is drawn once a season and stays as
  it is; the player climbs through it by upgrading. Rivals buying parts
  over a career, or a field that follows the player's progress, would keep
  a fully upgraded career challenging.
- **The difficulty is measured with a scripted player.** `RivalDifficulty`
  places a player who drives as the reference driver does, always in the
  best gear. How a person places, with touch controls, is not measured.

## Competition

- **Pull-off modes.** A pull-off is run on a heavier sled, again among
  those who make a full pull again. Some events use a floating finish
  instead (pull until you stop, at most 130 m, on a sled heavy enough that
  pullers stop around 100 to 110 m), or keep going until the strongest is
  found. Could become an event option.
- **How drivers read the track.** A driver who reads the track right picks
  the gear that really pulls furthest (found by simulating each gear); one
  who misreads it is one gear off. Real drivers learn from the test pulls
  and the pullers before them, and may change gear between pulls.
- **Changing the sled between pulls.** The operator changes the sled only
  after a test pull. When the test pullers happen to stay short of the
  target, the strong pullers after them can make many full pulls (11 at one
  dry-clay event in the club season); a real operator might step in sooner.
- **Smarter sled operator and pullers.** The operator learns what a step is
  worth from each change, but treats every test puller as equal (a strong
  puller after a change looks like a big effect) and only changes knives
  and push-down. Scripted pullers keep a test pull only when no one has
  beaten it.

- **Scripted pullers that keep the lane.** Event and season pullers drive
  straight: on a track the same across its width nothing pushes them off
  line, so no one is disqualified. Pullers that steer and brake to stay in
  the lane come with the left and right track differences.
- **Disqualification in a pull-off.** A pull-off pull out of bounds counts
  0 m in that round, with no mark of its own in the standings.
- **The rules on a disqualified test pull.** A disqualified test pull
  counts 0 m for its puller (no keep-or-re-pull), the sled stays and the
  next puller tests it; a disqualified pull scores no season points. These
  are reasonable readings, not checked against the NTTO rule book.
- **Seeds of consecutive seasons.** An event's seed is SplitMix64 of the
  season's seed plus the event's number (`field::event_seed`, as in
  `tp_pull season`), so season seed s + 1's first event draws as season
  s's second. Harmless while each launch picks a random seed; a career
  that numbers its seasons should mix the season's seed first.
- **The rivals' gear trials.** The rivals choose their gear by trying
  each one (three gears of about 35 s each): a few seconds of simulation
  for nine rivals on a phone, spread over the frames of the start screen
  and the board before the first pull.
- **Pull-offs between several tractors.** A pull-off is run per tractor; the
  ranking of several full pullers on the pull-off sled is not modelled.
- **The operator's expectations.** Each tractor's expected distance comes
  from its gear trials on the class's sled, which stop at the event's
  maximum distance: several tractors reaching it tie, and a tractor that
  far outpulls the class counts in the average only that far. The operator
  takes the gap to the average as the same on every sled, where on a sled
  far too heavy the field's distances bunch up.

## Controls

- **Clutch heat.** The easy-mode assist may slip the clutch for seconds at
  full engine torque to keep the engine from stalling. A real clutch
  would overheat and fade; the clutch model has no temperature.
- **A smarter assist.** The assist's full-throttle guard speed is one share
  of the rated speed for every engine, and it does not know the gear or the
  sled. It could take the guard from the engine's torque curve (its peak),
  or ease the clutch in more gently in low gears to keep the front down. At
  a touch of throttle in a high gear against a heavy sled it hunts slowly
  around the guard (about 4 reversals a second); a little hysteresis would
  settle it if that shows on screen.
- **A launch that never gets going.** In top gear with a touch of throttle
  the engine cannot move the sled: the assist slips, the sled creeps a
  centimetre and stops, and that first stop ends the run at 0.01 m. Right
  physically, but a beginner may read it as the game failing them. The
  easy front end could not count a stop before the sled has really moved,
  or hint at a lower gear.
- **Speed-sensitive steering's model.** The limit takes the forward speed
  and a kinematic steady turn; it ignores sideways sliding, and scales the
  easy layout's inner brake as the wheels rather than against the same
  sideways limit.
- **Steering back to centre.** The steering returns to straight ahead at
  the same rate it turns. A real tractor's wheels self-centre faster under
  way (caster); a separate return rate could follow.

## Platforms

- **Android ARM tests on a device.** CI builds for arm64-v8a but runs the
  tests on an x86_64 emulator: GitHub's runners cannot emulate an ARM phone
  quickly. Determinism on ARM is covered by the Linux arm64 jobs and
  Android's runtime by the emulator; `scripts/android_test.sh` runs the arm64
  build on a real phone by hand. That split holds because `core/` takes
  nothing from libm but `std::sqrt`, which is correctly rounded everywhere
  (its sine, cosine and exponential are its own, in `tp::math`). Calling
  libm's transcendental functions from `core/` would reopen it: Android's
  libm on ARM could then differ from glibc's.
- **The speed budget on a real phone.** The phone budget (50 µs per step,
  `docs/performance.md`) is checked by hand with `scripts/android_bench.sh`;
  CI's ARM runner is a server core, so its limits catch regressions rather
  than prove the phone budget. A device farm could check it in CI.
- **The C++ runtime for the Godot extension.** The Android presets link
  libc++ statically, which suits standalone test binaries. A GDExtension
  shares a process with Godot, which may want `c++_shared`; decide with the
  binding layer.
- **The extension on a phone.** CI builds the Godot extension for Android
  (arm64 and x86_64) but loads it only in the desktop Godot (under Xvfb in CI). It
  runs on Android once the game is exported (milestone 5, step 5); its
  libc++ is static, see "The C++ runtime for the Godot extension".
- **Godot's crash on discovering the extension.** Godot 4.7.2 crashes as it
  quits when its editor found a GDExtension while scanning the project
  (any registered class). The build therefore lists the extension in
  `.godot/extension_list.cfg` so it loads at startup, before
  `scripts/godot_test.sh` runs the import. Drop the workaround once Godot fixes it.
- **Release libraries of the extension.** The presets build godot-cpp's
  `template_debug` libraries only, which the editor and debug exports
  load, and `tractor_pulling.gdextension` lists only those. Release
  (`template_release`) presets and entries come with the Android export
  (milestone 5, step 5).
- **GDScript checks.** The GDScript runs in CI (the scene and run tests)
  but has no linter or formatter there; gdtoolkit's `gdlint` and `gdformat`
  would add a Python dependency, worth it as the scene and UI grow.
- **A stylized look, not a realistic one.** The tractor, the sled and the
  people are built in code from primitives; there are no textures beyond
  procedural noise, rows and lug prints, and no sound. The people do not
  turn their heads to follow the tractor, and the berm is a smooth mound.
  A modelled tractor and sled would replace the views' meshes; their
  interfaces (`build` from the dimensions, `show_snapshot`) stay.
- **Marks are cleared, not raked.** Between pulls the ruts and the band
  are gone at once; the track crew's tractor raking the lane is not shown.
- **The visuals are not measured on a phone.** The venue is a few thousand
  instances in MultiMeshes, one shadowed sun (60 m of shadow distance),
  glow, and a few soft-particle emitters; the frame time on a mid-range
  phone is not measured yet, nor is a lower-detail setting there to fall
  back on (fewer people, no glow, no dust).
- **The chain is straight.** `ChainView` draws its links on a straight
  line from hitch to hook, darker as the tension rises, even when the
  chain is slack and should sag.
- **The game's season.** The season is saved between events only: an
  event left halfway starts again from its beginning when continued (the
  rivals pull the same, from the seed, but the player's pulls in it are
  gone), and a practice pull is never kept. One season is saved at a time;
  a save of another version or season, or one the shipped data no longer
  fits, gives way to a new season with only a warning on the console (a
  line on the start screen would be kinder). The
  player pulls the class's reference tractor as the garage has it, with no
  other tractor to choose; before a pull they tune the weights and the tire
  pressure only. An event's pull
  counts as it is: no pulling again outside the rules; practice pulls are
  free, on the event's sled as it is. The board shows the last ten lines; the rivals have names
  but no tractors or faces of their own. The gamepad has no brakes yet.
- **The pedals' feel.** The clutch slider moves the pedal, not the clutch's
  engagement; its slipping range is marked, but nothing on screen shows
  how much it is slipping now (the HUD shows the pedal). The brakes act at
  once.
- **Tilt steering.** Its range (35° either way) and response curve are
  fixed, with no sensitivity setting; the centre is taken at Pull only,
  not mid-pull; and
  the sensor's readings are not filtered beyond the core's steering rate.
  Which way Android reports the roll is taken from its sensor convention
  and checked with a simulated sensor only: a real phone may steer the
  wrong way round, to fix with one sign. The gamepad's stick is not read
  while tilting.
- **Control layout options.** The touch controls sit at fixed places and
  sizes in the 1280×720 base layout, stretched to the screen: no
  left-handed mirror, resizing or haptics. A gamepad is
  read from the first device only.
- **Screenshots out of CI.** `tests/screenshots.gd` saves a PNG per camera
  for reviewing the look by hand; CI does not run it or compare images,
  since software rendering differs between Mesa versions.
- **Cameras.** Three fixed offsets (chase, side, overhead) follow the
  tractor with exponential smoothing; no free camera, zoom, sled camera or
  replay cuts.
- **The snapshot as a Dictionary.** `TractorPullRun.get_snapshot()` builds a
  Dictionary every frame: an allocation on Godot's heap, outside the
  simulation step and well under a frame's budget. If it shows in a frame
  profile, typed properties or a packed array instead.
- **What the session shows.** A snapshot (`tp::session`) holds what a
  first 3D scene needs: the tractor's and sled's place, heading and pitch,
  the rear wheels' speeds, the engine and the chain. The front wheels' speed,
  the tires' sinkage, the berm and the thrown soil are left out until the
  scene draws them. Headings blend as plain numbers, which is wrong only
  across ±π, far outside a lane.
- **Windows paths in `tp::data::resolve`.** Only `/` and `scheme://` count
  as roots: on a Windows desktop `C:/data` is taken as relative (`..` could
  climb past the drive, and an absolute `C:/x.toml` in a data file would be
  joined to the base), and a UNC `//server/share` loses its leading `//`.
  Android and Godot's `res://` are unaffected; teach it drive letters and
  UNC roots with a Windows build.
- **One run per session.** The session plays one run; each of the
  player's pulls at an event is a new session on the sled as it is then.

- **Debug exports only.** The APK and the Windows game are debug exports
  with the extension's debug libraries, the APK signed with a committed
  debug key. Publishing needs release libraries (godot-cpp's
  template_release), a release key kept as a CI secret, an app bundle for
  Google Play, and the Windows game signed. No icon of its own yet
  (`application/modify_resources` is off: it needs rcedit).
- **The web game is only started in CI, in Chrome.** The export job
  serves it and starts it in headless Chrome (scripts/web_game_test.sh:
  its season loaded through the extension, no console errors); no
  scripted run is played through it, and Safari, which iPhones use, is not
  tried. The debug templates make it about 50 MB (most of it Godot's own
  engine); release templates and libraries would roughly halve that.
- **No tilt steering on the web.** Godot's web build reads no motion
  sensors (Input.get_gravity() is zero), so the web game offers slider
  steering only. Tilt there needs the browser's DeviceMotionEvent passed
  in through JavaScriptBridge, and on iPhones a permission prompt from a
  tap (DeviceMotionEvent.requestPermission).
- **The Windows game is only started in CI.** The windows-game job starts
  the export headless (its season loaded, no errors) and checks that the
  Windows tp_pull gives Linux's results (scripts/platform_runs.sh: the
  scripted runs' hashes and a whole event); no scripted run is played
  through the extension in the Windows game itself. Wine 9 cannot run
  Godot 4.7 (it runs tp_pull.exe, with Linux's results).

## Performance

- **Derived chassis constants at setup.** The pitch step recomputes a few
  derived values (front tire preload, pitch inertia about the axle) every
  step. Moving them into a model built at setup would save some of its
  ~60 ns.
- **A lighter rear step.** Steering, the differential and the brakes took a
  vehicle step from about 210 to 400 ns (x86-64 release, still about 0.05 %
  of a core at 1 kHz): each side's traction, rolling resistance, thrown
  soil and side grip are worked out separately, and the solve advances a
  copy of the driveline to read the end speeds. Sides on the same ground
  share their footing already; the rest could share more, or skip the
  difference motion while it and both brakes are at rest.

- **The sled's sideways step.** The sled's sideways motion added about
  100 ns to a pull step (about 580 ns against 490 ns, x86-64 release): its
  wheels' side grip, the pan's sliding speed and the headings' sine and
  cosine. The hook's position and the sled's heading could share one
  sine and cosine per step.

- **The big screen's second view.** The LED screen at a club or big event
  draws the scene a second time from its TV camera, at 384 × 216 and 12
  times a second, to keep a phone's frame. If that still costs too much on
  a phone it could drop lower, or show a still frame between redraws.

## The venue

- **A replay on the big screen.** After a run the screen shows the day's
  standings; a real event often replays the run first. That needs the
  run's snapshots kept and played back through a second tractor and sled
  view, or the screen's frames kept as video: left for later.
- **The pile of dirt is scenery.** It stands past the 30 m run-off, where
  no sled reaches; nothing in the simulation stops on it.

## Garage and career

- **The parts are values, not parts.** An upgrade sets one value (a torque
  scale, a traction limit, a ballast mass, a drawbar height, a downshift
  step); a new tire does not change the tire's size, rolling radius or
  inflation range, and a wider tire not its section width or the lane it
  needs. A heavier engine part, a wheelie bar, a clutch, final drive ratios
  and gear ratios are not offered as upgrades; a choice (garage::Choice)
  can swap a whole engine, rear tire or driveline, but a rear tire must
  keep the tractor's radius (the rear axle's height is the tire's).
- **Ballast at the centre of gravity's height.** Weights bought sit where
  the tractor's weights sit, at the height of its centre of gravity, so the
  centre of gravity moves along the tractor only.
- **Three classes on the ladder.** The tractor path's career has the
  standard 3.5 t class (one level), Sport 2.5 t (promo and top) and
  Sport 4.5 t (its three levels) so far; the classes above come a class at a time (docs/career.md).
  Supersport 3.6 t comes next (docs/plans/garage_choices.md): the
  engine choice (7.0 l at 2700 rpm or 6.06 l at 3200 rpm, each with its
  own build) and the tires and gearbox choice (traction or wheelspin), on
  the garage's choices; then Supersport 4.5 t with engine tuning.
  Supersport 3.4 t is left out for now.
- **Sport's engine rules are not checked.** Sport and up limit the
  engine's build (cylinders, displacement, valves, the turbo's inlet, no
  intercooler at 2500 kg), not its power; the class rules have no PTO
  limit for them, and the core checks only mass, engine speed and hitch.
  The Sport engines' 350 and 780 hp are set from videos of top tractors
  (their pace and wheelies), not measured on a dyno.
- **Puller-tire grip is an estimate.** The Sport tires' grip (1.5 times
  a farm tire's, building at a third of its rate with slip) is set with
  the engines from the videos and so that a too tall gear bogs and a too
  low one spins out, not from measurements.
  Every stage of the Sport garages' tires, the worn farm tires too, uses
  the puller tire's curve; only the limit changes.
- **The operator's box step is one share for every class.** A lighter
  step that takes weight out of the box (once the knives are out and the
  push-down is past the end) takes a tenth of it, a constant in the core
  (event.cpp); heavier classes may want their own, from the sled's data.
- **The operator expects the player's tuning at the start of the event.**
  His gear trials see the player's tractor as tuned when the event starts;
  a ballast or pressure change later in the event is not in them.
- **The scripted drivers never shift down in the launch.** A rival in too
  tall a gear eases the clutch back while the revs are low, but shifts
  down only once the clutch is fully in and the engine has come back; a
  player can shift down at once.
- **The roll bar is drawn only.** A tractor's protection (cab or roll
  bar) changes the picture, not its mass, centre of gravity or the
  driver's view (the cab camera is the same).
- **No relegation.** A level or class, once reached, is kept; the NTTO's
  Pro Stocks relegate group I's twelfth to group II.
- **The first powershift step can cost a little.** In 5th, one gear down
  under load ends a standard-class pull about 0.5 m shorter than none
  (93.0 against 93.5 m in the levels' tuning): 5th to 4th is a big step
  (see "What a big step does"). A player who buys it gains nothing in 5th
  until the tractor pulls 6th.
- **No resale.** An upgrade cannot be sold or taken back. Prize money is paid by place only: no start money for
  everyone, no costs per event (fuel, entry fees, repairs), no sponsors.
- **The rivals do not upgrade.** Their tractors are drawn once for the
  season around the reference tractor.
- **The rivals do not set up for the track.** A rival keeps the reference
  tractor's parts and the weights, tire pressure and hitch the field
  draws for it, on every track; a good puller would move the weights back
  on the loose sand and forward on the heavy clay.
- **A high hitch costs the scripted pullers only through the wheelie
  bar.** They drive straight, so the steering lost with the front up
  costs them nothing; the lane is the player's worry.
- **No cost or time to swap a part.** An owned alternative is fitted for
  nothing between events; a real swap of a turbo or an engine takes a
  day's work.
- **The player's pull does not vary from spot to spot.** The rivals'
  pulls each draw the grip, the clutch timing and the sled pan's drag
  (field::PullToPull); the player's run uses the event's sled as it is.
- **"+ € 1000" in the settings is for testing.** It puts money in the
  bank at a tap (TractorPullEvent.add_money); take it out, with "Start",
  before a release.

## Roadmap features (from CLAUDE.md "Later")

- Track changes: left and right differences, a lane that wears; pullers that
  keep the lane.

## The 15 t sled

- **The push-down is a wall.** With the whole 15 t sled on the pan once
  the push-down is fully on, every tractor stops within a few metres of
  where it engages, so a field finishes closer together than on the light
  sled the game had before, and the difficulty bands in the middle of the
  garage are a place or two wider (`RivalDifficulty.*`). A gentler
  push-down (a longer ramp, or one that takes only part of the wheels'
  load) would spread the field by power again; tried with a 25 m ramp it
  spread it by about a third and changed the wheelies, so it is left for
  a balancing pass.
- **The weights' best position is the same on every track.** On the 15 t
  sled a fully built Sport 4.5 t pulls furthest with its weights a
  quarter forward everywhere; the track decides only what the wrong
  setting costs. Tracks that want the weights elsewhere (a heavy clay
  that lifts the nose) would bring back a reason to move them per track.
- **The spots barely show on the heavy sled.** A hard spot bites the pan,
  which carries a small share of the 15 t until the box comes forward,
  and a weak spot costs grip the push-down then takes anyway: the crew's
  line gains tenths of a metre. More impact (a weak spot under one tire
  turning the tractor) is planned.
- **The sled's own rolling dominates the start.** Its eight tires roll
  with about 6 % of its weight on a prepared track (1/Bn and their own
  losses), some 13 kN, most of what a farm tractor pulls at the start, so
  the farm classes pull in 4th and 5th only and slowly (about 7 to 11
  km/h). Real standard-class pulls may be quicker; the farm engines are
  not yet set from videos.
- **The crew's hitch tip is one rule.** The crew chief says to raise a
  hitch in its lowest hole, which pulls clearly further on every track
  but the moist heavy clay, where a low hitch may gain about a metre. The crew
  could try the player's tractor at the top and lowest holes in its gear
  trials (two more pulls, spread over the frames as the rivals' work is)
  and advise per track, or re-try the gears when the player changes the
  setup on the pit board.
