#include "tp/core/driver/driver.hpp"

#include <gtest/gtest.h>

#include <cmath>

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

}  // namespace
}  // namespace tp::driver
