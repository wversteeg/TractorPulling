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

/// Metric horsepower (pk, PS, ch): the unit of Dutch and European class rules.
inline constexpr double kWattsPerMetricHorsepower = 735.49875;

[[nodiscard]] constexpr double metric_hp_to_w(double horsepower) noexcept {
    return horsepower * kWattsPerMetricHorsepower;
}
[[nodiscard]] constexpr double w_to_metric_hp(double power_w) noexcept {
    return power_w / kWattsPerMetricHorsepower;
}

inline constexpr double kDegreesPerRad = 180.0 / std::numbers::pi;

[[nodiscard]] constexpr double rad_to_deg(double angle_rad) noexcept {
    return angle_rad * kDegreesPerRad;
}
[[nodiscard]] constexpr double deg_to_rad(double angle_deg) noexcept {
    return angle_deg / kDegreesPerRad;
}

}  // namespace tp::units
