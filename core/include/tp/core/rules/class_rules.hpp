#pragma once

#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::rules {

/// A pulling class's technical limits, such as the NTTO standard 3.5 t
/// class. Only rules about what the simulation models are checked; rules on
/// engine layout (cylinders, turbos) or safety equipment stay with the data.
struct ClassRules {
    /// The whole tractor as weighed, driver included [kg].
    double max_mass_kg = 0.0;
    /// Measured at the PTO shaft [W].
    double max_pto_power_w = 0.0;
    /// Unloaded engine speed: the governed high idle [rad/s].
    double max_engine_speed_rad_s = 0.0;
    /// Hitch point above the ground [m]. It may also never be above the rear
    /// axle's centre.
    double max_hitch_height_m = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kLimitNotPositive,
};

[[nodiscard]] std::optional<ParamError> validate(const ClassRules& rules) noexcept;

/// Which rules a tractor breaks; all false means it may compete.
struct Compliance {
    bool too_heavy = false;
    bool too_much_pto_power = false;
    bool engine_speed_too_high = false;
    bool hitch_too_high = false;
    bool hitch_above_rear_axle = false;

    [[nodiscard]] constexpr bool ok() const noexcept {
        return !too_heavy && !too_much_pto_power && !engine_speed_too_high && !hitch_too_high &&
               !hitch_above_rear_axle;
    }
};

/// Peak engine power at the PTO shaft [W]: the engine's net peak times the
/// tractor's PTO efficiency.
[[nodiscard]] double pto_power_w(const vehicle::Params& tractor) noexcept;

/// Checks a valid tractor against valid rules. Not for the simulation step:
/// it samples the engine's power curve.
[[nodiscard]] Compliance check(const ClassRules& rules, const vehicle::Params& tractor) noexcept;

}  // namespace tp::rules
