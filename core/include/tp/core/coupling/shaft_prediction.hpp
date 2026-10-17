#pragma once

namespace tp::coupling {

/// A shaft's speed at the end of the current step as a linear function of an
/// extra torque applied to it during that step:
///
///   omega_next = free_omega_next_rad_s + omega_per_torque * torque_nm
///
/// Couplings between shafts (the clutch now, tires later) solve these lines
/// for the torque that makes speeds match. Solving at the velocity level
/// keeps stiff friction contacts stable and free of chatter at 1 kHz.
struct ShaftPrediction {
    /// Speed at the end of the step with no coupling torque [rad/s].
    double free_omega_next_rad_s = 0.0;
    /// Speed change per N·m of coupling torque over the step [rad/s per N·m].
    /// Positive: torque in the direction of rotation speeds the shaft up.
    double omega_per_torque = 0.0;
};

}  // namespace tp::coupling
