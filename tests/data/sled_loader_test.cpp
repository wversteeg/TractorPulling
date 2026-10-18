#include "tp/data/sled/sled_loader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

#include "../sled/reference_sled.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidSled = R"(
[sled]
frame_mass_kg = 3000.0
frame_cg_ahead_of_wheels_m = 0.8
pan_ahead_of_wheels_m = 3.0
pan_friction_coefficient = 0.55
wheel_rolling_resistance_coefficient = 0.04

[sled.hook]
ahead_of_wheels_m = 3.2
height_m = 0.5

[sled.weight_box]
mass_kg = 2500.0
start_ahead_of_wheels_m = 0.2
end_ahead_of_wheels_m = 2.8
travel_distance_m = 90.0

[sled.pushdown]
start_m = 80.0
ramp_m = 1.5

[sled.chain]
length_m = 1.2
stiffness_n_per_m = 1.5e6
damping_n_s_per_m = 10000.0

[sled.pull_off]
extra_box_mass_kg = 400.0
pushdown_earlier_m = 15.0
extra_pan_friction = 0.04
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidSled};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

// The shipped sled and the parameters the core tests use must agree.
TEST(SledLoader, ShippedFileMatchesTestReference) {
    const auto result = load_sled(std::string{TP_DATA_DIR} + "/sleds/light_class.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const sled::Params& loaded = result.value();
    const sled::Params reference = sled::testing::reference_params();
    EXPECT_EQ(loaded.frame_mass_kg, reference.frame_mass_kg);
    EXPECT_EQ(loaded.frame_cg_ahead_of_wheels_m, reference.frame_cg_ahead_of_wheels_m);
    EXPECT_EQ(loaded.pan_ahead_of_wheels_m, reference.pan_ahead_of_wheels_m);
    EXPECT_EQ(loaded.hook_ahead_of_wheels_m, reference.hook_ahead_of_wheels_m);
    EXPECT_EQ(loaded.hook_height_m, reference.hook_height_m);
    EXPECT_EQ(loaded.box_mass_kg, reference.box_mass_kg);
    EXPECT_EQ(loaded.box_start_ahead_of_wheels_m, reference.box_start_ahead_of_wheels_m);
    EXPECT_EQ(loaded.box_end_ahead_of_wheels_m, reference.box_end_ahead_of_wheels_m);
    EXPECT_EQ(loaded.box_travel_distance_m, reference.box_travel_distance_m);
    EXPECT_EQ(loaded.pushdown_start_m, reference.pushdown_start_m);
    EXPECT_EQ(loaded.pushdown_ramp_m, reference.pushdown_ramp_m);
    EXPECT_EQ(loaded.pan_friction_coefficient, reference.pan_friction_coefficient);
    EXPECT_EQ(loaded.wheel_rolling_resistance_coefficient,
              reference.wheel_rolling_resistance_coefficient);
    EXPECT_EQ(loaded.chain.length_m, reference.chain.length_m);
    EXPECT_EQ(loaded.chain.stiffness_n_per_m, reference.chain.stiffness_n_per_m);
    EXPECT_EQ(loaded.chain.damping_n_s_per_m, reference.chain.damping_n_s_per_m);
    EXPECT_EQ(loaded.pull_off.extra_box_mass_kg, reference.pull_off.extra_box_mass_kg);
    EXPECT_EQ(loaded.pull_off.pushdown_earlier_m, reference.pull_off.pushdown_earlier_m);
    EXPECT_EQ(loaded.pull_off.extra_pan_friction, reference.pull_off.extra_pan_friction);
}

TEST(SledLoader, ParsesValues) {
    const auto result = parse_sled(kValidSled, "sled.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().pushdown_start_m, 80.0);
    EXPECT_DOUBLE_EQ(result.value().box_start_ahead_of_wheels_m, 0.2);
    EXPECT_DOUBLE_EQ(result.value().chain.length_m, 1.2);
    EXPECT_DOUBLE_EQ(result.value().hook_height_m, 0.5);
    EXPECT_DOUBLE_EQ(result.value().pull_off.pushdown_earlier_m, 15.0);
}

TEST(SledLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 6> cases = {{
        {"extra_box_mass_kg = 400.0", "extra_box_mass_kg = -1.0",
         "sled.pull_off: all values must be >= 0"},
        {"frame_mass_kg = 3000.0", "frame_mass_kg = -1.0", "sled.frame_mass_kg: must be > 0"},
        {"end_ahead_of_wheels_m = 2.8", "end_ahead_of_wheels_m = 3.5",
         "sled.weight_box: must stay between the wheels and the pan"},
        {"ramp_m = 1.5", "ramp_m = 0.0", "sled.pushdown.ramp_m: must be > 0"},
        {"length_m = 1.2", "length_m = 0.0", "sled.chain: length and stiffness must be > 0"},
        {"start_m = 80.0", "", "sled.pushdown.start_m: missing or not a number"},
    }};
    for (const Case& c : cases) {
        const auto result = parse_sled(with(c.from, c.to), "sled.toml");
        ASSERT_FALSE(result.ok()) << c.from;
        EXPECT_NE(result.error().message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << result.error().message;
    }
}

TEST(SledLoader, ReportsMissingFile) {
    const auto result = load_sled("/nonexistent/sled.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/sled.toml"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
