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
};

/// What the driver remembers during a pull; a new pull starts from {}.
struct State {
    /// The engine has come back from the launch: a drop now is the sled's.
    bool watching = false;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kGearOutOfRange,
    kTimesOutOfOrder,
    kDownshiftShareOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Plan& plan, int gear_count) noexcept;

/// The driver's input at `t_s` into the run, seeing the tractor as it is
/// (`tractor`, of `params`), with what they remember in `state`.
[[nodiscard]] vehicle::Input input(const Plan& plan, State& state, double t_s,
                                   const vehicle::State& tractor,
                                   const vehicle::Params& params) noexcept;

}  // namespace tp::driver
