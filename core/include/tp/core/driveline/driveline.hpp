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
///
/// Gears change with the clutch pressed, except that some gearboxes shift a
/// gear or two down under load, the clutch in (a powershift step, worked by
/// clutch packs inside the gearbox): load_downshift_gears says how many,
/// and load_shifts_per_pull how often in one pull.
/// The packs' slip is taken as the main clutch's (see shift_under_load).
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
    /// How many gears down the gearbox shifts under load, the clutch in: 0
    /// for none, below gear_count.
    int load_downshift_gears = 0;
    /// How many shifts under load one pull allows: after them the gear goes
    /// up with the clutch pressed, and under way never down. 0 or more;
    /// without a step (load_downshift_gears 0) it does nothing.
    int load_shifts_per_pull = 1;
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
    kLoadDownshiftOutOfRange,
    kLoadShiftsNegative,
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
    /// Shifts under load so far in this pull.
    int load_shifts = 0;
    /// Once the pull is under way (start_pull), the lowest gear the gearbox
    /// goes into with the clutch: the gear it pulls in, rising as it shifts
    /// up, so it never goes back down but by a shift under load. Neutral (0)
    /// before: standing, any gear may be chosen.
    int floor_gear = kNeutral;
};

/// The pull is under way: from now on the gearbox goes down only by shifts
/// under load (see floor_gear). Keeps the floor once set.
void start_pull(State& state) noexcept;

/// The lowest gear the gearbox goes into now, by a shift with the clutch or
/// one under load: 1 before the pull is under way; then the floor, or
/// lower by what a shift under load still allows. Neutral is always there.
[[nodiscard]] int lowest_gear(const State& state, const Params& params) noexcept;

/// Whether the gearbox shifts from its gear now to gear `to` with the
/// clutch in: down, to a gear (not neutral), at most load_downshift_gears,
/// while this pull has load shifts left (load_shifts_per_pull).
[[nodiscard]] bool shifts_under_load(const State& state, const Params& params, int to) noexcept;

/// Shifts down under load, the clutch in (see shifts_under_load; nothing if
/// it does not). The packs sit between the input shaft and the gears, so the
/// axle and the wheels keep their speed: only the pack's side jumps to the
/// new gear's speed, which the input shaft stands in for here. Its change of
/// momentum comes from the engine, through the clutch that stands in for
/// the packs: returns that angular impulse [N·m·s], for the engine to give
/// up. After it the input shaft turns faster than the engine, and the clutch
/// slips, at most its capacity, until the engine has caught up.
[[nodiscard]] std::optional<double> shift_under_load(State& state, const Params& params,
                                                     int gear) noexcept;

/// Changes gear. Refused (returns false) for a gear that does not exist,
/// while the clutch still transmits torque (gears are chosen with the
/// clutch pressed; a shift under load is shift_under_load's), or below the
/// floor once the pull is under way (see floor_gear). Engaging a gear works
/// like a synchronizer, done in one step: the input shaft and the axle side
/// exchange momentum through the gears until they turn at the same speed,
/// so momentum is conserved and energy only lost. The input shaft is light,
/// so the axle side barely changes speed.
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
    hash.add(static_cast<std::uint64_t>(static_cast<std::int64_t>(state.load_shifts)));
    hash.add(static_cast<std::uint64_t>(static_cast<std::int64_t>(state.floor_gear)));
}

}  // namespace tp::driveline
