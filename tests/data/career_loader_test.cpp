#include "tp/data/career/career_loader.hpp"

#include "tp/data/season/season_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

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
    EXPECT_EQ(career.ladder.promotion_place, 1);
    EXPECT_EQ(career.ladder.min_seasons, 2);
    ASSERT_EQ(career.ladder.rungs, 3);
    ASSERT_EQ(career.rungs.size(), 3U);
    for (const CareerRung& rung : career.rungs) {
        EXPECT_EQ(rung.class_name, "Standard 3.5 t");
        // The season's path is resolved, ready to load.
        EXPECT_TRUE(load_season(rung.season, read_from_disk).ok()) << rung.season;
    }
    EXPECT_EQ(career.rungs[0].level_name, "Belofte");
    EXPECT_EQ(career.rungs[1].level_name, "Promo");
    EXPECT_EQ(career.rungs[2].level_name, "Top");
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
