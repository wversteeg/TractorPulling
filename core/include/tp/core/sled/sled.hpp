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

/// A transfer sled: a frame on rear wheels with a pan (a steel plate) at the
/// front that drags on the track, and a weight box that moves forward as the
/// sled advances. Lengths along the sled are measured forward from the rear
/// wheels' contact.
///
/// The weights are shared between the wheels and the pan by their levers,
/// quasi-statically (the sled does not pitch). The chain's pull at the hook
/// height presses the pan down, its vertical part lifts or presses the
/// front. As the box moves forward, more weight goes onto the pan, which
/// drags far harder than the wheels roll. At a set distance the push-down
/// engages: it lifts the wheels off the track, over a short ramp, until the
/// pan carries everything.
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
struct Params {
    /// Sled without the weight box [kg].
    double frame_mass_kg = 0.0;
    double frame_cg_ahead_of_wheels_m = 0.0;
    /// Centre of the pan's contact with the track [m].
    double pan_ahead_of_wheels_m = 0.0;
    double hook_ahead_of_wheels_m = 0.0;
    double hook_height_m = 0.0;
    double box_mass_kg = 0.0;
    double box_start_ahead_of_wheels_m = 0.0;
    double box_end_ahead_of_wheels_m = 0.0;
    /// Sled travel over which the box moves from start to end [m].
    double box_travel_distance_m = 0.0;
    /// Sled travel at which the push-down starts to engage [m]; set per
    /// event from the test pull and the track's state.
    double pushdown_start_m = 0.0;
    /// Further travel until it is fully engaged [m].
    double pushdown_ramp_m = 0.0;
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
    int wheel_count = 2;
    Chain chain;
    /// One pull-off step, about 5 m of pull on this class's sled; a pull-off
    /// may take several.
    Adjustment pull_off_step;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kFrameMassNotPositive,
    kBoxMassNegative,
    kPanNotAheadOfWheels,
    kFrameCgOutsideSled,
    kBoxOutsideSled,
    kBoxTravelNotPositive,
    kHookInvalid,
    kPushdownStartNegative,
    kPushdownRampNotPositive,
    kPanFrictionOutOfRange,
    kPanAreaNotPositive,
    kPanPloughNegative,
    kKnivesOutOfRange,
    kBermOutOfRange,
    kWheel,
    kWheelCountOutOfRange,
    kChainInvalid,
    /// The pull-off step would not make the sled heavier.
    kPullOffStepNotHeavier,
};

inline constexpr int kMaxWheels = 8;

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// What the day's track changes about a class's sled: the sled operator
/// keeps the class's sled but may set the push-down for the track's state.
/// Unset keeps the sled's value.
struct Conditions {
    std::optional<double> pushdown_start_m;
};

/// The sled with the day's conditions applied.
[[nodiscard]] Params with_conditions(const Params& params, const Conditions& conditions) noexcept;

struct State {
    /// Rear wheels' contact along the track [m].
    double position_m = 0.0;
    double velocity_m_s = 0.0;
    /// Travelled since the start: drives the box and the push-down [m].
    double distance_m = 0.0;
    /// Soil piled up in front of the pan [kg].
    double berm_kg = 0.0;
};

/// What the chain puts on the sled at the hook [N].
struct HookForce {
    double forward_n = 0.0;
    /// Negative when the chain runs up from the hook to the tractor's hitch.
    double up_n = 0.0;
};

/// The ground under the pan and under the rear wheels, some metres apart.
struct Ground {
    soil::Surface pan;
    soil::Surface wheels;
};

struct Loads {
    /// The track's vertical forces on the pan and the rear wheels [N].
    double pan_n = 0.0;
    double wheels_n = 0.0;
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

/// The sled after `adjustment`, applied `times` over. Box mass, knife count
/// and depth and the push-down stay within what validate() accepts.
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

[[nodiscard]] Prediction predict(const State& state, const Params& params, double resistance_n,
                                 double dt_s) noexcept;

void advance(State& state, const Prediction& prediction, double forward_n, double dt_s) noexcept;

/// The berm over a step, after advance(): soil the tractor threw back
/// [m³/s] lands in front of the pan, and the berm spills by the distance
/// the sled moved. Implicit in the spill, so it never goes negative.
void collect(State& state, const Params& params, double thrown_soil_m3_s, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.distance_m);
    hash.add(state.berm_kg);
}

}  // namespace tp::sled
