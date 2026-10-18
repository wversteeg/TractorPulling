#include "tp/core/vehicle/vehicle.hpp"

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/units/constants.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "reference_vehicle.hpp"

namespace tp::vehicle {
namespace {

using testing::reference_ground;
using testing::reference_model;
using testing::reference_params;

constexpr double kDt_s = 0.001;
const Ground kGround = reference_ground();

// A driver: idle in neutral, select a gear, rev up with the clutch pressed,
// then slip the clutch in over 3 s or dump it, and pull at full throttle.
struct Pull {
    Model model = reference_model();
    State state = initial_state(model);
    Ground ground = kGround;
    Output last;
    double max_slip = 0.0;
    double min_front_n = 1e12;
    double max_pitch_rad = 0.0;

    void run(int gear, double drawbar_n, bool dump, double seconds) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            const double t_s = i * kDt_s;
            Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                        .clutch_pedal = 1.0,
                        .gear = t_s < 0.5 ? driveline::kNeutral : gear};
            if (t_s >= 3.0) {
                input.clutch_pedal = dump ? 0.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0);
            }
            last = step(state, model, input, ground, {.pull_n = drawbar_n}, kDt_s);
            EXPECT_FALSE(last.shift_refused);
            if (t_s >= 3.0) {
                max_slip = std::max(max_slip, last.slip_ratio);
            }
            min_front_n = std::min(min_front_n, last.axle_loads.front_n);
            max_pitch_rad = std::max(max_pitch_rad, state.chassis.pitch_rad);
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
    EXPECT_EQ(with([](Params& p) { p.rear_tire.size.radius_m = 0.0; }), ParamError::kRearTire);
    EXPECT_EQ(with([](Params& p) { p.rear_tire_count = 0; }), ParamError::kRearTireCount);
    EXPECT_EQ(with([](Params& p) { p.front_tire.section_width_m = 0.0; }), ParamError::kFrontTire);
    EXPECT_EQ(with([](Params& p) { p.chassis.mass_kg = 0.0; }), ParamError::kChassis);
    EXPECT_EQ(with([](Params& p) { p.chassis.rear_axle_height_m = 0.7; }),
              ParamError::kRearAxleHeight);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = 0.0; }),
              ParamError::kPtoEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = 1.1; }),
              ParamError::kPtoEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = std::nan(""); }),
              ParamError::kPtoEfficiencyOutOfRange);
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
        pull.last = step(pull.state, pull.model, {}, kGround, {}, kDt_s);
    }
    EXPECT_TRUE(pull.state.engine.running);
    EXPECT_EQ(pull.state.chassis.position_m, 0.0);
    EXPECT_EQ(pull.state.driveline.output_omega_rad_s, 0.0);
    EXPECT_NEAR(pull.state.chassis.pitch_rad, 0.0, 1e-12);
}

TEST(Vehicle, RefusesToShiftWithTheClutchEngaged) {
    Pull pull;
    const Output out = step(pull.state, pull.model,
                            {.throttle = 0.0, .clutch_pedal = 0.0, .gear = 3}, kGround, {}, kDt_s);
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
        pull.last.front_rolling_resistance * pull.last.axle_loads.front_n;
    EXPECT_NEAR(pull.last.tire_force_n, kDrawbar_n + front_rolling_n, 1.0);
    EXPECT_GT(pull.last.slip_ratio, 0.0);
    EXPECT_LT(pull.last.slip_ratio, 0.1) << "well below the 28 % peak";
    const double wheel_m_s = pull.state.driveline.output_omega_rad_s * p.rear_tire.size.radius_m;
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

// A load the tires cannot hold (beyond μ 0.91 of the loaded rear axle): in a
// low gear the engine spins the wheels and the tractor stays where it is.
TEST(Vehicle, TooHeavyALoadSpinsTheWheelsInALowGear) {
    Pull pull;
    pull.run(1, 26000.0, false, 15.0);
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

// Pulling steadily, the front axle carries its static share less the
// moments about the rear contact patch: the drawbar pull at hitch height and
// the rear tires' rolling resistance (a forward shift of their load).
TEST(Vehicle, SteadyPullUnloadsTheFrontByItsMoments) {
    Pull pull;
    constexpr double kDrawbar_n = 5000.0;
    pull.run(4, kDrawbar_n, false, 15.0);
    ASSERT_TRUE(pull.state.engine.running);
    const Params& p = pull.model.params();
    const chassis::AxleLoads& loads = pull.last.axle_loads;
    const double weight_n = p.chassis.mass_kg * units::kStandardGravity_m_s2;
    const double rolling_nm =
        pull.last.rear_rolling_resistance * loads.rear_n * p.rear_tire.size.radius_m;
    const double expected_front_n =
        weight_n * p.chassis.cg_ahead_of_rear_axle_m / p.chassis.wheelbase_m -
        (kDrawbar_n * p.chassis.hitch_height_m + rolling_nm) / p.chassis.wheelbase_m;
    EXPECT_NEAR(loads.front_n, expected_front_n, 20.0);
    EXPECT_GT(pull.state.chassis.pitch_rad, 0.0) << "squats onto the rear";
    EXPECT_NEAR(loads.rear_n + loads.front_n, weight_n, 1.0);
}

// Dumping the clutch in a low gear throws the front up: the wheels spin up
// against the tires' grip and the axle torque's reaction briefly outweighs
// the tractor. Slipping the clutch in keeps the front on the ground. (Holding
// a wheelie takes more: a traction torque above the weight's moment about
// the rear axle, μ r > b, so μ > 1.2 here.)
TEST(Vehicle, DumpingTheClutchInALowGearLiftsTheFront) {
    Pull dumped;
    dumped.run(2, 8000.0, true, 15.0);
    ASSERT_TRUE(dumped.state.engine.running);
    EXPECT_EQ(dumped.min_front_n, 0.0);

    Pull slipped;
    slipped.run(2, 8000.0, false, 15.0);
    EXPECT_GT(slipped.min_front_n, 1000.0);
    EXPECT_GT(dumped.max_pitch_rad, slipped.max_pitch_rad);
}

// How a pull against a slowly growing load ends: like a sled whose weight
// box moves forward, the drawbar pull rises steadily after the launch.
struct GrowingLoad {
    bool stalled = false;
    bool spun_out = false;
    double max_slip = 0.0;
    double load_n = 0.0;
};

GrowingLoad pull_growing_load(int gear) {
    constexpr double kStartLoad_n = 3000.0;
    constexpr double kLoadRate_n_per_s = 500.0;
    constexpr double kRiseFrom_s = 6.0;
    constexpr double kSpunOutSlip = 0.9;
    const Model model = reference_model();
    State state = initial_state(model);
    GrowingLoad result;
    for (int i = 0; i < 120000; ++i) {
        const double t_s = i * kDt_s;
        const Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                          .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
                          .gear = t_s < 0.5 ? driveline::kNeutral : gear};
        result.load_n = kStartLoad_n + std::max(0.0, t_s - kRiseFrom_s) * kLoadRate_n_per_s;
        const Output out = step(state, model, input, kGround, {.pull_n = result.load_n}, kDt_s);
        if (t_s > kRiseFrom_s + 2.0) {
            result.max_slip = std::max(result.max_slip, out.slip_ratio);
        }
        if (!state.engine.running) {
            result.stalled = true;
            break;
        }
        if (out.slip_ratio > kSpunOutSlip && t_s > kRiseFrom_s) {
            result.spun_out = true;
            break;
        }
    }
    return result;
}

// The light class pulls on engine torque: in its pulling gears the engine
// lugs down and stalls while the tires still grip (slip below the 28 % peak).
// In a low gear the engine multiplies enough torque to break the tires loose
// first, and the pull ends in wheelspin. Choosing the gear matters.
TEST(Vehicle, LightClassPullsOnTorqueInItsPullingGears) {
    // Where traction peaks on the reference track (see the tire tests).
    constexpr double kPeakSlip = 0.28;
    for (const int gear : {6, 7}) {
        const GrowingLoad pull = pull_growing_load(gear);
        EXPECT_TRUE(pull.stalled) << "gear " << gear;
        EXPECT_LT(pull.max_slip, kPeakSlip) << "gear " << gear;
    }
    const GrowingLoad low = pull_growing_load(4);
    EXPECT_TRUE(low.spun_out);
    EXPECT_FALSE(low.stalled);
}

// On softer ground both axles sink in further: the same drawbar pull costs
// more rolling resistance, so the rear tires work harder for it.
TEST(Vehicle, SofterGroundRollsHarder) {
    Pull firm;
    firm.run(4, 5000.0, false, 15.0);
    Pull soft;
    soft.ground.rear.cone_index_pa *= 0.3;
    soft.ground.front.cone_index_pa *= 0.3;
    soft.run(4, 5000.0, false, 15.0);
    ASSERT_TRUE(firm.state.engine.running);
    ASSERT_TRUE(soft.state.engine.running);
    EXPECT_GT(soft.last.front_rolling_resistance, firm.last.front_rolling_resistance);
    EXPECT_GT(soft.last.rear_rolling_resistance, firm.last.rear_rolling_resistance);
    EXPECT_GT(soft.last.tire_force_n, firm.last.tire_force_n);
    EXPECT_GT(soft.last.slip_ratio, firm.last.slip_ratio);
}

// Pulling with grip throws back a little soil for every metre gained;
// spinning in place throws it back without gaining any ground.
TEST(Vehicle, SpinningThrowsSoilBack) {
    Pull gripping;
    gripping.run(4, 5000.0, false, 15.0);
    const double per_metre_m3 =
        gripping.last.thrown_soil_m3_s / gripping.state.chassis.velocity_m_s;
    EXPECT_GT(per_metre_m3, 0.0);

    Pull spinning;
    spinning.run(1, 26000.0, false, 15.0);
    ASSERT_DOUBLE_EQ(spinning.last.slip_ratio, 1.0);
    EXPECT_LT(spinning.state.chassis.position_m, 0.1);
    EXPECT_GT(spinning.last.thrown_soil_m3_s, 0.5 * gripping.last.thrown_soil_m3_s);
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
        step(pull.state, pull.model, input, kGround, {.pull_n = 8000.0}, kDt_s);
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
    EXPECT_EQ(pull_hash(), 0xdcf03755977420c1ULL);
}

}  // namespace
}  // namespace tp::vehicle
