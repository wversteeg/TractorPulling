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

### Tire (`tp::tire`)

Longitudinal force from slip, with load sensitivity. The force is
linearized over a step so the stiff contact can be solved implicitly.

![Tire classes](diagrams/svg/tire_classes.svg)

### Chassis (`tp::chassis`)

The body moving along the track and pitching about the rear axle. The
front tires and the wheelie bar are one-sided spring-damper contacts; the
rear tires carry the rest of the weight.

![Chassis classes](diagrams/svg/chassis_classes.svg)

### Vehicle (`tp::vehicle`)

The whole tractor: components, the validated `Model`, and the exact chain
solve that couples engine, clutch, driveline, tires and body each step.

![Vehicle classes](diagrams/svg/vehicle_classes.svg)

### Sled (`tp::sled`)

The transfer sled: rear wheels and a pan share the frame and the weight box
by their levers. The box moves forward with the sled's travel and the
push-down lifts the wheels, so the pan drags harder along the track.

![Sled classes](diagrams/svg/sled_classes.svg)

### Pull (`tp::pull`)

A pull: the tractor and the sled joined by the chain, which pulls along its
own line between the hitch and the hook.

![Pull classes](diagrams/svg/pull_classes.svg)

### Run (`tp::run`)

A run from the start line to its result: it ends at the sled's first stop, or
with a full pull at the end of the track, which calls for a pull-off on a
heavier sled.

![Run classes](diagrams/svg/run_classes.svg)

### Class rules (`tp::rules`)

A pulling class's technical limits and the check whether a tractor may
compete. Competition rules, not physics: they are data, checked at setup.

![Class rules](diagrams/svg/rules_classes.svg)

### Data loading (`tp::data`)

Parses TOML from `data/`, converts to SI units and reuses the core's
validation rules. Never used inside the simulation step.

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

### Changing gear

![Gear shift](diagrams/svg/gear_shift_sequence.svg)

### Loading a component definition

![Load component](diagrams/svg/load_component_sequence.svg)

### Frame loop

How a front end drives the fixed-step simulation from a variable frame rate.

![Frame loop](diagrams/svg/frame_loop_sequence.svg)
