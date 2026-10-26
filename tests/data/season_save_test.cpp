#include "tp/data/season/season_save.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace tp::data {
namespace {

SeasonSave example() {
    SeasonSave save;
    save.season = "res://data/seasons/club_season.toml";
    // Past what a signed 64-bit TOML integer holds.
    save.seed = UINT64_MAX - 1;
    save.tuning = {.ballast_position = 0.35, .rear_tire_pressure_bar = 0.75};
    save.events = {
        {{.puller = 2, .place = 1},
         {.puller = 0, .place = 2},
         {.puller = 1, .place = 3, .disqualified = true}},
        {{.puller = 0, .place = 1}, {.puller = 1, .place = 1}, {.puller = 2, .place = 3}}};
    return save;
}

// Written and read back, the save is what it was.
TEST(SeasonSave, ReadsBackWhatItWrote) {
    const SeasonSave save = example();
    const Result<SeasonSave> read = parse_season_save(write_season_save(save), "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    const SeasonSave& back = read.value();
    EXPECT_EQ(back.season, save.season);
    EXPECT_EQ(back.seed, save.seed);
    EXPECT_EQ(back.tuning.ballast_position, save.tuning.ballast_position);
    EXPECT_EQ(back.tuning.rear_tire_pressure_bar, save.tuning.rear_tire_pressure_bar);
    ASSERT_EQ(back.events.size(), save.events.size());
    for (std::size_t e = 0; e < save.events.size(); ++e) {
        ASSERT_EQ(back.events[e].size(), save.events[e].size()) << e;
        for (std::size_t i = 0; i < save.events[e].size(); ++i) {
            EXPECT_EQ(back.events[e][i].puller, save.events[e][i].puller) << e << ' ' << i;
            EXPECT_EQ(back.events[e][i].place, save.events[e][i].place) << e << ' ' << i;
            EXPECT_EQ(back.events[e][i].disqualified, save.events[e][i].disqualified)
                << e << ' ' << i;
        }
    }
}

// Quotes and backslashes in the season's name, and a tuning value with
// every digit, read back as written.
TEST(SeasonSave, KeepsAwkwardTextAndExactNumbers) {
    SeasonSave save = example();
    save.season = R"(C:\games\"club" season.toml)";
    save.tuning.ballast_position = 0.1 + 0.2;
    save.tuning.rear_tire_pressure_bar = 1.0;
    const Result<SeasonSave> read = parse_season_save(write_season_save(save), "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_EQ(read.value().season, save.season);
    EXPECT_EQ(read.value().tuning.ballast_position, save.tuning.ballast_position);
    EXPECT_EQ(read.value().tuning.rear_tire_pressure_bar, 1.0);
}

// A season before its first event has no events to list.
TEST(SeasonSave, ANewSeasonHasNoEvents) {
    SeasonSave save = example();
    save.events.clear();
    const Result<SeasonSave> read = parse_season_save(write_season_save(save), "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_TRUE(read.value().events.empty());
}

std::string error_of(const std::string& text) {
    const Result<SeasonSave> read = parse_season_save(text, "season.toml");
    return read ? std::string{} : read.error().message;
}

constexpr const char* kHead = R"(version = 1
season = "res://data/seasons/club_season.toml"
seed = "7"
[tuning]
ballast_position = 0.5
rear_tire_pressure_bar = 1.0
)";

TEST(SeasonSave, RefusesNonsense) {
    EXPECT_EQ(error_of(kHead), "");
    std::string other_version = kHead;
    other_version.replace(0, 11, "version = 2");
    EXPECT_NE(error_of(other_version).find("season.toml: version: must be 1"), std::string::npos)
        << error_of(other_version);
    std::string bad_seed = kHead;
    bad_seed.replace(bad_seed.find("\"7\""), 3, "\"-7\"");
    EXPECT_NE(error_of(bad_seed).find("seed"), std::string::npos) << error_of(bad_seed);
    std::string huge_seed = kHead;
    huge_seed.replace(huge_seed.find("\"7\""), 3, "\"99999999999999999999\"");
    EXPECT_NE(error_of(huge_seed).find("seed"), std::string::npos) << error_of(huge_seed);
    EXPECT_NE(error_of("version = 1\n").find("season"), std::string::npos);
    EXPECT_NE(error_of(std::string{kHead} + "[[events]]\n").find("standings"), std::string::npos);
    EXPECT_NE(
        error_of(std::string{kHead} +
                 "[[events]]\nstandings = [{ puller = 0, place = 0, disqualified = false }]\n")
            .find("events.standings"),
        std::string::npos);
    EXPECT_NE(error_of(std::string{kHead} + "[[events]]\nstandings = [{ puller = 0 }]\n")
                  .find("events.standings"),
              std::string::npos);
    EXPECT_NE(error_of("version = [").find("season.toml:1"), std::string::npos)
        << error_of("version = [");
}

}  // namespace
}  // namespace tp::data
