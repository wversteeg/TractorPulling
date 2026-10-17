#include "tp/core/engine/engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::engine {

std::optional<ParamError> validate(const Params& params) noexcept {
    const std::array scalars = {
        params.inertia_kg_m2,
        params.idle_omega_rad_s,
        params.rated_omega_rad_s,
        params.high_idle_omega_rad_s,
        params.stall_omega_rad_s,
        params.friction_torque_nm,
        params.viscous_friction_nm_s_rad,
        params.na_torque_fraction,
        params.turbo_time_constant_s,
        params.boost_onset_omega_rad_s,
        params.full_boost_omega_rad_s,
    };
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.inertia_kg_m2 <= 0.0) {
        return ParamError::kInertiaNotPositive;
    }
    if (params.stall_omega_rad_s <= 0.0) {
        return ParamError::kStallNotPositive;
    }
    if (params.stall_omega_rad_s >= params.idle_omega_rad_s) {
        return ParamError::kStallNotBelowIdle;
    }
    if (params.idle_omega_rad_s >= params.rated_omega_rad_s) {
        return ParamError::kIdleNotBelowRated;
    }
    if (params.rated_omega_rad_s >= params.high_idle_omega_rad_s) {
        return ParamError::kRatedNotBelowHighIdle;
    }
    if (params.friction_torque_nm < 0.0 || params.viscous_friction_nm_s_rad < 0.0) {
        return ParamError::kFrictionNegative;
    }
    if (params.na_torque_fraction <= 0.0 || params.na_torque_fraction > 1.0) {
        return ParamError::kNaTorqueFractionOutOfRange;
    }
    if (params.turbo_time_constant_s <= 0.0) {
        return ParamError::kTurboTimeConstantNotPositive;
    }
    if (params.boost_onset_omega_rad_s < 0.0 ||
        params.boost_onset_omega_rad_s >= params.full_boost_omega_rad_s) {
        return ParamError::kBoostRangeInvalid;
    }
    const auto points = params.full_load_torque_nm.points();
    if (std::ranges::any_of(points, [](const math::CurvePoint& p) { return p.y < 0.0; })) {
        return ParamError::kTorqueCurveNegative;
    }
    if (params.full_load_torque_nm.min_x() > params.idle_omega_rad_s ||
        params.full_load_torque_nm.max_x() < params.high_idle_omega_rad_s) {
        return ParamError::kTorqueCurveDoesNotCoverRange;
    }
    return std::nullopt;
}

State initial_state(const Params& params) noexcept {
    return State{.omega_rad_s = params.idle_omega_rad_s, .boost = 0.0, .running = true};
}

namespace {

// Below this the turbo is at rest. The lag decays geometrically and would
// otherwise reach subnormal values after ~10 minutes without fuel, which are
// very slow on many CPUs.
constexpr double kBoostAtRest = 1e-9;

double governor_fuel(const Params& params, double throttle, double omega_rad_s) noexcept {
    const double target_rad_s = params.idle_omega_rad_s +
                                throttle * (params.high_idle_omega_rad_s - params.idle_omega_rad_s);
    const double droop_rad_s = params.high_idle_omega_rad_s - params.rated_omega_rad_s;
    return std::clamp((target_rad_s - omega_rad_s) / droop_rad_s, 0.0, 1.0);
}

// Exhaust energy available to spin the turbo, 0 to 1, rising linearly with
// engine speed between boost onset and full boost.
double exhaust_energy(const Params& params, double omega_rad_s) noexcept {
    return std::clamp((omega_rad_s - params.boost_onset_omega_rad_s) /
                          (params.full_boost_omega_rad_s - params.boost_onset_omega_rad_s),
                      0.0, 1.0);
}

}  // namespace

Prediction predict(const State& state, const Params& params, double throttle,
                   double dt_s) noexcept {
    throttle = std::clamp(throttle, 0.0, 1.0);
    const double omega_rad_s = state.omega_rad_s;

    Prediction p;
    p.output.fuel = state.running ? governor_fuel(params, throttle, omega_rad_s) : 0.0;

    // Implicit (backward Euler) form of the first-order lag. It is stable for
    // any dt and uses only basic arithmetic: exp() is not guaranteed to be
    // bit-identical across platforms' math libraries.
    const double boost_target = p.output.fuel * exhaust_energy(params, omega_rad_s);
    p.boost_next =
        state.boost + (boost_target - state.boost) * dt_s / (params.turbo_time_constant_s + dt_s);
    if (p.boost_next < kBoostAtRest) {
        p.boost_next = 0.0;
    }

    const double boost_factor =
        params.na_torque_fraction + (1.0 - params.na_torque_fraction) * p.boost_next;
    p.output.combustion_torque_nm =
        p.output.fuel * params.full_load_torque_nm.evaluate(omega_rad_s) * boost_factor;
    p.output.friction_torque_nm =
        params.friction_torque_nm + params.viscous_friction_nm_s_rad * omega_rad_s;

    // Semi-implicit Euler: explicit in combustion and load torque, implicit in
    // the viscous term so strong damping cannot destabilise the step.
    p.omega_rad_s = omega_rad_s;
    p.drive_torque_nm = p.output.combustion_torque_nm - params.friction_torque_nm;
    p.dt_over_inertia = dt_s / params.inertia_kg_m2;
    p.viscous_divisor = 1.0 + p.dt_over_inertia * params.viscous_friction_nm_s_rad;
    return p;
}

coupling::ShaftPrediction shaft_prediction(const Prediction& p) noexcept {
    return {
        .free_omega_next_rad_s =
            (p.omega_rad_s + p.dt_over_inertia * p.drive_torque_nm) / p.viscous_divisor,
        .omega_per_torque = p.dt_over_inertia / p.viscous_divisor,
    };
}

void advance(State& state, const Params& params, const Prediction& p,
             double load_torque_nm) noexcept {
    state.boost = p.boost_next;
    const double next_omega_rad_s =
        (p.omega_rad_s + p.dt_over_inertia * (p.drive_torque_nm - load_torque_nm)) /
        p.viscous_divisor;

    // Friction only opposes forward rotation, so it must not drive the engine
    // backwards; it holds the crankshaft at rest instead.
    state.omega_rad_s = std::max(next_omega_rad_s, 0.0);

    if (state.running && state.omega_rad_s < params.stall_omega_rad_s) {
        state.running = false;
    }
}

Output step(State& state, const Params& params, const Input& input, double dt_s) noexcept {
    const Prediction p = predict(state, params, input.throttle, dt_s);
    advance(state, params, p, input.load_torque_nm);
    return p.output;
}

}  // namespace tp::engine
