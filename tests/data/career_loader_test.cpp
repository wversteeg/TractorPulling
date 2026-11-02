#include "tp/data/career/career_loader.hpp"

#include "tp/data/season/season_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

const std::filesystem::path kCareers = std::filesystem::path{TP_DATA_DIR} / "careers";

// Two classes on the shipped season, as a ladder to climb.
constexpr std::string_view kTwoClasses = R"(
[career]
name = "Test path"
promotion_place = 1

[[career.classes]]
name = "Low"
season = "../seasons/club_season.toml"

[[career.classes]]
name = "High"
season = "../seasons/club_season.toml"
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
    ASSERT_EQ(career.ladder.classes, 1);
    ASSERT_EQ(career.classes.size(), 1U);
    EXPECT_EQ(career.classes[0].name, "Standard 3.5 t");
    // The season's path is resolved, ready to load.
    EXPECT_TRUE(load_season(career.classes[0].season, read_from_disk).ok());
}

TEST(CareerLoader, LoadsTheClassesInOrder) {
    const auto result = parse(kTwoClasses);
    ASSERT_TRUE(result.ok()) << result.error().message;
    ASSERT_EQ(result.value().ladder.classes, 2);
    EXPECT_EQ(result.value().classes[0].name, "Low");
    EXPECT_EQ(result.value().classes[1].name, "High");
    EXPECT_NE(result.value().classes[1].season.find("seasons/club_season.toml"), std::string::npos);
}

TEST(CareerLoader, RefusesAPromotionPlaceNoFieldHas) {
    EXPECT_NE(
        error_of(with("promotion_place = 1", "promotion_place = 0")).find("career.promotion_place"),
        std::string::npos);
    EXPECT_NE(error_of(with("promotion_place = 1", "promotion_place = 33"))
                  .find("career.promotion_place"),
              std::string::npos);
}

TEST(CareerLoader, RefusesMissingFields) {
    EXPECT_NE(error_of(with("name = \"Test path\"\n", "")).find("career.name"), std::string::npos);
    EXPECT_NE(error_of(with("promotion_place = 1\n", "")).find("career.promotion_place"),
              std::string::npos);
    EXPECT_NE(error_of(with("name = \"High\"\n", "")).find("career.classes"), std::string::npos);
    EXPECT_NE(error_of(with("name = \"High\"", "name = \"\"")).find("career.classes"),
              std::string::npos);
}

TEST(CareerLoader, RefusesNoClassesAndTooMany) {
    const std::string none = "[career]\nname = \"x\"\npromotion_place = 1\n";
    EXPECT_NE(error_of(none).find("career.classes"), std::string::npos);
    std::string many = none;
    for (int i = 0; i <= career::kMaxClasses; ++i) {
        many += "[[career.classes]]\nname = \"c" + std::to_string(i) +
                "\"\nseason = \"../seasons/club_season.toml\"\n";
    }
    EXPECT_NE(error_of(many).find("career.classes"), std::string::npos);
}

TEST(CareerLoader, RefusesAClassNameUsedTwice) {
    EXPECT_NE(error_of(with("name = \"High\"", "name = \"Low\"")).find("used twice"),
              std::string::npos);
}

TEST(CareerLoader, PassesOnASeasonsOwnError) {
    const std::string message =
        error_of(with("name = \"High\"\nseason = \"../seasons/club_season.toml\"",
                      "name = \"High\"\nseason = \"../seasons/missing.toml\""));
    EXPECT_NE(message.find("missing.toml"), std::string::npos) << message;
}

TEST(CareerLoader, SaysWhenTheFileCannotBeOpened) {
    const auto result = load_career(kCareers / "missing.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("cannot open"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
