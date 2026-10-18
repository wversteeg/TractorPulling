#pragma once

#include "tp/core/sled/sled.hpp"

namespace tp::sled::testing {

// Mirrors data/sleds/light_class.toml.
inline Params reference_params() {
    return Params{
        .frame_mass_kg = 3200.0,
        .frame_cg_ahead_of_wheels_m = 0.75,
        .pan_ahead_of_wheels_m = 3.0,
        .hook_ahead_of_wheels_m = 3.3,
        .hook_height_m = 0.45,
        .box_mass_kg = 2000.0,
        .box_start_ahead_of_wheels_m = 0.0,
        .box_end_ahead_of_wheels_m = 3.0,
        .box_travel_distance_m = 100.0,
        .pushdown_start_m = 60.0,
        .pushdown_ramp_m = 2.0,
        .pan_friction_coefficient = 0.5,
        .wheel_rolling_resistance_coefficient = 0.05,
        .chain = {.length_m = 1.15, .stiffness_n_per_m = 2.0e6, .damping_n_s_per_m = 15000.0},
    };
}

}  // namespace tp::sled::testing
