#include "tp/data/season/season_save.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace tp::data {
namespace {

SeasonSave example() {
    SeasonSave save;
    save.season = "res://data/seasons/club_season.toml";
    // Past what a signed 64-bit TOML integer holds.
    save.seed = UINT64_MAX - 1;
    save.tuning = {
        .ballast_position = 0.35, .rear_tire_pressure_bar = 0.75, .hitch_height_m = 0.47};
    save.career = SavedCareer{.money_eur = 1234,
                              .upgrades = {{.name = "Injection pump", .level = 2},
                                           {.name = R"(The "big" tires)", .level = 0}},
                              .choices = {{.name = "Turbo",
                                           .fitted = "68 mm turbo",
                                           .owned = {"68 mm turbo", R"(A "huge" one)"}},
                                          {.name = "Gearbox", .fitted = "Stock", .owned = {}}},
                              .class_name = "Sport",
                              .level_name = "Promo",
                              .unlocked_class = R"(Super "stock")",
                              .unlocked_level = "Belofte",
                              .seasons_on_level = 2,
                              .title_on_level = true};
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
    EXPECT_EQ(back.tuning.hitch_height_m, save.tuning.hitch_height_m);
    ASSERT_TRUE(back.career.has_value());
    const SavedCareer career = back.career.value_or(SavedCareer{});
    EXPECT_EQ(career.money_eur, 1234);
    ASSERT_EQ(career.upgrades.size(), 2U);
    EXPECT_EQ(career.upgrades[0].name, "Injection pump");
    EXPECT_EQ(career.upgrades[0].level, 2);
    EXPECT_EQ(career.upgrades[1].name, R"(The "big" tires)");
    EXPECT_EQ(career.upgrades[1].level, 0);
    EXPECT_EQ(career.class_name, "Sport");
    EXPECT_EQ(career.unlocked_class, R"(Super "stock")");
    EXPECT_EQ(career.level_name, "Promo");
    EXPECT_EQ(career.unlocked_level, "Belofte");
    EXPECT_EQ(career.seasons_on_level, 2);
    EXPECT_TRUE(career.title_on_level);
    ASSERT_EQ(career.choices.size(), 2U);
    EXPECT_EQ(career.choices[0].name, "Turbo");
    EXPECT_EQ(career.choices[0].fitted, "68 mm turbo");
    EXPECT_EQ(career.choices[0].owned,
              (std::vector<std::string>{"68 mm turbo", R"(A "huge" one)"}));
    EXPECT_EQ(career.choices[1].name, "Gearbox");
    EXPECT_TRUE(career.choices[1].owned.empty());
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

// A save from before the garage reads as a career not yet begun.
TEST(SeasonSave, ReadsASaveFromBeforeTheGarage) {
    const Result<SeasonSave> read = parse_season_save(kHead, "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_FALSE(read.value().career.has_value());
}

// A version 2 save's career is in the career's first class.
TEST(SeasonSave, ReadsASaveFromBeforeTheClasses) {
    std::string version_2 = kHead;
    version_2.replace(0, 11, "version = 2");
    const Result<SeasonSave> read =
        parse_season_save(version_2 + "[career]\nmoney_eur = 5\n", "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    ASSERT_TRUE(read.value().career.has_value());
    const SavedCareer career = read.value().career.value_or(SavedCareer{});
    EXPECT_EQ(career.money_eur, 5);
    EXPECT_EQ(career.class_name, "");
    EXPECT_EQ(career.unlocked_class, "");
}

// A version 3 save's career is at its classes' first levels.
TEST(SeasonSave, ReadsASaveFromBeforeTheLevels) {
    std::string version_3 = kHead;
    version_3.replace(0, 11, "version = 3");
    const Result<SeasonSave> read = parse_season_save(
        version_3 + "[career]\nmoney_eur = 5\nclass = \"Sport\"\nunlocked = \"Sport\"\n",
        "season.toml");
    ASSERT_TRUE(read) << read.error().message;
    const SavedCareer career = read.value().career.value_or(SavedCareer{});
    EXPECT_EQ(career.class_name, "Sport");
    EXPECT_EQ(career.level_name, "");
    EXPECT_EQ(career.unlocked_level, "");
    EXPECT_EQ(career.seasons_on_level, 0);
    EXPECT_FALSE(career.title_on_level);
}

TEST(SeasonSave, RefusesNonsense) {
    EXPECT_EQ(error_of(kHead), "");
    std::string other_version = kHead;
    other_version.replace(0, 11, "version = 6");
    EXPECT_NE(error_of(other_version).find("season.toml: version: must be from 1 to 5"),
              std::string::npos)
        << error_of(other_version);
    std::string version_2 = kHead;
    version_2.replace(0, 11, "version = 2");
    EXPECT_NE(error_of(version_2).find("career.money_eur"), std::string::npos)
        << error_of(version_2);
    for (const char* career :
         {"[career]\nmoney_eur = -1\n", "[career]\nmoney_eur = 5\nupgrades = [{ name = \"x\" }]\n",
          "[career]\nmoney_eur = 5\nupgrades = [{ name = \"x\", level = -1 }]\n"}) {
        EXPECT_NE(error_of(version_2 + career).find("season.toml: career"), std::string::npos)
            << career << ": " << error_of(version_2 + career);
    }
    EXPECT_EQ(error_of(version_2 + "[career]\nmoney_eur = 5\n"), "");
    // Version 3 names the player's class and the highest one open.
    std::string version_3 = kHead;
    version_3.replace(0, 11, "version = 3");
    EXPECT_NE(
        error_of(version_3 + "[career]\nmoney_eur = 5\nunlocked = \"\"\n").find("career.class"),
        std::string::npos);
    EXPECT_NE(
        error_of(version_3 + "[career]\nmoney_eur = 5\nclass = \"\"\n").find("career.unlocked"),
        std::string::npos);
    EXPECT_EQ(error_of(version_3 + "[career]\nmoney_eur = 5\nclass = \"\"\nunlocked = \"\"\n"), "");
    // Version 4 adds the levels.
    std::string version_4 = kHead;
    version_4.replace(0, 11, "version = 4");
    const std::string classes = "[career]\nmoney_eur = 5\nclass = \"\"\nunlocked = \"\"\n";
    const std::string seasons = "seasons_on_level = 1\ntitle_on_level = false\n";
    EXPECT_NE(
        error_of(version_4 + classes + "unlocked_level = \"\"\n" + seasons).find("career.level"),
        std::string::npos);
    EXPECT_NE(
        error_of(version_4 + classes + "level = \"\"\n" + seasons).find("career.unlocked_level"),
        std::string::npos);
    const std::string levels = classes + "level = \"\"\nunlocked_level = \"\"\n";
    EXPECT_EQ(error_of(version_4 + levels + seasons), "");
    EXPECT_NE(
        error_of(version_4 + levels + "title_on_level = false\n").find("career.seasons_on_level"),
        std::string::npos);
    EXPECT_NE(error_of(version_4 + levels + "seasons_on_level = -1\ntitle_on_level = false\n")
                  .find("career.seasons_on_level"),
              std::string::npos);
    EXPECT_NE(error_of(version_4 + levels + "seasons_on_level = 11\ntitle_on_level = false\n")
                  .find("career.seasons_on_level: must be 0 to 10"),
              std::string::npos);
    EXPECT_NE(error_of(version_4 + levels + "seasons_on_level = 1\n").find("career.title_on_level"),
              std::string::npos);
    // Version 5 adds the garage's choices; a version 4 save has none.
    const Result<SeasonSave> before_choices =
        parse_season_save(version_4 + levels + seasons, "season.toml");
    ASSERT_TRUE(before_choices && before_choices.value().career.has_value());
    EXPECT_TRUE(before_choices.value().career.value_or(SavedCareer{}).choices.empty());
    std::string version_5 = kHead;
    version_5.replace(0, 11, "version = 5");
    // Version 5 adds the hitch's height to the tuning.
    EXPECT_NE(
        error_of(version_5 + levels + seasons + "choices = []\n").find("tuning.hitch_height_m"),
        std::string::npos);
    const std::string pressure_line = "rear_tire_pressure_bar = 1.0\n";
    version_5.replace(version_5.find(pressure_line), pressure_line.size(),
                      pressure_line + "hitch_height_m = 0.45\n");
    std::string low_hitch = version_5;
    low_hitch.replace(low_hitch.find("0.45"), 4, "-0.1");
    EXPECT_NE(error_of(low_hitch + levels + seasons + "choices = []\n")
                  .find("tuning.hitch_height_m: must be"),
              std::string::npos);
    const std::string head_5 = version_5 + levels + seasons;
    EXPECT_NE(error_of(head_5).find("career.choices"), std::string::npos);
    EXPECT_EQ(error_of(head_5 + "choices = []\n"), "");
    EXPECT_EQ(error_of(head_5 + "choices = [{ name = \"T\", fitted = \"A\", owned = [] }]\n"), "");
    for (const char* choices : {"choices = [{ name = \"T\", owned = [] }]\n",
                                "choices = [{ name = \"T\", fitted = \"A\" }]\n",
                                "choices = [{ fitted = \"A\", owned = [] }]\n",
                                "choices = [{ name = \"T\", fitted = \"A\", owned = [1] }]\n"}) {
        EXPECT_NE(error_of(head_5 + choices).find("season.toml: career.choices"), std::string::npos)
            << choices << ": " << error_of(head_5 + choices);
    }
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
    for (const auto& [from, to] : std::initializer_list<std::pair<const char*, const char*>>{
             {"ballast_position = 0.5", "ballast_position = nan"},
             {"ballast_position = 0.5", "ballast_position = 1.5"},
             {"ballast_position = 0.5", "ballast_position = -0.1"},
             {"rear_tire_pressure_bar = 1.0", "rear_tire_pressure_bar = inf"},
             {"rear_tire_pressure_bar = 1.0", "rear_tire_pressure_bar = -1.0"}}) {
        std::string text = kHead;
        text.replace(text.find(from), std::string{from}.size(), to);
        EXPECT_NE(error_of(text).find("season.toml: tuning."), std::string::npos)
            << to << ": " << error_of(text);
    }
    EXPECT_NE(error_of("version = [").find("season.toml:1"), std::string::npos)
        << error_of("version = [");
}

}  // namespace
}  // namespace tp::data
