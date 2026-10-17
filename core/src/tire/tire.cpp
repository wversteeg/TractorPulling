#include "tp/core/tire/tire.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::tire {

namespace {

constexpr double kFullSlip = 1.0;

// μ scale for the load. Linear load sensitivity alone would eventually make
// force fall as load grows; beyond the load where force peaks the scale is
// held so that more load never means less traction.
double load_factor(const Params& params, double load_n) noexcept {
    const double k = params.load_sensitivity;
    if (k == 0.0) {
        return 1.0;
    }
    const double force_peak_load_n = params.nominal_load_n * (1.0 + k) / (2.0 * k);
    const double effective_load_n = std::min(load_n, force_peak_load_n);
    return 1.0 - k * (effective_load_n / params.nominal_load_n - 1.0);
}

double reference_speed(const Params& params, double wheel_m_s, double ground_m_s) noexcept {
    return std::max({std::abs(wheel_m_s), std::abs(ground_m_s), params.min_reference_speed_m_s});
}

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    const std::array scalars = {params.radius_m,
                                params.nominal_load_n,
                                params.load_sensitivity,
                                params.rolling_resistance_coefficient,
                                params.wheel_inertia_kg_m2,
                                params.min_reference_speed_m_s};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.radius_m <= 0.0) {
        return ParamError::kRadiusNotPositive;
    }
    const math::Curve& mu = params.traction_coefficient;
    if (mu.min_x() != 0.0 || mu.max_x() != kFullSlip) {
        return ParamError::kCurveDoesNotSpanSlip;
    }
    if (mu.evaluate(0.0) != 0.0) {
        return ParamError::kCurveNotZeroAtZeroSlip;
    }
    if (std::ranges::any_of(mu.points(), [](const math::CurvePoint& p) { return p.y < 0.0; })) {
        return ParamError::kCurveNegative;
    }
    if (params.nominal_load_n <= 0.0) {
        return ParamError::kNominalLoadNotPositive;
    }
    if (params.load_sensitivity < 0.0 || params.load_sensitivity >= 1.0) {
        return ParamError::kLoadSensitivityOutOfRange;
    }
    if (params.rolling_resistance_coefficient < 0.0 ||
        params.rolling_resistance_coefficient >= 1.0) {
        return ParamError::kRollingResistanceOutOfRange;
    }
    if (params.wheel_inertia_kg_m2 <= 0.0) {
        return ParamError::kInertiaNotPositive;
    }
    if (params.min_reference_speed_m_s <= 0.0) {
        return ParamError::kMinReferenceSpeedNotPositive;
    }
    return std::nullopt;
}

double slip_ratio(const Params& params, double wheel_surface_speed_m_s,
                  double ground_speed_m_s) noexcept {
    // With wheel and ground moving in opposite directions the raw ratio
    // exceeds 1 in magnitude; that is simply full slip.
    return std::clamp((wheel_surface_speed_m_s - ground_speed_m_s) /
                          reference_speed(params, wheel_surface_speed_m_s, ground_speed_m_s),
                      -kFullSlip, kFullSlip);
}

Traction traction(const Params& params, double wheel_surface_speed_m_s, double ground_speed_m_s,
                  double load_n) noexcept {
    Traction out;
    if (load_n <= 0.0) {
        return out;
    }
    const double reference_m_s = reference_speed(params, wheel_surface_speed_m_s, ground_speed_m_s);
    out.slip_ratio = slip_ratio(params, wheel_surface_speed_m_s, ground_speed_m_s);
    const double magnitude = std::abs(out.slip_ratio);
    const double grip_n = load_n * load_factor(params, load_n);
    out.force_n =
        std::copysign(params.traction_coefficient.evaluate(magnitude) * grip_n, out.slip_ratio);
    // dF/d(slip speed) = grip · μ'(|s|) / reference speed, holding the
    // reference speed: the tangent for a change in ground speed. μ is
    // mirrored for braking, so the slope is the same on both sides; at full
    // slip it is zero.
    out.force_per_slip_speed =
        std::max(grip_n * params.traction_coefficient.slope(magnitude) / reference_m_s, 0.0);
    return out;
}

double rolling_resistance_n(const Params& params, double load_n) noexcept {
    return params.rolling_resistance_coefficient * std::max(load_n, 0.0);
}

}  // namespace tp::tire
