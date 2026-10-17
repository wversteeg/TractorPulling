#pragma once

#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/math/curve.hpp"

#include <cstdint>
#include <optional>

namespace tp::engine {

/// Diesel engine as a single rotating mass:
///
///   I dω/dt = T_combustion(ω, fuel, boost) - T_friction(ω) - T_load
///
/// Throttle drives an all-speed governor, as on a farm tractor's hand
/// throttle: it sets a target speed between idle and high idle, and fuel falls
/// linearly from full to zero across the droop band (high idle - rated speed)
/// below that target. The governor is also the RPM limit: fuel is zero above
/// the target, which never exceeds high idle.
///
/// The turbo is a first-order lag. Without boost the engine makes
/// `na_torque_fraction` of full-load torque; boost raises that towards 1.
struct Params {
    /// Full-load torque at full boost [N·m] against engine speed [rad/s].
    math::Curve full_load_torque_nm;
    /// Crankshaft, flywheel and everything rotating with them [kg·m²].
    double inertia_kg_m2 = 0.0;
    /// Governed speed at zero throttle [rad/s].
    double idle_omega_rad_s = 0.0;
    /// Speed at which the engine delivers full fuel at full throttle [rad/s].
    double rated_omega_rad_s = 0.0;
    /// Governed no-load speed at full throttle [rad/s].
    double high_idle_omega_rad_s = 0.0;
    /// Below this speed the engine stalls [rad/s].
    double stall_omega_rad_s = 0.0;
    /// Constant part of friction and pumping losses [N·m].
    double friction_torque_nm = 0.0;
    /// Speed-proportional part of the losses [N·m per rad/s].
    double viscous_friction_nm_s_rad = 0.0;
    /// Fraction of full-load torque available without boost, in (0, 1].
    double na_torque_fraction = 0.0;
    /// Turbo spool time constant [s].
    double turbo_time_constant_s = 0.0;
    /// Exhaust energy starts to build boost at this speed [rad/s].
    double boost_onset_omega_rad_s = 0.0;
    /// Full boost is available from this speed [rad/s].
    double full_boost_omega_rad_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kInertiaNotPositive,
    kStallNotPositive,
    kStallNotBelowIdle,
    kIdleNotBelowRated,
    kRatedNotBelowHighIdle,
    kFrictionNegative,
    kNaTorqueFractionOutOfRange,
    kTurboTimeConstantNotPositive,
    kBoostRangeInvalid,
    kTorqueCurveNegative,
    kTorqueCurveDoesNotCoverRange,
};

/// Returns the first physically meaningless value in `params`, if any.
/// `step` assumes valid parameters.
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

struct State {
    double omega_rad_s = 0.0;
    /// Turbo boost, from 0 (none) to 1 (full).
    double boost = 0.0;
    bool running = false;
};

struct Input {
    /// Hand throttle position, clamped to [0, 1].
    double throttle = 0.0;
    /// Torque the driveline draws from the crankshaft [N·m]. Negative when
    /// the driveline drives the engine.
    double load_torque_nm = 0.0;
};

struct Output {
    /// Fuel command from the governor, 0 to 1.
    double fuel = 0.0;
    /// Torque from combustion [N·m].
    double combustion_torque_nm = 0.0;
    /// Friction and pumping losses at the start of the step [N·m].
    double friction_torque_nm = 0.0;
};

/// Feeds every field of `state` into `hash`, for replay and determinism tests.
constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.omega_rad_s);
    hash.add(state.boost);
    hash.add(state.running);
}

/// First half of a step: everything that does not depend on the load torque.
/// Splitting the step lets a coupling such as the clutch compute the load
/// from the engine's predicted speed before the engine integrates.
struct Prediction {
    Output output;
    /// Turbo boost at the end of the step.
    double boost_next = 0.0;
    // Terms of the semi-implicit speed update, kept so advance() reproduces
    // the step bit for bit.
    double omega_rad_s = 0.0;
    double drive_torque_nm = 0.0;
    double dt_over_inertia = 0.0;
    double viscous_divisor = 1.0;
};

/// Running at idle speed, no boost.
[[nodiscard]] State initial_state(const Params& params) noexcept;

/// Computes fuel, boost and torques for a step of `dt_s` seconds. Pure.
[[nodiscard]] Prediction predict(const State& state, const Params& params, double throttle,
                                 double dt_s) noexcept;

/// The engine's end-of-step speed as a function of torque applied to the
/// crankshaft (the negative of the load torque).
[[nodiscard]] coupling::ShaftPrediction shaft_prediction(const Prediction& prediction) noexcept;

/// Second half of a step: integrates speed under `load_torque_nm` and checks
/// for a stall.
void advance(State& state, const Params& params, const Prediction& prediction,
             double load_torque_nm) noexcept;

/// predict() then advance(): a whole step with a known load torque.
Output step(State& state, const Params& params, const Input& input, double dt_s) noexcept;

}  // namespace tp::engine
