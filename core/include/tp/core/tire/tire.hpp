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

/// A tire's side grip on soil. Like traction, it follows the mobility
/// number: soft ground gives a tire less side grip, and lets it build more
/// slowly with slip angle. With m = 1 - e^(-mobility_rate Bn) and g the
/// surface's grip factor, a tire under load W has the cornering stiffness
/// and side-force limit
///
///   C = stiffness_per_rad · g m W    [N/rad]
///   F_max = limit · g m W            [N]
///
/// The side force builds as the contact patch deflects sideways and relaxes
/// as the tire rolls (a first-order relaxation-length model): a lateral
/// deflection d gives F = -(C / σ) d, and d' = v_lat - |v_roll| d / σ with
/// σ the relaxation length. Rolling steadily, F = C α for small slip angles
/// α = -v_lat / v_roll; at a standstill the patch holds like a spring
/// instead of the slip angle becoming undefined. Past F_max the patch slides.
struct Cornering {
    /// Side force per unit of load per radian of slip angle on strong
    /// ground [1/rad].
    double stiffness_per_rad = 0.0;
    /// Side force per unit of load at most, on strong ground.
    double limit = 0.0;
    /// How quickly side grip approaches its strong-ground values as the
    /// mobility number grows.
    double mobility_rate = 0.0;
    /// Distance the tire rolls while its side force builds [m].
    double relaxation_length_m = 0.0;
};

/// What the inflation pressure does to a tire, which the puller sets before
/// a pull. A tire carries its load on its air: at a lower pressure it
/// deflects more, δ = δ_ref p_ref / p, so its footprint grows and the
/// mobility number with it, which grips better on soft ground. But a soft
/// carcass lets the lugs fold and squirm instead of biting, which costs
/// grip on firm ground where the footprint barely helps:
///
///   limit = limit_ref (1 - lug_squirm max(δ - δ_ref, 0) / h)
///
/// so the best pressure depends on the track. Harder than the reference,
/// the lugs gain nothing and the smaller footprint costs grip: the
/// reference is what suits firm ground. Left out: the rolling radius
/// shrinking with the deflection (it would move the axle and the gearing).
struct Pressure {
    /// The pressure the tire's deflection and traction limit are given at;
    /// 0 for a tire whose pressure is not set [bar].
    double reference_bar = 0.0;
    /// The range a puller may set it to [bar].
    double min_bar = 0.0;
    double max_bar = 0.0;
    /// Traction limit lost per unit of extra deflection over the section
    /// height.
    double lug_squirm = 0.0;
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
    /// Side grip; see Cornering.
    Cornering cornering;
    /// Wheel and tire about the axle [kg·m²].
    double wheel_inertia_kg_m2 = 0.0;
    /// Smallest reference speed for the slip ratio [m/s].
    double min_reference_speed_m_s = 0.0;
    Pressure pressure;
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
    kCorneringNotPositive,
    kCorneringLimitOutOfRange,
    /// The range not 0 < min <= reference <= max, the squirm outside
    /// [0, 1), or the deflection at the lowest pressure not less than the
    /// section height.
    kPressureOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Size& size) noexcept;
[[nodiscard]] std::optional<ParamError> validate(const Cornering& cornering) noexcept;
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// The tire at `pressure_bar` (held to its range; a pressure that is not a
/// number leaves it at the reference): its deflection and
/// traction limit (see Pressure). A tire whose pressure is not set, or the
/// reference pressure, gives `params` unchanged, bit for bit.
[[nodiscard]] Params at_pressure(const Params& params, double pressure_bar) noexcept;

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
    /// full_bite() for this load and ground, which traction works from.
    double full_bite = 0.0;
};

[[nodiscard]] Traction traction(const Params& params, double wheel_surface_speed_m_s,
                                double ground_speed_m_s, double load_n,
                                const soil::Surface& surface) noexcept;

/// What the ground gives a tire under its load, which traction, motion
/// resistance and side grip all start from. Working it out once per step
/// and sharing it between tires on the same ground and load saves the
/// repeated mobility number and exponentials.
struct Footing {
    /// Brixius' mobility number, and its square root.
    double mobility_number = 0.0;
    double root_mobility = 0.0;
    /// full_bite(); 0 without load.
    double full_bite = 0.0;
};

[[nodiscard]] Footing footing(const Params& params, double load_n,
                              const soil::Surface& surface) noexcept;

/// traction(), driven_rolling_resistance(), thrown_soil_m3_s() and
/// side_grip() on a footing worked out beforehand; the same results.
[[nodiscard]] Traction traction(const Params& params, const Footing& footing,
                                double wheel_surface_speed_m_s, double ground_speed_m_s,
                                double load_n, const soil::Surface& surface) noexcept;
[[nodiscard]] double driven_rolling_resistance(const Footing& footing, double slip_ratio) noexcept;
[[nodiscard]] double thrown_soil_m3_s(const Size& size, const Footing& footing, double load_n,
                                      double slip_ratio, double wheel_surface_speed_m_s) noexcept;

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

/// Traction coefficient at full bite on this ground: what the tire could
/// pull per unit of load at the best slip, before spin fall-off [-].
[[nodiscard]] double full_bite(const Params& params, double load_n,
                               const soil::Surface& surface) noexcept;

/// A tire's side grip under its load on this ground, for the step.
struct SideGrip {
    /// Lateral stiffness of the contact patch, C / σ [N/m].
    double stiffness_n_per_m = 0.0;
    /// Side force at most [N].
    double limit_n = 0.0;
};

[[nodiscard]] SideGrip side_grip(const Size& size, const Cornering& cornering, double load_n,
                                 const soil::Surface& surface) noexcept;
[[nodiscard]] SideGrip side_grip(const Cornering& cornering, const Footing& footing, double load_n,
                                 const soil::Surface& surface) noexcept;

/// A driven tire's side grip is what its traction leaves: the force it
/// pulls with uses up part of its grip (a friction ellipse against full
/// bite), and a spinning tire's patch slides mostly along the tire, so its
/// side grip falls with slip and is gone at full spin.
[[nodiscard]] SideGrip with_traction(SideGrip grip, double traction_n, double full_bite_n,
                                     double slip_ratio) noexcept;

/// The side force from a lateral patch deflection, positive to the tire's
/// left [N].
[[nodiscard]] double side_force_n(const SideGrip& grip, double deflection_m) noexcept;

/// The patch deflection after a step: it follows the contact's sideways
/// speed `lateral_m_s` (to the tire's left) and relaxes as the tire rolls at
/// `rolling_m_s`, taken implicitly so it is stable at any speed. Below
/// `min_rolling_m_s` the patch relaxes as if rolling that fast, so a tire at
/// rest creeps under a side load rather than holding it for ever. Where the
/// force would pass the limit, the patch slides and stays at the limit.
[[nodiscard]] double relax_deflection_m(const SideGrip& grip, const Cornering& cornering,
                                        double deflection_m, double lateral_m_s, double rolling_m_s,
                                        double min_rolling_m_s, double dt_s) noexcept;

}  // namespace tp::tire
