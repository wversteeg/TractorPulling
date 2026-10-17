#pragma once

#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/hash/fnv1a.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tp::driveline {

inline constexpr std::size_t kMaxGears = 16;
inline constexpr int kNeutral = 0;

/// Gearbox, final drive and rear axle as a rigid gear train.
///
/// In gear the train has one degree of freedom: the input shaft (clutch side)
/// turns at the axle speed times the overall ratio G = gear ratio × final
/// drive ratio, and seen from the clutch it has the inertia
/// input + output / G². In neutral the input shaft and the axle turn freely.
///
/// Gear meshes lose a fraction 1 - efficiency of the power flowing through
/// them, in whichever direction it flows: driving, the axle gets
/// efficiency × the clutch torque (times G); on overrun the clutch gets only
/// efficiency × the power the axle feeds back. Churning and bearing drag act
/// on the input shaft.
struct Params {
    /// Gearbox ratios, 1st first; strictly decreasing.
    std::array<double, kMaxGears> gear_ratios{};
    std::size_t gear_count = 0;
    /// Bevel gear and final reduction together.
    double final_drive_ratio = 0.0;
    /// Overall mesh efficiency, in (0, 1].
    double efficiency = 0.0;
    /// Clutch disc, input shaft and countershaft, at input speed [kg·m²].
    double input_inertia_kg_m2 = 0.0;
    /// Final drive and axle, at axle speed [kg·m²]. Wheels belong to the tires.
    double output_inertia_kg_m2 = 0.0;
    /// Constant drag on the input shaft while it turns [N·m].
    double drag_torque_nm = 0.0;
    /// Speed-proportional drag on the input shaft [N·m per rad/s].
    double viscous_drag_nm_s_rad = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kNoGears,
    kTooManyGears,
    kRatioNotPositive,
    kRatiosNotDecreasing,
    kFinalDriveNotPositive,
    kEfficiencyOutOfRange,
    kInertiaNotPositive,
    kDragNegative,
};

/// Returns the first physically meaningless value in `params`, if any.
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// Input shaft speed per axle speed in `gear` (1-based); 0 in neutral or for
/// a gear that does not exist.
[[nodiscard]] double overall_ratio(const Params& params, int gear) noexcept;

struct State {
    /// 0 is neutral, 1 the lowest gear.
    int gear = kNeutral;
    double input_omega_rad_s = 0.0;
    /// Axle speed [rad/s].
    double output_omega_rad_s = 0.0;
};

/// Changes gear. Refused (returns false) for a gear that does not exist, or
/// while the clutch still transmits torque: gears are chosen with the clutch
/// pressed. Engaging a gear works like a synchronizer, done in one step: the
/// input shaft and the axle side exchange momentum through the gears until
/// they turn at the same speed, so momentum is conserved and energy only
/// lost. The input shaft is light, so the axle side barely changes speed.
bool shift(State& state, const Params& params, int gear, bool clutch_disengaged) noexcept;

/// First half of a step, under `axle_load_torque_nm` (resistance at the axle,
/// positive opposing forward rotation). Pure.
struct Prediction {
    double ratio = 0.0;
    double efficiency = 1.0;
    // Input shaft speed at the end of the step: free + k * effective torque.
    double free_input_rad_s = 0.0;
    double input_per_nm = 0.0;
    // Axle speed at the end of the step in neutral (independent of the input).
    double free_output_rad_s = 0.0;
    double output_per_nm = 0.0;
};

[[nodiscard]] Prediction predict(const State& state, const Params& params,
                                 double axle_load_torque_nm, double dt_s) noexcept;

/// The input shaft's end-of-step speed as a function of clutch torque, for
/// the clutch solve. Includes the mesh efficiency in each direction.
[[nodiscard]] coupling::ShaftPrediction input_shaft(const Prediction& prediction) noexcept;

/// End-of-step speeds of both shafts as linear functions of the clutch
/// torque Tc (into the input shaft, through the meshes in gear) and of a
/// torque τ applied at the axle (by the tires; positive drives forward):
///
///   input' = free_input + input_per_clutch(±) · Tc + input_per_axle · τ
///   axle'  = free_axle  + axle_per_clutch(±)  · Tc + axle_per_axle  · τ
///
/// "negative" coefficients apply for Tc < 0 (power flowing back). In neutral
/// the cross terms are zero.
struct Response {
    double free_input_rad_s = 0.0;
    double input_per_clutch = 0.0;
    double input_per_negative_clutch = 0.0;
    double input_per_axle = 0.0;
    double free_axle_rad_s = 0.0;
    double axle_per_clutch = 0.0;
    double axle_per_negative_clutch = 0.0;
    double axle_per_axle = 0.0;
};

[[nodiscard]] Response response(const Prediction& prediction) noexcept;

/// Integrates under `clutch_torque_nm` (from the clutch into the input shaft)
/// and `axle_torque_nm` (applied at the axle, e.g. by the tires).
void advance(State& state, const Prediction& prediction, double clutch_torque_nm,
             double axle_torque_nm = 0.0) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(static_cast<std::uint64_t>(static_cast<std::int64_t>(state.gear)));
    hash.add(state.input_omega_rad_s);
    hash.add(state.output_omega_rad_s);
}

}  // namespace tp::driveline
