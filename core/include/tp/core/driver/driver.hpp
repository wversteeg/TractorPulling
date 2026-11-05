#pragma once

#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::driver {

/// How a scripted driver pulls: idle in neutral, select the gear, rev up
/// with the clutch pressed, then let the clutch in over `clutch_slip_in_s`
/// (0 dumps it) and pull at full throttle until the run ends. A driver
/// watches the rev counter once the clutch is fully in and the engine has
/// come back from the launch above `downshift_share` of its rated speed;
/// when the sled then pulls it below that again, they shift a gear down
/// under load, if the gearbox can (driveline::shifts_under_load): once, one
/// gear, whatever more the gearbox allows, and then they stay in it. The
/// pullers in a field differ in gear, clutch technique and when they shift.
///
/// A driver who holds the launch (launch_hold_share above 0) lets the clutch
/// in at the same rate but eases it back at that rate while the engine is
/// below that share of its rated speed, so the revs and the turbo's boost
/// do not collapse in a tall gear; they hold it at most launch_hold_max_s in
/// all, and then let it come in whatever the engine does. Once the clutch
/// is fully in the launch is over: a later drop of the revs is the sled's,
/// for a shift down, not the clutch.
struct Plan {
    int gear = 0;
    /// When the gear goes in, clutch pressed [s].
    double select_gear_s = 0.0;
    /// When the clutch starts to come up, at full throttle [s].
    double launch_s = 0.0;
    double clutch_slip_in_s = 0.0;
    /// The engine speed, as a share of its rated speed, below which the
    /// driver shifts down under load: 0 never, below 1.
    double downshift_share = 0.0;
    /// The engine speed, as a share of its rated speed, below which the
    /// clutch stops coming up during the launch: 0 never holds, below 1.
    double launch_hold_share = 0.0;
    /// The longest the launch is held, in all [s].
    double launch_hold_max_s = 0.0;
};

/// What the driver remembers during a pull; a new pull starts from {}.
struct State {
    /// The engine has come back from the launch: a drop now is the sled's.
    bool watching = false;
    /// A held launch: how far the clutch has come up (0 pressed, 1 fully
    /// in), how long it has been held [s], and the time of the last input
    /// [s], for the step since.
    double clutch_in = 0.0;
    double held_s = 0.0;
    double last_t_s = 0.0;
    /// The held launch is over: the clutch has been fully in.
    bool launched = false;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kGearOutOfRange,
    kTimesOutOfOrder,
    kDownshiftShareOutOfRange,
    kLaunchHoldOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Plan& plan, int gear_count) noexcept;

/// The driver's input at `t_s` into the run, seeing the tractor as it is
/// (`tractor`, of `params`), with what they remember in `state`.
[[nodiscard]] vehicle::Input input(const Plan& plan, State& state, double t_s,
                                   const vehicle::State& tractor,
                                   const vehicle::Params& params) noexcept;

/// The scripted driver's gear down under load, the clutch in (see Plan):
/// the gear to ask for instead of `gear`. They watch the rev counter once
/// the engine is above `downshift_share` of its rated speed (`watching`,
/// kept between steps); when it falls below that again they ask for a gear
/// down, if the gearbox takes it under load, and hold the gear while it
/// shifts. Otherwise `gear`.
[[nodiscard]] int under_load_gear(double downshift_share, bool& watching, int gear,
                                  const vehicle::State& tractor,
                                  const vehicle::Params& params) noexcept;

}  // namespace tp::driver
