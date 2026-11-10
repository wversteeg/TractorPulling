#include "tp/core/vehicle/tuning.hpp"

#include <algorithm>

namespace tp::vehicle {

Tuning as_built(const Params& params) noexcept {
    return {.ballast_position = params.chassis.ballast.position,
            .rear_tire_pressure_bar = params.rear_tire.pressure.reference_bar,
            .hitch_height_m = params.chassis.hitch_height_m};
}

Params tuned(const Params& params, const Tuning& tuning) noexcept {
    Params out = params;
    out.chassis = chassis::with_ballast_at(params.chassis, tuning.ballast_position);
    out.rear_tire = tire::at_pressure(params.rear_tire, tuning.rear_tire_pressure_bar);
    // Written so that NaN keeps the top hole too.
    const double low_m = params.chassis.hitch_min_height_m;
    if (low_m > 0.0 && tuning.hitch_height_m > 0.0) {
        out.chassis.hitch_height_m =
            std::clamp(tuning.hitch_height_m, low_m, params.chassis.hitch_height_m);
    }
    return out;
}

}  // namespace tp::vehicle
