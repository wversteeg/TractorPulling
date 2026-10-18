#include "tp/core/sled/sled.hpp"

#include "tp/core/units/constants.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "../tire/reference_tire.hpp"
#include "reference_sled.hpp"

namespace tp::sled {
namespace {

using testing::reference_params;
using units::kStandardGravity_m_s2;

constexpr double kDt_s = 0.001;

double weight_n(const Params& params) {
    return total_mass_kg(params) * kStandardGravity_m_s2;
}

Ground reference_ground() {
    const soil::Surface surface = tire::testing::reference_surface();
    return {.pan = surface, .wheels = surface};
}

Loads at(const Params& params, double distance_m, const HookForce& hook = {},
         const Ground& ground = reference_ground()) {
    return loads(params, State{.position_m = distance_m, .distance_m = distance_m}, hook, ground);
}

Ground softer(Ground ground) {
    ground.pan.cone_index_pa *= 0.25;
    ground.wheels.cone_index_pa *= 0.25;
    return ground;
}

TEST(SledParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(SledParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.frame_mass_kg = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.frame_mass_kg = 0.0; }), ParamError::kFrameMassNotPositive);
    EXPECT_EQ(with([](Params& p) { p.box_mass_kg = -1.0; }), ParamError::kBoxMassNegative);
    EXPECT_EQ(with([](Params& p) { p.pan_ahead_of_wheels_m = 0.0; }),
              ParamError::kPanNotAheadOfWheels);
    EXPECT_EQ(with([](Params& p) { p.frame_cg_ahead_of_wheels_m = 3.5; }),
              ParamError::kFrameCgOutsideSled);
    EXPECT_EQ(with([](Params& p) { p.box_end_ahead_of_wheels_m = 3.5; }),
              ParamError::kBoxOutsideSled);
    EXPECT_EQ(with([](Params& p) { p.box_travel_distance_m = 0.0; }),
              ParamError::kBoxTravelNotPositive);
    EXPECT_EQ(with([](Params& p) { p.hook_height_m = 0.0; }), ParamError::kHookInvalid);
    EXPECT_EQ(with([](Params& p) { p.pushdown_start_m = -1.0; }),
              ParamError::kPushdownStartNegative);
    EXPECT_EQ(with([](Params& p) { p.pushdown_ramp_m = 0.0; }),
              ParamError::kPushdownRampNotPositive);
    EXPECT_EQ(with([](Params& p) { p.pan_friction_coefficient = 0.0; }),
              ParamError::kPanFrictionOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pan_area_m2 = 0.0; }), ParamError::kPanAreaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.pan_plough_coefficient = -1.0; }),
              ParamError::kPanPloughNegative);
    EXPECT_EQ(with([](Params& p) { p.wheel.radius_m = 0.0; }), ParamError::kWheel);
    EXPECT_EQ(with([](Params& p) { p.wheel_count = 0; }), ParamError::kWheelCountOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.wheel_count = kMaxWheels + 1; }),
              ParamError::kWheelCountOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pull_off.pushdown_start_m = -1.0; }),
              ParamError::kPullOffNegative);
    EXPECT_EQ(with([](Params& p) { p.chain.length_m = 0.0; }), ParamError::kChainInvalid);
    EXPECT_EQ(with([](Params& p) { p.pull_off.extra_pan_friction = -0.1; }),
              ParamError::kPullOffNegative);
}

// At the start the box sits over the wheels: the pan carries only its
// share of the frame by the levers.
TEST(Sled, StartsWithMostWeightOnTheWheels) {
    const Params params = reference_params();
    const Loads start = at(params, 0.0);
    const double frame_n = params.frame_mass_kg * kStandardGravity_m_s2;
    EXPECT_NEAR(start.pan_n, frame_n * 0.75 / 3.0, 1e-9);
    EXPECT_NEAR(start.pan_n + start.wheels_n, weight_n(params), 1e-9);
    EXPECT_GT(start.wheels_n, 3.0 * start.pan_n);
    EXPECT_EQ(start.pushdown, 0.0);
}

// The box moves forward with the sled's travel and stops at its end.
TEST(Sled, BoxMovesForwardWithTravel) {
    const Params params = reference_params();
    EXPECT_DOUBLE_EQ(at(params, 0.0).box_ahead_of_wheels_m, params.box_start_ahead_of_wheels_m);
    EXPECT_NEAR(at(params, 50.0).box_ahead_of_wheels_m, 1.5, 1e-12);
    EXPECT_DOUBLE_EQ(at(params, 100.0).box_ahead_of_wheels_m, params.box_end_ahead_of_wheels_m);
    EXPECT_DOUBLE_EQ(at(params, 130.0).box_ahead_of_wheels_m, params.box_end_ahead_of_wheels_m);
}

// Resistance only grows along the track, and the load always adds up to the
// weight.
TEST(Sled, ResistanceGrowsWithDistance) {
    const Params params = reference_params();
    double previous_n = 0.0;
    for (int metre = 0; metre <= 120; ++metre) {
        const Loads l = at(params, metre);
        ASSERT_GE(l.resistance_n, previous_n - 1e-9) << metre << " m";
        ASSERT_NEAR(l.pan_n + l.wheels_n, weight_n(params), 1e-6);
        previous_n = l.resistance_n;
    }
    EXPECT_GT(at(params, 59.0).resistance_n, 1.5 * at(params, 0.0).resistance_n);
}

// The push-down lifts the wheels over its ramp: from then on the pan
// carries everything and the resistance jumps to the pan's friction (with
// its ploughing) times the weight.
TEST(Sled, PushdownPutsAllWeightOnThePan) {
    const Params params = reference_params();
    EXPECT_EQ(at(params, params.pushdown_start_m).pushdown, 0.0);
    const Loads half = at(params, params.pushdown_start_m + 0.5 * params.pushdown_ramp_m);
    EXPECT_NEAR(half.pushdown, 0.5, 1e-12);
    const Loads full = at(params, params.pushdown_start_m + params.pushdown_ramp_m);
    EXPECT_EQ(full.pushdown, 1.0);
    EXPECT_EQ(full.wheels_n, 0.0);
    EXPECT_NEAR(full.pan_n, weight_n(params), 1e-9);
    EXPECT_NEAR(full.resistance_n, full.pan_friction * weight_n(params), 1e-9);
    EXPECT_GT(full.pan_friction, params.pan_friction_coefficient);
    EXPECT_GT(full.resistance_n, 1.5 * at(params, params.pushdown_start_m).resistance_n);
}

// The chain's pull at hook height tips the sled onto its pan; pulling up at
// the hook takes load off the pan (and off the track).
TEST(Sled, ChainForceShiftsLoadOntoOrOffThePan) {
    const Params params = reference_params();
    const Loads free = at(params, 10.0);
    constexpr double kPull_n = 10000.0;
    const Loads pulled = at(params, 10.0, {.forward_n = kPull_n});
    EXPECT_NEAR(pulled.pan_n - free.pan_n,
                kPull_n * params.hook_height_m / params.pan_ahead_of_wheels_m, 1e-9);
    EXPECT_NEAR(pulled.pan_n + pulled.wheels_n, weight_n(params), 1e-9);

    constexpr double kUp_n = 2000.0;
    const Loads lifted = at(params, 10.0, {.up_n = kUp_n});
    EXPECT_NEAR(free.pan_n - lifted.pan_n,
                kUp_n * params.hook_ahead_of_wheels_m / params.pan_ahead_of_wheels_m, 1e-9);
    EXPECT_NEAR(lifted.pan_n + lifted.wheels_n, weight_n(params) - kUp_n, 1e-9);

    const Loads pressed = at(params, 10.0, {.up_n = -kUp_n});
    EXPECT_GT(pressed.resistance_n, free.resistance_n);
}

// Each pull-off sled drags harder everywhere along the track: more weight
// in the box, the pull-off's (earlier) push-down and knives in the pan.
TEST(Sled, PullOffSledIsHeavierEverywhere) {
    const Params first = reference_params();
    const Params second = heavier(first);
    const Params third = heavier(second);
    EXPECT_EQ(validate(second), std::nullopt);
    EXPECT_DOUBLE_EQ(second.box_mass_kg, first.box_mass_kg + first.pull_off.extra_box_mass_kg);
    EXPECT_DOUBLE_EQ(second.pushdown_start_m, first.pull_off.pushdown_start_m);
    EXPECT_DOUBLE_EQ(third.pushdown_start_m, first.pull_off.pushdown_start_m);
    EXPECT_DOUBLE_EQ(second.pan_friction_coefficient,
                     first.pan_friction_coefficient + first.pull_off.extra_pan_friction);
    for (int metre = 0; metre <= 100; ++metre) {
        ASSERT_GT(at(second, metre).resistance_n, at(first, metre).resistance_n) << metre;
        ASSERT_GT(at(third, metre).resistance_n, at(second, metre).resistance_n) << metre;
    }
}

// However many pull-offs, the pan's friction stays valid.
TEST(Sled, PullOffsStayValid) {
    Params params = reference_params();
    for (int i = 0; i < 50; ++i) {
        params = heavier(params);
        ASSERT_EQ(validate(params), std::nullopt) << "pull-off " << i;
    }
    EXPECT_EQ(params.pushdown_start_m, reference_params().pull_off.pushdown_start_m);
}

// The day's track sets the push-down for the run and for pull-offs; the
// rest of the class's sled stays.
TEST(Sled, ConditionsSetThePushdown) {
    const Params sled = reference_params();
    const Params wet =
        with_conditions(sled, {.pushdown_start_m = 75.0, .pull_off_pushdown_start_m = 65.0});
    EXPECT_EQ(wet.pushdown_start_m, 75.0);
    EXPECT_EQ(wet.pull_off.pushdown_start_m, 65.0);
    EXPECT_EQ(heavier(wet).pushdown_start_m, 65.0);
    EXPECT_EQ(wet.box_mass_kg, sled.box_mass_kg);
    EXPECT_EQ(validate(wet), std::nullopt);

    const Params unchanged = with_conditions(sled, {});
    EXPECT_EQ(unchanged.pushdown_start_m, sled.pushdown_start_m);
    EXPECT_EQ(unchanged.pull_off.pushdown_start_m, sled.pull_off.pushdown_start_m);
}

// On softer ground the pan ploughs deeper and the wheels sink in: the same
// sled drags harder everywhere.
TEST(Sled, SofterTrackDragsHarder) {
    const Params params = reference_params();
    for (int metre = 0; metre <= 100; metre += 5) {
        const Loads firm = at(params, metre);
        const Loads soft = at(params, metre, {}, softer(reference_ground()));
        ASSERT_GT(soft.resistance_n, firm.resistance_n) << metre << " m";
        ASSERT_GT(soft.pan_friction, firm.pan_friction) << metre << " m";
    }
}

// Ploughing grows with the pan's pressure over the soil's strength; without
// it the pan just slides.
TEST(Sled, PanPloughsWithPressure) {
    Params params = reference_params();
    const Loads start = at(params, 0.0);
    const Loads end = at(params, 100.0);
    ASSERT_GT(end.pan_n, start.pan_n);
    EXPECT_GT(end.pan_friction, start.pan_friction);
    const double cone_index_pa = reference_ground().pan.cone_index_pa;
    EXPECT_NEAR(end.pan_friction,
                params.pan_friction_coefficient * (1.0 + params.pan_plough_coefficient * end.pan_n /
                                                             params.pan_area_m2 / cone_index_pa),
                1e-12);

    params.pan_plough_coefficient = 0.0;
    EXPECT_EQ(at(params, 100.0).pan_friction, params.pan_friction_coefficient);
}

// The wheels roll as towed tires: their resistance is Brixius' for their
// share of the load.
TEST(Sled, WheelsRollAsTowedTires) {
    Params params = reference_params();
    params.pan_plough_coefficient = 0.0;
    const Loads start = at(params, 0.0);
    const double per_wheel_n = start.wheels_n / params.wheel_count;
    const double rolling =
        tire::towed_rolling_resistance(params.wheel, per_wheel_n, reference_ground().wheels);
    EXPECT_NEAR(start.resistance_n,
                params.pan_friction_coefficient * start.pan_n + rolling * start.wheels_n, 1e-9);
    EXPECT_GT(rolling, 0.04);
    EXPECT_LT(rolling, 0.15);
}

TEST(Sled, ResistanceHoldsAtRestWithoutPullingBack) {
    const Params params = reference_params();
    State state;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 20000.0, kDt_s);
        advance(state, p, 5000.0, kDt_s);
        ASSERT_EQ(state.velocity_m_s, 0.0);
        ASSERT_EQ(state.distance_m, 0.0);
    }
}

TEST(Sled, NetForceAcceleratesTheWholeSled) {
    const Params params = reference_params();
    State state{.position_m = 5.0};
    constexpr double kNet_n = 6500.0;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 1000.0, kDt_s);
        advance(state, p, 1000.0 + kNet_n, kDt_s);
    }
    EXPECT_NEAR(state.velocity_m_s, kNet_n / total_mass_kg(params), 1e-9);
    EXPECT_NEAR(state.position_m - 5.0, state.distance_m, 1e-12);
}

}  // namespace
}  // namespace tp::sled
