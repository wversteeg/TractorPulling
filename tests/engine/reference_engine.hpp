#pragma once

#include "tp/core/engine/engine.hpp"
#include "tp/core/math/curve.hpp"
#include "tp/core/units/conversions.hpp"

#include <array>
#include <cstdlib>

namespace tp::engine::testing {

using units::rpm_to_rad_s;

// Light farm-stock class: ~68 kW net four-cylinder turbo diesel. Mirrors
// data/engines/farm_stock_light.toml so tests do not depend on file loading.
inline Params reference_params() {
    const std::array<math::CurvePoint, 11> torque = {{
        {rpm_to_rad_s(800.0), 279.0},
        {rpm_to_rad_s(1000.0), 335.0},
        {rpm_to_rad_s(1200.0), 381.0},
        {rpm_to_rad_s(1400.0), 400.0},
        {rpm_to_rad_s(1600.0), 395.0},
        {rpm_to_rad_s(1800.0), 381.0},
        {rpm_to_rad_s(2000.0), 358.0},
        {rpm_to_rad_s(2200.0), 316.0},
        {rpm_to_rad_s(2400.0), 290.0},
        {rpm_to_rad_s(2600.0), 266.0},
        {rpm_to_rad_s(2800.0), 238.0},
    }};
    const auto curve = math::Curve::create(torque);
    if (!curve) {
        std::abort();
    }
    return Params{
        .full_load_torque_nm = *curve,
        .inertia_kg_m2 = 1.2,
        .idle_omega_rad_s = rpm_to_rad_s(800.0),
        .rated_omega_rad_s = rpm_to_rad_s(2200.0),
        .high_idle_omega_rad_s = rpm_to_rad_s(2400.0),
        .stall_omega_rad_s = rpm_to_rad_s(450.0),
        .friction_torque_nm = 15.0,
        .viscous_friction_nm_s_rad = 8.4 / rpm_to_rad_s(1000.0),
        .na_torque_fraction = 0.65,
        .turbo_time_constant_s = 0.8,
        .boost_onset_omega_rad_s = rpm_to_rad_s(1000.0),
        .full_boost_omega_rad_s = rpm_to_rad_s(1600.0),
    };
}

}  // namespace tp::engine::testing
