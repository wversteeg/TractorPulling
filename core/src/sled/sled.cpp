#include "tp/core/sled/sled.hpp"

#include "tp/core/units/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::sled {

namespace {

// A steel pan on a pulling track; anything above is a data error.
constexpr double kMaxPanFrictionCoefficient = 2.0;

bool within(double value, double low, double high) noexcept {
    return value >= low && value <= high;
}

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    const std::array scalars = {params.frame_mass_kg,
                                params.frame_cg_ahead_of_wheels_m,
                                params.pan_ahead_of_wheels_m,
                                params.hook_ahead_of_wheels_m,
                                params.hook_height_m,
                                params.box_mass_kg,
                                params.box_start_ahead_of_wheels_m,
                                params.box_end_ahead_of_wheels_m,
                                params.box_travel_distance_m,
                                params.pushdown_start_m,
                                params.pushdown_ramp_m,
                                params.pan_friction_coefficient,
                                params.wheel_rolling_resistance_coefficient,
                                params.chain.length_m,
                                params.chain.stiffness_n_per_m,
                                params.chain.damping_n_s_per_m,
                                params.pull_off.extra_box_mass_kg,
                                params.pull_off.pushdown_earlier_m,
                                params.pull_off.extra_pan_friction};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.frame_mass_kg <= 0.0) {
        return ParamError::kFrameMassNotPositive;
    }
    if (params.box_mass_kg < 0.0) {
        return ParamError::kBoxMassNegative;
    }
    const double pan_m = params.pan_ahead_of_wheels_m;
    if (pan_m <= 0.0) {
        return ParamError::kPanNotAheadOfWheels;
    }
    if (!within(params.frame_cg_ahead_of_wheels_m, 0.0, pan_m)) {
        return ParamError::kFrameCgOutsideSled;
    }
    if (!within(params.box_start_ahead_of_wheels_m, 0.0, pan_m) ||
        !within(params.box_end_ahead_of_wheels_m, 0.0, pan_m)) {
        return ParamError::kBoxOutsideSled;
    }
    if (params.box_travel_distance_m <= 0.0) {
        return ParamError::kBoxTravelNotPositive;
    }
    if (params.hook_ahead_of_wheels_m <= 0.0 || params.hook_height_m <= 0.0) {
        return ParamError::kHookInvalid;
    }
    if (params.pushdown_start_m < 0.0) {
        return ParamError::kPushdownStartNegative;
    }
    if (params.pushdown_ramp_m <= 0.0) {
        return ParamError::kPushdownRampNotPositive;
    }
    if (params.pan_friction_coefficient <= 0.0 ||
        params.pan_friction_coefficient > kMaxPanFrictionCoefficient) {
        return ParamError::kPanFrictionOutOfRange;
    }
    if (params.wheel_rolling_resistance_coefficient < 0.0 ||
        params.wheel_rolling_resistance_coefficient >= 1.0) {
        return ParamError::kRollingResistanceOutOfRange;
    }
    if (params.chain.length_m <= 0.0 || params.chain.stiffness_n_per_m <= 0.0 ||
        params.chain.damping_n_s_per_m < 0.0) {
        return ParamError::kChainInvalid;
    }
    if (params.pull_off.extra_box_mass_kg < 0.0 || params.pull_off.pushdown_earlier_m < 0.0 ||
        params.pull_off.extra_pan_friction < 0.0) {
        return ParamError::kPullOffNegative;
    }
    return std::nullopt;
}

double total_mass_kg(const Params& params) noexcept {
    return params.frame_mass_kg + params.box_mass_kg;
}

Params heavier(const Params& params) noexcept {
    Params next = params;
    next.box_mass_kg += params.pull_off.extra_box_mass_kg;
    next.pushdown_start_m =
        std::max(params.pushdown_start_m - params.pull_off.pushdown_earlier_m, 0.0);
    next.pan_friction_coefficient =
        std::min(params.pan_friction_coefficient + params.pull_off.extra_pan_friction,
                 kMaxPanFrictionCoefficient);
    return next;
}

Loads loads(const Params& params, const State& state, const HookForce& hook) noexcept {
    using units::kStandardGravity_m_s2;
    const double travel = std::clamp(state.distance_m / params.box_travel_distance_m, 0.0, 1.0);
    const double box_m =
        params.box_start_ahead_of_wheels_m +
        travel * (params.box_end_ahead_of_wheels_m - params.box_start_ahead_of_wheels_m);
    const double frame_n = params.frame_mass_kg * kStandardGravity_m_s2;
    const double box_n = params.box_mass_kg * kStandardGravity_m_s2;

    // Moments about the rear wheels' contact: the weights press the pan down
    // by their levers; the chain's pull at hook height tips the sled forward
    // onto the pan, and its upward part takes load off the front. Friction
    // and rolling resistance act at the track, with no lever about it.
    const double down_n = std::max(frame_n + box_n - hook.up_n, 0.0);
    const double pan_lever_n =
        (frame_n * params.frame_cg_ahead_of_wheels_m + box_n * box_m -
         hook.up_n * params.hook_ahead_of_wheels_m + hook.forward_n * params.hook_height_m) /
        params.pan_ahead_of_wheels_m;
    double pan_n = std::clamp(pan_lever_n, 0.0, down_n);
    double wheels_n = down_n - pan_n;

    // The push-down lifts the wheels: their share moves onto the pan.
    const double pushdown =
        std::clamp((state.distance_m - params.pushdown_start_m) / params.pushdown_ramp_m, 0.0, 1.0);
    pan_n += pushdown * wheels_n;
    wheels_n -= pushdown * wheels_n;

    return {.pan_n = pan_n,
            .wheels_n = wheels_n,
            .box_ahead_of_wheels_m = box_m,
            .pushdown = pushdown,
            .resistance_n = params.pan_friction_coefficient * pan_n +
                            params.wheel_rolling_resistance_coefficient * wheels_n};
}

Prediction predict(const State& state, const Params& params, double resistance_n,
                   double dt_s) noexcept {
    const double velocity_per_n = dt_s / total_mass_kg(params);
    return {.free_velocity_m_s = state.velocity_m_s - velocity_per_n * resistance_n,
            .velocity_per_n = velocity_per_n};
}

void advance(State& state, const Prediction& prediction, double forward_n, double dt_s) noexcept {
    // Friction holds the sled but never drives it backwards.
    const double velocity_m_s =
        std::max(prediction.free_velocity_m_s + prediction.velocity_per_n * forward_n, 0.0);
    state.velocity_m_s = velocity_m_s;
    state.position_m += velocity_m_s * dt_s;
    state.distance_m += velocity_m_s * dt_s;
}

}  // namespace tp::sled
