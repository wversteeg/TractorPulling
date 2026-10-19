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
    EXPECT_EQ(with([](Params& p) { p.chain.length_m = 0.0; }), ParamError::kChainInvalid);
    EXPECT_EQ(with([](Params& p) { p.knives.count = kMaxKnives + 1; }),
              ParamError::kKnivesOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.knives.depth_m = kMaxKnifeDepth_m + 0.01; }),
              ParamError::kKnivesOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.knives.width_m = 0.0; }), ParamError::kKnivesOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.berm.collected_share = 1.5; }), ParamError::kBermOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.berm.spill_distance_m = 0.0; }), ParamError::kBermOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.berm.friction_coefficient = -0.1; }),
              ParamError::kBermOutOfRange);
    EXPECT_EQ(with([](Params& p) {
                  p.knives = {.count = 0, .depth_m = 0.0, .width_m = 0.0};
                  p.pull_off_step = {.box_mass_kg = 100.0};
              }),
              std::nullopt)
        << "no knives need no width";
    EXPECT_EQ(with([](Params& p) { p.pull_off_step = {}; }), ParamError::kPullOffStepNotHeavier);
    EXPECT_EQ(with([](Params& p) { p.pull_off_step = {.box_mass_kg = -100.0, .knives = 2}; }),
              ParamError::kPullOffStepNotHeavier);
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
    EXPECT_GT(at(params, 59.0).resistance_n, 1.25 * at(params, 0.0).resistance_n);
}

// The push-down lifts the wheels over its ramp: from then on the pan
// carries everything and the resistance is the pan's friction (with its
// ploughing) times the weight, plus the knives.
TEST(Sled, PushdownPutsAllWeightOnThePan) {
    const Params params = reference_params();
    EXPECT_EQ(at(params, params.pushdown_start_m).pushdown, 0.0);
    const Loads half = at(params, params.pushdown_start_m + 0.5 * params.pushdown_ramp_m);
    EXPECT_NEAR(half.pushdown, 0.5, 1e-12);
    const Loads full = at(params, params.pushdown_start_m + params.pushdown_ramp_m);
    EXPECT_EQ(full.pushdown, 1.0);
    EXPECT_EQ(full.wheels_n, 0.0);
    EXPECT_NEAR(full.pan_n, weight_n(params), 1e-9);
    EXPECT_NEAR(full.resistance_n, full.pan_friction * weight_n(params) + full.knives_n, 1e-9);
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

// Each pull-off step drags harder everywhere along the track, and several
// steps more than one.
TEST(Sled, PullOffStepsAreHeavierEverywhere) {
    const Params first = reference_params();
    const Params second = heavier(first);
    const Params fourth = heavier(first, 3);
    EXPECT_EQ(validate(second), std::nullopt);
    EXPECT_EQ(second.knives.count, first.knives.count + first.pull_off_step.knives);
    EXPECT_EQ(fourth.knives.count, first.knives.count + 3 * first.pull_off_step.knives);
    for (int metre = 0; metre <= 100; ++metre) {
        ASSERT_GT(at(second, metre).resistance_n, at(first, metre).resistance_n) << metre;
        ASSERT_GT(at(fourth, metre).resistance_n, at(second, metre).resistance_n) << metre;
    }
}

// The operator's changes add up, either way, and stay within what the sled
// can take: no negative box, knives within their count and depth, the
// push-down on the track.
TEST(Sled, AdjustmentsStayValid) {
    const Params sled = reference_params();
    const Adjustment heavier_by{
        .box_mass_kg = 200.0, .knives = 2, .knife_depth_m = 0.01, .pushdown_earlier_m = 5.0};
    const Params up = adjusted(sled, heavier_by);
    EXPECT_EQ(up.box_mass_kg, sled.box_mass_kg + 200.0);
    EXPECT_EQ(up.knives.count, sled.knives.count + 2);
    EXPECT_NEAR(up.knives.depth_m, sled.knives.depth_m + 0.01, 1e-15);
    EXPECT_EQ(up.pushdown_start_m, sled.pushdown_start_m - 5.0);
    const Params back = adjusted(
        up,
        {.box_mass_kg = -200.0, .knives = -2, .knife_depth_m = -0.01, .pushdown_earlier_m = -5.0});
    EXPECT_EQ(back.box_mass_kg, sled.box_mass_kg);
    EXPECT_EQ(back.knives.count, sled.knives.count);
    EXPECT_EQ(back.pushdown_start_m, sled.pushdown_start_m);

    for (const int times : {-100, 100}) {
        const Params extreme = adjusted(sled, heavier_by, times);
        EXPECT_EQ(validate(extreme), std::nullopt) << times;
    }
    EXPECT_EQ(adjusted(sled, heavier_by, -100).box_mass_kg, 0.0);
    EXPECT_EQ(adjusted(sled, heavier_by, 100).knives.count, kMaxKnives);
    EXPECT_EQ(adjusted(sled, heavier_by, 100).pushdown_start_m, 0.0);
}

// Knives cut harder on a firmer track and the deeper they go; without
// knives the pan only slides.
TEST(Sled, KnivesCutWithTheSoilsStrength) {
    const Params params = reference_params();
    const Loads firm = at(params, 30.0);
    const Loads soft = at(params, 30.0, {}, softer(reference_ground()));
    EXPECT_GT(firm.knives_n, 0.0);
    EXPECT_NEAR(soft.knives_n, 0.25 * firm.knives_n, 1e-9);

    const Params deeper = adjusted(params, {.knife_depth_m = 0.02});
    EXPECT_NEAR(at(deeper, 30.0).knives_n,
                firm.knives_n * deeper.knives.depth_m / params.knives.depth_m, 1e-9);

    // On very hard ground they no longer reach full depth: the draft levels
    // off instead of growing with the strength.
    Ground hard = reference_ground();
    hard.pan.cone_index_pa = 4.0e6;
    Ground harder = hard;
    harder.pan.cone_index_pa = 6.0e6;
    EXPECT_GT(at(params, 30.0, {}, hard).knives_n, firm.knives_n);
    EXPECT_EQ(at(params, 30.0, {}, harder).knives_n, at(params, 30.0, {}, hard).knives_n);

    Params bare = params;
    bare.knives.count = 0;
    EXPECT_EQ(at(bare, 30.0).knives_n, 0.0);
    EXPECT_NEAR(at(params, 30.0).resistance_n - at(bare, 30.0).resistance_n, firm.knives_n, 1e-9);
}

// Soil thrown back lands in front of the pan; standing still, nothing
// spills, so a tractor spinning in place buries its own sled.
TEST(Sled, ThrownSoilBuildsABermAtRest) {
    const Params params = reference_params();
    State state;
    constexpr double kThrown_m3_s = 0.01;
    for (int i = 0; i < 1000; ++i) {
        collect(state, params, kThrown_m3_s, kDt_s);
    }
    EXPECT_NEAR(state.berm_kg,
                params.berm.collected_share * soil::kLooseDensity_kg_m3 * kThrown_m3_s * 1.0, 1e-9);
    const Loads l = loads(params, state, {}, reference_ground());
    EXPECT_NEAR(l.berm_n, params.berm.friction_coefficient * state.berm_kg * kStandardGravity_m_s2,
                1e-9);
}

// Moving, the berm spills to the sides: with nothing thrown it shrinks by
// e over its spill distance, and with a steady supply it levels off where
// what lands balances what spills.
TEST(Sled, BermSpillsAsTheSledMoves) {
    const Params params = reference_params();
    constexpr double kSpeed_m_s = 2.0;
    State coasting{.velocity_m_s = kSpeed_m_s, .berm_kg = 300.0};
    const int steps = static_cast<int>(params.berm.spill_distance_m / kSpeed_m_s / kDt_s);
    for (int i = 0; i < steps; ++i) {
        collect(coasting, params, 0.0, kDt_s);
    }
    EXPECT_NEAR(coasting.berm_kg, 300.0 / std::exp(1.0), 0.5);

    constexpr double kThrown_m3_s = 0.02;
    State fed{.velocity_m_s = kSpeed_m_s};
    for (int i = 0; i < 20 * steps; ++i) {
        collect(fed, params, kThrown_m3_s, kDt_s);
    }
    const double landing_kg_s =
        params.berm.collected_share * soil::kLooseDensity_kg_m3 * kThrown_m3_s;
    EXPECT_NEAR(fed.berm_kg, landing_kg_s * params.berm.spill_distance_m / kSpeed_m_s,
                0.01 * fed.berm_kg);
}

// The berm only adds drag, and only ever holds the sled.
TEST(Sled, BermAddsToTheResistance) {
    const Params params = reference_params();
    const State clean{.position_m = 30.0, .distance_m = 30.0};
    State buried = clean;
    buried.berm_kg = 200.0;
    const Loads a = loads(params, clean, {}, reference_ground());
    const Loads b = loads(params, buried, {}, reference_ground());
    EXPECT_EQ(a.berm_n, 0.0);
    EXPECT_NEAR(b.resistance_n - a.resistance_n, b.berm_n, 1e-9);
}

// The day's track sets the push-down; the rest of the class's sled stays.
TEST(Sled, ConditionsSetThePushdown) {
    const Params sled = reference_params();
    const Params wet = with_conditions(sled, {.pushdown_start_m = 75.0});
    EXPECT_EQ(wet.pushdown_start_m, 75.0);
    EXPECT_EQ(wet.box_mass_kg, sled.box_mass_kg);
    EXPECT_EQ(validate(wet), std::nullopt);
    EXPECT_EQ(with_conditions(sled, {}).pushdown_start_m, sled.pushdown_start_m);
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
    EXPECT_NEAR(
        start.resistance_n,
        params.pan_friction_coefficient * start.pan_n + start.knives_n + rolling * start.wheels_n,
        1e-9);
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

// ---- Sideways

// Steps only the sideways motion, at a fixed forward speed, with a side
// pull at the hook.
State swing(const Params& params, double forward_m_s, double hook_side_n, int steps) {
    State state{.position_m = 10.0, .velocity_m_s = forward_m_s, .distance_m = 10.0};
    const Ground ground = reference_ground();
    for (int i = 0; i < steps; ++i) {
        const Loads l = loads(params, state, {.forward_n = 20000.0}, ground);
        advance_lateral(state, params, l, hook_side_n, ground.wheels, kDt_s);
    }
    return state;
}

TEST(SledPlane, StaysInLineWithoutSidePull) {
    const State state = swing(reference_params(), 2.0, 0.0, 1000);
    EXPECT_EQ(state.lateral_velocity_m_s, 0.0);
    EXPECT_EQ(state.yaw_rate_rad_s, 0.0);
    EXPECT_EQ(state.heading_rad, 0.0);
    EXPECT_EQ(state.wheel_side_deflection_m, 0.0);
}

// Pulled left at the hook, ahead of the wheels, the sled turns left.
TEST(SledPlane, SidePullAtTheHookTurnsItThatWay) {
    const State left = swing(reference_params(), 2.0, 3000.0, 500);
    EXPECT_GT(left.heading_rad, 0.0);
    const State right = swing(reference_params(), 2.0, -3000.0, 500);
    EXPECT_EQ(right.heading_rad, -left.heading_rad);
}

// Sliding friction acts against the pan's sliding direction: sliding
// forward, little of it is left to resist sideways, so a moving sled
// swings far more easily than one at rest (the dog-tail).
TEST(SledPlane, SlidingSledSwingsMoreEasily) {
    const State moving = swing(reference_params(), 2.0, 3000.0, 500);
    const State at_rest = swing(reference_params(), 0.0, 3000.0, 500);
    EXPECT_GT(std::abs(moving.heading_rad), 5.0 * std::abs(at_rest.heading_rad));
}

TEST(SledPlane, HookSwingsWithTheHeading) {
    const Params params = reference_params();
    const Hook straight = hook(params, State{.position_m = 5.0, .velocity_m_s = 2.0});
    EXPECT_EQ(straight.x_m, 5.0 + params.hook_ahead_of_wheels_m);
    EXPECT_EQ(straight.y_m, 0.0);
    EXPECT_EQ(straight.z_m, params.hook_height_m);
    EXPECT_EQ(straight.velocity_x_m_s, 2.0);
    const Hook turned =
        hook(params, State{.position_m = 5.0, .heading_rad = 0.2, .yaw_rate_rad_s = 0.1});
    EXPECT_NEAR(turned.y_m, params.hook_ahead_of_wheels_m * std::sin(0.2), 1e-15);
    // Turning left, the hook ahead of the wheels moves left.
    EXPECT_NEAR(turned.velocity_y_m_s, params.hook_ahead_of_wheels_m * 0.1 * std::cos(0.2), 1e-15);
}

TEST(SledPlane, MovesAlongItsHeading) {
    const Params params = reference_params();
    State state{.velocity_m_s = 2.0, .heading_rad = 0.3};
    const Prediction p = predict(state, params, 0.0, kDt_s);
    advance(state, p, 0.0, kDt_s);
    EXPECT_NEAR(state.position_m, 2.0 * std::cos(0.3) * kDt_s, 1e-15);
    EXPECT_NEAR(state.lateral_position_m, 2.0 * std::sin(0.3) * kDt_s, 1e-15);
    EXPECT_NEAR(state.distance_m, 2.0 * kDt_s, 1e-15);
}

TEST(SledPlane, RejectsNonsense) {
    Params params = reference_params();
    params.frame_yaw_inertia_kg_m2 = 0.0;
    EXPECT_EQ(validate(params), ParamError::kYawInertiaNotPositive);
    params = reference_params();
    params.wheel_cornering.limit = 0.0;
    EXPECT_EQ(validate(params), ParamError::kWheelCornering);
}

}  // namespace
}  // namespace tp::sled
