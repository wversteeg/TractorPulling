#pragma once

#include "tp/core/hash/fnv1a.hpp"

#include <cstdint>
#include <optional>

namespace tp::chassis {

/// The tractor body moving along the track, and how its weight is shared
/// between the axles.
///
/// For now the body only translates; pitch (and wheelies) arrive in
/// Milestone 1, step 5. Until then weight transfer is quasi-static: taking
/// moments about the front contact patch,
///
///   rear load = m g (L - b) / L + (m a h_cg + F_drawbar h_hitch) / L
///
/// with L the wheelbase and b the centre of gravity's distance ahead of the
/// rear axle. The drawbar pull is horizontal. When the front load would go
/// negative the front axle has lifted: it carries nothing and the rear
/// carries the whole weight.
struct Params {
    /// Whole tractor, wheels included [kg].
    double mass_kg = 0.0;
    double wheelbase_m = 0.0;
    /// Centre of gravity ahead of the rear axle, between the axles [m].
    double cg_ahead_of_rear_axle_m = 0.0;
    /// Centre of gravity above the ground [m].
    double cg_height_m = 0.0;
    /// Drawbar hitch point above the ground [m].
    double hitch_height_m = 0.0;
    /// Rolling resistance of the (unpowered) front tires per unit of load.
    double front_rolling_resistance_coefficient = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kMassNotPositive,
    kWheelbaseNotPositive,
    kCgOutsideWheelbase,
    kCgHeightNotPositive,
    kHitchHeightNegative,
    kRollingResistanceOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

struct State {
    double position_m = 0.0;
    double velocity_m_s = 0.0;
    /// Over the last step; feeds weight transfer in the next one.
    double acceleration_m_s2 = 0.0;
};

struct AxleLoads {
    double rear_n = 0.0;
    double front_n = 0.0;
    bool front_lifted = false;
};

[[nodiscard]] AxleLoads axle_loads(const Params& params, double acceleration_m_s2,
                                   double drawbar_force_n) noexcept;

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

void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(state.position_m);
    hash.add(state.velocity_m_s);
    hash.add(state.acceleration_m_s2);
}

}  // namespace tp::chassis
