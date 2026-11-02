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
///
/// The response may differ by the sign of the torque: a gearbox loses a
/// fraction of the power flowing through it in either direction, so it
/// responds less to torque that drives it than to torque that brakes it.
/// The line is still exact once the sign of the torque is known, which a
/// coupling knows before solving (it is the sign of the free slip).
struct ShaftPrediction {
    /// Speed at the end of the step with no coupling torque [rad/s].
    double free_omega_next_rad_s = 0.0;
    /// Speed change per N·m of positive coupling torque over the step
    /// [rad/s per N·m]. Positive torque acts in the direction of rotation.
    double omega_per_torque = 0.0;
    /// As omega_per_torque, for negative torque. Same unless losses depend on
    /// the direction of power flow.
    double omega_per_negative_torque = omega_per_torque;
};

}  // namespace tp::coupling
