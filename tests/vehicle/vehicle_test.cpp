#include "tp/core/vehicle/vehicle.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include "reference_vehicle.hpp"

namespace tp::vehicle {
namespace {

using testing::reference_model;
using testing::reference_params;

constexpr double kDt_s = 0.001;

// A driver: idle in neutral, select a gear, rev up with the clutch pressed,
// then slip the clutch in over 3 s or dump it, and pull at full throttle.
struct Pull {
    Model model = reference_model();
    State state = initial_state(model);
    Output last;
    double max_slip = 0.0;
    double min_front_n = 1e12;

    void run(int gear, double drawbar_n, bool dump, double seconds) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            const double t_s = i * kDt_s;
            Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                        .clutch_pedal = 1.0,
                        .gear = t_s < 0.5 ? driveline::kNeutral : gear};
            if (t_s >= 3.0) {
                input.clutch_pedal = dump ? 0.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0);
            }
            last = step(state, model, input, drawbar_n, kDt_s);
            EXPECT_FALSE(last.shift_refused);
            if (t_s >= 3.0) {
                max_slip = std::max(max_slip, last.slip_ratio);
            }
            min_front_n = std::min(min_front_n, last.axle_loads.front_n);
            if (!state.engine.running) {
                return;
            }
        }
    }
};

TEST(VehicleParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(VehicleParams, NamesTheInvalidPart) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.engine.inertia_kg_m2 = 0.0; }), ParamError::kEngine);
    EXPECT_EQ(with([](Params& p) { p.clutch.kinetic_ratio = 0.0; }), ParamError::kClutch);
    EXPECT_EQ(with([](Params& p) { p.driveline.gear_count = 0; }), ParamError::kDriveline);
    EXPECT_EQ(with([](Params& p) { p.rear_tire.radius_m = 0.0; }), ParamError::kRearTire);
    EXPECT_EQ(with([](Params& p) { p.rear_tire_count = 0; }), ParamError::kRearTireCount);
    EXPECT_EQ(with([](Params& p) { p.chassis.mass_kg = 0.0; }), ParamError::kChassis);
    EXPECT_EQ(with([](Params& p) { p.chassis.rear_axle_height_m = 0.7; }),
              ParamError::kRearAxleHeight);
    EXPECT_FALSE(Model::create([] {
                     Params p = reference_params();
                     p.chassis.mass_kg = 0.0;
                     return p;
                 }())
                     .has_value());
}

TEST(Vehicle, ModelPutsTheRearWheelsOnTheAxle) {
    const Model model = reference_model();
    const Params& p = model.params();
    EXPECT_DOUBLE_EQ(model.driveline().output_inertia_kg_m2,
                     p.driveline.output_inertia_kg_m2 + 2 * p.rear_tire.wheel_inertia_kg_m2);
}

TEST(Vehicle, IdlesAtRestInNeutral) {
    Pull pull;
    for (int i = 0; i < 3000; ++i) {
        pull.last = step(pull.state, pull.model, {}, 0.0, kDt_s);
    }
    EXPECT_TRUE(pull.state.engine.running);
    EXPECT_EQ(pull.state.chassis.position_m, 0.0);
    EXPECT_EQ(pull.state.driveline.output_omega_rad_s, 0.0);
}

TEST(Vehicle, RefusesToShiftWithTheClutchEngaged) {
    Pull pull;
    const Output out =
        step(pull.state, pull.model, {.throttle = 0.0, .clutch_pedal = 0.0, .gear = 3}, 0.0, kDt_s);
    EXPECT_TRUE(out.shift_refused);
    EXPECT_EQ(pull.state.driveline.gear, driveline::kNeutral);
}

// Pulling steadily, the tires find the slip at which traction balances the
// drawbar pull and the front tires' rolling resistance; the clutch is locked
// and road speed is wheel speed less the slip.
TEST(Vehicle, SteadyPullBalancesTractionAndResistance) {
    Pull pull;
    constexpr double kDrawbar_n = 5000.0;
    pull.run(4, kDrawbar_n, false, 15.0);
    ASSERT_TRUE(pull.state.engine.running);
    EXPECT_TRUE(pull.state.clutch.locked);
    const Params& p = pull.model.params();
    const double front_rolling_n =
        p.chassis.front_rolling_resistance_coefficient * pull.last.axle_loads.front_n;
    EXPECT_NEAR(pull.last.tire_force_n, kDrawbar_n + front_rolling_n, 1.0);
    EXPECT_GT(pull.last.slip_ratio, 0.0);
    EXPECT_LT(pull.last.slip_ratio, 0.1) << "well below the 25 % peak";
    const double wheel_m_s = pull.state.driveline.output_omega_rad_s * p.rear_tire.radius_m;
    EXPECT_NEAR(pull.state.chassis.velocity_m_s, wheel_m_s * (1.0 - pull.last.slip_ratio), 1e-3);
    EXPECT_NEAR(pull.state.engine.omega_rad_s, pull.state.driveline.input_omega_rad_s, 1e-9);
}

// Dumping the clutch at full revs spins the wheels far past the traction
// peak; they hook up again once the tractor is moving.
TEST(Vehicle, DumpingTheClutchSpinsTheWheels) {
    Pull dumped;
    dumped.run(3, 5000.0, true, 15.0);
    ASSERT_TRUE(dumped.state.engine.running);
    EXPECT_GT(dumped.max_slip, 0.5);
    EXPECT_LT(dumped.last.slip_ratio, 0.1);

    Pull slipped;
    slipped.run(3, 5000.0, false, 15.0);
    EXPECT_LT(slipped.max_slip, dumped.max_slip);
}

// A load the tires cannot hold: in a low gear the engine spins the wheels
// and the tractor stays where it is.
TEST(Vehicle, TooHeavyALoadSpinsTheWheelsInALowGear) {
    Pull pull;
    pull.run(1, 15000.0, false, 15.0);
    EXPECT_TRUE(pull.state.engine.running);
    EXPECT_LT(pull.state.chassis.position_m, 0.1);
    EXPECT_DOUBLE_EQ(pull.last.slip_ratio, 1.0);
}

// In top gear the tires grip better than the engine can turn them: the
// load stalls the engine.
TEST(Vehicle, TooHeavyALoadStallsTheEngineInTopGear) {
    Pull pull;
    pull.run(8, 10000.0, false, 15.0);
    EXPECT_FALSE(pull.state.engine.running);
}

TEST(Vehicle, PullingLoadsTheRearAxle) {
    Pull light;
    light.run(4, 2000.0, false, 15.0);
    Pull heavy;
    heavy.run(4, 10000.0, false, 15.0);
    ASSERT_TRUE(heavy.state.engine.running);
    EXPECT_GT(heavy.last.axle_loads.rear_n, light.last.axle_loads.rear_n);
}

std::uint64_t pull_hash() {
    Pull pull;
    hash::Fnv1a hash;
    for (int i = 0; i < 15000; ++i) {
        const double t_s = i * kDt_s;
        const Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                          .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
                          .gear = t_s < 0.5 ? driveline::kNeutral : 3};
        step(pull.state, pull.model, input, 8000.0, kDt_s);
        hash_append(hash, pull.state);
    }
    return hash.value();
}

TEST(Vehicle, PullIsDeterministic) {
    EXPECT_EQ(pull_hash(), pull_hash());
}

// Regression hash; must match on every compiler, build type and platform.
// Update deliberately, with a note in the commit, when the model changes.
TEST(Vehicle, PullMatchesRegressionHash) {
    EXPECT_EQ(pull_hash(), 0x85e14ca8fc4b4949ULL);
}

}  // namespace
}  // namespace tp::vehicle
