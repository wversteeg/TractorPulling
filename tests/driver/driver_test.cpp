#include "tp/core/driver/driver.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "../vehicle/reference_vehicle.hpp"

namespace tp::driver {
namespace {

constexpr Plan kPlan{.gear = 6, .select_gear_s = 0.5, .launch_s = 3.0, .clutch_slip_in_s = 2.0};

TEST(DriverPlan, RejectsNonsense) {
    EXPECT_EQ(validate(kPlan, 8), std::nullopt);
    Plan p = kPlan;
    p.gear = 0;
    EXPECT_EQ(validate(p, 8), ParamError::kGearOutOfRange);
    p.gear = 9;
    EXPECT_EQ(validate(p, 8), ParamError::kGearOutOfRange);
    p = kPlan;
    p.launch_s = 0.2;
    EXPECT_EQ(validate(p, 8), ParamError::kTimesOutOfOrder);
    p.launch_s = std::nan("");
    EXPECT_EQ(validate(p, 8), ParamError::kNotFinite);
    for (const double share : {-0.1, 1.0, std::nan("")}) {
        p = kPlan;
        p.downshift_share = share;
        EXPECT_EQ(validate(p, 8), ParamError::kDownshiftShareOutOfRange) << share;
    }
}

// The input at `t_s` with the tractor as it stands still in 6th, the engine
// at its rated speed: what a time-only plan sees.
vehicle::Input input(const Plan& plan, double t_s) {
    const vehicle::Params params = vehicle::testing::reference_params();
    vehicle::State tractor;
    tractor.engine.running = true;
    tractor.engine.omega_rad_s = params.engine.rated_omega_rad_s;
    tractor.driveline.gear = plan.gear;
    State state;
    return input(plan, state, t_s, tractor, params);
}

// Neutral at first, then the gear with the clutch pressed at full throttle,
// then the clutch coming up linearly until it is fully engaged.
TEST(Driver, SelectsTheGearRevsUpAndLetsTheClutchIn) {
    const vehicle::Input idle = input(kPlan, 0.2);
    EXPECT_EQ(idle.gear, driveline::kNeutral);
    EXPECT_EQ(idle.throttle, 0.0);
    EXPECT_EQ(idle.clutch_pedal, 1.0);

    const vehicle::Input revving = input(kPlan, 2.0);
    EXPECT_EQ(revving.gear, 6);
    EXPECT_EQ(revving.throttle, 1.0);
    EXPECT_EQ(revving.clutch_pedal, 1.0);

    EXPECT_DOUBLE_EQ(input(kPlan, 4.0).clutch_pedal, 0.5);
    EXPECT_EQ(input(kPlan, 5.0).clutch_pedal, 0.0);
    EXPECT_EQ(input(kPlan, 60.0).clutch_pedal, 0.0);
    EXPECT_EQ(input(kPlan, 60.0).throttle, 1.0);
}

TEST(Driver, ZeroSlipInDumpsTheClutch) {
    Plan dump = kPlan;
    dump.clutch_slip_in_s = 0.0;
    EXPECT_EQ(input(dump, 2.999).clutch_pedal, 1.0);
    EXPECT_EQ(input(dump, 3.0).clutch_pedal, 0.0);
}

// With the clutch fully in, the driver waits for the engine to come back
// from the launch; when the sled then pulls it below the share, they shift
// one gear down, once, and stay in it.
TEST(Driver, ShiftsDownOnceAsTheSledPullsTheEngineDown) {
    const vehicle::Params params = vehicle::testing::reference_params();
    Plan plan = kPlan;
    plan.downshift_share = 0.9;
    const double rated_rad_s = params.engine.rated_omega_rad_s;
    vehicle::State tractor;
    tractor.engine.running = true;
    tractor.driveline.gear = 6;
    State state;
    const auto gear_at = [&](double t_s, double share_of_rated) {
        tractor.engine.omega_rad_s = share_of_rated * rated_rad_s;
        return input(plan, state, t_s, tractor, params).gear;
    };
    EXPECT_EQ(gear_at(4.0, 0.6), 6) << "the clutch is still coming in";
    EXPECT_EQ(gear_at(6.0, 0.6), 6) << "the launch's dip";
    EXPECT_FALSE(state.watching);
    EXPECT_EQ(gear_at(8.0, 1.0), 6);
    EXPECT_TRUE(state.watching);
    EXPECT_EQ(gear_at(20.0, 0.91), 6);
    EXPECT_EQ(gear_at(25.0, 0.89), 5) << "lugging";
    tractor.driveline.gear = 5;
    tractor.driveline.load_shifts = 1;
    EXPECT_EQ(gear_at(26.0, 0.8), 5) << "once";
    EXPECT_EQ(gear_at(27.0, 1.0), 5) << "and no shift back up";

    // A driver who never shifts, or a gearbox that cannot, stays in 6th.
    const Plan never = kPlan;
    State fresh{.watching = true};
    tractor.driveline = {.gear = 6};
    tractor.engine.omega_rad_s = 0.5 * rated_rad_s;
    EXPECT_EQ(input(never, fresh, 25.0, tractor, params).gear, 6);
    vehicle::Params plain = params;
    plain.driveline.load_downshift_gears = 0;
    EXPECT_EQ(input(plan, fresh, 25.0, tractor, plain).gear, 6);
    tractor.engine.running = false;
    EXPECT_EQ(input(plan, fresh, 25.0, tractor, params).gear, 6) << "stalled";
}

}  // namespace
}  // namespace tp::driver
