#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <numbers>

namespace tp::units {
namespace {

TEST(Conversions, RpmToRadiansPerSecond) {
    EXPECT_DOUBLE_EQ(rpm_to_rad_s(60.0), 2.0 * std::numbers::pi);
    EXPECT_DOUBLE_EQ(rad_s_to_rpm(rpm_to_rad_s(2200.0)), 2200.0);
}

TEST(Conversions, RadiansToDegrees) {
    EXPECT_DOUBLE_EQ(rad_to_deg(std::numbers::pi), 180.0);
    EXPECT_DOUBLE_EQ(rad_to_deg(-std::numbers::pi / 2.0), -90.0);
}

// 1 metric hp lifts 75 kg by 1 m in 1 s.
TEST(Conversions, MetricHorsepowerToWatts) {
    EXPECT_DOUBLE_EQ(metric_hp_to_w(1.0), 75.0 * 9.80665);
    EXPECT_DOUBLE_EQ(w_to_metric_hp(metric_hp_to_w(85.0)), 85.0);
}

}  // namespace
}  // namespace tp::units
