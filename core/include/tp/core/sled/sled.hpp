#pragma once

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/soil/soil.hpp"
#include "tp/core/tire/tire.hpp"

#include <cstdint>
#include <optional>

namespace tp::sled {

/// The chain (or drawbar) between the tractor's hitch and the sled's hook:
/// a one-sided spring-damper that pulls only when taut.
struct Chain {
    double length_m = 0.0;
    /// Chain, shackles and hitch together [N/m].
    double stiffness_n_per_m = 0.0;
    double damping_n_s_per_m = 0.0;
};

/// Blades under the pan that cut grooves in the track. Each cuts with a
/// force that grows with the soil's strength and the groove's cross-section
/// (width × depth), so knives bite hardest on a firm track. They cut while
/// the pan carries load.
struct Knives {
    int count = 0;
    double depth_m = 0.0;
    /// One knife's cutting width [m].
    double width_m = 0.0;
};

/// Rubber fenders either side of the hook, which a tractor backing up to
/// the sled stops against with its rear tires (turn::fender_stop_m): their
/// faces this far behind the hook, stopping a hitch this far either side of
/// the sled's line and up to this deep past the faces [m]. No span, no
/// fenders.
struct Fenders {
    double behind_hook_m = 0.0;
    double half_span_m = 0.0;
    double depth_m = 0.0;
};

/// Soil the tractor's rear tires throw back piles up in front of the pan,
/// which has to push it along: a berm. It grows with what the tires throw
/// (more when they spin, even with the sled standing still) and spills off
/// to the sides as the sled moves, so over a pull it builds up and then
/// levels off.
struct Berm {
    /// Share of the thrown soil that lands in front of the pan.
    double collected_share = 0.0;
    /// Sled travel over which the berm spills off its own size [m].
    double spill_distance_m = 0.0;
    /// Pushing it costs this times its weight (soil sliding on soil).
    double friction_coefficient = 0.0;
};

/// The weight box's gearbox: the sled operator chooses the sled travel by
/// which the box reaches its end, from first_m in steps of step_m, one of
/// `gears` settings (a Dutch sled's: 75, 80, 85 or 90 m). No gears: a sled
/// without a gearbox, whose box may take any travel.
struct BoxGearbox {
    double first_m = 0.0;
    double step_m = 0.0;
    int gears = 0;
};

inline constexpr int kMaxBoxGears = 16;

inline constexpr int kMaxKnives = 24;
/// Knives deeper than this would dig the pan in: a data error [m].
inline constexpr double kMaxKnifeDepth_m = 0.15;

/// A change to the sled's setting, as the sled operator makes it between
/// pulls: each field is added (negative makes the sled lighter, or the
/// push-down later for pushdown_earlier_m). After a test pull operators
/// prefer knives and push-down; the box takes longer to change.
struct Adjustment {
    double box_mass_kg = 0.0;
    int knives = 0;
    double knife_depth_m = 0.0;
    /// Moves the push-down this much earlier [m].
    double pushdown_earlier_m = 0.0;
};

/// Simplified: every sled's pan is taken as this wide either side of its
/// middle where it meets the track's hard and weak spots (the sleds' data
/// give its area, not its width) [m].
inline constexpr double kPanHalfWidth_m = 1.0;

/// A transfer sled: a frame on rear wheels with a pan (a steel plate) at the
/// front that drags on the track, and a weight box that moves forward as the
/// sled advances. Lengths along the sled are measured forward from the rear
/// wheels' contact.
///
/// The sled is two bodies, quasi-statically (neither pitches): the pan,
/// with the front and the hook on it, and the frame with the weight box,
/// which stands on the rear wheels and on a pivot in the pan's middle.
/// The chain pulls the pan; the pan tows the frame through the pivot. The
/// frame's weights and the box are shared between the wheels and the
/// pivot by their levers, and the frame's own drag (its wheels' rolling),
/// pulled at the pivot's height, tips it a little onto the pan. The
/// chain's vertical part lifts or presses the pan itself, one for one,
/// leaving the wheels' load as it was; its pull along the track acts on
/// the pan alone.
///
/// The push-down's cylinders stand between the frame and the pan's back:
/// pressing, they lift the frame off its wheels onto the pan, and hold the
/// pan level against the frame.
///
/// The pan's front lifts where the chain's upward pull at the hook, ahead
/// of the pan's middle, outweighs what holds it down: the ground's
/// pressure under the pan, taken as linear along it, moves back until
/// only its rear part carries it (a rigid plate on soil). A pan whose
/// front is up does not push soil ahead of its lip: its ploughing drag
/// falls with the share of it off the ground. As the box comes forward
/// over the pivot the pan's load grows and its front comes down; with the
/// push-down engaged it stays down. As the box moves forward, more weight goes onto the pan, which
/// drags far harder than the wheels roll. At a set distance the push-down
/// engages: over a short ramp it presses the pan down and takes the weight
/// off the wheels, until the pan carries its set share of the sled (all of
/// it, or less: a sled set so its pan carries half of its weight at most
/// leaves the strongest tractors room to go on, so a field spreads out
/// instead of all stopping at the push-down as at a wall).
///
/// On soil the pan sinks in until the ground carries its pressure, and
/// pushes soil ahead of it: the softer the track for the pan's pressure p,
/// the harder it drags. Its friction coefficient becomes
///
///   μ = μ_pan (1 + plough · p / CI)
///
/// with CI the soil's cone index. Knives under the pan cut grooves on top
/// of that. The rear wheels roll as towed tires (Brixius), sinking further
/// into softer ground.
///
/// Sideways the sled is a trailer on the chain: it moves and turns (yaws)
/// in the ground plane under the chain's pull at the hook, its rear wheels'
/// side grip (tire::Cornering) and the pan's friction. Sliding friction
/// acts against the pan's sliding direction, so while the sled slides
/// forward, a sideways speed v turns only a share v / √(u² + v²) of the
/// pan's drag against it: a sliding sled swings sideways easily (the
/// dog-tail), one at rest holds. The weight box moves the centre of gravity
/// and the yaw inertia as it travels.
struct Params {
    /// Sled without the weight box [kg].
    double frame_mass_kg = 0.0;
    double frame_cg_ahead_of_wheels_m = 0.0;
    /// The frame's yaw inertia about its own centre of gravity; the box
    /// adds its own as a point mass [kg·m²].
    double frame_yaw_inertia_kg_m2 = 0.0;
    /// Centre of the pan's contact with the track, and the pivot over it
    /// on which the frame stands [m].
    double pan_ahead_of_wheels_m = 0.0;
    /// The pan with the front on it: part of frame_mass_kg, at its middle
    /// [kg].
    double pan_mass_kg = 0.0;
    /// The pan's length along the track: over it the ground carries it
    /// [m].
    double pan_length_m = 0.0;
    /// The pivot's height over the track, where the pan tows the frame [m].
    double pivot_height_m = 0.0;
    /// The push-down's cylinders, between the frame and the pan's back:
    /// on the pan, behind the pivot [m].
    double cylinders_ahead_of_wheels_m = 0.0;
    double hook_ahead_of_wheels_m = 0.0;
    double hook_height_m = 0.0;
    double box_mass_kg = 0.0;
    /// What the weight box holds at most: the sled operator sets the
    /// class's weight within it, and may change it between qualifying and
    /// the pull-off [kg].
    double box_max_mass_kg = 0.0;
    double box_start_ahead_of_wheels_m = 0.0;
    double box_end_ahead_of_wheels_m = 0.0;
    /// Sled travel over which the box moves from start to end: one of the
    /// gearbox's settings, where it has one [m].
    double box_travel_distance_m = 0.0;
    BoxGearbox box_gearbox;
    /// Sled travel at which the push-down starts to engage [m]; set per
    /// event from the test pull and the track's state.
    double pushdown_start_m = 0.0;
    /// Further travel until it is fully engaged [m].
    double pushdown_ramp_m = 0.0;
    /// The most of the sled's weight the push-down puts on the pan, fully
    /// engaged: 1 lifts the wheels off; less leaves them that share. The
    /// box over the pan may put more there by itself.
    double pushdown_most_pan_share = 1.0;
    /// Sliding friction of the steel pan on a firm track.
    double pan_friction_coefficient = 0.0;
    /// The pan's contact with the track [m²].
    double pan_area_m2 = 0.0;
    /// How much harder the pan drags per unit of its pressure over the
    /// soil's strength.
    double pan_plough_coefficient = 0.0;
    Knives knives;
    Berm berm;
    tire::Size wheel;
    /// Side grip of one rear wheel.
    tire::Cornering wheel_cornering;
    int wheel_count = 2;
    /// One rear wheel's own rolling losses per unit of load, on top of its
    /// sinking in (1/Bn): hard-pumped sled tires lose less than a
    /// tractor's soft ones (tire::towed_rolling_resistance).
    double wheel_rolling_losses = 0.0;
    Chain chain;
    /// One pull-off step, about 5 m of pull on this class's sled; a pull-off
    /// may take several.
    Adjustment pull_off_step;
    Fenders fenders;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kFrameMassNotPositive,
    kYawInertiaNotPositive,
    kBoxMassNegative,
    /// The box holds more than its maximum.
    kBoxOverMax,
    kPanNotAheadOfWheels,
    /// The pan is not part of the frame's mass (0 to frame_mass_kg), its
    /// length is not positive or reaches back past the wheels, or the
    /// pivot is below the track.
    kPanInvalid,
    /// The push-down's cylinders are not on the pan behind the pivot.
    kCylindersInvalid,
    kFrameCgOutsideSled,
    kBoxOutsideSled,
    /// The box starts so far behind the wheels that the sled's centre of
    /// gravity is behind them: it would sit on its tail.
    kSledTipsBack,
    kBoxTravelNotPositive,
    /// The gearbox's first setting or step is not positive, or it has
    /// more than kMaxBoxGears gears.
    kBoxGearboxInvalid,
    /// The box's travel is not one of the gearbox's settings.
    kBoxTravelNotAGear,
    kHookInvalid,
    kPushdownStartNegative,
    kPushdownRampNotPositive,
    /// The push-down's share of the sled on the pan is not in (0, 1].
    kPushdownShareOutOfRange,
    kPanFrictionOutOfRange,
    kPanAreaNotPositive,
    kPanPloughNegative,
    kKnivesOutOfRange,
    kBermOutOfRange,
    kWheel,
    kWheelCornering,
    kWheelCountOutOfRange,
    kWheelLossesOutOfRange,
    kChainInvalid,
    /// The pull-off step would not make the sled heavier.
    kPullOffStepNotHeavier,
    kFendersInvalid,
};

inline constexpr int kMaxWheels = 8;
/// A tire losing more than this share of its load rolling, on hard ground,
/// is a data error.
inline constexpr double kMaxWheelRollingLosses = 0.2;

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// What the day's track changes about a class's sled: the sled operator
/// keeps the class's sled but may set the push-down, and the weight box's
/// gear (the travel by which it reaches its end), for the track's state.
/// Unset keeps the sled's value.
struct Conditions {
    std::optional<double> pushdown_start_m;
    std::optional<double> box_travel_distance_m;
};

/// The sled with the day's conditions applied.
[[nodiscard]] Params with_conditions(const Params& params, const Conditions& conditions) noexcept;

/// Positions are in the world: x along the track, y to the left of its
/// centre line. Speeds are in the sled's own frame.
struct State {
    /// Rear wheels' contact along the track [m].
    double position_m = 0.0;
    /// Forward speed [m/s].
    double velocity_m_s = 0.0;
    /// Travelled along the sled's heading since the start: drives the box
    /// and the push-down. Not track x: a sled that weaves travels further
    /// than it pulls down the track [m].
    double distance_m = 0.0;
    /// Soil piled up in front of the pan [kg].
    double berm_kg = 0.0;
    /// Rear wheels' contact to the left of the centre line [m].
    double lateral_position_m = 0.0;
    /// The sled's x from the track's direction, positive turned left [rad].
    double heading_rad = 0.0;
    /// Sideways speed of the rear wheels' contact, to the left [m/s].
    double lateral_velocity_m_s = 0.0;
    /// Positive turning left [rad/s].
    double yaw_rate_rad_s = 0.0;
    /// Sideways deflection of each rear wheel's contact patch [m].
    double wheel_side_deflection_m = 0.0;
    /// The share of the pan's length off the ground at its front: it
    /// follows Loads::pan_front_lift_target over kPanTiltTime_s (tilt()).
    double pan_front_lift = 0.0;
};

/// How quickly the pan's front follows what the forces on it ask: a
/// 2.5 t plate on soil rises and settles over about this time, not at
/// every jolt of the chain [s].
inline constexpr double kPanTiltTime_s = 0.3;

/// What the chain puts on the sled at the hook, in the sled's frame [N].
struct HookForce {
    double forward_n = 0.0;
    /// Negative when the chain runs up from the hook to the tractor's hitch.
    double up_n = 0.0;
    /// To the sled's left.
    double side_n = 0.0;
};

/// The hook in the world and how it moves [m, m/s].
struct Hook {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double velocity_x_m_s = 0.0;
    double velocity_y_m_s = 0.0;
};

[[nodiscard]] Hook hook(const Params& params, const State& state) noexcept;

/// The ground under the pan and under the rear wheels, some metres apart.
struct Ground {
    soil::Surface pan;
    soil::Surface wheels;
};

struct Loads {
    /// The track's vertical forces on the pan and the rear wheels [N].
    double pan_n = 0.0;
    double wheels_n = 0.0;
    /// The frame's weight on the pivot (negative: the pivot holds the frame
    /// down) and the push-down's cylinders' force between frame and pan
    /// [N].
    double pivot_n = 0.0;
    double cylinders_n = 0.0;
    /// The share of the pan's length off the ground at its front, the
    /// chain lifting it: 0 flat on the track to 1 standing on its back
    /// edge. As it is (State::pan_front_lift, which the drag uses), and
    /// where the forces on the pan would put it now.
    double pan_front_lift = 0.0;
    double pan_front_lift_target = 0.0;
    double box_ahead_of_wheels_m = 0.0;
    /// 0 off to 1 fully engaged.
    double pushdown = 0.0;
    /// The pan's friction coefficient with ploughing.
    double pan_friction = 0.0;
    /// All knives' cutting force [N].
    double knives_n = 0.0;
    /// Pushing the berm [N].
    double berm_n = 0.0;
    /// Pan friction, knives, the berm and wheel rolling resistance; it holds
    /// the sled at rest but never pushes it back [N].
    double resistance_n = 0.0;
};

[[nodiscard]] double total_mass_kg(const Params& params) noexcept;

/// The sled after `adjustment`, applied `times` over. Box mass (up to its
/// maximum), knife count
/// and depth and the push-down stay within what validate() accepts; a box
/// made heavier behind the wheels starts no further back than the sled
/// balances on them.
[[nodiscard]] Params adjusted(const Params& params, const Adjustment& adjustment,
                              int times = 1) noexcept;

/// The pull-off sled, `steps` pull-off steps heavier.
[[nodiscard]] Params heavier(const Params& params, int steps = 1) noexcept;

/// Loads for the sled's travel so far and the chain's force on the hook, on
/// `ground`.
[[nodiscard]] Loads loads(const Params& params, const State& state, const HookForce& hook,
                          const Ground& ground) noexcept;

/// Speed at the end of a step as a line in the chain's forward pull.
struct Prediction {
    double free_velocity_m_s = 0.0;
    double velocity_per_n = 0.0;
};

/// A turning sled's sideways motion feeds its forward speed (v_cg r).
[[nodiscard]] Prediction predict(const State& state, const Params& params, double resistance_n,
                                 double dt_s) noexcept;

/// Sideways motion and yaw over the step, before advance(): the hook's
/// side pull, the rear wheels' side grip on `wheel_ground` and the pan's
/// sliding friction (implicit, so it holds a sled at rest without
/// stiffness trouble), with the loads of this step.
void advance_lateral(State& state, const Params& params, const Loads& loads, double hook_side_n,
                     const soil::Surface& wheel_ground, double dt_s) noexcept;

/// Forward speed over the step, then position along the heading.
void advance(State& state, const Prediction& prediction, double forward_n, double dt_s) noexcept;

/// The pan's front over a step: it follows the lift the loads ask for,
/// over kPanTiltTime_s (implicit, so it never overshoots).
void tilt(State& state, const Loads& loads, double dt_s) noexcept;

/// The berm over a step, after advance(): soil the tractor threw back
/// [m³/s] lands in front of the pan, and the berm spills by the distance
/// the sled moved. Implicit in the spill, so it never goes negative.
void collect(State& state, const Params& params, double thrown_soil_m3_s, double dt_s) noexcept;

/// How fast the sled drives itself back to its start after a pull, on its
/// own engine [m/s].
inline constexpr double kDriveBack_m_s = 2.0;

/// How fast the operator runs the weight box back along the rails after a
/// pull, the sled standing, before it drives back [m/s].
inline constexpr double kBoxBack_m_s = 0.75;

/// The sled after a pull, the chain off: the operator runs the weight box
/// back to its start at kBoxBack_m_s, the sled standing, so it drives back
/// light (the push-down eases off with the box, and at once past where the
/// box stops); then it drives
/// itself back, the pan lifted (the berm is dropped: the track keeps no
/// soil), straight towards its start at `start_x_m`, `start_y_m` across
/// the track, at kDriveBack_m_s. Returns true once it stands there,
/// straight, ready for the next pull. Its velocity is negative while it
/// drives.
bool drive_back(State& state, const Params& params, double start_x_m, double start_y_m,
                double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.distance_m);
    hash.add(state.berm_kg);
    hash.add(state.lateral_position_m);
    hash.add(state.heading_rad);
    hash.add(state.lateral_velocity_m_s);
    hash.add(state.yaw_rate_rad_s);
    hash.add(state.wheel_side_deflection_m);
    hash.add(state.pan_front_lift);
}

}  // namespace tp::sled
