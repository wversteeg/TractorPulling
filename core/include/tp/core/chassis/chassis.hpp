#pragma once

#include "tp/core/hash/fnv1a.hpp"

#include <cstdint>
#include <optional>

namespace tp::chassis {

/// A one-sided spring-damper ground contact: it pushes, never pulls.
struct Contact {
    double stiffness_n_per_m = 0.0;
    double damping_n_s_per_m = 0.0;
};

/// The bar behind the rear axle that catches the tractor in a wheelie. NTTO
/// requires one in the standard classes.
struct WheelieBar {
    /// Tip behind the rear axle [m].
    double behind_rear_axle_m = 0.0;
    /// Tip above the ground with the tractor level [m].
    double tip_height_m = 0.0;
    Contact contact;
    /// Drag of the skids or wheels at the tip per unit of load.
    double rolling_resistance_coefficient = 0.0;
};

/// The tractor body: it moves along the track and pitches about the rear
/// axle.
///
/// Planar rigid body. The rear axle stays at the rear tire's radius above the
/// ground (rear tire compliance is ignored) and is the pitch pivot; pitch θ
/// is positive nose-up. Body points are given relative to the rear axle, x
/// forward and z up with the tractor level. Two one-sided contacts hold the
/// body up besides the rear tires: the front tires, below the front axle,
/// and the wheelie bar tip. The front tires are preloaded so that θ = 0 is
/// the balance at rest.
///
/// Moments about the rear axle, nose-up positive:
///
///   I θ'' = τ_axle + F_drawbar z_hitch(θ) - m g x_cg(θ) + m a z_cg(θ)
///           + front tires + wheelie bar
///
/// with I the pitch inertia about the axle, τ_axle the torque the gearbox
/// puts on the rear axle (its reaction lifts the nose), and m a the inertial
/// load of the accelerating pivot. The rear tires carry what the contacts do
/// not, including the vertical inertia of the pitching body.
///
/// Left out: the centre of gravity moving along x as the body pitches does
/// not feed back into the longitudinal motion, and the drawbar pull stays
/// horizontal.
struct Params {
    /// Whole tractor, wheels included [kg].
    double mass_kg = 0.0;
    double wheelbase_m = 0.0;
    /// Centre of gravity ahead of the rear axle, between the axles [m].
    double cg_ahead_of_rear_axle_m = 0.0;
    /// Centre of gravity above the ground [m].
    double cg_height_m = 0.0;
    /// Pitch inertia of the whole tractor about its centre of gravity [kg·m²].
    double pitch_inertia_kg_m2 = 0.0;
    /// Rear axle above the ground: the rear tire radius [m].
    double rear_axle_height_m = 0.0;
    /// Drawbar hitch point above the ground [m].
    double hitch_height_m = 0.0;
    /// Drawbar hitch point behind the rear axle [m].
    double hitch_behind_rear_axle_m = 0.0;
    /// Rolling resistance of the (unpowered) front tires per unit of load.
    double front_rolling_resistance_coefficient = 0.0;
    /// Both front tires together, acting below the front axle.
    Contact front_tires;
    WheelieBar wheelie_bar;
};

/// The wheelie bar must touch down before the tractor pitches this far
/// [rad]: 30°.
inline constexpr double kMaxWheelieBarContactPitch_rad = 0.523598775598298873077;

enum class ParamError : std::uint8_t {
    kNotFinite,
    kMassNotPositive,
    kWheelbaseNotPositive,
    kCgOutsideWheelbase,
    kCgHeightNotPositive,
    kPitchInertiaNotPositive,
    kRearAxleHeightNotPositive,
    kHitchHeightNegative,
    kHitchAheadOfRearAxle,
    kRollingResistanceOutOfRange,
    kFrontTireStiffnessNotPositive,
    kFrontTireDampingNegative,
    kWheelieBarNotBehindRearAxle,
    kWheelieBarTipNotAboveGround,
    kWheelieBarTooHigh,
    kWheelieBarStiffnessNotPositive,
    kWheelieBarDampingNegative,
    kWheelieBarRollingResistanceOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

struct State {
    double position_m = 0.0;
    double velocity_m_s = 0.0;
    /// Over the last step; feeds the pitch moment and the rear axle load.
    double acceleration_m_s2 = 0.0;
    double pitch_rad = 0.0;
    double pitch_rate_rad_s = 0.0;
    /// Over the last step; feeds the rear axle load.
    double pitch_acceleration_rad_s2 = 0.0;
};

/// The ground's vertical forces on the tractor [N].
struct AxleLoads {
    double rear_n = 0.0;
    double front_n = 0.0;
    double wheelie_bar_n = 0.0;
    /// The front tires are off the ground.
    bool front_lifted = false;
};

/// Loads at the start of a step, from the state: the contacts' spring and
/// damper forces, and the rear tires carrying the rest of the weight and
/// the body's vertical inertia (from the last step's pitch acceleration).
[[nodiscard]] AxleLoads axle_loads(const Params& params, const State& state) noexcept;

/// Speed at the end of a step as a line in the drive force: free + k · F.
struct Prediction {
    double free_velocity_m_s = 0.0;
    double velocity_per_n = 0.0;
};

/// `resisting_force_n` opposes forward motion (drawbar pull, rolling
/// resistance). It can hold the body at rest but never push it backwards;
/// there is no reverse.
[[nodiscard]] Prediction predict(const State& state, const Params& params, double resisting_force_n,
                                 double dt_s) noexcept;

/// Longitudinal motion over the step.
void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept;

/// Pitch over the step, after advance() has set this step's acceleration.
/// The contacts are linearized and integrated with the implicit midpoint
/// rule: stable however stiff the wheelie bar, and free of the numerical
/// damping backward Euler would add to the front tires' bounce.
void advance_pitch(State& state, const Params& params, double axle_torque_nm,
                   double drawbar_force_n, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.acceleration_m_s2);
    hash.add(state.pitch_rad);
    hash.add(state.pitch_rate_rad_s);
    hash.add(state.pitch_acceleration_rad_s2);
}

}  // namespace tp::chassis
