#pragma once

#include "tp/core/soil/soil.hpp"
#include "tp/core/tire/tire.hpp"

namespace tp::tire::testing {

// Mirrors data/tires/pulling_rear.toml: a large rear tractor tire with
// pulling lugs.
inline Params reference_params() {
    return Params{
        .size = {.radius_m = 0.8,
                 .section_width_m = 0.47,
                 .section_height_m = 0.4,
                 .deflection_m = 0.05},
        .traction = {.limit = 0.93, .mobility_rate = 0.1, .slip_rate = 18.0},
        .wheel_inertia_kg_m2 = 30.0,
        .min_reference_speed_m_s = 0.5,
    };
}

// Mirrors the front tires in data/tractors/farm_stock_light.toml.
inline Size front_size() {
    return Size{
        .radius_m = 0.5, .section_width_m = 0.28, .section_height_m = 0.25, .deflection_m = 0.025};
}

// The reference track's soil: moist, rolled clay and sand.
inline soil::Composition reference_soil() {
    return soil::Composition{.clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};
}

inline soil::Surface reference_surface() {
    return soil::surface(reference_soil());
}

}  // namespace tp::tire::testing
