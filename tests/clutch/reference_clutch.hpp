#pragma once

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/math/curve.hpp"

#include <array>
#include <cstdlib>

namespace tp::clutch::testing {

// Mirrors data/clutches/farm_stock_light.toml: free play to 30 % pedal, bite
// around 55 %, fully open from 70 %.
inline Params reference_params() {
    const std::array<math::CurvePoint, 6> engagement = {{
        {0.0, 1.0},
        {0.3, 1.0},
        {0.45, 0.6},
        {0.55, 0.15},
        {0.7, 0.0},
        {1.0, 0.0},
    }};
    const auto curve = math::Curve::create(engagement);
    if (!curve) {
        std::abort();
    }
    return Params{
        .engagement = *curve,
        .static_torque_capacity_nm = 650.0,
        .kinetic_ratio = 0.8,
    };
}

}  // namespace tp::clutch::testing
