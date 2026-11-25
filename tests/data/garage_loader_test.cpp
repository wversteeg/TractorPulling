#include "tp/data/garage/garage_loader.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/files.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
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

// The turbo's levels are in bar in the file, Pa in the core.
TEST(GarageLoader, ReadsTheTurboInBar) {
    const auto result =
        parse_garage(with("part = \"load_downshift_gears\"\nname = \"Powershift\"\n"
                          "levels = [{ value = 0.0, cost_eur = 0, label = \"None\" }]",
                          "part = \"boost_bar\"\nname = \"Turbo\"\n"
                          "levels = [{ value = 3.0, cost_eur = 0, label = \"Stock\" }, "
                          "{ value = 6.0, cost_eur = 18000, label = \"Big\" }]"),
                     "garage.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const garage::Upgrade& turbo = result.value().catalogue.upgrades[1];
    EXPECT_EQ(turbo.part, garage::Part::kBoostPressure);
    EXPECT_EQ(turbo.levels[1].value, units::bar_to_pa(6.0));
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
    EXPECT_NE(error_of(with("name = \"Drawbar\"\n", "name = \"Drawbar\"\nformerly = \"Hitch\"\n"))
                  .find("garage.upgrades.formerly"),
              std::string::npos)
        << "a list";
    EXPECT_NE(error_of(with("name = \"Drawbar\"\n", "name = \"Drawbar\"\nformerly = [\"\"]\n"))
                  .find("garage.upgrades.formerly"),
              std::string::npos)
        << "of names";
    EXPECT_EQ(
        error_of(with("name = \"Drawbar\"\n", "name = \"Drawbar\"\nformerly = [\"Hitch\"]\n")), "");
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

// The shipped Supersport garage: each final drive says which tires it is
// made for, by the choices' names.
TEST(GarageLoader, ReadsWhatAFinalDriveSuits) {
    const auto result = load_garage(
        (std::filesystem::path{TP_DATA_DIR} / "garages" / "supersport_3600.toml").generic_string(),
        read_from_disk);
    ASSERT_TRUE(result.ok()) << result.error().message;
    const GarageDefinition& g = result.value();
    ASSERT_EQ(g.catalogue.choice_count, 3U);
    EXPECT_EQ(g.choice_texts[1].name, "Rear tires");
    const garage::Choice& drive = g.catalogue.choices[2];
    EXPECT_EQ(drive.alternatives[0].suits_choice, 1);
    EXPECT_EQ(drive.alternatives[0].suits_alternative, 0) << "short: the farm tires";
    EXPECT_EQ(drive.alternatives[1].suits_choice, 1);
    EXPECT_EQ(drive.alternatives[1].suits_alternative, 1) << "tall: the puller tires";
    EXPECT_EQ(g.catalogue.choices[0].alternatives[1].suits_choice, garage::kNoChoice);
}

// What an alternative suits is named by its label: a value that is not a
// string says so, rather than naming an empty alternative.
TEST(GarageLoader, RefusesASuitThatIsNotALabel) {
    const std::string garage_path =
        (std::filesystem::path{TP_DATA_DIR} / "garages" / "supersport_3600.toml").generic_string();
    const ReadFile read = [&](const std::string& path) -> std::optional<std::string> {
        std::optional<std::string> text = read_from_disk(path);
        if (text && path == garage_path) {
            const std::string from = R"("Rear tires" = "Puller tires")";
            const auto pos = text->find(from);
            EXPECT_NE(pos, std::string::npos) << "test bug: the suit not found";
            if (pos != std::string::npos) {
                text->replace(pos, from.size(), R"("Rear tires" = 1)");
            }
        }
        return text;
    };
    const auto result = load_garage(garage_path, read);
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("must be one of its alternatives' labels (a string)"),
              std::string::npos)
        << result.error().message;
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
    EXPECT_NE(choice_error_of("label = \"Big turbo\" }",
                              "label = \"Big turbo\", suits = { \"Turbo\" = \"Small turbo\" } }")
                  .find("no other choice 'Turbo'"),
              std::string::npos)
        << "made for its own choice";
    EXPECT_NE(choice_error_of("label = \"Big turbo\" }",
                              "label = \"Big turbo\", suits = { \"Tires\" = \"Wide\" } }")
                  .find("garage.choices.alternatives.suits"),
              std::string::npos);
    EXPECT_NE(choice_error_of("name = \"Weights\"", "name = \"Big build\"")
                  .find("'Big build' is listed twice"),
              std::string::npos);
}

// The pulling hitch's holes are 2 cm apart, counted from the drawbar bought
// down to the farm drawbar's height (the setup screen's HITCH_STEP_M), so
// every drawbar the garages sell sits a whole number of holes above the
// first level's: one only 1 cm up would leave no hole to choose below it.
TEST(GarageLoader, ShippedDrawbarsSitOnWholeHoles) {
    constexpr double kHoleStep_m = 0.02;
    constexpr double kTolerance_m = 1e-9;
    const std::filesystem::path garages = std::filesystem::path{TP_DATA_DIR} / "garages";
    int drawbars = 0;
    for (const auto& file : std::filesystem::directory_iterator{garages}) {
        const auto result = load_garage(file.path().generic_string(), read_from_disk);
        ASSERT_TRUE(result.ok()) << result.error().message;
        for (const garage::Upgrade& upgrade : result.value().catalogue.offer()) {
            if (upgrade.part != garage::Part::kHitchHeight) {
                continue;
            }
            ++drawbars;
            const double first_m = upgrade.levels[0].value;
            for (std::size_t i = 1; i < upgrade.level_count; ++i) {
                const double holes = (upgrade.levels[i].value - first_m) / kHoleStep_m;
                EXPECT_NEAR(holes, std::round(holes), kTolerance_m / kHoleStep_m)
                    << file.path().filename() << " level " << i;
                EXPECT_GE(holes, 1.0 - kTolerance_m) << file.path().filename() << " level " << i;
            }
        }
    }
    EXPECT_GT(drawbars, 0);
}

constexpr std::string_view kDealer = R"(
[garage.dealer]
paint_cost_eur = 300

[[garage.dealer.models]]
name = "Veldmeester"
style = "farm_classic"
cost_eur = 1500
about = "The all-rounder."

[[garage.dealer.models]]
name = "Oldtimer"
style = "farm_vintage"
cost_eur = 1750
about = "A classic."

[[garage.dealer.paints]]
name = "Field red"
paint = "#b3241c"
trim = "#F2F0E8"
rims = "#e8dcb4"
)";

std::string dealer_with(std::string_view from, std::string_view to) {
    std::string text = std::string{kGarage} + std::string{kDealer};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

TEST(GarageLoader, ReadsTheDealer) {
    const auto result = parse_garage(dealer_with("", ""), "garage.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const GarageDefinition& g = result.value();
    const garage::Dealer& dealer = g.catalogue.dealer;
    ASSERT_EQ(dealer.model_count, 2U);
    EXPECT_EQ(dealer.model_cost_eur[1], 1750);
    EXPECT_EQ(dealer.paint_count, 1U);
    EXPECT_EQ(dealer.paint_cost_eur, 300);
    ASSERT_EQ(g.model_texts.size(), 2U);
    EXPECT_EQ(g.model_texts[1].name, "Oldtimer");
    EXPECT_EQ(g.model_texts[1].style, "farm_vintage");
    EXPECT_EQ(g.model_texts[0].about, "The all-rounder.");
    ASSERT_EQ(g.paint_texts.size(), 1U);
    EXPECT_EQ(g.paint_texts[0].trim, "#F2F0E8");
    // Without a dealer, none: the class's tractor comes with the career.
    const auto bare = parse_garage(kGarage, "garage.toml");
    ASSERT_TRUE(bare.ok());
    EXPECT_EQ(bare.value().catalogue.dealer.model_count, 0U);
}

TEST(GarageLoader, RefusesNonsenseAtTheDealer) {
    const auto error = [](std::string_view from, std::string_view to) {
        return error_of(dealer_with(from, to));
    };
    EXPECT_NE(error("paint_cost_eur = 300", "paint_cost_eur = -1").find("paint_cost_eur"),
              std::string::npos);
    EXPECT_NE(error("cost_eur = 1750", "cost_eur = -1").find("must not be negative"),
              std::string::npos);
    EXPECT_NE(error("name = \"Oldtimer\"", "name = \"Veldmeester\"").find("listed twice"),
              std::string::npos);
    EXPECT_NE(error("about = \"A classic.\"", "").find("every model needs"), std::string::npos);
    EXPECT_NE(error("rims = \"#e8dcb4\"", "rims = \"cream\"").find("#rrggbb"), std::string::npos);
    EXPECT_NE(error("paint = \"#b3241c\"", "paint = \"#b3241\"").find("#rrggbb"),
              std::string::npos);
    EXPECT_NE(error("[[garage.dealer.paints]]", "[[garage.dealer.nothing]]").find("paints"),
              std::string::npos);
}

// Each class's dealer: five models, the cheapest within the money a career
// starts the class with (all of it at the start, none after: the wallet
// carries over), all the same price (they differ only in looks), and a paint job a small
// part of the cheapest.
TEST(GarageLoader, ShippedDealersSellFiveModels) {
    constexpr std::size_t kModels = 5;
    constexpr std::int64_t kPaintShareOfCheapest = 5;
    for (const char* name : {"standard_3500", "sport_2500", "sport_4500"}) {
        const std::filesystem::path file =
            std::filesystem::path{TP_DATA_DIR} / "garages" / (std::string{name} + ".toml");
        const auto result = load_garage(file.generic_string(), read_from_disk);
        ASSERT_TRUE(result.ok()) << result.error().message;
        const garage::Dealer& dealer = result.value().catalogue.dealer;
        ASSERT_EQ(dealer.model_count, kModels) << name;
        // Every model costs the same: they differ only in looks.
        for (std::size_t m = 1; m < kModels; ++m) {
            EXPECT_EQ(dealer.model_cost_eur[m], dealer.model_cost_eur[0]) << name;
        }
        EXPECT_GE(dealer.paint_count, 8U) << name;
        EXPECT_GT(dealer.paint_cost_eur, 0) << name;
        EXPECT_LE(dealer.paint_cost_eur * kPaintShareOfCheapest, dealer.model_cost_eur[0]) << name;
    }
    // The career's first class: the cheapest leaves the € 1,000 the garage
    // was balanced for (docs/career.md).
    constexpr std::int64_t kGarageMoney_eur = 1000;
    const auto standard =
        load_garage(std::string{TP_DATA_DIR} + "/garages/standard_3500.toml", read_from_disk);
    ASSERT_TRUE(standard.ok()) << standard.error().message;
    const garage::Catalogue& first = standard.value().catalogue;
    EXPECT_EQ(first.start_money_eur - first.dealer.model_cost_eur[0], kGarageMoney_eur);
}

}  // namespace
}  // namespace tp::data
