#include "tp/core/vehicle/tuning.hpp"

namespace tp::vehicle {

Tuning as_built(const Params& params) noexcept {
    return {.ballast_position = params.chassis.ballast.position,
            .rear_tire_pressure_bar = params.rear_tire.pressure.reference_bar};
}

Params tuned(const Params& params, const Tuning& tuning) noexcept {
    Params out = params;
    out.chassis = chassis::with_ballast_at(params.chassis, tuning.ballast_position);
    out.rear_tire = tire::at_pressure(params.rear_tire, tuning.rear_tire_pressure_bar);
    return out;
}

}  // namespace tp::vehicle
