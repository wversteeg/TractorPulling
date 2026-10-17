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

}  // namespace
}  // namespace tp::units
