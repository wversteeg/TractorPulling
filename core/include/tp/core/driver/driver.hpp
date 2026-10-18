#pragma once

#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::driver {

/// How a scripted driver pulls: idle in neutral, select the gear, rev up
/// with the clutch pressed, then let the clutch in over `clutch_slip_in_s`
/// (0 dumps it) and pull at full throttle until the run ends. The pullers in
/// a field differ in gear and clutch technique.
struct Plan {
    int gear = 0;
    /// When the gear goes in, clutch pressed [s].
    double select_gear_s = 0.0;
    /// When the clutch starts to come up, at full throttle [s].
    double launch_s = 0.0;
    double clutch_slip_in_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kGearOutOfRange,
    kTimesOutOfOrder,
};

[[nodiscard]] std::optional<ParamError> validate(const Plan& plan, int gear_count) noexcept;

/// The driver's input at `t_s` into the run.
[[nodiscard]] vehicle::Input input(const Plan& plan, double t_s) noexcept;

}  // namespace tp::driver
