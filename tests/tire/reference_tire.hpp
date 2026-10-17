#pragma once

#include "tp/core/math/curve.hpp"
#include "tp/core/tire/tire.hpp"

#include <array>
#include <cstdlib>

namespace tp::tire::testing {

// Mirrors data/tires/pulling_rear.toml: a large rear tractor tire on a
// prepared pulling track. Peak μ 0.75 at 25 % slip, 0.5 at full wheelspin.
inline Params reference_params() {
    const std::array<math::CurvePoint, 6> mu = {{
        {0.0, 0.0},
        {0.05, 0.35},
        {0.15, 0.65},
        {0.25, 0.75},
        {0.5, 0.62},
        {1.0, 0.5},
    }};
    const auto curve = math::Curve::create(mu);
    if (!curve) {
        std::abort();
    }
    return Params{
        .radius_m = 0.8,
        .traction_coefficient = *curve,
        .nominal_load_n = 10000.0,
        .load_sensitivity = 0.1,
        .rolling_resistance_coefficient = 0.06,
        .wheel_inertia_kg_m2 = 30.0,
        .min_reference_speed_m_s = 0.5,
    };
}

}  // namespace tp::tire::testing
