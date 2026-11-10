#include "tp/data/garage/garage_loader.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

constexpr std::string_view kGarage = R"(
[garage]
start_money_eur = 500

[[garage.upgrades]]
part = "hitch_height_m"
name = "Drawbar"
levels = [
    { value = 0.40, cost_eur = 0, label = "Farm drawbar" },
    { value = 0.50, cost_eur = 300, label = "Pulling hitch" },
]

[[garage.upgrades]]
part = "load_downshift_gears"
name = "Powershift"
levels = [{ value = 0.0, cost_eur = 0, label = "None" }]
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kGarage};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view text) {
    const auto result = parse_garage(text, "garage.toml");
    return result.ok() ? std::string{} : result.error().message;
}

TEST(GarageLoader, ReadsTheCatalogueAndItsNames) {
    const auto result = parse_garage(kGarage, "garage.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const GarageDefinition& g = result.value();
    EXPECT_EQ(g.catalogue.start_money_eur, 500);
    ASSERT_EQ(g.catalogue.count, 2U);
    EXPECT_EQ(g.catalogue.upgrades[0].part, garage::Part::kHitchHeight);
    ASSERT_EQ(g.catalogue.upgrades[0].level_count, 2U);
    EXPECT_EQ(g.catalogue.upgrades[0].levels[1].value, 0.50);
    EXPECT_EQ(g.catalogue.upgrades[0].levels[1].cost_eur, 300);
    EXPECT_EQ(g.catalogue.upgrades[1].part, garage::Part::kLoadDownshift);
    ASSERT_EQ(g.texts.size(), 2U);
    EXPECT_EQ(g.texts[0].name, "Drawbar");
    ASSERT_EQ(g.texts[0].level_labels.size(), 2U);
    EXPECT_EQ(g.texts[0].level_labels[1], "Pulling hitch");
}

// The governor's levels are in rpm in the file, rad/s in the core.
TEST(GarageLoader, ReadsTheGovernorInRpm) {
    const auto result =
        parse_garage(with("part = \"load_downshift_gears\"\nname = \"Powershift\"\n"
                          "levels = [{ value = 0.0, cost_eur = 0, label = \"None\" }]",
                          "part = \"rated_rpm\"\nname = \"Governor\"\n"
                          "levels = [{ value = 2200.0, cost_eur = 0, label = \"Stock\" }, "
                          "{ value = 2500.0, cost_eur = 1800, label = \"Turned up\" }]"),
                     "garage.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const garage::Upgrade& governor = result.value().catalogue.upgrades[1];
    EXPECT_EQ(governor.part, garage::Part::kRatedSpeed);
    EXPECT_EQ(governor.levels[1].value, units::rpm_to_rad_s(2500.0));
    EXPECT_EQ(governor.levels[1].cost_eur, 1800);
}

TEST(GarageLoader, RefusesNonsense) {
    EXPECT_EQ(error_of(kGarage), "");
    EXPECT_NE(error_of(with("start_money_eur = 500", "start_money_eur = -1"))
                  .find("garage.toml: garage.start_money_eur"),
              std::string::npos);
    EXPECT_NE(error_of(with("\"hitch_height_m\"", "\"turbo\"")).find("unknown part 'turbo'"),
              std::string::npos);
    EXPECT_NE(error_of(with("\"load_downshift_gears\"", "\"hitch_height_m\"")).find("twice"),
              std::string::npos);
    EXPECT_NE(error_of(with("cost_eur = 0, label = \"Farm drawbar\"",
                            "cost_eur = 10, label = \"Farm drawbar\""))
                  .find("the first level"),
              std::string::npos);
    EXPECT_NE(error_of(with("cost_eur = 300", "cost_eur = -300")).find("must not be negative"),
              std::string::npos);
    EXPECT_NE(error_of(with(", label = \"Pulling hitch\"", "")).find("every level needs"),
              std::string::npos);
    EXPECT_NE(
        error_of(with("levels = [{ value = 0.0, cost_eur = 0, label = \"None\" }]", "levels = []"))
            .find("must list 1 to"),
        std::string::npos);
    EXPECT_NE(error_of(with("name = \"Drawbar\"\n", "")).find("needs a part and a name"),
              std::string::npos);
    EXPECT_NE(error_of("[garage]\nstart_money_eur = 1\n").find("garage.upgrades"),
              std::string::npos);
}

// A garage with a turbo choice between two of the shipped engines, each
// with its own build, and weights that count on either.
constexpr std::string_view kChoiceGarage = R"(
[garage]
start_money_eur = 500

[[garage.choices]]
slot = "engine"
name = "Turbo"
alternatives = [
    { file = "../engines/sport_2500.toml", cost_eur = 0, label = "Small turbo" },
    { file = "../engines/sport_4500.toml", cost_eur = 900, label = "Big turbo" },
]

[[garage.upgrades]]
part = "engine_torque"
name = "Small build"
choice = "Turbo"
alternative = "Small turbo"
levels = [{ value = 0.5, cost_eur = 0, label = "Stock" }]

[[garage.upgrades]]
part = "engine_torque"
name = "Big build"
choice = "Turbo"
alternative = "Big turbo"
levels = [{ value = 0.4, cost_eur = 0, label = "Stock" }]

[[garage.upgrades]]
part = "ballast_kg"
name = "Weights"
levels = [{ value = 0.0, cost_eur = 0, label = "None" }]
)";

// The garage as if it were in data/garages/.
Result<GarageDefinition> parse_choices(std::string_view text) {
    const std::string garages = (std::filesystem::path{TP_DATA_DIR} / "garages").generic_string();
    return parse_garage(text, "garage.toml", garages, read_from_disk);
}

std::string choice_error_of(std::string_view from, std::string_view to) {
    std::string text{kChoiceGarage};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    const auto result = parse_choices(text);
    return result.ok() ? std::string{} : result.error().message;
}

TEST(GarageLoader, ReadsChoicesAndTheUpgradesBelongingToThem) {
    const auto result = parse_choices(kChoiceGarage);
    ASSERT_TRUE(result.ok()) << result.error().message;
    const GarageDefinition& g = result.value();
    ASSERT_EQ(g.catalogue.choice_count, 1U);
    const garage::Choice& turbo = g.catalogue.choices[0];
    EXPECT_EQ(turbo.slot, garage::Slot::kEngine);
    ASSERT_EQ(turbo.alternative_count, 2U);
    const std::optional<engine::Params>& small = turbo.alternatives[0].engine;
    const std::optional<engine::Params>& big = turbo.alternatives[1].engine;
    ASSERT_TRUE(small.has_value() && big.has_value());
    EXPECT_GT(big.has_value() ? big->inertia_kg_m2 : 0.0,
              small.has_value() ? small->inertia_kg_m2 : 0.0);
    EXPECT_EQ(turbo.alternatives[1].cost_eur, 900);
    ASSERT_EQ(g.choice_texts.size(), 1U);
    EXPECT_EQ(g.choice_texts[0].name, "Turbo");
    EXPECT_EQ(g.choice_texts[0].alternative_labels.at(1), "Big turbo");
    ASSERT_EQ(g.catalogue.count, 3U);
    EXPECT_EQ(g.catalogue.upgrades[0].choice, 0);
    EXPECT_EQ(g.catalogue.upgrades[0].alternative, 0);
    EXPECT_EQ(g.catalogue.upgrades[1].choice, 0);
    EXPECT_EQ(g.catalogue.upgrades[1].alternative, 1);
    EXPECT_EQ(g.catalogue.upgrades[2].choice, garage::kNoChoice);
}

TEST(GarageLoader, RefusesNonsenseInChoices) {
    EXPECT_EQ(choice_error_of("", ""), "");
    EXPECT_NE(choice_error_of("slot = \"engine\"", "slot = \"turbo\"").find("unknown slot"),
              std::string::npos);
    EXPECT_NE(choice_error_of("sport_4500.toml", "nowhere.toml").find("cannot open"),
              std::string::npos);
    EXPECT_NE(choice_error_of("cost_eur = 900", "cost_eur = -900").find("must not be negative"),
              std::string::npos);
    EXPECT_NE(choice_error_of("cost_eur = 0, label = \"Small turbo\"",
                              "cost_eur = 5, label = \"Small turbo\"")
                  .find("the first alternative"),
              std::string::npos);
    EXPECT_NE(
        choice_error_of("label = \"Big turbo\"", "label = \"Small turbo\"").find("listed twice"),
        std::string::npos);
    EXPECT_NE(choice_error_of(
                  "    { file = \"../engines/sport_4500.toml\", cost_eur = 900, label = \"Big "
                  "turbo\" },\n",
                  "")
                  .find("must list 2 to"),
              std::string::npos);
    EXPECT_NE(choice_error_of("alternative = \"Big turbo\"", "alternative = \"Huge turbo\"")
                  .find("has no alternative 'Huge turbo'"),
              std::string::npos);
    EXPECT_NE(choice_error_of("choice = \"Turbo\"\nalternative = \"Big turbo\"",
                              "choice = \"Pump\"\nalternative = \"Big turbo\"")
                  .find("no choice 'Pump'"),
              std::string::npos);
    EXPECT_NE(
        choice_error_of("choice = \"Turbo\"\nalternative = \"Big turbo\"\n", "choice = \"Turbo\"\n")
            .find("or neither"),
        std::string::npos);
    // Builds that could count together.
    EXPECT_NE(
        choice_error_of("choice = \"Turbo\"\nalternative = \"Big turbo\"\n", "").find("twice"),
        std::string::npos);
    EXPECT_NE(choice_error_of("name = \"Weights\"", "name = \"Big build\"")
                  .find("'Big build' is listed twice"),
              std::string::npos);
}

}  // namespace
}  // namespace tp::data
