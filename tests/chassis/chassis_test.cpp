#include "tp/core/chassis/chassis.hpp"

#include "tp/core/units/constants.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "reference_chassis.hpp"

namespace tp::chassis {
namespace {

using testing::reference_params;
using units::kStandardGravity_m_s2;

constexpr double kDt_s = 0.001;

double weight_n(const Params& params) {
    return params.mass_kg * kStandardGravity_m_s2;
}

TEST(ChassisParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(ChassisParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.mass_kg = 0.0; }), ParamError::kMassNotPositive);
    EXPECT_EQ(with([](Params& p) { p.mass_kg = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.wheelbase_m = -1.0; }), ParamError::kWheelbaseNotPositive);
    EXPECT_EQ(with([](Params& p) { p.cg_ahead_of_rear_axle_m = p.wheelbase_m; }),
              ParamError::kCgOutsideWheelbase);
    EXPECT_EQ(with([](Params& p) { p.cg_ahead_of_rear_axle_m = 0.0; }),
              ParamError::kCgOutsideWheelbase);
    EXPECT_EQ(with([](Params& p) { p.cg_height_m = 0.0; }), ParamError::kCgHeightNotPositive);
    EXPECT_EQ(with([](Params& p) { p.hitch_height_m = -0.1; }), ParamError::kHitchHeightNegative);
    EXPECT_EQ(with([](Params& p) { p.front_rolling_resistance_coefficient = 1.0; }),
              ParamError::kRollingResistanceOutOfRange);
}

TEST(Chassis, StaticLoadsFollowTheCentreOfGravity) {
    const Params params = reference_params();
    const AxleLoads loads = axle_loads(params, 0.0, 0.0);
    EXPECT_NEAR(loads.rear_n, 0.6 * weight_n(params), 1e-9);
    EXPECT_NEAR(loads.front_n, 0.4 * weight_n(params), 1e-9);
    EXPECT_FALSE(loads.front_lifted);
}

// Whatever the pull or acceleration, the axles carry exactly the weight.
TEST(Chassis, AxleLoadsAlwaysAddUpToTheWeight) {
    const Params params = reference_params();
    for (int a = -5; a <= 5; ++a) {
        for (int pull_kn = 0; pull_kn <= 80; pull_kn += 10) {
            const AxleLoads loads = axle_loads(params, a, pull_kn * 1000.0);
            EXPECT_NEAR(loads.rear_n + loads.front_n, weight_n(params), 1e-9);
            EXPECT_GE(loads.rear_n, 0.0);
            EXPECT_GE(loads.front_n, 0.0);
        }
    }
}

// Pulling and accelerating both move load onto the rear axle, by the
// moments about the front contact patch.
TEST(Chassis, DrawbarPullAndAccelerationLoadTheRear) {
    const Params params = reference_params();
    const double static_n = axle_loads(params, 0.0, 0.0).rear_n;
    EXPECT_NEAR(axle_loads(params, 0.0, 10000.0).rear_n - static_n,
                10000.0 * params.hitch_height_m / params.wheelbase_m, 1e-9);
    EXPECT_NEAR(axle_loads(params, 1.0, 0.0).rear_n - static_n,
                params.mass_kg * 1.0 * params.cg_height_m / params.wheelbase_m, 1e-9);
    EXPECT_LT(axle_loads(params, -1.0, 0.0).rear_n, static_n);
}

// Enough pull lifts the front: it then carries nothing.
TEST(Chassis, HeavyPullLiftsTheFront) {
    const Params params = reference_params();
    // Front load reaches zero when F h_hitch / L = 0.4 m g.
    const double lift_n = 0.4 * weight_n(params) * params.wheelbase_m / params.hitch_height_m;
    EXPECT_FALSE(axle_loads(params, 0.0, 0.99 * lift_n).front_lifted);
    const AxleLoads lifted = axle_loads(params, 0.0, 1.01 * lift_n);
    EXPECT_TRUE(lifted.front_lifted);
    EXPECT_EQ(lifted.front_n, 0.0);
    EXPECT_NEAR(lifted.rear_n, weight_n(params), 1e-9);
}

TEST(Chassis, ConstantForceAcceleratesTheMass) {
    const Params params = reference_params();
    State state;
    constexpr double kForce_n = 7000.0;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 0.0, kDt_s);
        advance(state, p, kForce_n, kDt_s);
    }
    const double a_m_s2 = kForce_n / params.mass_kg;
    EXPECT_NEAR(state.velocity_m_s, a_m_s2 * 1.0, 1e-9);
    EXPECT_NEAR(state.acceleration_m_s2, a_m_s2, 1e-9);
    // Semi-implicit Euler is half a step ahead of the exact a t² / 2.
    EXPECT_NEAR(state.position_m, 0.5 * a_m_s2 * (1.0 + kDt_s), 1e-9);
}

TEST(Chassis, ResistanceHoldsAtRestWithoutPushingBack) {
    const Params params = reference_params();
    State state;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 20000.0, kDt_s);
        advance(state, p, 5000.0, kDt_s);
        ASSERT_EQ(state.velocity_m_s, 0.0);
        ASSERT_EQ(state.position_m, 0.0);
    }
}

}  // namespace
}  // namespace tp::chassis
