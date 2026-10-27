#pragma once

#include "tp/core/sled/sled.hpp"

namespace tp::sled::testing {

// Mirrors data/sleds/light_class.toml.
inline Params reference_params() {
    return Params{
        .frame_mass_kg = 3200.0,
        .frame_cg_ahead_of_wheels_m = 0.75,
        .frame_yaw_inertia_kg_m2 = 5000.0,
        .pan_ahead_of_wheels_m = 3.0,
        .hook_ahead_of_wheels_m = 3.3,
        .hook_height_m = 0.45,
        .box_mass_kg = 1500.0,
        .box_start_ahead_of_wheels_m = 0.0,
        .box_end_ahead_of_wheels_m = 3.0,
        .box_travel_distance_m = 88.0,
        .pushdown_start_m = 78.0,
        .pushdown_ramp_m = 10.0,
        .pan_friction_coefficient = 0.46,
        .pan_area_m2 = 1.8,
        .pan_plough_coefficient = 5.0,
        .knives = {.count = 4, .depth_m = 0.04, .width_m = 0.05},
        .berm = {.collected_share = 0.15, .spill_distance_m = 40.0, .friction_coefficient = 0.6},
        .wheel = {.radius_m = 0.55,
                  .section_width_m = 0.43,
                  .section_height_m = 0.35,
                  .deflection_m = 0.04},
        .wheel_cornering = {.stiffness_per_rad = 4.0,
                            .limit = 0.8,
                            .mobility_rate = 0.1,
                            .relaxation_length_m = 0.55},
        .wheel_count = 2,
        .chain = {.length_m = 1.15, .stiffness_n_per_m = 2.0e6, .damping_n_s_per_m = 15000.0},
        .pull_off_step = {.box_mass_kg = 0.0,
                          .knives = 1,
                          .knife_depth_m = 0.0,
                          .pushdown_earlier_m = 0.0},
    };
}

}  // namespace tp::sled::testing
