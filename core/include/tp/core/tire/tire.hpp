#pragma once

#include "tp/core/soil/soil.hpp"

#include <cstdint>
#include <optional>

namespace tp::tire {

/// A tire's size, as the mobility number needs it.
struct Size {
    /// Rolling radius; the diameter is twice this [m].
    double radius_m = 0.0;
    double section_width_m = 0.0;
    double section_height_m = 0.0;
    /// Sidewall deflection at its working load [m].
    double deflection_m = 0.0;
};

/// The gross traction curve's coefficients. Brixius (1987) gives 0.88, 0.1
/// and 7.5 for field tires in agricultural soil; pulling tires on a
/// prepared track grip harder and bite at less slip.
struct TractionCurve {
    /// Traction coefficient on very strong ground at full bite.
    double limit = 0.0;
    /// How quickly grip approaches its limit as the mobility number grows.
    double mobility_rate = 0.0;
    /// How quickly grip builds with slip.
    double slip_rate = 0.0;
};

/// A driven tire on soil, after Brixius (1987, ASAE D497): soil strength
/// (cone index) and the tire's size and load combine into a mobility number
///
///   Bn = CI b d / W · (1 + 5 δ/h) / (1 + 3 b/d)
///
/// that sets both grip and motion resistance. Slip ratio
/// s = (wheel surface speed - ground speed) / reference speed, where the
/// reference is the larger of the two and a small minimum, clamped to
/// [-1, 1]. The traction coefficient is
///
///   μ(s) = g · limit (1 - e^(-mobility_rate Bn)) (1 - e^(-slip_rate |s|)) (1 - f s²)
///
/// with g the surface's grip factor and f its spin fall-off, which Brixius
/// leaves out: past the peak, a spinning tire smears or digs and loses
/// grip, which makes wheelspin run away once it starts. Load sensitivity
/// follows from Bn. Braking (s < 0) mirrors traction.
struct Params {
    Size size;
    TractionCurve traction;
    /// Wheel and tire about the axle [kg·m²].
    double wheel_inertia_kg_m2 = 0.0;
    /// Smallest reference speed for the slip ratio [m/s].
    double min_reference_speed_m_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kRadiusNotPositive,
    kSectionNotPositive,
    kDeflectionOutOfRange,
    kTractionLimitOutOfRange,
    kTractionRateNotPositive,
    kInertiaNotPositive,
    kMinReferenceSpeedNotPositive,
};

[[nodiscard]] std::optional<ParamError> validate(const Size& size) noexcept;
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// Brixius' mobility number for one tire under `load_n` [-]; very large
/// when the tire carries (almost) nothing.
[[nodiscard]] double mobility_number(const Size& size, double load_n,
                                     const soil::Surface& surface) noexcept;

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
                                double ground_speed_m_s, double load_n,
                                const soil::Surface& surface) noexcept;

/// Motion resistance of a driven tire per unit of load: sinkage, 1/Bn, plus
/// the extra rut a slipping tire digs, 0.5 |s| / √Bn. Brixius adds 0.04 for
/// the tire's own losses to both gross traction and motion resistance; for
/// a driven tire it cancels in the net pull, so both leave it out.
[[nodiscard]] double driven_rolling_resistance(const Size& size, double load_n, double slip_ratio,
                                               const soil::Surface& surface) noexcept;

/// Soil a driven tire loosens and throws back [m³/s]: the rut it works,
/// section width times sinkage (diameter times its motion resistance, which
/// grows with slip), along the distance its surface travels. A tire that
/// spins in place still throws soil.
[[nodiscard]] double thrown_soil_m3_s(const Size& size, double load_n, double slip_ratio,
                                      double wheel_surface_speed_m_s,
                                      const soil::Surface& surface) noexcept;

/// Motion resistance of a towed (unpowered) tire per unit of load: 1/Bn
/// plus the tire's own losses, 0.04.
[[nodiscard]] double towed_rolling_resistance(const Size& size, double load_n,
                                              const soil::Surface& surface) noexcept;

}  // namespace tp::tire
