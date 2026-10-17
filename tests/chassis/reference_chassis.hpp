#pragma once

#include "tp/core/chassis/chassis.hpp"

namespace tp::chassis::testing {

// Mirrors the chassis in data/tractors/farm_stock_light.toml: 3500 kg with
// 60 % of the weight on the rear axle at rest.
inline Params reference_params() {
    return Params{
        .mass_kg = 3500.0,
        .wheelbase_m = 2.4,
        .cg_ahead_of_rear_axle_m = 0.96,
        .cg_height_m = 1.0,
        .hitch_height_m = 0.5,
        .front_rolling_resistance_coefficient = 0.06,
    };
}

}  // namespace tp::chassis::testing
