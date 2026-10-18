#include "tp/core/pull/pull.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "../sled/reference_sled.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::pull {
namespace {

constexpr double kDt_s = 0.001;
constexpr double kSpunOutSlip = 0.9;

Model reference_model() {
    const auto model = Model::create({.tractor = vehicle::testing::reference_params(),
                                      .sled = sled::testing::reference_params(),
                                      .soil = tire::testing::reference_soil()});
    if (!model) {
        std::abort();
    }
    return *model;
}

// A driver: idle in neutral, select the gear, rev up with the clutch
// pressed, slip it in over 3 s or dump it, and pull at full throttle until
// the engine stalls, the wheels spin with the sled stopped, or 100 m.
struct Driver {
    Model model = reference_model();
    State state = initial_state(model);
    Output last;
    bool stalled = false;
    bool spun_out = false;
    double max_tension_n = 0.0;
    double resistance_at_20_m_n = 0.0;
    double resistance_at_50_m_n = 0.0;

    void drive(int gear, bool dump, double max_seconds = 90.0) {
        for (int i = 0; i < static_cast<int>(max_seconds / kDt_s); ++i) {
            const double t_s = i * kDt_s;
            vehicle::Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                                 .clutch_pedal = 1.0,
                                 .gear = t_s < 0.5 ? driveline::kNeutral : gear};
            if (t_s >= 3.0) {
                input.clutch_pedal = dump ? 0.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0);
            }
            last = step(state, model, input, kDt_s);
            max_tension_n = std::max(max_tension_n, last.chain.tension_n);
            if (state.sled.distance_m < 20.0) {
                resistance_at_20_m_n = last.sled.resistance_n;
            }
            if (state.sled.distance_m < 50.0) {
                resistance_at_50_m_n = last.sled.resistance_n;
            }
            if (!state.tractor.engine.running) {
                stalled = true;
                return;
            }
            if (t_s > 10.0 && state.sled.velocity_m_s == 0.0 &&
                last.tractor.slip_ratio > kSpunOutSlip) {
                spun_out = true;
                return;
            }
            if (state.sled.distance_m >= 100.0) {
                return;
            }
        }
    }
};

TEST(PullParams, NamesTheInvalidPart) {
    Params params{.tractor = vehicle::testing::reference_params(),
                  .sled = sled::testing::reference_params(),
                  .soil = tire::testing::reference_soil()};
    EXPECT_EQ(validate(params), std::nullopt);
    params.sled.pushdown_ramp_m = 0.0;
    EXPECT_EQ(validate(params), ParamError::kSled);
    EXPECT_FALSE(Model::create(params).has_value());
    params = {.tractor = vehicle::testing::reference_params(),
              .sled = sled::testing::reference_params(),
              .soil = tire::testing::reference_soil()};
    params.tractor.chassis.mass_kg = 0.0;
    EXPECT_EQ(validate(params), ParamError::kTractor);
    params = {.tractor = vehicle::testing::reference_params(),
              .sled = sled::testing::reference_params(),
              .soil = tire::testing::reference_soil()};
    params.soil.moisture = -0.1;
    EXPECT_EQ(validate(params), ParamError::kSoil);
}

TEST(Chain, PullsOnlyWhenStretched) {
    const sled::Chain chain = sled::testing::reference_params().chain;
    const ChainEnd hook{.x_m = 0.0, .z_m = 0.45};
    const ChainForce slack = chain_force(chain, {.x_m = 1.0, .z_m = 0.45}, hook);
    EXPECT_EQ(slack.tension_n, 0.0);
    const ChainForce taut = chain_force(chain, {.x_m = chain.length_m + 0.01, .z_m = 0.45}, hook);
    EXPECT_NEAR(taut.tension_n, chain.stiffness_n_per_m * 0.01, 1e-6);
    EXPECT_DOUBLE_EQ(taut.direction_x, 1.0);
    EXPECT_DOUBLE_EQ(taut.direction_z, 0.0);
}

// Stretched but closing fast, the damper would push: a chain never does.
TEST(Chain, NeverPushes) {
    const sled::Chain chain = sled::testing::reference_params().chain;
    const ChainForce closing =
        chain_force(chain, {.x_m = chain.length_m + 0.001, .z_m = 0.45, .velocity_x_m_s = -2.0},
                    {.x_m = 0.0, .z_m = 0.45});
    EXPECT_EQ(closing.tension_n, 0.0);
    const ChainForce opening =
        chain_force(chain, {.x_m = chain.length_m + 0.001, .z_m = 0.45, .velocity_x_m_s = 0.1},
                    {.x_m = 0.0, .z_m = 0.45});
    EXPECT_NEAR(opening.tension_n, chain.stiffness_n_per_m * 0.001 + chain.damping_n_s_per_m * 0.1,
                1e-6);
}

// The chain pulls along its own line: from a hitch above the hook it lifts
// the sled's front a little and pulls the hitch down. Nose-up on the
// wheelie bar, the hitch drops about 5 cm, level with the hook, and the
// chain runs flat.
TEST(Chain, RunsFromHookToHitch) {
    const Model model = reference_model();
    State state = initial_state(model);
    const chassis::Params& chassis = model.tractor().params().chassis;
    const double hook_x_m = state.sled.position_m + model.sled().hook_ahead_of_wheels_m;
    const auto direction_z = [&](double pitch_rad) {
        state.tractor.chassis.pitch_rad = pitch_rad;
        const chassis::Hitch hitch = chassis::hitch(chassis, state.tractor.chassis);
        return chain_force(model.sled().chain, {.x_m = hitch.x_m + 0.01, .z_m = hitch.z_m},
                           {.x_m = hook_x_m, .z_m = model.sled().hook_height_m})
            .direction_z;
    };
    EXPECT_NEAR(direction_z(0.0),
                (chassis.hitch_height_m - model.sled().hook_height_m) / model.sled().chain.length_m,
                1e-2);
    EXPECT_GT(direction_z(0.0), 0.0);
    EXPECT_LT(std::abs(direction_z(0.12)), 0.1 * direction_z(0.0));
}

// At the start line the chain lies slack: nothing pulls while the tractor
// idles in neutral.
TEST(Pull, StartsWithTheChainSlack) {
    Driver run;
    for (int i = 0; i < 3000; ++i) {
        run.last = step(run.state, run.model, {}, kDt_s);
        ASSERT_EQ(run.last.chain.tension_n, 0.0);
    }
    EXPECT_EQ(run.state.sled.distance_m, 0.0);
    EXPECT_EQ(run.state.tractor.chassis.position_m, 0.0);
}

// In its pulling gear the light class pulls on torque: the sled drags
// harder as the box moves forward, and after the push-down the engine lugs
// down and stalls with the tires still gripping, short of a full pull.
TEST(Pull, PullingGearStallsAfterThePushdown) {
    Driver run;
    run.drive(6, false);
    EXPECT_TRUE(run.stalled);
    const sled::Params& sled = run.model.sled();
    EXPECT_GT(run.state.sled.distance_m, sled.pushdown_start_m);
    EXPECT_LT(run.state.sled.distance_m, 100.0);
    EXPECT_GT(run.resistance_at_50_m_n, 1.2 * run.resistance_at_20_m_n);
    EXPECT_LT(run.last.tractor.slip_ratio, 0.2) << "below the grip peak at the stall";
}

// In a low gear the engine has torque to spare: the wheels break loose
// once the push-down loads the sled, and the run ends in wheelspin.
TEST(Pull, LowGearSpinsOutAfterThePushdown) {
    Driver run;
    run.drive(4, false);
    EXPECT_TRUE(run.spun_out);
    EXPECT_FALSE(run.stalled);
    EXPECT_GT(run.state.sled.distance_m, run.model.sled().pushdown_start_m);
}

// Dumping the clutch snaps the chain tight: a sharper peak than slipping
// the clutch in.
TEST(Pull, DumpingTheClutchSnapsTheChain) {
    Driver slipped;
    slipped.drive(5, false, 10.0);
    Driver dumped;
    dumped.drive(5, true, 10.0);
    EXPECT_GT(dumped.max_tension_n, 1.1 * slipped.max_tension_n);
}

// Tractor and sled move as one: the chain keeps them about its length
// apart all the way.
TEST(Pull, TheChainKeepsTractorAndSledTogether) {
    Driver run;
    run.drive(6, false, 20.0);
    const double gap_m = run.state.tractor.chassis.position_m - run.state.sled.position_m;
    const State start = initial_state(run.model);
    const double start_gap_m = start.tractor.chassis.position_m - start.sled.position_m;
    EXPECT_NEAR(gap_m, start_gap_m, 0.05);
}

std::uint64_t pull_hash() {
    Driver run;
    hash::Fnv1a hash;
    for (int i = 0; i < 20000; ++i) {
        const double t_s = i * kDt_s;
        const vehicle::Input input{
            .throttle = t_s < 0.5 ? 0.0 : 1.0,
            .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
            .gear = t_s < 0.5 ? driveline::kNeutral : 6};
        step(run.state, run.model, input, kDt_s);
        hash_append(hash, run.state);
    }
    return hash.value();
}

TEST(Pull, IsDeterministic) {
    EXPECT_EQ(pull_hash(), pull_hash());
}

// Regression hash; must match on every compiler, build type and platform.
TEST(Pull, MatchesRegressionHash) {
    EXPECT_EQ(pull_hash(), 0xd11a086930a2c267ULL);
}

}  // namespace
}  // namespace tp::pull
