#include "tp/core/clutch/clutch.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "flywheel.hpp"
#include "reference_clutch.hpp"

namespace tp::clutch {
namespace {

using testing::Flywheel;
using testing::reference_params;

constexpr double kDt_s = 0.001;

// One coupled step of two flywheels through the clutch.
Output step_pair(State& state, const Params& params, double pedal, Flywheel& engine,
                 Flywheel& driveline) {
    const Output out =
        step(state, params, {.pedal = pedal}, engine.predict(kDt_s), driveline.predict(kDt_s));
    engine.advance(kDt_s, -out.torque_nm);
    driveline.advance(kDt_s, out.torque_nm);
    return out;
}

TEST(ClutchParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(ClutchParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.static_torque_capacity_nm = 0.0; }),
              ParamError::kCapacityNotPositive);
    EXPECT_EQ(with([](Params& p) { p.static_torque_capacity_nm = std::nan(""); }),
              ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.kinetic_ratio = 0.0; }), ParamError::kKineticRatioOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.kinetic_ratio = 1.2; }), ParamError::kKineticRatioOutOfRange);
}

TEST(ClutchParams, RejectsBadEngagementCurves) {
    const auto with_curve = [](std::span<const math::CurvePoint> points) {
        const auto curve = math::Curve::create(points);
        if (!curve) {
            ADD_FAILURE() << "invalid test curve";
            return std::optional<ParamError>{};
        }
        Params p = reference_params();
        p.engagement = *curve;
        return validate(p);
    };
    const std::array<math::CurvePoint, 2> short_travel = {{{0.0, 1.0}, {0.8, 0.0}}};
    const std::array<math::CurvePoint, 2> above_one = {{{0.0, 1.5}, {1.0, 0.0}}};
    const std::array<math::CurvePoint, 3> increasing = {{{0.0, 1.0}, {0.5, 0.2}, {1.0, 0.4}}};
    const std::array<math::CurvePoint, 2> slipping_released = {{{0.0, 0.9}, {1.0, 0.0}}};
    const std::array<math::CurvePoint, 2> dragging_pressed = {{{0.0, 1.0}, {1.0, 0.1}}};
    EXPECT_EQ(with_curve(short_travel), ParamError::kEngagementDoesNotSpanPedalTravel);
    EXPECT_EQ(with_curve(above_one), ParamError::kEngagementOutOfRange);
    EXPECT_EQ(with_curve(increasing), ParamError::kEngagementIncreasesWithPedal);
    EXPECT_EQ(with_curve(slipping_released), ParamError::kEngagementNotFullWhenReleased);
    EXPECT_EQ(with_curve(dragging_pressed), ParamError::kEngagementNotZeroWhenPressed);
}

TEST(Clutch, CapacityNeverIncreasesWithPedalTravel) {
    const Params params = reference_params();
    State state;
    double previous_nm = params.static_torque_capacity_nm;
    for (int i = 0; i <= 100; ++i) {
        const double pedal = i / 100.0;
        const Output out = step(state, params, {.pedal = pedal}, {}, {.omega_per_torque = 1.0});
        EXPECT_LE(out.capacity_nm, previous_nm) << "pedal " << pedal;
        previous_nm = out.capacity_nm;
    }
    EXPECT_DOUBLE_EQ(previous_nm, 0.0);
}

TEST(Clutch, CapacityHelperMatchesStep) {
    const Params params = reference_params();
    State state;
    for (const double pedal : {-1.0, 0.0, 0.4, 0.5, 0.6, 1.0, 2.0}) {
        const Output out = step(state, params, {.pedal = pedal}, {}, {.omega_per_torque = 1.0});
        EXPECT_EQ(capacity_nm(params, pedal), out.capacity_nm) << "pedal " << pedal;
    }
}

TEST(Clutch, PressedPedalTransmitsNothing) {
    const Params params = reference_params();
    State state{.locked = true};
    Flywheel engine{.inertia_kg_m2 = 1.2, .omega_rad_s = 200.0};
    Flywheel driveline{.inertia_kg_m2 = 2.0, .omega_rad_s = 0.0};
    for (int i = 0; i < 1000; ++i) {
        const Output out = step_pair(state, params, 1.0, engine, driveline);
        ASSERT_EQ(out.torque_nm, 0.0);
    }
    EXPECT_FALSE(state.locked);
    EXPECT_EQ(engine.omega_rad_s, 200.0);
    EXPECT_EQ(driveline.omega_rad_s, 0.0);
}

// Releasing the pedal on two shafts at different speeds: slip falls
// monotonically to zero without changing sign, momentum is conserved, kinetic
// energy only decreases, and the clutch then stays locked.
TEST(Clutch, LocksUpWithoutOvershootConservingMomentum) {
    const Params params = reference_params();
    State state;
    Flywheel engine{.inertia_kg_m2 = 1.2, .omega_rad_s = 200.0};
    Flywheel driveline{.inertia_kg_m2 = 2.0, .omega_rad_s = 0.0};
    const double momentum = engine.momentum() + driveline.momentum();
    double energy_j = engine.kinetic_energy_j() + driveline.kinetic_energy_j();
    double previous_slip_rad_s = engine.omega_rad_s - driveline.omega_rad_s;
    double dissipated_j = 0.0;

    int locked_at = -1;
    for (int i = 0; i < 2000; ++i) {
        const Output out = step_pair(state, params, 0.0, engine, driveline);
        const double slip_rad_s = engine.omega_rad_s - driveline.omega_rad_s;
        ASSERT_GE(slip_rad_s, -1e-9) << "slip overshot at step " << i;
        ASSERT_LE(slip_rad_s, previous_slip_rad_s + 1e-9);
        ASSERT_NEAR(engine.momentum() + driveline.momentum(), momentum, 1e-9);
        const double new_energy_j = engine.kinetic_energy_j() + driveline.kinetic_energy_j();
        ASSERT_LE(new_energy_j, energy_j + 1e-9);
        energy_j = new_energy_j;
        dissipated_j += out.slip_power_w * kDt_s;
        previous_slip_rad_s = slip_rad_s;
        if (state.locked && locked_at < 0) {
            locked_at = i;
        }
        if (locked_at >= 0) {
            ASSERT_TRUE(state.locked) << "released again at step " << i;
            ASSERT_NEAR(slip_rad_s, 0.0, 1e-9);
        }
    }
    EXPECT_GE(locked_at, 0);

    // Perfectly inelastic coupling: the energy lost is fixed by momentum.
    const double common_rad_s = momentum / (1.2 + 2.0);
    const double expected_loss_j =
        0.5 * 1.2 * 200.0 * 200.0 - 0.5 * 3.2 * common_rad_s * common_rad_s;
    const double initial_energy_j = 0.5 * 1.2 * 200.0 * 200.0;
    EXPECT_NEAR(initial_energy_j - energy_j, expected_loss_j, 1e-6 * expected_loss_j);
    // Integrated slip power accounts for that loss to within the step error.
    EXPECT_NEAR(dissipated_j, expected_loss_j, 0.01 * expected_loss_j);
}

// Through a lossy gearbox the driveline responds less to driving torque than
// to braking torque. The clutch must use the response for the direction the
// torque actually acts in, or a locked clutch would leave residual slip.
TEST(Clutch, LocksExactlyWithDirectionDependentResponse) {
    const Params params = reference_params();
    for (const double engine_rad_s : {150.0, 50.0}) {
        State state{.locked = true};
        Flywheel engine{.inertia_kg_m2 = 1.2, .omega_rad_s = engine_rad_s};
        Flywheel driveline{.inertia_kg_m2 = 2.0, .omega_rad_s = 100.0, .efficiency = 0.9};
        // Small speed differences the locked clutch absorbs in one step.
        engine.omega_rad_s = 100.0 + (engine_rad_s - 100.0) * 1e-3;
        const Output out = step_pair(state, params, 0.0, engine, driveline);
        EXPECT_TRUE(state.locked);
        EXPECT_NEAR(engine.omega_rad_s, driveline.omega_rad_s, 1e-12) << "torque " << out.torque_nm;
    }
}

// Slipping clutch transmits its kinetic capacity, in the direction that
// opposes slip, whichever shaft is faster.
TEST(Clutch, SlipsAtKineticCapacityAgainstSlip) {
    const Params params = reference_params();
    const double kinetic_nm = params.kinetic_ratio * params.static_torque_capacity_nm;
    for (const double engine_rad_s : {300.0, -300.0}) {
        State state;
        Flywheel engine{.inertia_kg_m2 = 1.2, .omega_rad_s = engine_rad_s};
        Flywheel ground{.inertia_kg_m2 = 1e12, .omega_rad_s = 0.0};
        const Output out = step_pair(state, params, 0.0, engine, ground);
        EXPECT_FALSE(state.locked);
        EXPECT_DOUBLE_EQ(out.torque_nm, std::copysign(kinetic_nm, engine_rad_s));
        EXPECT_GT(out.slip_power_w, 0.0);
    }
}

// A locked clutch holds until the torque it must carry exceeds static
// capacity, then slips at the lower kinetic capacity, and grabs again once
// kinetic capacity is enough.
TEST(Clutch, BreaksAwayAtStaticAndRelocksAtKineticCapacity) {
    const Params params = reference_params();
    const double static_nm = params.static_torque_capacity_nm;
    const double kinetic_nm = params.kinetic_ratio * static_nm;
    State state{.locked = true};
    Flywheel engine{.inertia_kg_m2 = 1.2};
    Flywheel ground{.inertia_kg_m2 = 1e12};

    // Ramp the engine-side torque up: the clutch carries it until breakaway.
    double breakaway_nm = 0.0;
    for (int i = 0; i < 1000 && state.locked; ++i) {
        engine.external_torque_nm = i;
        const Output out = step_pair(state, params, 0.0, engine, ground);
        if (state.locked) {
            ASSERT_NEAR(out.torque_nm, engine.external_torque_nm, 1e-6);
        } else {
            breakaway_nm = engine.external_torque_nm;
            EXPECT_DOUBLE_EQ(out.torque_nm, kinetic_nm);
        }
    }
    EXPECT_GT(breakaway_nm, static_nm);
    EXPECT_LE(breakaway_nm, static_nm + 1.0);

    // Between kinetic and static capacity it keeps slipping...
    engine.external_torque_nm = 0.5 * (kinetic_nm + static_nm);
    step_pair(state, params, 0.0, engine, ground);
    EXPECT_FALSE(state.locked);

    // ...and grabs once the torque falls below kinetic capacity and the
    // engine side has slowed back to the ground's speed.
    engine.external_torque_nm = 0.5 * kinetic_nm;
    for (int i = 0; i < 5000 && !state.locked; ++i) {
        step_pair(state, params, 0.0, engine, ground);
    }
    EXPECT_TRUE(state.locked);
}

}  // namespace
}  // namespace tp::clutch
