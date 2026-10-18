#include "tp/data/track/track_loader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#include "../track/reference_track.hpp"

namespace tp::data {
namespace {

const std::filesystem::path kTracks = std::filesystem::path{TP_DATA_DIR} / "tracks";

constexpr std::string_view kValidTrack = R"(
[track]
length_m = 100.0
max_duration_s = 90.0

[track.soil]
clay_fraction = 0.6
moisture = 0.18
compaction = 0.7

[track.profile.moisture]
x_m = [0.0, 40.0, 100.0]
value = [0.16, 0.22, 0.18]

[track.profile.compaction]
x_m = [0.0, 100.0]
value = [0.8, 0.6]

[track.pushdown]
start_m = 70.0
pull_off_start_m = 55.0
)";

constexpr std::string_view kPlainTrack = R"(
[track]
length_m = 100.0
max_duration_s = 120.0

[track.soil]
clay_fraction = 0.5
moisture = 0.14
compaction = 0.8
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidTrack};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

// The reference track is the one the core tests pull on, with the class's
// own push-down.
TEST(TrackLoader, ShippedReferenceMatchesTestReference) {
    const auto result = load_track(kTracks / "ideal_clay_sand.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const Track& track = result.value();
    const track::Params reference = track::testing::reference_params();
    EXPECT_EQ(track.rules.track_length_m, 100.0);
    EXPECT_EQ(track.rules.max_duration_s, 120.0);
    EXPECT_EQ(track.ground.soil.clay_fraction, reference.soil.clay_fraction);
    EXPECT_EQ(track.ground.soil.moisture, reference.soil.moisture);
    EXPECT_EQ(track.ground.soil.compaction, reference.soil.compaction);
    EXPECT_FALSE(track.ground.moisture_along_m.has_value());
    EXPECT_FALSE(track.ground.compaction_along_m.has_value());
    EXPECT_FALSE(track.sled.pushdown_start_m.has_value());
    EXPECT_FALSE(track.sled.pull_off_pushdown_start_m.has_value());
}

TEST(TrackLoader, LoadsEveryShippedTrack) {
    for (const char* name :
         {"ideal_clay_sand.toml", "dry_hard_clay.toml", "wet_heavy_clay.toml", "loose_sand.toml"}) {
        const auto result = load_track(kTracks / name);
        ASSERT_TRUE(result.ok()) << result.error().message;
        EXPECT_TRUE(track::Model::create(result.value().ground).has_value()) << name;
    }
}

TEST(TrackLoader, ParsesValues) {
    const auto result = parse_track(kValidTrack, "t.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const Track& track = result.value();
    EXPECT_EQ(track.rules.max_duration_s, 90.0);
    EXPECT_EQ(track.ground.soil.clay_fraction, 0.6);
    EXPECT_EQ(track.ground.soil.moisture, 0.18);
    EXPECT_EQ(track.ground.soil.compaction, 0.7);
    ASSERT_TRUE(track.ground.moisture_along_m.has_value());
    ASSERT_TRUE(track.ground.compaction_along_m.has_value());
    EXPECT_EQ(track::composition_at(track.ground, 40.0).moisture, 0.22);
    EXPECT_EQ(track::composition_at(track.ground, 100.0).compaction, 0.6);
    EXPECT_EQ(track.sled.pushdown_start_m, 70.0);
    EXPECT_EQ(track.sled.pull_off_pushdown_start_m, 55.0);
}

// Profiles and push-down are optional: without them the soil holds
// everywhere and the class's push-down stays.
TEST(TrackLoader, ProfileAndPushdownAreOptional) {
    const auto result = parse_track(kPlainTrack, "t.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_FALSE(result.value().ground.moisture_along_m.has_value());
    EXPECT_FALSE(result.value().sled.pushdown_start_m.has_value());
}

TEST(TrackLoader, RejectsNonsense) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 9> cases = {{
        {"length_m = 100.0", "length_m = -5.0", "t.toml: track.length_m: must be > 0"},
        {"max_duration_s = 90.0", "", "track.max_duration_s: missing or not a number"},
        {"clay_fraction = 0.6", "clay_fraction = 1.2",
         "track.soil.clay_fraction: must be in [0, 1]"},
        {"moisture = 0.18", "moisture = 0.7", "track.soil.moisture: must be in [0, 0.5]"},
        {"compaction = 0.7", "", "track.soil.compaction: missing or not a number"},
        {"value = [0.16, 0.22, 0.18]", "value = [0.16, 0.62, 0.18]",
         "track.profile.moisture.value: every value must be in [0, 0.5]"},
        {"x_m = [0.0, 40.0, 100.0]", "x_m = [0.0, 100.0]",
         "track.profile.moisture: x_m and value must have the same length"},
        {"start_m = 70.0", "start_m = -1.0", "track.pushdown.start_m: must be >= 0"},
        {"pull_off_start_m = 55.0", "pull_off_start_m = \"early\"",
         "track.pushdown.pull_off_start_m: missing or not a number"},
    }};
    for (const Case& c : cases) {
        const auto result = parse_track(with(c.from, c.to), "t.toml");
        ASSERT_FALSE(result.ok()) << c.from;
        EXPECT_NE(result.error().message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << result.error().message;
    }
}

TEST(TrackLoader, ReportsMissingFile) {
    const auto result = load_track("/nonexistent/track.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/track.toml"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
