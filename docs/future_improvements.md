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
- **Speed-dependent pan resistance.** The pan's drag does not depend on speed.
  Right for the stock classes, which crawl because they lack the power to run
  fast; faster classes may need the pan to plough harder with speed.
- **Sled pitch.** The sled shares its weight between wheels and pan
  quasi-statically; it does not pitch or bounce.

## Tractor

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

- **Pull-offs between several tractors.** A pull-off is run per tractor; the
  ranking of several full pullers on the pull-off sled is not modelled.
- **Easy-mode clutch assist.** The lowest classes get an assist around the
  same clutch model (see CLAUDE.md).

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

## Roadmap features (from CLAUDE.md "Later")

- Steering plus two independent brakes, with a "pro" input mapping.
- Touch-only input (sliders and tilt).
- Godot 4 GDExtension front end: 3D scene, cameras, interpolated rendering.
