#include "tp/core/controls/controls.hpp"

#include "tp/core/clutch/clutch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace tp::controls {

namespace {

// A control held to its travel, 0 to 1 (an infinity too, as any value out
// of range). Not a number (a 0/0 in a front end's slider or tilt maths)
// reads as the control at `rest`: std::clamp would pass a NaN on, and it
// would ruin the run for good.
double unit(double value, double rest) noexcept {
    return std::isnan(value) ? rest : std::clamp(value, 0.0, 1.0);
}

// A steering control, -1 to 1; not a number reads as straight ahead.
double signed_unit(double value) noexcept {
    return std::isnan(value) ? 0.0 : std::clamp(value, -1.0, 1.0);
}

// Where each control rests: throttle closed, clutch pressed, brakes off.
constexpr double kThrottleRest = 0.0;
constexpr double kClutchPedalRest = 1.0;
constexpr double kBrakeRest = 0.0;

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

// The steering after this step: the control past the dead zone, stretched
// to the whole range, and followed at the steering rate (at once without
// one).
double steering_after(double dead_zone, std::optional<double> rate_per_s, double steering,
                      double control, double dt_s) noexcept {
    const double travel = std::abs(control);
    const double past_dead_zone =
        travel > dead_zone ? std::copysign((travel - dead_zone) / (1.0 - dead_zone), control) : 0.0;
    if (!rate_per_s) {
        return past_dead_zone;
    }
    const double max_change = *rate_per_s * dt_s;
    return std::clamp(past_dead_zone, steering - max_change, steering + max_change);
}

}  // namespace

std::optional<ParamError> validate(const EasyParams& params) noexcept {
    const ClutchAssist& assist = params.clutch_assist;
    const std::array values = {params.steer_share,      params.steer_dead_zone,
                               params.steer_rate_per_s, assist.engage_time_s,
                               assist.release_time_s,   assist.guard_speed_share,
                               assist.guard_band_share, assist.min_throttle};
    if (!std::ranges::all_of(values, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.steer_share <= 0.0 || params.steer_share > 1.0) {
        return ParamError::kSteerShareOutOfRange;
    }
    if (params.steer_dead_zone < 0.0 || params.steer_dead_zone >= params.steer_share) {
        return ParamError::kSteerDeadZoneOutOfRange;
    }
    if (params.steer_rate_per_s <= 0.0) {
        return ParamError::kSteerRateNotPositive;
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

std::optional<ParamError> validate(const ProParams& params) noexcept {
    const double rate_per_s = params.steer_rate_per_s.value_or(1.0);
    if (!std::isfinite(params.steer_dead_zone) || !std::isfinite(rate_per_s)) {
        return ParamError::kNotFinite;
    }
    if (params.steer_dead_zone < 0.0 || params.steer_dead_zone >= 1.0) {
        return ParamError::kSteerDeadZoneOutOfRange;
    }
    if (rate_per_s <= 0.0) {
        return ParamError::kSteerRateNotPositive;
    }
    return std::nullopt;
}

vehicle::Input pro(const ProInput& input, const ProParams& params, AssistState& state,
                   const vehicle::Params& tractor, double dt_s) noexcept {
    state.steering = steering_after(params.steer_dead_zone, params.steer_rate_per_s, state.steering,
                                    signed_unit(input.steering), dt_s);
    ProInput shaped = input;
    shaped.steering = state.steering;
    return pro(shaped, tractor);
}

vehicle::Input pro(const ProInput& input, const vehicle::Params& tractor) noexcept {
    return {.throttle = unit(input.throttle, kThrottleRest),
            .clutch_pedal = unit(input.clutch_pedal, kClutchPedalRest),
            .gear = input.gear,
            .steering_rad = signed_unit(input.steering) * tractor.steering.max_angle_rad,
            .brake_left = unit(input.brake_left, kBrakeRest),
            .brake_right = unit(input.brake_right, kBrakeRest)};
}

vehicle::Input easy(const EasyInput& input, const EasyParams& params, AssistState& assist,
                    const vehicle::Params& tractor, const vehicle::State& state,
                    double dt_s) noexcept {
    const double throttle = unit(input.throttle, kThrottleRest);
    const int gear_in = state.driveline.gear;
    // A gear the gearbox does not have is held to its range: otherwise the
    // assist would keep the clutch out for a shift that never comes.
    // Reverse, where the gearbox has it, waits with the clutch out until the
    // tractor stands (see driveline::shift).
    const int lowest =
        tractor.driveline.reverse_ratio > 0.0 ? driveline::kReverse : driveline::kNeutral;
    const int wanted_gear =
        std::clamp(input.gear, lowest, static_cast<int>(tractor.driveline.gear_count));
    // A gear down that the gearbox shifts under load goes in at once, the
    // clutch staying in; every other change waits for the clutch out.
    const bool load_shift =
        driveline::shifts_under_load(state.driveline, tractor.driveline, wanted_gear);
    // A gear the gearbox would refuse (under way, below its floor) is not
    // asked for: the assist keeps the clutch in and the gear as it is.
    const bool allowed = wanted_gear <= driveline::kNeutral ||
                         wanted_gear >= driveline::lowest_gear(state.driveline, tractor.driveline);
    const bool shifting = wanted_gear != gear_in && !load_shift && allowed;
    const bool clutch_wanted_out =
        throttle < params.clutch_assist.min_throttle || shifting || gear_in == driveline::kNeutral;
    assist.engagement = engagement_after(params.clutch_assist, assist.engagement, clutch_wanted_out,
                                         tractor.engine, throttle, state.engine.omega_rad_s, dt_s);

    // The first stretch of travel turns the wheels; the rest brakes the
    // rear wheels on the inside of the turn.
    assist.steering = steering_after(params.steer_dead_zone, params.steer_rate_per_s,
                                     assist.steering, signed_unit(input.steering), dt_s);
    const double steering = assist.steering;
    const double travel = std::abs(steering);
    const double wheels = std::min(travel / params.steer_share, 1.0);
    const double brake = travel > params.steer_share
                             ? (travel - params.steer_share) / (1.0 - params.steer_share)
                             : 0.0;
    const bool left = steering > 0.0;
    return {.throttle = throttle,
            .clutch_pedal = clutch::pedal_for(tractor.clutch, assist.engagement),
            .gear = load_shift || (shifting && assist.engagement == 0.0) ? wanted_gear : gear_in,
            .steering_rad = std::copysign(wheels, steering) * tractor.steering.max_angle_rad,
            .brake_left = left ? brake : 0.0,
            .brake_right = left ? 0.0 : brake};
}

}  // namespace tp::controls
