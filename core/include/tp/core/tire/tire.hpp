#pragma once

#include "tp/core/math/curve.hpp"

#include <cstdint>
#include <optional>

namespace tp::tire {

/// Longitudinal tire force from slip, for one tire.
///
/// Slip ratio s = (wheel surface speed - ground speed) / reference speed,
/// where the reference is the larger of the two speeds and a small minimum,
/// clamped to [-1, 1]. It is 1 for full wheelspin from standstill and well
/// defined at rest. Traction is
///
///   Fx = sign(s) · μ(|s|) · Fz · load_factor(Fz)
///
/// with μ from a data table: it rises steeply to a peak and falls beyond it,
/// which makes wheelspin run away once it starts. Load sensitivity lowers μ
/// as the load grows, so more weight gives more but less than proportional
/// traction. Braking (s < 0) mirrors traction.
struct Params {
    /// Rolling radius [m].
    double radius_m = 0.0;
    /// Traction coefficient μ against slip ratio, from 0 to 1. μ(0) = 0.
    math::Curve traction_coefficient;
    /// Load at which `traction_coefficient` applies unchanged [N].
    double nominal_load_n = 0.0;
    /// Fractional loss of μ per unit of load above nominal, in [0, 1).
    double load_sensitivity = 0.0;
    /// Rolling resistance force per unit of load.
    double rolling_resistance_coefficient = 0.0;
    /// Wheel and tire about the axle [kg·m²].
    double wheel_inertia_kg_m2 = 0.0;
    /// Smallest reference speed for the slip ratio [m/s].
    double min_reference_speed_m_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kRadiusNotPositive,
    kCurveDoesNotSpanSlip,
    kCurveNotZeroAtZeroSlip,
    kCurveNegative,
    kNominalLoadNotPositive,
    kLoadSensitivityOutOfRange,
    kRollingResistanceOutOfRange,
    kInertiaNotPositive,
    kMinReferenceSpeedNotPositive,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// Slip ratio for a wheel surface speed ω·r and a ground speed [m/s].
[[nodiscard]] double slip_ratio(const Params& params, double wheel_surface_speed_m_s,
                                double ground_speed_m_s) noexcept;

/// Traction at the current speeds, with its sensitivity to slip speed for a
/// linearly implicit solve: over a step, the force is taken as
/// force_n + force_per_slip_speed · (change in wheel surface speed minus
/// ground speed). It is the tangent with the reference speed held (exact for
/// a change in ground speed), never negative: beyond the peak the force is
/// held explicit, as the curve's falling side is unstable anyway.
struct Traction {
    double force_n = 0.0;
    double force_per_slip_speed = 0.0;  // [N per m/s]
    double slip_ratio = 0.0;
};

[[nodiscard]] Traction traction(const Params& params, double wheel_surface_speed_m_s,
                                double ground_speed_m_s, double load_n) noexcept;

/// Rolling resistance of one tire under `load_n` [N].
[[nodiscard]] double rolling_resistance_n(const Params& params, double load_n) noexcept;

}  // namespace tp::tire
