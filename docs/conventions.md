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

- **Pitch** is rotation about y, positive nose-up. A wheelie is positive pitch.
- **Longitudinal velocity** is positive when moving forward.
- **Engine and wheel speed** are positive in the direction that drives the tractor forward.
- **Torque** is positive when it accelerates its shaft in the positive direction.
- **Tire slip** is positive when the wheel surface moves faster than the ground (traction), negative when braking.

## Time

The simulation advances in fixed steps of 1 ms (1 kHz). The core never reads a clock; front ends use `tp::FixedStepClock` to turn frame time into step counts.
