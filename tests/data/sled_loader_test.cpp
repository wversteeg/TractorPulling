#include "tp/data/sled/sled_loader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <map>
#include <optional>
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
rolling_losses = 0.04

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
max_mass_kg = 11000.0
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

// The core tests' light sled (tests/sled/reference_sled.hpp) as a sled
// file: 3.2 t on two wheels, as the light class pulled before the 15 t
// sled (data/sleds/pulling_sled.toml). The loader reads it to the
// parameters the core tests use.
constexpr std::string_view kTestSled = R"(
[sled]
frame_mass_kg = 3200.0
frame_cg_ahead_of_wheels_m = 0.75
frame_yaw_inertia_kg_m2 = 5000.0     # about the frame's own centre of gravity; m L² / 12, L ≈ 4.3 m
pan_ahead_of_wheels_m = 3.0          # centre of the pan's contact
pan_friction_coefficient = 0.46      # steel sliding on a firm track
pan_area_m2 = 1.8                    # the pan's contact with the track
pan_plough_coefficient = 5.0         # extra drag per unit of pressure / cone index

# The rear wheels (16.9-24 class) roll as towed tires on the track's soil.
[sled.wheels]
count = 2
radius_m = 0.55
section_width_m = 0.43
section_height_m = 0.35
deflection_m = 0.04
rolling_losses = 0.04               # the tire's own losses, on top of sinking in

# The wheels' side grip (see tp::tire::Cornering): they keep the sled's
# tail in line; the pan, sliding forward, resists sideways far less, so a
# sled swings behind a tractor that wanders (the dog-tail). Starting values
# to tune, as for the tractor's tires.
[sled.wheels.cornering]
stiffness_per_rad = 4.0
limit = 0.8
mobility_rate = 0.1
relaxation_length_m = 0.55           # about the wheel's radius

[sled.hook]
ahead_of_wheels_m = 4.0               # on a drawbar ahead of the pan, fenders either side
height_m = 0.45                      # 0.40 to 0.50 m, depending on the sled

# Rubber fenders either side of the drawbar: a tractor backing up stops
# with its rear tires against them (their faces 0.1 m behind the hook),
# with its hitch within 1 m of the sled's line, up to 1 m past them.
[sled.fenders]
behind_hook_m = 0.1
half_span_m = 1.0
depth_m = 1.0

[sled.weight_box]
max_mass_kg = 11000.0                # what the box holds at most
mass_kg = 1500.0
start_ahead_of_wheels_m = -0.5       # behind the rear wheels: its weight lifts the pan for the start
end_ahead_of_wheels_m = 3.0          # over the pan
travel_distance_m = 88.0             # sled travel to move it from start to end: at its stop as the push-down is fully set

# Engages at a set distance on the track, chosen per event from the test
# pull and the track's state (for example 70 or 80 m).
[sled.pushdown]
start_m = 78.0
ramp_m = 10.0                        # travel until the wheels are fully off

[sled.chain]
length_m = 1.15                      # 1.1 to 1.2 m
stiffness_n_per_m = 2.0e6            # chain, shackles and hitch
damping_n_s_per_m = 80000.0          # about half of critical for a 2.5 to 4.5 t tractor
# on the held sled: links and shackles take a jolt's energy, so a tractor
# that creeps into the chain is not sprung back off it like a rubber band

# Knives under the pan cut grooves in the track; the sled operator sets
# how many and how deep.
[sled.knives]
count = 4
depth_m = 0.04
width_m = 0.05

# Soil the tractor's tires throw back piles up in front of the pan and has
# to be pushed along; it spills off to the sides as the sled moves.
[sled.berm]
collected_share = 0.15               # of what the rear tires throw back
spill_distance_m = 40.0              # the berm spills its own size over this travel
friction_coefficient = 0.6           # soil sliding on soil

# After the full pulls, the qualifiers pull off on a heavier sled: as many
# of these steps as the sled operator judges they need. One step costs the
# reference tractor about 5 m.
[sled.pull_off_step]
box_mass_kg = 0.0
knives = 1
knife_depth_m = 0.0
pushdown_earlier_m = 0.0
)";

TEST(SledLoader, TestSledTextMatchesTestReference) {
    const auto result = parse_sled(kTestSled, "test_sled.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const sled::Params& loaded = result.value();
    const sled::Params reference = sled::testing::reference_params();
    EXPECT_EQ(loaded.frame_mass_kg, reference.frame_mass_kg);
    EXPECT_EQ(loaded.frame_cg_ahead_of_wheels_m, reference.frame_cg_ahead_of_wheels_m);
    EXPECT_EQ(loaded.pan_ahead_of_wheels_m, reference.pan_ahead_of_wheels_m);
    EXPECT_EQ(loaded.hook_ahead_of_wheels_m, reference.hook_ahead_of_wheels_m);
    EXPECT_EQ(loaded.hook_height_m, reference.hook_height_m);
    EXPECT_EQ(loaded.box_mass_kg, reference.box_mass_kg);
    EXPECT_EQ(loaded.box_max_mass_kg, reference.box_max_mass_kg);
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
    EXPECT_EQ(loaded.wheel_rolling_losses, reference.wheel_rolling_losses);
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
    // Without a [sled.fenders] table: no fenders.
    EXPECT_EQ(result.value().fenders.half_span_m, 0.0);
}

// Fenders, when given, are read and checked.
TEST(SledLoader, ReadsAndChecksTheFenders) {
    const std::string with_fenders = std::string{kValidSled} +
                                     "\n[sled.fenders]\nbehind_hook_m = 0.1\n"
                                     "half_span_m = 1.0\ndepth_m = 1.0\n";
    const auto result = parse_sled(with_fenders, "sled.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().fenders.behind_hook_m, 0.1);
    EXPECT_DOUBLE_EQ(result.value().fenders.half_span_m, 1.0);
    EXPECT_DOUBLE_EQ(result.value().fenders.depth_m, 1.0);
    const std::string no_depth = std::string{kValidSled} + "\n[sled.fenders]\nhalf_span_m = 1.0\n";
    const auto refused = parse_sled(no_depth, "sled.toml");
    ASSERT_FALSE(refused.ok());
    EXPECT_NE(refused.error().message.find("sled.fenders:"), std::string::npos)
        << refused.error().message;
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

// Files in memory, by path, for the base sled.
ReadFile files(std::map<std::string, std::string> texts) {
    return [texts = std::move(texts)](const std::string& path) -> std::optional<std::string> {
        const auto found = texts.find(path);
        return found == texts.end() ? std::nullopt : std::optional<std::string>{found->second};
    };
}

// A class's sled names the sled every class pulls and sets its own values
// over it: what it sets wins, what it leaves out is the base's.
TEST(SledLoader, AClassSledSetsItsValuesOverTheBase) {
    const auto result =
        load_sled("sleds/class.toml",
                  files({{"sleds/base.toml", std::string{kValidSled}}, {"sleds/class.toml", R"(
[sled]
base = "base.toml"
[sled.weight_box]
mass_kg = 900.0
[sled.knives]
count = 6
)"}}));
    ASSERT_TRUE(result.ok()) << result.error().message;
    const auto base = parse_sled(kValidSled, "base.toml");
    ASSERT_TRUE(base.ok()) << base.error().message;
    EXPECT_EQ(result.value().box_mass_kg, 900.0);
    EXPECT_EQ(result.value().knives.count, 6);
    EXPECT_EQ(result.value().knives.depth_m, base.value().knives.depth_m);
    EXPECT_EQ(result.value().frame_mass_kg, base.value().frame_mass_kg);
    EXPECT_EQ(result.value().box_max_mass_kg, base.value().box_max_mass_kg);
}

TEST(SledLoader, ReportsABadBase) {
    const auto message = [](const ReadFile& read) {
        const auto result = load_sled("sleds/class.toml", read);
        EXPECT_FALSE(result.ok());
        return result.ok() ? std::string{} : result.error().message;
    };
    const std::string class_text = "[sled]\nbase = \"base.toml\"\n";
    EXPECT_NE(message(files({{"sleds/class.toml", class_text}}))
                  .find("sled.base: cannot open sleds/base.toml"),
              std::string::npos);
    EXPECT_NE(message(files({{"sleds/class.toml", "[sled]\nbase = 3\n"}}))
                  .find("sled.base: must be a file name"),
              std::string::npos);
    EXPECT_NE(message(files({{"sleds/class.toml", class_text},
                             {"sleds/base.toml", "[sled]\nbase = \"other.toml\"\n"}}))
                  .find("a base sled names no base of its own"),
              std::string::npos);
    // The class's value is checked as any other: the box over its maximum.
    EXPECT_NE(
        message(files({{"sleds/class.toml", class_text + "[sled.weight_box]\nmass_kg = 20000.0\n"},
                       {"sleds/base.toml", std::string{kValidSled}}}))
            .find("sled.weight_box.mass_kg: must not be over max_mass_kg"),
        std::string::npos);
    // A bad value from the base: the message names the base too.
    std::string bad_base{kValidSled};
    const std::string mass = "frame_mass_kg = ";
    bad_base.replace(bad_base.find(mass), mass.size(), mass + "-");
    const std::string from_base =
        message(files({{"sleds/class.toml", class_text}, {"sleds/base.toml", bad_base}}));
    EXPECT_NE(from_base.find("sled.frame_mass_kg"), std::string::npos) << from_base;
    EXPECT_NE(from_base.find("laid over the base sled sleds/base.toml"), std::string::npos)
        << from_base;
    // Text alone cannot find the base.
    const auto parsed = parse_sled(class_text, "class.toml");
    ASSERT_FALSE(parsed.ok());
    EXPECT_NE(parsed.error().message.find("sled.base"), std::string::npos);
}

// Every class pulls the same sled, 15 t empty with a box for up to 11 t
// (pulling_sled.toml); the classes differ in the box's weight and where
// the push-down starts, the Sport 4.5 t's box the heaviest.
TEST(SledLoader, EveryClassPullsTheSameSled) {
    const std::string dir = std::string{TP_DATA_DIR} + "/sleds/";
    const auto light = load_sled(dir + "light_class.toml");
    const auto sport_25 = load_sled(dir + "sport_2500.toml");
    const auto sport_45 = load_sled(dir + "sport_4500.toml");
    ASSERT_TRUE(light.ok()) << light.error().message;
    ASSERT_TRUE(sport_25.ok()) << sport_25.error().message;
    ASSERT_TRUE(sport_45.ok()) << sport_45.error().message;
    for (const sled::Params* sled : {&light.value(), &sport_25.value(), &sport_45.value()}) {
        EXPECT_EQ(sled->frame_mass_kg, 15000.0);
        EXPECT_EQ(sled->box_max_mass_kg, 11000.0);
        EXPECT_EQ(sled->pan_ahead_of_wheels_m, light.value().pan_ahead_of_wheels_m);
        EXPECT_EQ(sled->hook_ahead_of_wheels_m, light.value().hook_ahead_of_wheels_m);
        EXPECT_EQ(sled->wheel_count, light.value().wheel_count);
    }
    // Between qualifying and the pull-off the operator adds weight to the
    // box as well as knives.
    EXPECT_GT(light.value().pull_off_step.box_mass_kg, 0.0);
    EXPECT_GT(light.value().pull_off_step.knives, 0);
    EXPECT_LT(light.value().box_mass_kg, sport_45.value().box_mass_kg);
    EXPECT_LT(sport_25.value().box_mass_kg, sport_45.value().box_mass_kg);
}

}  // namespace
}  // namespace tp::data
