#pragma once

#include "tp/core/hash/fnv1a.hpp"

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
    double pan_friction_coefficient = 0.0;
    double wheel_rolling_resistance_coefficient = 0.0;
    Chain chain;
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
    kRollingResistanceOutOfRange,
    kChainInvalid,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

struct State {
    /// Rear wheels' contact along the track [m].
    double position_m = 0.0;
    double velocity_m_s = 0.0;
    /// Travelled since the start: drives the box and the push-down [m].
    double distance_m = 0.0;
};

/// What the chain puts on the sled at the hook [N].
struct HookForce {
    double forward_n = 0.0;
    /// Negative when the chain runs up from the hook to the tractor's hitch.
    double up_n = 0.0;
};

struct Loads {
    /// The track's vertical forces on the pan and the rear wheels [N].
    double pan_n = 0.0;
    double wheels_n = 0.0;
    double box_ahead_of_wheels_m = 0.0;
    /// 0 off to 1 fully engaged.
    double pushdown = 0.0;
    /// Pan friction plus wheel rolling resistance; it holds the sled at rest
    /// but never pushes it back [N].
    double resistance_n = 0.0;
};

[[nodiscard]] double total_mass_kg(const Params& params) noexcept;

/// Loads for the sled's travel so far and the chain's force on the hook.
[[nodiscard]] Loads loads(const Params& params, const State& state, const HookForce& hook) noexcept;

/// Speed at the end of a step as a line in the chain's forward pull.
struct Prediction {
    double free_velocity_m_s = 0.0;
    double velocity_per_n = 0.0;
};

[[nodiscard]] Prediction predict(const State& state, const Params& params, double resistance_n,
                                 double dt_s) noexcept;

void advance(State& state, const Prediction& prediction, double forward_n, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.distance_m);
}

}  // namespace tp::sled
