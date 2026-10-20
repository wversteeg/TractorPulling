#include "tp/core/controls/controls.hpp"

#include "tp/core/clutch/clutch.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::controls {

namespace {

double unit(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

double signed_unit(double value) noexcept {
    return std::clamp(value, -1.0, 1.0);
}

// The guard speed at `throttle`: near peak torque at full throttle, and at
// part throttle where the governor reaches full fuel (below it the engine
// lugs), but never so low that the clutch would still be in at the stall.
double guard_rad_s(const ClutchAssist& assist, const engine::Params& engine,
                   double throttle) noexcept {
    const double band_rad_s = assist.guard_band_share * engine.rated_omega_rad_s;
    const double lugging_rad_s = std::min(assist.guard_speed_share * engine.rated_omega_rad_s,
                                          engine::full_fuel_omega_rad_s(engine, throttle));
    return std::max(lugging_rad_s, engine.stall_omega_rad_s + band_rad_s);
}

// How far the assist lets the clutch in after this step: in while the
// engine holds the guard speed, out again below it, and out at the fastest
// rate when the driver lifts off, when neutral is in, or before a shift.
double engagement_after(const ClutchAssist& assist, double engagement, bool clutch_wanted_out,
                        const engine::Params& engine, double throttle, double omega_rad_s,
                        double dt_s) noexcept {
    const double out_rate_per_s = 1.0 / assist.release_time_s;
    if (clutch_wanted_out) {
        return std::max(engagement - out_rate_per_s * dt_s, 0.0);
    }
    const double below_rad_s = guard_rad_s(assist, engine, throttle) - omega_rad_s;
    if (below_rad_s <= 0.0) {
        return std::min(engagement + dt_s / assist.engage_time_s, 1.0);
    }
    const double band_rad_s = assist.guard_band_share * engine.rated_omega_rad_s;
    const double urgency = std::min(below_rad_s / band_rad_s, 1.0);
    return std::max(engagement - urgency * out_rate_per_s * dt_s, 0.0);
}

}  // namespace

std::optional<ParamError> validate(const EasyParams& params) noexcept {
    const ClutchAssist& assist = params.clutch_assist;
    const std::array values = {params.steer_share,      assist.engage_time_s,
                               assist.release_time_s,   assist.guard_speed_share,
                               assist.guard_band_share, assist.min_throttle};
    if (!std::ranges::all_of(values, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.steer_share <= 0.0 || params.steer_share > 1.0) {
        return ParamError::kSteerShareOutOfRange;
    }
    if (assist.engage_time_s <= 0.0 || assist.release_time_s <= 0.0) {
        return ParamError::kAssistTimeNotPositive;
    }
    if (assist.guard_speed_share <= 0.0 || assist.guard_speed_share > 1.0 ||
        assist.guard_band_share <= 0.0 || assist.guard_band_share >= assist.guard_speed_share) {
        return ParamError::kGuardOutOfRange;
    }
    if (assist.min_throttle < 0.0 || assist.min_throttle >= 1.0) {
        return ParamError::kMinThrottleOutOfRange;
    }
    return std::nullopt;
}

vehicle::Input pro(const ProInput& input, const vehicle::Params& tractor) noexcept {
    return {.throttle = unit(input.throttle),
            .clutch_pedal = unit(input.clutch_pedal),
            .gear = input.gear,
            .steering_rad = signed_unit(input.steering) * tractor.steering.max_angle_rad,
            .brake_left = unit(input.brake_left),
            .brake_right = unit(input.brake_right)};
}

vehicle::Input easy(const EasyInput& input, const EasyParams& params, AssistState& assist,
                    const vehicle::Params& tractor, const vehicle::State& state,
                    double dt_s) noexcept {
    const double throttle = unit(input.throttle);
    const int gear_in = state.driveline.gear;
    // A gear the gearbox does not have is held to its range: otherwise the
    // assist would keep the clutch out for a shift that never comes.
    const int wanted_gear =
        std::clamp(input.gear, driveline::kNeutral, static_cast<int>(tractor.driveline.gear_count));
    const bool shifting = wanted_gear != gear_in;
    const bool clutch_wanted_out =
        throttle < params.clutch_assist.min_throttle || shifting || gear_in == driveline::kNeutral;
    assist.engagement = engagement_after(params.clutch_assist, assist.engagement, clutch_wanted_out,
                                         tractor.engine, throttle, state.engine.omega_rad_s, dt_s);

    // The first stretch of travel turns the wheels; the rest brakes the
    // rear wheels on the inside of the turn.
    const double steering = signed_unit(input.steering);
    const double travel = std::abs(steering);
    const double wheels = std::min(travel / params.steer_share, 1.0);
    const double brake = travel > params.steer_share
                             ? (travel - params.steer_share) / (1.0 - params.steer_share)
                             : 0.0;
    const bool left = steering > 0.0;
    return {.throttle = throttle,
            .clutch_pedal = clutch::pedal_for(tractor.clutch, assist.engagement),
            .gear = shifting && assist.engagement == 0.0 ? wanted_gear : gear_in,
            .steering_rad = std::copysign(wheels, steering) * tractor.steering.max_angle_rad,
            .brake_left = left ? brake : 0.0,
            .brake_right = left ? 0.0 : brake};
}

}  // namespace tp::controls
