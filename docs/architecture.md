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

`field::LiveSeason` is a season as the game plays it: the player is puller
0, on the class's reference tractor, among a field drawn for the season;
each event is a `LiveEvent` with the draws the tools' season gives it
(`event_seed`), scored in the championship once finished.

`field::LiveEvent` is the event as the game plays it: `play_event` with one
puller the player. It makes the same draws, so with the player pulling as a
scripted puller would it is `play_event`, line for line (a test checks it).
The rivals' gear trials and pulls run in slices of a given number of steps,
so a game spreads them over its frames, and wait when the player is due. It
does not allocate.

![Field classes](diagrams/svg/field_classes.svg)

### Controls (`tp::controls`)

How a front end's controls become the vehicle's input. The pro layout has
every control on its own. The easy layout has one steering control that
brakes the inner rear wheels past full lock, and a clutch assist that works
the clutch: it slips it to keep the engine near peak torque instead of
stalling. Both layouts' steering has a dead zone in the middle and a rate
(a touch slider can jump), from their parameters; the pro layout's default
parameters steer as the control is, for scripted drivers. The assist's
state, the steering's too, is plain data, like the simulation's.

![Controls classes](diagrams/svg/controls_classes.svg)

### Session (`tp::session`)

One run as the game plays it, and the interface the Godot binding will use:
a setup given once, the player's controls and the frame's real time in,
snapshots to draw out. The frame time decides only how many 1 ms steps run,
never their length, so the same controls on the same steps give the same run
however the time is split into frames. It wraps the run, the controls and
the fixed-step clock; `tp_pull run` plays through it too. Tools and tests
look at the whole simulation inside it through `session::Inspector`
(`inspect.hpp`), which the binding does not use.

![Session classes](diagrams/svg/session_classes.svg)

### Event (`tp::event`)

One class at an event, pull by pull: a seeded running order, the test pull
and the sled operator's changes, test pullers keeping or re-pulling, the
full-pull line (100 or 110 m), qualifying for the pull-off and the
pull-offs. Plain rules logic: the caller runs each pull and records where
the sled stopped.

![Event classes](diagrams/svg/event_classes.svg)

![Event sequence](diagrams/svg/event_sequence.svg)

The same event with a player (`field::LiveEvent`): the rivals in slices of
steps, the player's pulls and decisions from the game.

![Live event](diagrams/svg/live_event_sequence.svg)

### Season (`tp::season`)

A season's championship: the same field at every event, each on its own
track; each event's places score points (20, 17, 15, then one less per place
for the club season), the most points win and more event wins break a tie.

![Season classes](diagrams/svg/season_classes.svg)

### Class rules (`tp::rules`)

A pulling class's technical limits and the check whether a tractor may
compete. Competition rules, not physics: they are data, checked at setup.

![Class rules](diagrams/svg/rules_classes.svg)

### Godot binding (`app/godot/extension`)

The C++ GDExtension that hands GDScript a `TractorPullRun`: `tp::session`
behind Godot types, and a `TractorPullEvent`: `field::LiveSeason` (the
player and the named rivals, each event's board and standings, the
championship), with the data read through Godot's `FileAccess` (so
`res://` works). It is the only code that includes Godot headers, and holds
no game rules of its own.

![Godot binding classes](diagrams/svg/godot_classes.svg)

### The pull scene (`app/godot/scenes`, `app/godot/scripts`)

GDScript that plays a season's events against the rivals: the start
screen, the event's board (`BoardScreen`, filled in by `EventTexts`), the
player's pulls and the result screen, then the event's top ten and the
season's. The root script owns the `TractorPullEvent`, advancing the
rivals' work a few milliseconds each frame, and the `TractorPullRun`,
which it sets up for each of the player's pulls (`setup_event_pull()`) or
a practice pull; it builds a view of the track, tractor, sled and chain
from the run's dimensions, and each frame hands it the controls, advances
it by the frame's time and places the views from the blended snapshot; the
cameras follow the tractor and the `Hud` shows the engine and the
distance. Who pulls next, and the standings, are the event's answer.
`DriveControls` is the easy layout on a touch screen: a steering slider, a
hand throttle and gear buttons, each following its own finger so both
thumbs drive at once; the keyboard and a gamepad work it in development.
`TrackSpace` turns the track's coordinates into Godot's. No physics or rules
live here.

![Pull scene classes](diagrams/svg/godot_scene_classes.svg)

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
the clutch's own curve; the steering follows the control past its dead zone
at the steering rate.

![Easy controls](diagrams/svg/easy_controls_sequence.svg)

### Changing gear

![Gear shift](diagrams/svg/gear_shift_sequence.svg)

### Loading a component definition

![Load component](diagrams/svg/load_component_sequence.svg)

### Frame loop

How a front end drives the fixed-step simulation from a variable frame rate,
through the session.

![Frame loop](diagrams/svg/frame_loop_sequence.svg)

### A run in Godot

Setup loads the data through `res://` and the scene builds itself from the
run's dimensions; then every rendered frame the scene sets the controls,
advances the run by the frame's time and draws the blended snapshot.

![Godot frame](diagrams/svg/godot_frame_sequence.svg)

### The game's flow

An event of the season, from the start screen: the board filling in with
the rivals' pulls, the player's pulls and decisions when they are due, the
event's top ten, the season's, and on to the next event and its track.

![Game flow](diagrams/svg/godot_run_flow_sequence.svg)
