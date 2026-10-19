# Conventions

The single source of truth for units, axes and signs in the simulation.

## Units

SI everywhere inside `core/`: m, kg, s, N, N·m, rad, rad/s, W. Conversions (rpm, km/h, hp) happen only at the edges: data loading, tools output and UI.

Every physical quantity carries its unit in its name or an adjacent comment, e.g. `torque_nm`, `omega_rad_s`, `mass_kg`, `dt_s`.

## Coordinate frame

Right-handed, body fixed to the tractor chassis:

- **x** forward, along the track.
- **y** to the left.
- **z** up.

## Signs

- **Pitch** θ is rotation about the lateral axis, positive nose-up. A wheelie is positive pitch. Note that with y to the left, nose-up is a *negative* right-handed rotation about y; the simulation works in the x–z plane and uses nose-up positive throughout.
- **Heading** (yaw) is the body's x from the track's direction, positive turned left (counter-clockwise seen from above); the **yaw rate** is positive turning left. Headings are kept within ±π.
- **Steering**: the front wheel angle is positive to the left.
- **Side forces and sideways speeds** are positive to the left, in the body frame (in the steered wheel's frame for a front tire).
- **Longitudinal velocity** is positive when moving forward.
- **Engine and wheel speed** are positive in the direction that drives the tractor forward.
- **Torque** is positive when it accelerates its shaft in the positive direction.
- **Tire slip** is positive when the wheel surface moves faster than the ground (traction), negative when braking.

## Chassis geometry

- The body pitches about the **rear axle**, which stays at the rear tire radius above the ground.
- Body points (centre of gravity, hitch, wheelie bar tip, front axle) are given relative to the rear axle with the tractor level: x forward, z up. A point (x, z) pitched by θ lies at (x cos θ − z sin θ, x sin θ + z cos θ).
- Moments about the rear axle are nose-up positive: a force (F_x, F_z) at (x, z) gives x F_z − z F_x.
- Contact forces (front tires, wheelie bar) are vertical and push only; rolling drag acts backwards at the ground.

## Along the track

- A pull's world x is measured from where the tractor's rear axle starts; the sled starts behind it, at negative x. World y is measured from the track's centre line, to the left. The chassis' position is the rear axle's; its forward and sideways speeds are in the body frame.
- **Track x** (soil profiles, `tp::track`) is measured from the **start line**: where the sled's rear wheels stand at the start. The sled's distance travelled is its rear wheels' track x.
- Each contact reads the ground where it is: the rear tires at the rear axle, the front tires a wheelbase ahead, the sled's wheels and its pan by their positions.

## Time

The simulation advances in fixed steps of 1 ms (1 kHz). The core never reads a clock; front ends use `tp::FixedStepClock` to turn frame time into step counts.
