#pragma once

#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/math/curve.hpp"

#include <cstdint>
#include <optional>

namespace tp::clutch {

/// Dry friction clutch between the engine flywheel and the driveline input
/// shaft. It has no inertia of its own; the plates belong to the shafts.
///
/// The pedal sets the torque capacity through the engagement curve. Each step
/// the clutch solves for the torque that would bring both shafts to the same
/// speed at the end of the step. If that torque is within capacity the clutch
/// is locked and transmits exactly it; otherwise it slips and transmits its
/// kinetic capacity against the direction of slip. A locked clutch breaks away
/// at its static capacity; a slipping one locks again once its (lower) kinetic
/// capacity suffices, which gives the familiar grab-and-release behaviour.
struct Params {
    /// Fraction of static capacity, 0 to 1, against pedal position 0
    /// (released) to 1 (fully pressed). Never increases with pedal travel.
    math::Curve engagement;
    /// Torque the fully engaged clutch holds without slipping [N·m].
    double static_torque_capacity_nm = 0.0;
    /// Sliding friction as a fraction of static friction, in (0, 1].
    double kinetic_ratio = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kCapacityNotPositive,
    kKineticRatioOutOfRange,
    kEngagementDoesNotSpanPedalTravel,
    kEngagementOutOfRange,
    kEngagementIncreasesWithPedal,
    kEngagementNotFullWhenReleased,
    kEngagementNotZeroWhenPressed,
};

/// Returns the first physically meaningless value in `params`, if any.
/// `step` assumes valid parameters.
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

struct State {
    /// Both shafts turn together; static friction applies.
    bool locked = false;
};

struct Input {
    /// Pedal position, clamped to [0, 1]: 0 released (engaged), 1 pressed.
    double pedal = 0.0;
};

struct Output {
    /// Torque from the engine side to the driveline side [N·m]. The engine
    /// receives -torque_nm, the driveline +torque_nm.
    double torque_nm = 0.0;
    /// Engine speed minus driveline speed at the end of the step [rad/s].
    double slip_rad_s = 0.0;
    /// Power turned into heat by slipping, >= 0 [W].
    double slip_power_w = 0.0;
    /// Static torque capacity at the current pedal position [N·m].
    double capacity_nm = 0.0;
};

/// Solves the clutch torque for this step from both shafts' predictions.
Output step(State& state, const Params& params, const Input& input,
            const coupling::ShaftPrediction& engine_side,
            const coupling::ShaftPrediction& driveline_side) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.locked);
}

}  // namespace tp::clutch
