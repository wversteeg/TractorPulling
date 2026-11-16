#include "tp/data/garage/career_names.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {
namespace {

// A garage with the given upgrades and a turbo choice of the given
// alternatives (two shipped engines stand in for them), as if it were in
// data/garages/.
GarageDefinition garage_of(std::string_view upgrades, std::string_view alternatives) {
    std::string text = "[garage]\nstart_money_eur = 0\n";
    text += "[[garage.choices]]\nslot = \"engine\"\nname = \"Turbo\"\nalternatives = [";
    text += alternatives;
    text += "]\n";
    text += upgrades;
    const std::string garages = (std::filesystem::path{TP_DATA_DIR} / "garages").generic_string();
    auto result = parse_garage(text, "garage.toml", garages, read_from_disk);
    EXPECT_TRUE(result.ok()) << result.error().message;
    return result.ok() ? result.value() : GarageDefinition{};
}

constexpr std::string_view kWeightsAndPump = R"(
[[garage.upgrades]]
part = "ballast_kg"
name = "Weights"
levels = [{ value = 0.0, cost_eur = 0, label = "a" }, { value = 50.0, cost_eur = 1, label = "b" }]
[[garage.upgrades]]
part = "engine_torque"
name = "Pump"
levels = [{ value = 0.5, cost_eur = 0, label = "a" }, { value = 0.6, cost_eur = 1, label = "b" }]
)";

constexpr std::string_view kTwoTurbos =
    R"({ file = "../engines/sport_4500.toml", cost_eur = 0, label = "Small" },
       { file = "../engines/sport_4500.toml", cost_eur = 5, label = "Big" })";

constexpr std::string_view kThreeTurbos =
    R"({ file = "../engines/sport_4500.toml", cost_eur = 0, label = "Stock" },
       { file = "../engines/sport_4500.toml", cost_eur = 5, label = "Huge" },
       { file = "../engines/sport_4500.toml", cost_eur = 5, label = "Big" })";

// Named and read back in the same garage, the career is what it was.
TEST(CareerNames, ReadsBackWhatItNamed) {
    const GarageDefinition g = garage_of(kWeightsAndPump, kTwoTurbos);
    const garage::Career career{.money_eur = 77, .levels = {1, 0}, .fitted = {1}, .owned = {0b10}};
    SavedCareer named;
    named.money_eur = career.money_eur;
    name_career(career, g, named);
    ASSERT_EQ(named.choices.size(), 1U);
    EXPECT_EQ(named.choices[0].fitted, "Big");
    EXPECT_EQ(named.choices[0].owned, std::vector<std::string>{"Big"});
    const garage::Career back = career_from_names(named, g);
    EXPECT_EQ(back.money_eur, 77);
    EXPECT_EQ(back.levels, career.levels);
    EXPECT_EQ(back.fitted, career.fitted);
    EXPECT_EQ(back.owned, career.owned);
}

// Into another garage by name: the upgrades and alternatives it offers
// keep their levels and ownership wherever they sit, what it does not
// offer stays behind.
TEST(CareerNames, CarriesByName) {
    const GarageDefinition from = garage_of(kWeightsAndPump, kTwoTurbos);
    const GarageDefinition to = garage_of(R"(
[[garage.upgrades]]
part = "engine_torque"
name = "Pump"
levels = [{ value = 0.5, cost_eur = 0, label = "a" }, { value = 0.6, cost_eur = 1, label = "b" }]
)",
                                          kThreeTurbos);
    const garage::Career career{.money_eur = 5, .levels = {1, 1}, .fitted = {1}, .owned = {0b10}};
    const garage::Career carried = carry_career(career, from, to);
    EXPECT_EQ(carried.money_eur, 5);
    EXPECT_EQ(carried.levels[0], 1) << "the pump";
    EXPECT_EQ(carried.levels[1], 0);
    EXPECT_EQ(carried.owned[0], 0b100) << "the big turbo, third here";
    EXPECT_EQ(carried.fitted[0], 2);
}

// A save that fits an alternative it does not own, or one there is not,
// has the first fitted; an unknown choice is left behind.
TEST(CareerNames, FitsOnlyWhatIsOwned) {
    const GarageDefinition g = garage_of(kWeightsAndPump, kTwoTurbos);
    SavedCareer saved;
    saved.choices = {{.name = "Turbo", .fitted = "Big", .owned = {}},
                     {.name = "Gearbox", .fitted = "Tall", .owned = {"Tall"}}};
    garage::Career career = career_from_names(saved, g);
    EXPECT_EQ(career.fitted[0], 0);
    EXPECT_EQ(career.owned[0], 0);
    saved.choices = {{.name = "Turbo", .fitted = "Huge", .owned = {"Big", "Huge"}}};
    career = career_from_names(saved, g);
    EXPECT_EQ(career.fitted[0], 0);
    EXPECT_EQ(career.owned[0], 0b10);
}

constexpr std::string_view kDealer = R"(
[garage.dealer]
paint_cost_eur = 1
models = [
    { name = "Classic", style = "farm_classic", cost_eur = 10, about = "a" },
    { name = "Low", style = "farm_low", cost_eur = 20, about = "b" },
]
paints = [
    { name = "Red", paint = "#ff0000", trim = "#ffffff", rims = "#ffffff" },
    { name = "Blue", paint = "#0000ff", trim = "#ffffff", rims = "#ffffff" },
]
)";

// The tractor's model and paint go by the dealer's names: read back as
// they were, a model the dealer does not sell is not bought (to buy
// again), a paint it does not offer its first, and a save from before the
// dealers has the tractor the player had, the dealer's first model.
TEST(CareerNames, KeepsTheModelAndPaintByName) {
    const GarageDefinition g = garage_of(std::string{kWeightsAndPump} + std::string{kDealer},
                                         kTwoTurbos);
    garage::Career career = garage::new_career(g.catalogue);
    career.model = 1;
    career.paint = 1;
    SavedCareer named;
    name_career(career, g, named);
    ASSERT_TRUE(named.look.has_value());
    EXPECT_EQ(named.look.value_or(SavedLook{}).model, "Low");
    EXPECT_EQ(named.look.value_or(SavedLook{}).paint, "Blue");
    garage::Career back = career_from_names(named, g);
    EXPECT_EQ(back.model, 1);
    EXPECT_EQ(back.paint, 1);
    named.look = SavedLook{.model = "Gone", .paint = "Green"};
    back = career_from_names(named, g);
    EXPECT_EQ(back.model, garage::kNoModel);
    EXPECT_EQ(back.paint, 0);
    named.look.reset();
    back = career_from_names(named, g);
    EXPECT_EQ(back.model, 0) << "before the dealers";
    EXPECT_EQ(back.paint, 0);
    // Not bought yet: named "", read back as not bought.
    name_career(garage::new_career(g.catalogue), g, named);
    EXPECT_EQ(named.look.value_or(SavedLook{.model = "x", .paint = ""}).model, "");
    EXPECT_EQ(career_from_names(named, g).model, garage::kNoModel);
    // Without a dealer there is nothing to buy, whatever the save says.
    const GarageDefinition bare = garage_of(kWeightsAndPump, kTwoTurbos);
    named.look.reset();
    EXPECT_EQ(career_from_names(named, bare).model, garage::kNoModel);
    EXPECT_FALSE(garage::needs_tractor(bare.catalogue, career_from_names(named, bare)));
}

}  // namespace
}  // namespace tp::data
