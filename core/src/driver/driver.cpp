#include "tp/core/driver/driver.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::driver {

std::optional<ParamError> validate(const Plan& plan, int gear_count) noexcept {
    const std::array times = {plan.select_gear_s, plan.launch_s, plan.clutch_slip_in_s};
    if (!std::ranges::all_of(times, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (plan.gear < 1 || plan.gear > gear_count) {
        return ParamError::kGearOutOfRange;
    }
    if (plan.select_gear_s < 0.0 || plan.launch_s < plan.select_gear_s ||
        plan.clutch_slip_in_s < 0.0) {
        return ParamError::kTimesOutOfOrder;
    }
    if (!std::isfinite(plan.downshift_share) || plan.downshift_share < 0.0 ||
        plan.downshift_share >= 1.0) {
        return ParamError::kDownshiftShareOutOfRange;
    }
    return std::nullopt;
}

vehicle::Input input(const Plan& plan, State& state, double t_s, const vehicle::State& tractor,
                     const vehicle::Params& params) noexcept {
    if (t_s < plan.select_gear_s) {
        return {.throttle = 0.0, .clutch_pedal = 1.0, .gear = driveline::kNeutral};
    }
    vehicle::Input in{.throttle = 1.0, .clutch_pedal = 1.0, .gear = plan.gear};
    if (t_s >= plan.launch_s) {
        const double slipping_s = t_s - plan.launch_s;
        in.clutch_pedal = plan.clutch_slip_in_s > 0.0
                              ? std::clamp(1.0 - slipping_s / plan.clutch_slip_in_s, 0.0, 1.0)
                              : 0.0;
    }
    // A gear down once, as the sled pulls the engine down: after it, the
    // gear that went in stays (another shift would need the clutch).
    if (in.clutch_pedal == 0.0 && plan.downshift_share > 0.0) {
        const driveline::State& gearbox = tractor.driveline;
        const bool below =
            tractor.engine.omega_rad_s < plan.downshift_share * params.engine.rated_omega_rad_s;
        state.watching = state.watching || !below;
        if (gearbox.load_shifts > 0) {
            in.gear = gearbox.gear;
        } else if (state.watching && below && tractor.engine.running &&
                   driveline::shifts_under_load(gearbox, params.driveline, gearbox.gear - 1)) {
            in.gear = gearbox.gear - 1;
        }
    }
    return in;
}

}  // namespace tp::driver
