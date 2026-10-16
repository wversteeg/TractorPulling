#pragma once

#include <numbers>

namespace tp::units {

inline constexpr double kRadPerSecPerRpm = 2.0 * std::numbers::pi / 60.0;

[[nodiscard]] constexpr double rpm_to_rad_s(double rpm) noexcept {
    return rpm * kRadPerSecPerRpm;
}
[[nodiscard]] constexpr double rad_s_to_rpm(double omega_rad_s) noexcept {
    return omega_rad_s / kRadPerSecPerRpm;
}

}  // namespace tp::units
