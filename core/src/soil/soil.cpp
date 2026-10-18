#include "tp/core/soil/soil.hpp"

#include "tp/core/math/exp.hpp"

#include <algorithm>
#include <cmath>

namespace tp::soil {

namespace {

// Semi-empirical constants, tuned to typical cone index readings: rolled
// clay a few MPa, loose sand a few hundred kPa. Strength curves follow the
// shapes of Ayers & Perumpral (1982) for sand and the exponential loss of
// strength with water content of cohesive soils.

// Sand: strongest at an optimum moisture, weaker drier (it runs) and wetter.
constexpr double kSandLoose_pa = 300.0e3;
constexpr double kSandRolled_pa = 1200.0e3;
constexpr double kSandOptimumMoisture = 0.08;
constexpr double kSandMoistureWidth = 0.05;

// Clay: strength falls exponentially with water content.
constexpr double kClayLoose_pa = 800.0e3;
constexpr double kClayRolled_pa = 2500.0e3;
constexpr double kClayReferenceMoisture = 0.15;
constexpr double kClayMoistureScale = 0.06;
constexpr double kMaxConeIndex_pa = 6.0e6;

// Surface grip: a dust layer forms on clay drier than kDustMoisture; above
// the plastic limit a slick film forms.
constexpr double kDustMoisture = 0.08;
constexpr double kDustMoistureRange = 0.06;
constexpr double kMaxDustGripLoss = 0.15;
constexpr double kPlasticLimit = 0.22;
constexpr double kSlickMoistureRange = 0.10;
constexpr double kMaxSlickGripLoss = 0.4;

// Grip lost in full wheelspin: a base, more as tires dig into sand, more as
// they smear wet clay.
constexpr double kBaseFallOff = 0.15;
constexpr double kSandDigFallOff = 0.15;
constexpr double kSmearStartMoisture = 0.18;
constexpr double kSmearMoistureRange = 0.10;
constexpr double kMaxSmearFallOff = 0.3;

double lerp(double from, double to, double t) noexcept {
    return from + t * (to - from);
}

double ramp(double value, double start, double range) noexcept {
    return std::clamp((value - start) / range, 0.0, 1.0);
}

double sand_strength_pa(double moisture, double compaction) noexcept {
    const double off_optimum = (moisture - kSandOptimumMoisture) / kSandMoistureWidth;
    return lerp(kSandLoose_pa, kSandRolled_pa, compaction) / (1.0 + off_optimum * off_optimum);
}

double clay_strength_pa(double moisture, double compaction) noexcept {
    return lerp(kClayLoose_pa, kClayRolled_pa, compaction) *
           math::exp(-(moisture - kClayReferenceMoisture) / kClayMoistureScale);
}

}  // namespace

std::optional<ParamError> validate(const Composition& composition) noexcept {
    if (!std::isfinite(composition.clay_fraction) || !std::isfinite(composition.moisture) ||
        !std::isfinite(composition.compaction)) {
        return ParamError::kNotFinite;
    }
    if (composition.clay_fraction < 0.0 || composition.clay_fraction > 1.0) {
        return ParamError::kClayFractionOutOfRange;
    }
    if (composition.moisture < 0.0 || composition.moisture > kMaxMoisture) {
        return ParamError::kMoistureOutOfRange;
    }
    if (composition.compaction < 0.0 || composition.compaction > 1.0) {
        return ParamError::kCompactionOutOfRange;
    }
    return std::nullopt;
}

Surface surface(const Composition& composition) noexcept {
    const double clay = composition.clay_fraction;
    const double moisture = composition.moisture;
    const double strength_pa = lerp(sand_strength_pa(moisture, composition.compaction),
                                    clay_strength_pa(moisture, composition.compaction), clay);

    // ramp(-w, -dust, range) is 1 for bone-dry clay, 0 from kDustMoisture up.
    const double dust = ramp(-moisture, -kDustMoisture, kDustMoistureRange);
    const double slick = ramp(moisture, kPlasticLimit, kSlickMoistureRange);
    const double grip =
        (1.0 - clay * kMaxDustGripLoss * dust) * (1.0 - clay * kMaxSlickGripLoss * slick);

    const double smear = ramp(moisture, kSmearStartMoisture, kSmearMoistureRange);
    const double fall_off =
        kBaseFallOff + kSandDigFallOff * (1.0 - clay) + kMaxSmearFallOff * clay * smear;

    return {.cone_index_pa = std::min(strength_pa, kMaxConeIndex_pa),
            .grip_factor = grip,
            .spin_fall_off = fall_off};
}

}  // namespace tp::soil
