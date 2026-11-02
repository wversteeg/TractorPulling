#include "tp/data/garage/garage_loader.hpp"

#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

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

}  // namespace
}  // namespace tp::data
