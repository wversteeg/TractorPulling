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

std::optional<ParamError> validate(const Cornering& cornering) noexcept {
    const std::array scalars = {cornering.stiffness_per_rad, cornering.limit,
                                cornering.mobility_rate, cornering.relaxation_length_m};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (cornering.stiffness_per_rad <= 0.0 || cornering.mobility_rate <= 0.0 ||
        cornering.relaxation_length_m <= 0.0) {
        return ParamError::kCorneringNotPositive;
    }
    if (cornering.limit <= 0.0 || cornering.limit > kMaxTractionLimit) {
        return ParamError::kCorneringLimitOutOfRange;
    }
    return std::nullopt;
}

std::optional<ParamError> validate(const Params& params) noexcept {
    if (const auto error = validate(params.size)) {
        return error;
    }
    if (const auto error = validate(params.cornering)) {
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
    const double bite = full_bite(params, load_n, surface);
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

double thrown_soil_m3_s(const Size& size, double load_n, double slip_ratio,
                        double wheel_surface_speed_m_s, const soil::Surface& surface) noexcept {
    if (load_n <= 0.0) {
        return 0.0;
    }
    const double sinkage_m =
        2.0 * size.radius_m * driven_rolling_resistance(size, load_n, slip_ratio, surface);
    return size.section_width_m * sinkage_m * std::abs(wheel_surface_speed_m_s);
}

double towed_rolling_resistance(const Size& size, double load_n,
                                const soil::Surface& surface) noexcept {
    return 1.0 / mobility_number(size, load_n, surface) + kTireLosses;
}

double full_bite(const Params& params, double load_n, const soil::Surface& surface) noexcept {
    const TractionCurve& curve = params.traction;
    return surface.grip_factor * curve.limit *
           (1.0 - math::exp(-curve.mobility_rate * mobility_number(params.size, load_n, surface)));
}

SideGrip side_grip(const Size& size, const Cornering& cornering, double load_n,
                   const soil::Surface& surface) noexcept {
    if (load_n <= 0.0) {
        return {};
    }
    const double grip_n =
        surface.grip_factor *
        (1.0 - math::exp(-cornering.mobility_rate * mobility_number(size, load_n, surface))) *
        load_n;
    return {
        .stiffness_n_per_m = cornering.stiffness_per_rad * grip_n / cornering.relaxation_length_m,
        .limit_n = cornering.limit * grip_n};
}

SideGrip with_traction(SideGrip grip, double traction_n, double full_bite_n,
                       double slip_ratio) noexcept {
    const double used = full_bite_n > 0.0 ? std::min(std::abs(traction_n) / full_bite_n, 1.0) : 1.0;
    const double sliding = std::min(std::abs(slip_ratio), kFullSlip);
    grip.limit_n *= std::sqrt(1.0 - used * used) * (kFullSlip - sliding);
    return grip;
}

double side_force_n(const SideGrip& grip, double deflection_m) noexcept {
    return std::clamp(-grip.stiffness_n_per_m * deflection_m, -grip.limit_n, grip.limit_n);
}

double relax_deflection_m(const SideGrip& grip, const Cornering& cornering, double deflection_m,
                          double lateral_m_s, double rolling_m_s, double min_rolling_m_s,
                          double dt_s) noexcept {
    if (grip.stiffness_n_per_m <= 0.0) {
        // Off the ground: the patch springs back.
        return 0.0;
    }
    const double rolling = std::max(std::abs(rolling_m_s), min_rolling_m_s);
    const double next_m = (deflection_m + dt_s * lateral_m_s) /
                          (1.0 + dt_s * rolling / cornering.relaxation_length_m);
    const double sliding_m = grip.limit_n / grip.stiffness_n_per_m;
    return std::clamp(next_m, -sliding_m, sliding_m);
}

}  // namespace tp::tire
