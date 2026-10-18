#include "tp/core/tire/tire.hpp"

#include "tp/core/math/exp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace tp::tire {

namespace {

constexpr double kFullSlip = 1.0;
// Traction coefficients above this are a data error.
constexpr double kMaxTractionLimit = 2.0;

// Brixius' constants for the mobility number and motion resistance.
constexpr double kDeflectionWeight = 5.0;
constexpr double kAspectWeight = 3.0;
constexpr double kSlipSinkage = 0.5;
constexpr double kTireLosses = 0.04;

double reference_speed(const Params& params, double wheel_m_s, double ground_m_s) noexcept {
    return std::max({std::abs(wheel_m_s), std::abs(ground_m_s), params.min_reference_speed_m_s});
}

}  // namespace

std::optional<ParamError> validate(const Size& size) noexcept {
    const std::array scalars = {size.radius_m, size.section_width_m, size.section_height_m,
                                size.deflection_m};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (size.radius_m <= 0.0) {
        return ParamError::kRadiusNotPositive;
    }
    if (size.section_width_m <= 0.0 || size.section_height_m <= 0.0) {
        return ParamError::kSectionNotPositive;
    }
    if (size.deflection_m < 0.0 || size.deflection_m >= size.section_height_m) {
        return ParamError::kDeflectionOutOfRange;
    }
    return std::nullopt;
}

std::optional<ParamError> validate(const Params& params) noexcept {
    if (const auto error = validate(params.size)) {
        return error;
    }
    const std::array scalars = {params.traction.limit, params.traction.mobility_rate,
                                params.traction.slip_rate, params.wheel_inertia_kg_m2,
                                params.min_reference_speed_m_s};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.traction.limit <= 0.0 || params.traction.limit > kMaxTractionLimit) {
        return ParamError::kTractionLimitOutOfRange;
    }
    if (params.traction.mobility_rate <= 0.0 || params.traction.slip_rate <= 0.0) {
        return ParamError::kTractionRateNotPositive;
    }
    if (params.wheel_inertia_kg_m2 <= 0.0) {
        return ParamError::kInertiaNotPositive;
    }
    if (params.min_reference_speed_m_s <= 0.0) {
        return ParamError::kMinReferenceSpeedNotPositive;
    }
    return std::nullopt;
}

double mobility_number(const Size& size, double load_n, const soil::Surface& surface) noexcept {
    if (load_n <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const double diameter_m = 2.0 * size.radius_m;
    return surface.cone_index_pa * size.section_width_m * diameter_m / load_n *
           (1.0 + kDeflectionWeight * size.deflection_m / size.section_height_m) /
           (1.0 + kAspectWeight * size.section_width_m / diameter_m);
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
                  double load_n, const soil::Surface& surface) noexcept {
    Traction out;
    if (load_n <= 0.0) {
        return out;
    }
    const double reference_m_s = reference_speed(params, wheel_surface_speed_m_s, ground_speed_m_s);
    out.slip_ratio = slip_ratio(params, wheel_surface_speed_m_s, ground_speed_m_s);
    const double s = std::abs(out.slip_ratio);

    const TractionCurve& curve = params.traction;
    const double bite =
        surface.grip_factor * curve.limit *
        (1.0 - math::exp(-curve.mobility_rate * mobility_number(params.size, load_n, surface)));
    const double slip_term = math::exp(-curve.slip_rate * s);
    const double built = 1.0 - slip_term;
    const double kept = 1.0 - surface.spin_fall_off * s * s;
    out.force_n = std::copysign(bite * built * kept * load_n, out.slip_ratio);

    // dμ/d|s| = bite · (slip_rate e^(-slip_rate s) kept - built 2 f s); the
    // slope is per change in slip speed, with the reference speed held. μ is
    // mirrored for braking, so the slope is the same on both sides.
    const double dmu_ds =
        bite * (curve.slip_rate * slip_term * kept - built * 2.0 * surface.spin_fall_off * s);
    out.force_per_slip_speed = std::max(load_n * dmu_ds / reference_m_s, 0.0);
    return out;
}

double driven_rolling_resistance(const Size& size, double load_n, double slip_ratio,
                                 const soil::Surface& surface) noexcept {
    const double mobility = mobility_number(size, load_n, surface);
    return 1.0 / mobility + kSlipSinkage * std::abs(slip_ratio) / std::sqrt(mobility);
}

double towed_rolling_resistance(const Size& size, double load_n,
                                const soil::Surface& surface) noexcept {
    return 1.0 / mobility_number(size, load_n, surface) + kTireLosses;
}

}  // namespace tp::tire
