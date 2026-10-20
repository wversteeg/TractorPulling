# Architecture

Diagrams of the simulation core and data loading. Sources are PlantUML files
in [`diagrams/`](diagrams/); the rendered SVGs in [`diagrams/svg/`](diagrams/svg/)
are committed so they can be reviewed directly on GitHub. Regenerate them with
`scripts/render_diagrams.sh` (CI fails if an SVG is stale).

## Class diagrams

### Simulation core (`tp::core`)

Pure C++20: no exceptions, no RTTI, no heap allocation in the step. All state
is plain data. Free functions operate on `Params`, `State` and `Input`.

![Core classes](diagrams/svg/core_classes.svg)

### Clutch (`tp::clutch`)

Couples two shafts through their `ShaftPrediction`s; see the coupling
sequence below.

![Clutch classes](diagrams/svg/clutch_classes.svg)

### Driveline (`tp::driveline`)

A rigid gear train: one degree of freedom in gear, two in neutral. Mesh
losses apply in the direction power flows, which is why `ShaftPrediction`
has a response for each torque direction.

![Driveline classes](diagrams/svg/driveline_classes.svg)

### Soil (`tp::soil`)

A track's soil, from clay fraction, moisture and compaction, gives the
surface the tires and the sled work on: cone index (strength), a grip factor
(dust on dry clay, a slick film on wet clay) and how fast grip falls off once
a tire spins (sand digs, wet clay smears).

![Soil classes](diagrams/svg/soil_classes.svg)

### Track (`tp::track`)

A track's soil and how its moisture and compaction change along it (a wet
patch, a worked stretch). Sampled into a fixed table at setup, so the step
only looks up and blends the surface under each contact.

![Track classes](diagrams/svg/track_classes.svg)

### Tire (`tp::tire`)

Longitudinal force from slip on soil, after Brixius: the soil's cone index
and the tire's size and load give a mobility number that sets grip, load
sensitivity and motion resistance. The force is linearized over a step so
the stiff contact can be solved implicitly. Side grip follows the same
mobility number: the contact patch deflects sideways like a spring and
relaxes as the tire rolls (relaxation length), so it works from a standstill
up, and a driven tire's traction and wheelspin use up its side grip.

![Tire classes](diagrams/svg/tire_classes.svg)

### Chassis (`tp::chassis`)

The body moving along the track and pitching about the rear axle, and in the
ground plane moving sideways and turning (yaw) under the front tires', rear
tires' and drawbar's side forces. The front tires and the wheelie bar are
one-sided spring-damper contacts; the rear tires carry the rest of the
weight.

![Chassis classes](diagrams/svg/chassis_classes.svg)

### Vehicle (`tp::vehicle`)

The whole tractor: components, the validated `Model`, and the exact chain
solve that couples engine, clutch, driveline, tires and body each step. The
input steers the front wheels; a front axle in the air (a wheelie) steers
nothing. An open differential drives the left and right rear wheels, each
side with its own brake: the sides' common motion goes through the chain
solve, their difference is solved alongside it, and a brake that would carry
its side through rest holds it there instead (Coulomb friction, like the
clutch).

![Vehicle classes](diagrams/svg/vehicle_classes.svg)

### Sled (`tp::sled`)

The transfer sled: rear wheels and a pan share the frame and the weight box
by their levers. The box moves forward with the sled's travel and the
push-down lifts the wheels, so the pan drags harder along the track. On
soft ground the pan ploughs and the wheels sink in. Knives under the pan
cut grooves, and soil the tractor's tires throw back piles up in front of
the pan as a berm it has to push. Sideways it is a trailer on the chain: its
rear wheels keep its tail in line, while the pan, sliding forward, resists
sideways only weakly, so the sled swings behind a tractor that corrects its
line (the dog-tail).

![Sled classes](diagrams/svg/sled_classes.svg)

### Pull (`tp::pull`)

A pull: the tractor and the sled joined by the chain, which pulls along its
own line between the hitch and the hook, on a track whose ground each
contact reads where it is.

![Pull classes](diagrams/svg/pull_classes.svg)

### Run (`tp::run`)

A run from the start line to its result: it ends at the sled's first stop, or
with a full pull at the end of the track, which calls for a pull-off on a
heavier sled. A tractor tire touching a chalk line before the full-pull line
disqualifies the pull (out of bounds, 0 m); the sled may cross the lines.

![Run classes](diagrams/svg/run_classes.svg)

### Driver and field (`tp::driver`, `tp::field`)

The competitors: a scripted driver's plan (gear and clutch technique) and a
seeded field of pullers, each the reference tractor and driver varied within
ranges and kept within the class's rules.

![Field classes](diagrams/svg/field_classes.svg)

### Controls (`tp::controls`)

How a front end's controls become the vehicle's input. The pro layout has
every control on its own. The easy layout has one steering control that
brakes the inner rear wheels past full lock, and a clutch assist that works
the clutch: it slips it to keep the engine near peak torque instead of
stalling. The assist's state is plain data, like the simulation's.

![Controls classes](diagrams/svg/controls_classes.svg)

### Session (`tp::session`)

One run as the game plays it, and the interface the Godot binding will use:
a setup given once, the player's controls and the frame's real time in,
snapshots to draw out. The frame time decides only how many 1 ms steps run,
never their length, so the same controls on the same steps give the same run
however the time is split into frames. It wraps the run, the controls and
the fixed-step clock; `tp_pull run` plays through it too.

![Session classes](diagrams/svg/session_classes.svg)

### Event (`tp::event`)

One class at an event, pull by pull: a seeded running order, the test pull
and the sled operator's changes, test pullers keeping or re-pulling, the
full-pull line (100 or 110 m), qualifying for the pull-off and the
pull-offs. Plain rules logic: the caller runs each pull and records where
the sled stopped.

![Event classes](diagrams/svg/event_classes.svg)

![Event sequence](diagrams/svg/event_sequence.svg)

### Season (`tp::season`)

A season's championship: the same field at every event, each on its own
track; each event's places score points (20, 17, 15, then one less per place
for the club season), the most points win and more event wins break a tie.

![Season classes](diagrams/svg/season_classes.svg)

### Class rules (`tp::rules`)

A pulling class's technical limits and the check whether a tractor may
compete. Competition rules, not physics: they are data, checked at setup.

![Class rules](diagrams/svg/rules_classes.svg)

### Data loading (`tp::data`)

Parses TOML from `data/`, converts to SI units and reuses the core's
validation rules. Never used inside the simulation step. Files are read
through a `ReadFile` function the front end supplies (Godot's `res://` lives
inside the app package, out of `std::filesystem`'s reach); the loaders that
take a `std::filesystem::path` read from the disk.

![Data loading classes](diagrams/svg/data_io_classes.svg)

## Sequence diagrams

### A full run

The driver's inputs step the run until it has a result; after a full pull the
tractor reruns on a heavier sled.

![Full run](diagrams/svg/full_run_sequence.svg)

### One pull step (1 kHz)

The chain's force from where the hitch and hook are, then the tractor and the
sled advance under it, equal and opposite.

![Pull step](diagrams/svg/pull_step_sequence.svg)

### One vehicle step (1 kHz)

The main flow: predict every body, solve the chain of couplings exactly,
advance every body.

![Vehicle step](diagrams/svg/vehicle_step_sequence.svg)

### Weight on the axles and pitch

Where the axle loads come from at the start of a step, and how the body
pitches at its end under the reaction of the axle torque. Wheelies are not
scripted: they appear when that reaction outweighs the tractor.

![Pitch step](diagrams/svg/pitch_step_sequence.svg)

### One engine step (1 kHz)

![Engine step](diagrams/svg/engine_step_sequence.svg)

### Engine, clutch and driveline coupled

The clutch solves for the torque that makes both shafts' speeds match at the
end of the step, limited by its capacity. This keeps the stiff friction
contact stable and free of chatter at 1 kHz.

![Clutch coupling](diagrams/svg/clutch_coupling_sequence.svg)

### Easy controls

One step with the easy layout: the assist reads the engine speed and the
gear in, moves the clutch's engagement, and turns it into a pedal through
the clutch's own curve.

![Easy controls](diagrams/svg/easy_controls_sequence.svg)

### Changing gear

![Gear shift](diagrams/svg/gear_shift_sequence.svg)

### Loading a component definition

![Load component](diagrams/svg/load_component_sequence.svg)

### Frame loop

How a front end drives the fixed-step simulation from a variable frame rate,
through the session.

![Frame loop](diagrams/svg/frame_loop_sequence.svg)
