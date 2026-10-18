#pragma once

#include "tp/core/vehicle/vehicle.hpp"

#include <cstdlib>

#include "../chassis/reference_chassis.hpp"
#include "../clutch/reference_clutch.hpp"
#include "../driveline/reference_driveline.hpp"
#include "../engine/reference_engine.hpp"
#include "../tire/reference_tire.hpp"

namespace tp::vehicle::testing {

// The reference light farm-stock tractor, as data/tractors/farm_stock_light.toml.
inline Params reference_params() {
    return Params{
        .engine = engine::testing::reference_params(),
        .clutch = clutch::testing::reference_params(),
        .driveline = driveline::testing::reference_params(),
        .rear_tire = tire::testing::reference_params(),
        .rear_tire_count = 2,
        .front_tire = tire::testing::front_size(),
        .chassis = chassis::testing::reference_params(),
        .pto_efficiency = 0.9,
    };
}

// The reference track's surface under both axles.
inline Ground reference_ground() {
    const soil::Surface surface = tire::testing::reference_surface();
    return Ground{.rear = surface, .front = surface};
}

inline Model reference_model() {
    const auto model = Model::create(reference_params());
    if (!model) {
        std::abort();
    }
    return *model;
}

}  // namespace tp::vehicle::testing
