#include "tp/data/run/track_loader.hpp"

#include <gtest/gtest.h>

#include <string>

namespace tp::data {
namespace {

TEST(TrackLoader, LoadsTheShippedTrack) {
    const auto result = load_track(std::string{TP_DATA_DIR} + "/tracks/standard_100m.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().track_length_m, 100.0);
    EXPECT_EQ(result.value().max_duration_s, 120.0);
}

TEST(TrackLoader, RejectsNonsense) {
    const auto negative =
        parse_track("[track]\nlength_m = -5.0\nmax_duration_s = 60.0\n", "t.toml");
    ASSERT_FALSE(negative.ok());
    EXPECT_NE(negative.error().message.find("t.toml: track.length_m: must be > 0"),
              std::string::npos)
        << negative.error().message;
    const auto missing = parse_track("[track]\nlength_m = 100.0\n", "t.toml");
    ASSERT_FALSE(missing.ok());
    EXPECT_NE(missing.error().message.find("track.max_duration_s: missing or not a number"),
              std::string::npos)
        << missing.error().message;
}

TEST(TrackLoader, ReportsMissingFile) {
    const auto result = load_track("/nonexistent/track.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/track.toml"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
