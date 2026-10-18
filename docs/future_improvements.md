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
- **Lane-to-lane differences.** Lanes are prepared and worn differently.
  Needs lane steering first.
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
- **Speed-dependent pan resistance.** The pan's drag does not depend on speed.
  Right for the stock classes, which crawl because they lack the power to run
  fast; faster classes may need the pan to plough harder with speed.
- **Sled pitch.** The sled shares its weight between wheels and pan
  quasi-statically; it does not pitch or bounce.

## Tractor

- **Rear tire compliance.** The rear axle stays at the tire radius; a springy
  rear tire would soften launches and let the tractor squat.
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

## Competition

- **Pull-offs between several tractors.** A pull-off is run per tractor; the
  ranking of several full pullers on the pull-off sled is not modelled.
- **Easy-mode clutch assist.** The lowest classes get an assist around the
  same clutch model (see CLAUDE.md).

## Performance

- **Derived chassis constants at setup.** The pitch step recomputes a few
  derived values (front tire preload, pitch inertia about the axle) every
  step. Moving them into a model built at setup would save some of its
  ~60 ns.

## Roadmap features (from CLAUDE.md "Later")

- Lane steering and sled sideways drag; leaving the lane ends the run.
- Steering plus two independent brakes, with a "pro" input mapping.
- Touch-only input (sliders and tilt).
- Godot 4 GDExtension front end: 3D scene, cameras, interpolated rendering.
