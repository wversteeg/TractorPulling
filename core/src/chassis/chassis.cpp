#include "tp/core/chassis/chassis.hpp"

#include "tp/core/units/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::chassis {

std::optional<ParamError> validate(const Params& params) noexcept {
    const std::array scalars = {
        params.mass_kg,     params.wheelbase_m,    params.cg_ahead_of_rear_axle_m,
        params.cg_height_m, params.hitch_height_m, params.front_rolling_resistance_coefficient};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.mass_kg <= 0.0) {
        return ParamError::kMassNotPositive;
    }
    if (params.wheelbase_m <= 0.0) {
        return ParamError::kWheelbaseNotPositive;
    }
    if (params.cg_ahead_of_rear_axle_m <= 0.0 ||
        params.cg_ahead_of_rear_axle_m >= params.wheelbase_m) {
        return ParamError::kCgOutsideWheelbase;
    }
    if (params.cg_height_m <= 0.0) {
        return ParamError::kCgHeightNotPositive;
    }
    if (params.hitch_height_m < 0.0) {
        return ParamError::kHitchHeightNegative;
    }
    if (params.front_rolling_resistance_coefficient < 0.0 ||
        params.front_rolling_resistance_coefficient >= 1.0) {
        return ParamError::kRollingResistanceOutOfRange;
    }
    return std::nullopt;
}

AxleLoads axle_loads(const Params& params, double acceleration_m_s2,
                     double drawbar_force_n) noexcept {
    const double weight_n = params.mass_kg * units::kStandardGravity_m_s2;
    const double static_rear_n =
        weight_n * (params.wheelbase_m - params.cg_ahead_of_rear_axle_m) / params.wheelbase_m;
    const double transfer_n = (params.mass_kg * acceleration_m_s2 * params.cg_height_m +
                               drawbar_force_n * params.hitch_height_m) /
                              params.wheelbase_m;

    AxleLoads loads;
    loads.rear_n = std::clamp(static_rear_n + transfer_n, 0.0, weight_n);
    loads.front_n = weight_n - loads.rear_n;
    loads.front_lifted = static_rear_n + transfer_n >= weight_n;
    return loads;
}

Prediction predict(const State& state, const Params& params, double resisting_force_n,
                   double dt_s) noexcept {
    const double velocity_per_n = dt_s / params.mass_kg;
    return {.free_velocity_m_s = state.velocity_m_s - velocity_per_n * resisting_force_n,
            .velocity_per_n = velocity_per_n};
}

void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept {
    const double velocity_m_s =
        std::max(prediction.free_velocity_m_s + prediction.velocity_per_n * drive_force_n, 0.0);
    state.acceleration_m_s2 = (velocity_m_s - state.velocity_m_s) / dt_s;
    state.velocity_m_s = velocity_m_s;
    // Semi-implicit Euler: position advances with the new velocity.
    state.position_m += velocity_m_s * dt_s;
}

}  // namespace tp::chassis
