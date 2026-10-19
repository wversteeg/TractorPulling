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
frame_yaw_inertia_kg_m2 = 4500.0
pan_ahead_of_wheels_m = 3.0
pan_friction_coefficient = 0.55
pan_area_m2 = 2.0
pan_plough_coefficient = 4.0

[sled.wheels]
count = 4
radius_m = 0.45
section_width_m = 0.3
section_height_m = 0.25
deflection_m = 0.03

[sled.wheels.cornering]
stiffness_per_rad = 3.5
limit = 0.7
mobility_rate = 0.12
relaxation_length_m = 0.45

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

[sled.knives]
count = 3
depth_m = 0.05
width_m = 0.04

[sled.berm]
collected_share = 0.2
spill_distance_m = 30.0
friction_coefficient = 0.7

[sled.pull_off_step]
box_mass_kg = 100.0
knives = 2
knife_depth_m = 0.01
pushdown_earlier_m = 3.0
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
    EXPECT_EQ(loaded.pan_area_m2, reference.pan_area_m2);
    EXPECT_EQ(loaded.pan_plough_coefficient, reference.pan_plough_coefficient);
    EXPECT_EQ(loaded.wheel.radius_m, reference.wheel.radius_m);
    EXPECT_EQ(loaded.wheel.section_width_m, reference.wheel.section_width_m);
    EXPECT_EQ(loaded.wheel.section_height_m, reference.wheel.section_height_m);
    EXPECT_EQ(loaded.wheel.deflection_m, reference.wheel.deflection_m);
    EXPECT_EQ(loaded.wheel_count, reference.wheel_count);
    EXPECT_EQ(loaded.frame_yaw_inertia_kg_m2, reference.frame_yaw_inertia_kg_m2);
    EXPECT_EQ(loaded.wheel_cornering.stiffness_per_rad,
              reference.wheel_cornering.stiffness_per_rad);
    EXPECT_EQ(loaded.wheel_cornering.limit, reference.wheel_cornering.limit);
    EXPECT_EQ(loaded.wheel_cornering.mobility_rate, reference.wheel_cornering.mobility_rate);
    EXPECT_EQ(loaded.wheel_cornering.relaxation_length_m,
              reference.wheel_cornering.relaxation_length_m);
    EXPECT_EQ(loaded.chain.length_m, reference.chain.length_m);
    EXPECT_EQ(loaded.chain.stiffness_n_per_m, reference.chain.stiffness_n_per_m);
    EXPECT_EQ(loaded.chain.damping_n_s_per_m, reference.chain.damping_n_s_per_m);
    EXPECT_EQ(loaded.knives.count, reference.knives.count);
    EXPECT_EQ(loaded.knives.depth_m, reference.knives.depth_m);
    EXPECT_EQ(loaded.knives.width_m, reference.knives.width_m);
    EXPECT_EQ(loaded.berm.collected_share, reference.berm.collected_share);
    EXPECT_EQ(loaded.berm.spill_distance_m, reference.berm.spill_distance_m);
    EXPECT_EQ(loaded.berm.friction_coefficient, reference.berm.friction_coefficient);
    EXPECT_EQ(loaded.pull_off_step.box_mass_kg, reference.pull_off_step.box_mass_kg);
    EXPECT_EQ(loaded.pull_off_step.knives, reference.pull_off_step.knives);
    EXPECT_EQ(loaded.pull_off_step.knife_depth_m, reference.pull_off_step.knife_depth_m);
    EXPECT_EQ(loaded.pull_off_step.pushdown_earlier_m, reference.pull_off_step.pushdown_earlier_m);
}

TEST(SledLoader, ParsesValues) {
    const auto result = parse_sled(kValidSled, "sled.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().pushdown_start_m, 80.0);
    EXPECT_DOUBLE_EQ(result.value().box_start_ahead_of_wheels_m, 0.2);
    EXPECT_DOUBLE_EQ(result.value().chain.length_m, 1.2);
    EXPECT_DOUBLE_EQ(result.value().hook_height_m, 0.5);
    EXPECT_DOUBLE_EQ(result.value().berm.collected_share, 0.2);
    EXPECT_DOUBLE_EQ(result.value().berm.spill_distance_m, 30.0);
    EXPECT_DOUBLE_EQ(result.value().berm.friction_coefficient, 0.7);
    EXPECT_EQ(result.value().knives.count, 3);
    EXPECT_DOUBLE_EQ(result.value().knives.depth_m, 0.05);
    EXPECT_DOUBLE_EQ(result.value().knives.width_m, 0.04);
    EXPECT_DOUBLE_EQ(result.value().pull_off_step.box_mass_kg, 100.0);
    EXPECT_EQ(result.value().pull_off_step.knives, 2);
    EXPECT_DOUBLE_EQ(result.value().pull_off_step.knife_depth_m, 0.01);
    EXPECT_DOUBLE_EQ(result.value().pull_off_step.pushdown_earlier_m, 3.0);
    EXPECT_DOUBLE_EQ(result.value().pan_area_m2, 2.0);
    EXPECT_DOUBLE_EQ(result.value().pan_plough_coefficient, 4.0);
    EXPECT_DOUBLE_EQ(result.value().wheel.radius_m, 0.45);
    EXPECT_DOUBLE_EQ(result.value().wheel.section_width_m, 0.3);
    EXPECT_DOUBLE_EQ(result.value().wheel.section_height_m, 0.25);
    EXPECT_DOUBLE_EQ(result.value().wheel.deflection_m, 0.03);
    EXPECT_EQ(result.value().wheel_count, 4);
    EXPECT_DOUBLE_EQ(result.value().frame_yaw_inertia_kg_m2, 4500.0);
    EXPECT_DOUBLE_EQ(result.value().wheel_cornering.stiffness_per_rad, 3.5);
    EXPECT_DOUBLE_EQ(result.value().wheel_cornering.limit, 0.7);
    EXPECT_DOUBLE_EQ(result.value().wheel_cornering.mobility_rate, 0.12);
    EXPECT_DOUBLE_EQ(result.value().wheel_cornering.relaxation_length_m, 0.45);
}

TEST(SledLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 18> cases = {{
        {"frame_yaw_inertia_kg_m2 = 4500.0", "frame_yaw_inertia_kg_m2 = 0.0",
         "sled.frame_yaw_inertia_kg_m2: must be > 0"},
        {"limit = 0.7", "limit = 3.0", "sled.wheels.cornering: stiffness_per_rad"},
        {"relaxation_length_m = 0.45", "", "sled.wheels.cornering.relaxation_length_m: missing"},
        {"collected_share = 0.2", "collected_share = 1.5",
         "sled.berm: collected_share 0 to 1, spill_distance_m > 0"},
        {"spill_distance_m = 30.0", "", "sled.berm.spill_distance_m: missing or not a number"},
        {"count = 3", "count = 30", "sled.knives.count: must be 0 to 24"},
        {"depth_m = 0.05", "depth_m = 0.5", "sled.knives: count 0 to 24, depth 0 to 0.15 m"},
        {"knives = 2", "knives = -1", "sled.pull_off_step.knives: must be 0 to 24"},
        {"pan_area_m2 = 2.0", "pan_area_m2 = 0.0", "sled.pan_area_m2: must be > 0"},
        {"pan_plough_coefficient = 4.0", "pan_plough_coefficient = -1.0",
         "sled.pan_plough_coefficient: must not be negative"},
        {"count = 4", "count = 0", "sled.wheels.count: must be 1 to 8"},
        {"deflection_m = 0.03", "deflection_m = 0.3",
         "sled.wheels.deflection_m: must be at least 0 and less than the section height"},
        {"box_mass_kg = 100.0", "box_mass_kg = -1.0",
         "sled.pull_off_step: must make the sled heavier"},
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
