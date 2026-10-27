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

/// Weights the puller moves along the tractor before a pull, from behind
/// the rear axle to the front weight bracket: more on the rear axle grips
/// harder but lifts the nose sooner. They are part of the tractor's mass,
/// and its centre of gravity and inertias are given with them where they
/// sit as built. Treated as a point mass at the centre of gravity's height
/// (it moves only along the tractor) and on its centre line.
struct Ballast {
    /// 0 for none [kg].
    double mass_kg = 0.0;
    /// Where the weights sit at their rearmost (position 0) and foremost
    /// (position 1), ahead of the rear axle; negative is behind it [m].
    double rear_ahead_of_rear_axle_m = 0.0;
    double front_ahead_of_rear_axle_m = 0.0;
    /// Where they sit as built, 0 (rearmost) to 1 (foremost).
    double position = 0.0;
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
///   I θ'' = τ_axle + F_pull z_hitch(θ) + F_lift x_hitch(θ) - m g x_cg(θ)
///           + m a z_cg(θ) + front tires + wheelie bar
///
/// with I the pitch inertia about the axle, τ_axle the torque the gearbox
/// puts on the rear axle (its reaction lifts the nose), and m a the inertial
/// load of the accelerating pivot. The rear tires carry what the contacts do
/// not, including the vertical inertia of the pitching body.
///
/// The drawbar (the chain to the sled) pulls at the hitch along its own
/// line, so it can pull the hitch down or up as well as back; that also
/// shifts load between the rear tires and the contacts.
///
/// In the ground plane the body moves forward and sideways and turns
/// (yaws) as a rigid body, written in the body frame at the centre of
/// gravity: with u forward and v_cg sideways (to the left) at the centre of
/// gravity and r the yaw rate (positive turning left),
///
///   m (u' - v_cg r) = ΣF_x      m (v_cg' + u r) = ΣF_y      I_z r' = ΣM_z
///
/// Side forces act at the front axle (front tires), the rear axle (rear
/// tires) and the hitch (the chain); unequal pull of the left and right rear
/// tires adds a yaw moment. The state keeps the sideways speed at
/// the rear axle, v = v_cg - b r, which is what the rear tires feel.
///
/// Left out: the centre of gravity moving along x as the body pitches does
/// not feed back into the longitudinal motion; the body does not roll, so
/// side forces shift no load between left and right; pitch and yaw do not
/// couple.

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
    /// Yaw inertia of the whole tractor about its centre of gravity [kg·m²].
    double yaw_inertia_kg_m2 = 0.0;
    /// Rear axle above the ground: the rear tire radius [m].
    double rear_axle_height_m = 0.0;
    /// Between the left and right rear tires' centres [m].
    double rear_track_m = 0.0;
    /// Between the left and right front tires' centres [m].
    double front_track_m = 0.0;
    /// Drawbar hitch point above the ground [m].
    double hitch_height_m = 0.0;
    /// Drawbar hitch point behind the rear axle [m].
    double hitch_behind_rear_axle_m = 0.0;
    /// Both front tires together, acting below the front axle.
    Contact front_tires;
    WheelieBar wheelie_bar;
    Ballast ballast;
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
    kYawInertiaNotPositive,
    kRearAxleHeightNotPositive,
    kRearTrackNotPositive,
    kFrontTrackNotPositive,
    kHitchHeightNegative,
    kHitchAheadOfRearAxle,
    kFrontTireStiffnessNotPositive,
    kFrontTireDampingNegative,
    kWheelieBarNotBehindRearAxle,
    kWheelieBarTipNotAboveGround,
    kWheelieBarTooHigh,
    kWheelieBarStiffnessNotPositive,
    kWheelieBarDampingNegative,
    kWheelieBarRollingResistanceOutOfRange,
    /// Negative or not less than the tractor's mass, its positions not
    /// rear to front, its position outside [0, 1], or the centre of gravity
    /// leaving the wheelbase as the weights move.
    kBallastOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// The tractor with its ballast moved to `position` (held to [0, 1]; a
/// position that is not finite, NaN or ±inf, leaves it where it was built): the
/// centre of gravity moves by the ballast's share of the mass, and the
/// pitch and yaw inertias follow (parallel axes). The ballast as built
/// gives `params` unchanged, bit for bit.
[[nodiscard]] Params with_ballast_at(const Params& params, double position) noexcept;

/// The tractor with `mass_kg` of ballast instead of what it has, where its
/// ballast sits now: the whole mass changes by the difference, the centre
/// of gravity moves towards the weights (or away), and the pitch and yaw
/// inertias follow (parallel axes; the weights as a point at the centre of
/// gravity's height). The ballast it has gives `params` unchanged, bit for
/// bit; so does a mass that is not finite or below 0. validate() judges
/// the result (e.g. ballast heavier than the tractor).
[[nodiscard]] Params with_ballast_mass(const Params& params, double mass_kg) noexcept;

/// The share of the tractor's weight on the rear axle at rest, 0 to 1:
/// from the centre of gravity alone, as a setup screen shows it (the front
/// tires' spring and the hitch do not enter).
[[nodiscard]] double rear_axle_share(const Params& params) noexcept;

/// Positions are in the world: x along the track from where the rear axle
/// started, y to its left. Speeds are in the body frame.
struct State {
    /// Rear axle along the track [m].
    double position_m = 0.0;
    /// Forward speed [m/s].
    double velocity_m_s = 0.0;
    /// Over the last step; feeds the pitch moment and the rear axle load.
    double acceleration_m_s2 = 0.0;
    double pitch_rad = 0.0;
    double pitch_rate_rad_s = 0.0;
    /// Over the last step; feeds the rear axle load.
    double pitch_acceleration_rad_s2 = 0.0;
    /// Rear axle to the left of the track's centre line [m].
    double lateral_position_m = 0.0;
    /// Body x from the track's direction, positive turned left [rad].
    double heading_rad = 0.0;
    /// Sideways speed of the rear axle, to the left [m/s].
    double lateral_velocity_m_s = 0.0;
    /// Positive turning left [rad/s].
    double yaw_rate_rad_s = 0.0;
};

/// What the drawbar puts on the tractor at the hitch [N].
struct Drawbar {
    /// Backward: the pull the tractor works against.
    double pull_n = 0.0;
    /// Upward; negative when the drawbar runs down from the hitch.
    double lift_n = 0.0;
    /// To the tractor's left, in the body frame.
    double side_n = 0.0;
};

/// The hitch point in the world: x along the track from where the rear axle
/// started, y to its left, z above the ground [m, m/s].
struct Hitch {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double velocity_x_m_s = 0.0;
    double velocity_y_m_s = 0.0;
    double velocity_z_m_s = 0.0;
};

/// Where the hitch is and how it moves, with the body's pitch. The rear axle
/// is at `state.position_m`.
[[nodiscard]] Hitch hitch(const Params& params, const State& state) noexcept;

/// The ground's vertical forces on the tractor [N].
struct AxleLoads {
    double rear_n = 0.0;
    double front_n = 0.0;
    double wheelie_bar_n = 0.0;
    /// The front tires are off the ground.
    bool front_lifted = false;
};

/// Loads at the start of a step, from the state: the contacts' spring and
/// damper forces, and the rear tires carrying the rest of the weight, the
/// drawbar's downward pull and the body's vertical inertia (from the last
/// step's pitch acceleration).
[[nodiscard]] AxleLoads axle_loads(const Params& params, const State& state,
                                   const Drawbar& drawbar) noexcept;

/// The loads with the tractor standing level, at rest, nothing on the
/// drawbar: what each axle carries of the weight.
[[nodiscard]] AxleLoads axle_loads_at_rest(const Params& params) noexcept;

/// Speed at the end of a step as a line in the drive force: free + k · F.
struct Prediction {
    double free_velocity_m_s = 0.0;
    double velocity_per_n = 0.0;
    /// The way the body travels: 1 forwards, -1 backwards (in reverse).
    double direction = 1.0;
};

/// `resisting_force_n` opposes forward motion (drawbar pull, rolling
/// resistance; negative against a body travelling backwards). It can hold
/// the body at rest but never push it against `direction`. A turning
/// body's sideways motion adds v_cg r.
[[nodiscard]] Prediction predict(const State& state, const Params& params, double resisting_force_n,
                                 double dt_s, double direction = 1.0) noexcept;

/// Forward speed at the end of the step under `drive_force_n`, as advance()
/// sets it: the resistance holds the body at rest but never pushes it
/// against its direction of travel.
[[nodiscard]] double end_velocity_m_s(const Prediction& prediction, double drive_force_n) noexcept;

/// Side forces on the body, to its left [N], and the yaw moment of forces
/// along it.
struct SideForces {
    /// Front tires, at the front axle.
    double front_n = 0.0;
    /// Rear tires, at the rear axle.
    double rear_n = 0.0;
    /// The drawbar, at the hitch.
    double hitch_n = 0.0;
    /// From forces along the body that differ left and right, turning left
    /// positive [N·m].
    double yaw_moment_nm = 0.0;
};

/// Sideways motion and yaw over the step, semi-implicit Euler: the forces
/// are this step's, the speeds advance first and the heading follows with
/// the new yaw rate. Comes before advance(), which moves the body along its
/// new heading.
void advance_lateral(State& state, const Params& params, const SideForces& forces,
                     double dt_s) noexcept;

/// Forward speed over the step, then position: the rear axle moves with its
/// new forward and sideways speeds along the heading.
void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept;

/// Pitch over the step, after advance() has set this step's acceleration.
/// The contacts are linearized and integrated with the implicit midpoint
/// rule: stable however stiff the wheelie bar, and free of the numerical
/// damping backward Euler would add to the front tires' bounce.
/// `front_rolling_resistance` is the front tires' motion resistance per unit
/// of load on today's ground (see tire::towed_rolling_resistance).
void advance_pitch(State& state, const Params& params, double axle_torque_nm,
                   const Drawbar& drawbar, double front_rolling_resistance, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.acceleration_m_s2);
    hash.add(state.pitch_rad);
    hash.add(state.pitch_rate_rad_s);
    hash.add(state.pitch_acceleration_rad_s2);
    hash.add(state.lateral_position_m);
    hash.add(state.heading_rad);
    hash.add(state.lateral_velocity_m_s);
    hash.add(state.yaw_rate_rad_s);
}

}  // namespace tp::chassis
