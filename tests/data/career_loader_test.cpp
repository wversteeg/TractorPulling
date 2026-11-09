#include "tp/data/career/career_loader.hpp"

#include "tp/data/season/season_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace tp::data {
namespace {

const std::filesystem::path kCareers = std::filesystem::path{TP_DATA_DIR} / "careers";

// Two classes on the shipped season, the first at two levels: three rungs.
constexpr std::string_view kTwoClasses = R"(
[career]
name = "Test path"
promotion_place = 1
min_seasons = 2

[[career.classes]]
name = "Low"
levels = [
    { name = "Belofte", season = "../seasons/club_season.toml" },
    { name = "Top", season = "../seasons/club_season.toml" },
]

[[career.classes]]
name = "High"
levels = [{ name = "Belofte", season = "../seasons/club_season.toml" }]
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kTwoClasses};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

Result<CareerDefinition> parse(std::string_view text) {
    return parse_career(text, "career.toml", kCareers.generic_string(), read_from_disk);
}

std::string error_of(std::string_view text) {
    const auto result = parse(text);
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

TEST(CareerLoader, LoadsTheShippedCareer) {
    const auto result = load_career(kCareers / "tractor_path.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const CareerDefinition& career = result.value();
    EXPECT_EQ(career.name, "Tractor path");
    EXPECT_EQ(career.ladder.promotion_place, 2) << "the first two move up";
    EXPECT_EQ(career.ladder.min_seasons, 2);
    // Standard 3.5 t at one level, the warming-up; Sport 2.5 t at promo
    // and top; Sport 4.5 t at belofte, promo and top.
    ASSERT_EQ(career.ladder.rungs, 6);
    ASSERT_EQ(career.rungs.size(), 6U);
    for (const CareerRung& rung : career.rungs) {
        // The season's path is resolved, ready to load.
        EXPECT_TRUE(load_season(rung.season, read_from_disk).ok()) << rung.season;
    }
    const std::array<std::pair<std::string_view, std::string_view>, 6> expected{{
        {"Standard 3.5 t", "Club"},
        {"Sport 2.5 t", "Promo"},
        {"Sport 2.5 t", "Top"},
        {"Sport 4.5 t", "Belofte"},
        {"Sport 4.5 t", "Promo"},
        {"Sport 4.5 t", "Top"},
    }};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(career.rungs[i].class_name, expected[i].first) << i;
        EXPECT_EQ(career.rungs[i].level_name, expected[i].second) << i;
    }
}

// A new tractor finishes at the back: every place of every level the career
// climbs pays, so a player at the back can still build their tractor up.
// At the career's start (its first rung) the last place pays the garage's
// cheapest upgrade every event; further up, one every two events (the
// player brings money and parts from below).
TEST(CareerLoader, EveryPlaceOfTheShippedCareerPaysForUpgrades) {
    const auto career = load_career(kCareers / "tractor_path.toml");
    ASSERT_TRUE(career.ok()) << career.error().message;
    bool first = true;
    for (const CareerRung& rung : career.value().rungs) {
        const auto season = load_season(rung.season, read_from_disk);
        ASSERT_TRUE(season.ok()) << rung.season;
        const SeasonDefinition& d = season.value();
        ASSERT_EQ(d.prize_money_eur.size(), static_cast<std::size_t>(d.event.game.pullers))
            << rung.season << ": a prize for every place";
        std::int64_t cheapest_eur = std::numeric_limits<std::int64_t>::max();
        const garage::Catalogue& catalogue = d.garage.catalogue;
        for (std::size_t i = 0; i < catalogue.count; ++i) {
            const garage::Upgrade& upgrade = catalogue.upgrades.at(i);
            if (upgrade.level_count > 1) {
                cheapest_eur = std::min(cheapest_eur, upgrade.levels.at(1).cost_eur);
            }
        }
        const std::int64_t events_an_upgrade = first ? 1 : 2;
        EXPECT_GE(d.prize_money_eur.back() * events_an_upgrade, cheapest_eur)
            << rung.season << ": the last place";
        first = false;
    }
}

TEST(CareerLoader, LoadsEachClassesLevelsInTurn) {
    const auto result = parse(kTwoClasses);
    ASSERT_TRUE(result.ok()) << result.error().message;
    const CareerDefinition& career = result.value();
    ASSERT_EQ(career.ladder.rungs, 3);
    EXPECT_EQ(career.rungs[0].class_name, "Low");
    EXPECT_EQ(career.rungs[0].level_name, "Belofte");
    EXPECT_EQ(career.rungs[1].class_name, "Low");
    EXPECT_EQ(career.rungs[1].level_name, "Top");
    EXPECT_EQ(career.rungs[2].class_name, "High");
    EXPECT_EQ(career.rungs[2].level_name, "Belofte");
    EXPECT_NE(career.rungs[2].season.find("seasons/club_season.toml"), std::string::npos);
}

TEST(CareerLoader, RefusesAPromotionPlaceNoFieldHas) {
    EXPECT_NE(
        error_of(with("promotion_place = 1", "promotion_place = 0")).find("career.promotion_place"),
        std::string::npos);
    EXPECT_NE(error_of(with("promotion_place = 1", "promotion_place = 33"))
                  .find("career.promotion_place"),
              std::string::npos);
}

TEST(CareerLoader, RefusesMinimumSeasonsOutOfRange) {
    EXPECT_NE(error_of(with("min_seasons = 2", "min_seasons = 0")).find("career.min_seasons"),
              std::string::npos);
    EXPECT_NE(error_of(with("min_seasons = 2", "min_seasons = 11")).find("career.min_seasons"),
              std::string::npos);
}

TEST(CareerLoader, RefusesMissingFields) {
    EXPECT_NE(error_of(with("name = \"Test path\"\n", "")).find("career.name"), std::string::npos);
    EXPECT_NE(error_of(with("promotion_place = 1\n", "")).find("career.promotion_place"),
              std::string::npos);
    EXPECT_NE(error_of(with("name = \"High\"\n", "")).find("career.classes"), std::string::npos);
    EXPECT_NE(error_of(with("name = \"High\"", "name = \"\"")).find("career.classes"),
              std::string::npos);
    EXPECT_NE(error_of(with("levels = [{ name = \"Belofte\", season = "
                            "\"../seasons/club_season.toml\" }]",
                            "levels = []"))
                  .find("needs its levels"),
              std::string::npos);
    EXPECT_NE(error_of(with("{ name = \"Top\", season", "{ season")).find("career.classes.levels"),
              std::string::npos);
}

TEST(CareerLoader, RefusesNoClassesAndTooManyRungs) {
    const std::string none = "[career]\nname = \"x\"\npromotion_place = 1\nmin_seasons = 1\n";
    EXPECT_NE(error_of(none).find("career.classes"), std::string::npos);
    std::string many = none;
    for (int i = 0; i <= career::kMaxRungs; ++i) {
        many += "[[career.classes]]\nname = \"c" + std::to_string(i) +
                "\"\nlevels = [{ name = \"Top\", season = \"../seasons/club_season.toml\" }]\n";
    }
    EXPECT_NE(error_of(many).find("levels in all"), std::string::npos);
}

TEST(CareerLoader, RefusesANameUsedTwice) {
    EXPECT_NE(error_of(with("name = \"High\"", "name = \"Low\"")).find("used twice"),
              std::string::npos);
    EXPECT_NE(error_of(with("{ name = \"Top\"", "{ name = \"Belofte\"")).find("used twice in"),
              std::string::npos);
}

TEST(CareerLoader, PassesOnASeasonsOwnError) {
    const std::string message =
        error_of(with(R"(levels = [{ name = "Belofte", season = "../seasons/club_season.toml" }])",
                      R"(levels = [{ name = "Belofte", season = "../seasons/missing.toml" }])"));
    EXPECT_NE(message.find("missing.toml"), std::string::npos) << message;
}

TEST(CareerLoader, SaysWhenTheFileCannotBeOpened) {
    const auto result = load_career(kCareers / "missing.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("cannot open"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
