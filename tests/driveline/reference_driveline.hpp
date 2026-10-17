#pragma once

#include "tp/core/driveline/driveline.hpp"

namespace tp::driveline::testing {

// Mirrors data/drivelines/farm_stock_light.toml: 8 forward gears; with a
// 0.8 m tire at 2200 rpm, 1st is ~2 km/h, 4th-5th ~6-8 km/h, 8th ~30 km/h.
inline Params reference_params() {
    return Params{
        .gear_ratios = {15.0, 10.5, 7.4, 5.2, 3.6, 2.5, 1.7, 1.0},
        .gear_count = 8,
        .final_drive_ratio = 22.0,
        .efficiency = 0.9,
        .input_inertia_kg_m2 = 0.05,
        .output_inertia_kg_m2 = 2.0,
        .drag_torque_nm = 2.0,
        .viscous_drag_nm_s_rad = 0.01,
    };
}

// Without losses, for conservation tests.
inline Params lossless_params() {
    Params p = reference_params();
    p.efficiency = 1.0;
    p.drag_torque_nm = 0.0;
    p.viscous_drag_nm_s_rad = 0.0;
    return p;
}

}  // namespace tp::driveline::testing
