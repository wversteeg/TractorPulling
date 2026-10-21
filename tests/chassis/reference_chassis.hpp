#pragma once

#include "tp/core/chassis/chassis.hpp"

namespace tp::chassis::testing {

// Mirrors the chassis in data/tractors/farm_stock_light.toml: 3500 kg with
// 60 % of the weight on the rear axle at rest, on 0.8 m rear tires.
inline Params reference_params() {
    return Params{
        .mass_kg = 3500.0,
        .wheelbase_m = 2.4,
        .cg_ahead_of_rear_axle_m = 0.96,
        .cg_height_m = 1.0,
        .pitch_inertia_kg_m2 = 2800.0,
        .yaw_inertia_kg_m2 = 5500.0,
        .rear_axle_height_m = 0.8,
        .rear_track_m = 1.9,
        .front_track_m = 1.6,
        .hitch_height_m = 0.5,
        .hitch_behind_rear_axle_m = 0.4,
        .front_tires = {.stiffness_n_per_m = 400000.0, .damping_n_s_per_m = 6000.0},
        .wheelie_bar = {.behind_rear_axle_m = 1.3,
                        .tip_height_m = 0.15,
                        .contact = {.stiffness_n_per_m = 2.0e6, .damping_n_s_per_m = 50000.0},
                        .rolling_resistance_coefficient = 0.05},
        .ballast = {.mass_kg = 400.0,
                    .rear_ahead_of_rear_axle_m = -0.35,
                    .front_ahead_of_rear_axle_m = 3.1,
                    .position = 0.5},
    };
}

}  // namespace tp::chassis::testing
