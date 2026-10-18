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

bool knives_valid(const Knives& knives) noexcept {
    return knives.count >= 0 && knives.count <= kMaxKnives &&
           within(knives.depth_m, 0.0, kMaxKnifeDepth_m) && knives.width_m >= 0.0 &&
           (knives.count == 0 || knives.width_m > 0.0);
}

// Every part of a step adds resistance or none, at least one adds some, and
// knives added need a width to cut with.
bool makes_heavier(const Adjustment& step, const Knives& knives) noexcept {
    const bool none_lighter = step.box_mass_kg >= 0.0 && step.knives >= 0 &&
                              step.knife_depth_m >= 0.0 && step.pushdown_earlier_m >= 0.0;
    const bool some_heavier = step.box_mass_kg > 0.0 || step.knives > 0 ||
                              step.knife_depth_m > 0.0 || step.pushdown_earlier_m > 0.0;
    return none_lighter && some_heavier && (step.knives == 0 || knives.width_m > 0.0);
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
                                params.pan_area_m2,
                                params.pan_plough_coefficient,
                                params.chain.length_m,
                                params.chain.stiffness_n_per_m,
                                params.chain.damping_n_s_per_m,
                                params.knives.depth_m,
                                params.knives.width_m,
                                params.pull_off_step.box_mass_kg,
                                params.pull_off_step.knife_depth_m,
                                params.pull_off_step.pushdown_earlier_m};
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
    if (params.pan_area_m2 <= 0.0) {
        return ParamError::kPanAreaNotPositive;
    }
    if (params.pan_plough_coefficient < 0.0) {
        return ParamError::kPanPloughNegative;
    }
    if (!knives_valid(params.knives)) {
        return ParamError::kKnivesOutOfRange;
    }
    if (tire::validate(params.wheel)) {
        return ParamError::kWheel;
    }
    if (params.wheel_count < 1 || params.wheel_count > kMaxWheels) {
        return ParamError::kWheelCountOutOfRange;
    }
    if (params.chain.length_m <= 0.0 || params.chain.stiffness_n_per_m <= 0.0 ||
        params.chain.damping_n_s_per_m < 0.0) {
        return ParamError::kChainInvalid;
    }
    if (!makes_heavier(params.pull_off_step, params.knives)) {
        return ParamError::kPullOffStepNotHeavier;
    }
    return std::nullopt;
}

double total_mass_kg(const Params& params) noexcept {
    return params.frame_mass_kg + params.box_mass_kg;
}

Params with_conditions(const Params& params, const Conditions& conditions) noexcept {
    Params out = params;
    out.pushdown_start_m = conditions.pushdown_start_m.value_or(params.pushdown_start_m);
    return out;
}

Params adjusted(const Params& params, const Adjustment& adjustment, int times) noexcept {
    const double n = times;
    Params out = params;
    out.box_mass_kg = std::max(params.box_mass_kg + n * adjustment.box_mass_kg, 0.0);
    out.knives.count = std::clamp(params.knives.count + times * adjustment.knives, 0, kMaxKnives);
    out.knives.depth_m =
        std::clamp(params.knives.depth_m + n * adjustment.knife_depth_m, 0.0, kMaxKnifeDepth_m);
    out.pushdown_start_m =
        std::max(params.pushdown_start_m - n * adjustment.pushdown_earlier_m, 0.0);
    return out;
}

Params heavier(const Params& params, int steps) noexcept {
    return adjusted(params, params.pull_off_step, steps);
}

namespace {

// The pan sinks in until the soil carries its pressure and pushes soil
// ahead of it: the ploughing grows with pressure over strength.
double pan_friction(const Params& params, double pan_n, const soil::Surface& surface) noexcept {
    const double pressure_pa = pan_n / params.pan_area_m2;
    return params.pan_friction_coefficient *
           (1.0 + params.pan_plough_coefficient * pressure_pa / surface.cone_index_pa);
}

// Soil cutting by a narrow blade: the draft is about this share of the
// soil's strength times the groove's cross-section (a narrow-tine estimate,
// lowered for blades that only skim the prepared top of the track).
constexpr double kKnifeCutting = 0.2;
// Above this strength the sled's weight no longer presses the knives to
// their full depth: in harder ground they cut shallower, so their draft
// stops growing with strength [Pa].
constexpr double kKnifeFullDepthStrength_pa = 2.0e6;

double knives_n(const Knives& knives, const soil::Surface& surface) noexcept {
    const double strength_pa = std::min(surface.cone_index_pa, kKnifeFullDepthStrength_pa);
    return kKnifeCutting * strength_pa * knives.count * knives.width_m * knives.depth_m;
}

}  // namespace

Loads loads(const Params& params, const State& state, const HookForce& hook,
            const Ground& ground) noexcept {
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

    const double wheel_count = params.wheel_count;
    const double rolling = wheels_n > 0.0 ? tire::towed_rolling_resistance(
                                                params.wheel, wheels_n / wheel_count, ground.wheels)
                                          : 0.0;
    const double friction = pan_friction(params, pan_n, ground.pan);
    const double cutting_n = pan_n > 0.0 ? knives_n(params.knives, ground.pan) : 0.0;
    return {.pan_n = pan_n,
            .wheels_n = wheels_n,
            .box_ahead_of_wheels_m = box_m,
            .pushdown = pushdown,
            .pan_friction = friction,
            .knives_n = cutting_n,
            .resistance_n = friction * pan_n + cutting_n + rolling * wheels_n};
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
