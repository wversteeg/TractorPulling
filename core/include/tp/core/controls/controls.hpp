#pragma once

#include "tp/core/driveline/driveline.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::controls {

/// The pro layout: every control on its own. Steering is the share of the
/// steering's range, -1 (full right) to 1 (full left); the pedals run 0 to
/// 1. Values outside their range, infinities too, are held to it; a value
/// that is not a number reads as the control at rest (throttle closed,
/// clutch pressed, steering straight, brakes off).
struct ProInput {
    double throttle = 0.0;
    /// 0 released (engaged), 1 pressed.
    double clutch_pedal = 1.0;
    int gear = driveline::kNeutral;
    double steering = 0.0;
    double brake_left = 0.0;
    double brake_right = 0.0;
};

/// The easy layout: the throttle, the gear and one steering control whose
/// last stretch brakes the inner rear wheels, as a driver steers a tight
/// turn with the brake. There is no clutch pedal: the clutch assist works
/// the clutch. As in the pro layout, values are held to their range and a
/// value that is not a number reads as the control at rest.
struct EasyInput {
    double throttle = 0.0;
    int gear = driveline::kNeutral;
    /// -1 (full right) to 1 (full left).
    double steering = 0.0;
};

/// Works the clutch for the easy layout. It lets the clutch in at a steady
/// rate while the engine holds its speed, and lets it slip again when the
/// engine is pulled down below the guard speed, faster the further below:
/// the engine keeps working near its peak torque and never stalls. At part
/// throttle the guard is lower: where the governor reaches full fuel for
/// that throttle, so the tractor creeps at a touch of throttle too. It
/// presses the clutch when the driver lifts off the throttle, and before a
/// gear change.
struct ClutchAssist {
    /// From fully out to fully in while the engine holds its speed [s].
    double engage_time_s = 0.0;
    /// From fully in to fully out at the fastest [s].
    double release_time_s = 0.0;
    /// The guard speed at full throttle, as a share of the engine's rated
    /// speed: near the speed of peak torque, below which the engine cannot
    /// pull itself back. At part throttle the guard is at most the speed
    /// where the governor gives full fuel, and at least a band above the
    /// stall speed.
    double guard_speed_share = 0.0;
    /// How far below the guard (a share of the rated speed) the clutch goes
    /// out at the fastest.
    double guard_band_share = 0.0;
    /// Below this throttle the driver has lifted off.
    double min_throttle = 0.0;
};

struct EasyParams {
    /// The share of the steering control's travel that turns the front
    /// wheels, up to full lock; past it the wheels stay at full lock and the
    /// inner rear brake comes on, up to full pedal at the end of the travel.
    double steer_share = 0.0;
    ClutchAssist clutch_assist;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kSteerShareOutOfRange,
    kAssistTimeNotPositive,
    kGuardOutOfRange,
    kMinThrottleOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const EasyParams& params) noexcept;

/// The assist's own state, plain data like the simulation's: how far the
/// clutch is in, as a share of its capacity (0 out, 1 fully in).
struct AssistState {
    double engagement = 0.0;
};

/// Adds the assist's state to a replay hash: a front end replaying an easy
/// session hashes it along with the simulation's state.
constexpr void hash_append(hash::Fnv1a& hash, const AssistState& state) noexcept {
    hash.add(state.engagement);
}

/// The vehicle's input for the pro layout.
[[nodiscard]] vehicle::Input pro(const ProInput& input, const vehicle::Params& tractor) noexcept;

/// The vehicle's input for the easy layout, before this step of the
/// vehicle: reads the tractor's state (engine speed, gear in) and advances
/// the assist by `dt_s`. A requested gear goes in once the assist has the
/// clutch out; until then the gear in stays. A gear outside the gearbox is
/// held to its range.
[[nodiscard]] vehicle::Input easy(const EasyInput& input, const EasyParams& params,
                                  AssistState& assist, const vehicle::Params& tractor,
                                  const vehicle::State& state, double dt_s) noexcept;

}  // namespace tp::controls
