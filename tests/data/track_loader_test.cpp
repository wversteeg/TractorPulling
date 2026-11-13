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
lane_width_m = 11.0

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
)";

constexpr std::string_view kGrippyTrack = R"(
[track]
length_m = 100.0
max_duration_s = 120.0
lane_width_m = 12.0
grip = 1.15

[track.soil]
clay_fraction = 0.5
moisture = 0.14
compaction = 0.8
)";

constexpr std::string_view kPlainTrack = R"(
[track]
length_m = 100.0
max_duration_s = 120.0
lane_width_m = 12.0

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
}

TEST(TrackLoader, LoadsEveryShippedTrack) {
    for (const char* name :
         {"ideal_clay_sand.toml", "dry_hard_clay.toml", "covered_heavy_clay.toml",
          "sandy_clay.toml", "firm_loam.toml", "watered_start.toml"}) {
        const auto result = load_track(kTracks / name);
        ASSERT_TRUE(result.ok()) << result.error().message;
        EXPECT_TRUE(track::Model::create(result.value().ground).has_value()) << name;
        EXPECT_FALSE(result.value().setup_tip.empty()) << name << ": the pit board's tip";
    }
}

TEST(TrackLoader, ParsesValues) {
    const auto result = parse_track(kValidTrack, "t.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const Track& track = result.value();
    EXPECT_EQ(track.rules.max_duration_s, 90.0);
    EXPECT_EQ(track.rules.lane_width_m, 11.0);
    EXPECT_EQ(track.rules.full_pull_line_m, track.rules.track_length_m);
    EXPECT_EQ(track.ground.soil.clay_fraction, 0.6);
    EXPECT_EQ(track.ground.soil.moisture, 0.18);
    EXPECT_EQ(track.ground.soil.compaction, 0.7);
    ASSERT_TRUE(track.ground.moisture_along_m.has_value());
    ASSERT_TRUE(track.ground.compaction_along_m.has_value());
    EXPECT_EQ(track::composition_at(track.ground, 40.0).moisture, 0.22);
    EXPECT_EQ(track::composition_at(track.ground, 100.0).compaction, 0.6);
    EXPECT_EQ(track.sled.pushdown_start_m, 70.0);
}

// Profiles and push-down are optional: without them the soil holds
// everywhere and the class's push-down stays.
TEST(TrackLoader, ProfileAndPushdownAreOptional) {
    const auto result = parse_track(kPlainTrack, "t.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_FALSE(result.value().ground.moisture_along_m.has_value());
    EXPECT_FALSE(result.value().sled.pushdown_start_m.has_value());
    EXPECT_EQ(result.value().ground.grip_scale, 1.0) << "a well-prepared track's grip";
    EXPECT_EQ(result.value().setup_tip, "") << "no tip";
}

// A tip that is not text is an error, not an empty tip.
TEST(TrackLoader, RefusesATipThatIsNotText) {
    std::string tipped{kPlainTrack};
    tipped.replace(tipped.find("[track]\n"), 8, "[track]\nsetup_tip = \"Weights back.\"\n");
    const auto read = parse_track(tipped, "t.toml");
    ASSERT_TRUE(read.ok()) << read.error().message;
    EXPECT_EQ(read.value().setup_tip, "Weights back.");
    std::string wrong{kPlainTrack};
    wrong.replace(wrong.find("[track]\n"), 8, "[track]\nsetup_tip = 3\n");
    const auto refused = parse_track(wrong, "t.toml");
    ASSERT_FALSE(refused.ok());
    EXPECT_NE(refused.error().message.find("t.toml: track.setup_tip: missing or not a string"),
              std::string::npos)
        << refused.error().message;
}

// A track's grip scales its soil's: how it was prepared.
TEST(TrackLoader, ReadsTheGrip) {
    const auto result = parse_track(kGrippyTrack, "t.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().ground.grip_scale, 1.15);
    std::string bad{kGrippyTrack};
    bad.replace(bad.find("grip = 1.15"), 11, "grip = 2.00");
    const auto refused = parse_track(bad, "t.toml");
    ASSERT_FALSE(refused.ok());
    EXPECT_NE(refused.error().message.find("t.toml: track.grip: must be in [0.5, 1.5]"),
              std::string::npos)
        << refused.error().message;
}

// The hard spots are optional: without them the drag is even, with them
// the table's values; values out of range name the table.
TEST(TrackLoader, ReadsTheHardSpots) {
    const auto plain = parse_track(kPlainTrack, "t.toml");
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_FALSE(plain.value().ground.hard_spots.has_value());
    const std::string spotted = std::string{kPlainTrack} +
                                "\n[track.hard_spots]\nevery_m = 9.0\nlength_m = 1.5\nmin_width_m "
                                "= 1.0\nmax_width_m = 3.0\n"
                                "extra = 0.4\nseed = 12\n";
    const auto read = parse_track(spotted, "t.toml");
    ASSERT_TRUE(read.ok()) << read.error().message;
    ASSERT_TRUE(read.value().ground.hard_spots.has_value());
    const track::HardSpots spots = read.value().ground.hard_spots.value_or(track::HardSpots{});
    EXPECT_EQ(spots.every_m, 9.0);
    EXPECT_EQ(spots.length_m, 1.5);
    EXPECT_EQ(spots.min_width_m, 1.0);
    EXPECT_EQ(spots.max_width_m, 3.0);
    EXPECT_EQ(spots.extra, 0.4);
    EXPECT_EQ(spots.seed, 12U);
    // The spots lie in the lane.
    EXPECT_EQ(read.value().ground.spots_across_m, read.value().rules.lane_width_m);
    for (const auto& [from, to, field] : std::array<std::array<std::string_view, 3>, 3>{
             {{"extra = 0.4", "extra = 5.0", "track.hard_spots: every_m must be at least 2"},
              {"max_width_m = 3.0", "max_width_m = 30.0", "at most the lane's width"},
              {"seed = 12", "seed = 0", "track.hard_spots.seed: must be in [1, 4294967295]"}}}) {
        std::string bad = spotted;
        bad.replace(bad.find(from), from.size(), to);
        const auto refused = parse_track(bad, "t.toml");
        ASSERT_FALSE(refused.ok()) << to;
        EXPECT_NE(refused.error().message.find(field), std::string::npos)
            << refused.error().message;
    }
}

// The weak spots likewise: optional, read from their table, checked.
TEST(TrackLoader, ReadsTheWeakSpots) {
    const auto plain = parse_track(kPlainTrack, "t.toml");
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_FALSE(plain.value().ground.weak_spots.has_value());
    const std::string spotted = std::string{kPlainTrack} +
                                "\n[track.weak_spots]\nevery_m = 25.0\nlength_m = 2.0\nmin_width_m "
                                "= 1.0\nmax_width_m = 3.0\n"
                                "grip_loss = 0.12\nseed = 9\n";
    const auto read = parse_track(spotted, "t.toml");
    ASSERT_TRUE(read.ok()) << read.error().message;
    const track::WeakSpots spots = read.value().ground.weak_spots.value_or(track::WeakSpots{});
    EXPECT_EQ(spots.every_m, 25.0);
    EXPECT_EQ(spots.length_m, 2.0);
    EXPECT_EQ(spots.min_width_m, 1.0);
    EXPECT_EQ(spots.max_width_m, 3.0);
    EXPECT_EQ(spots.grip_loss, 0.12);
    EXPECT_EQ(spots.seed, 9U);
    EXPECT_EQ(spots.min_length_m, 0.0) << "all as long without a shortest";
    std::string patches = spotted;
    patches.replace(patches.find("length_m = 2.0"), 14, "length_m = 2.0\nmin_length_m = 1.0");
    const auto varied = parse_track(patches, "t.toml");
    ASSERT_TRUE(varied.ok()) << varied.error().message;
    EXPECT_EQ(varied.value().ground.weak_spots.value_or(track::WeakSpots{}).min_length_m, 1.0);
    std::string too_long = spotted;
    too_long.replace(too_long.find("length_m = 2.0"), 14, "length_m = 2.0\nmin_length_m = 2.5");
    EXPECT_FALSE(parse_track(too_long, "t.toml").ok()) << "the shortest past the longest";
    std::string bad = spotted;
    bad.replace(bad.find("grip_loss = 0.12"), 16, "grip_loss = 0.9");
    const auto refused = parse_track(bad, "t.toml");
    ASSERT_FALSE(refused.ok());
    EXPECT_NE(refused.error().message.find("track.weak_spots: "), std::string::npos)
        << refused.error().message;
}

TEST(TrackLoader, RejectsNonsense) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 11> cases = {{
        {"lane_width_m = 11.0", "lane_width_m = 0.0", "t.toml: track.lane_width_m: must be > 0"},
        {"lane_width_m = 11.0", "", "track.lane_width_m: missing or not a number"},
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
        {"start_m = 70.0", "start_m = \"early\"",
         "track.pushdown.start_m: missing or not a number"},
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
