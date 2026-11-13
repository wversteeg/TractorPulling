#include "tp/core/garage/garage.hpp"

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "../rules/reference_class.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::garage {
namespace {

// Mirrors data/garages/ntto_standard_3500.toml, without the labels.
Catalogue reference_catalogue() {
    Catalogue c;
    c.start_money_eur = 1000;
    c.upgrades[0] = {.part = Part::kEngineTorque,
                     .levels = {{{.value = 0.90, .cost_eur = 0},
                                 {.value = 0.94, .cost_eur = 900},
                                 {.value = 0.97, .cost_eur = 1400},
                                 {.value = 1.0, .cost_eur = 2000}}},
                     .level_count = 4};
    c.upgrades[1] = {.part = Part::kRearTraction,
                     .levels = {{{.value = 0.86, .cost_eur = 0},
                                 {.value = 0.90, .cost_eur = 800},
                                 {.value = 0.93, .cost_eur = 1500},
                                 {.value = 0.95, .cost_eur = 2200}}},
                     .level_count = 4};
    c.upgrades[2] = {.part = Part::kBallastMass,
                     .levels = {{{.value = 100.0, .cost_eur = 0},
                                 {.value = 200.0, .cost_eur = 400},
                                 {.value = 300.0, .cost_eur = 400},
                                 {.value = 400.0, .cost_eur = 500}}},
                     .level_count = 4};
    c.upgrades[3] = {.part = Part::kHitchHeight,
                     .levels = {{{.value = 0.40, .cost_eur = 0},
                                 {.value = 0.45, .cost_eur = 300},
                                 {.value = 0.50, .cost_eur = 500}}},
                     .level_count = 3};
    c.upgrades[4] = {.part = Part::kLoadDownshift,
                     .levels = {{{.value = 0.0, .cost_eur = 0},
                                 {.value = 1.0, .cost_eur = 3000},
                                 {.value = 2.0, .cost_eur = 1500}}},
                     .level_count = 3};
    c.upgrades[5] = {.part = Part::kRatedSpeed,
                     .levels = {{{.value = units::rpm_to_rad_s(2200.0), .cost_eur = 0},
                                 {.value = units::rpm_to_rad_s(2350.0), .cost_eur = 1200},
                                 {.value = units::rpm_to_rad_s(2500.0), .cost_eur = 1800}}},
                     .level_count = 3};
    c.count = 6;
    return c;
}

const vehicle::Params kTractor = vehicle::testing::reference_params();
const rules::ClassRules kClass = rules::testing::ntto_standard_3500();

TEST(Garage, TheReferenceCatalogueFitsTheClass) {
    EXPECT_EQ(validate(reference_catalogue(), kTractor, kClass), std::nullopt);
}

// A new career starts on the first level of everything: a weaker tractor
// at the class's mass with most of its weights fixed at the back, on worse
// tires, a lower drawbar and no powershift step.
TEST(Garage, ANewCareerStartsOnAnOlderTractor) {
    const Catalogue catalogue = reference_catalogue();
    const Career career = new_career(catalogue);
    EXPECT_EQ(career.money_eur, 1000);
    const std::optional<vehicle::Params> start = equipped(kTractor, catalogue, career);
    ASSERT_TRUE(start.has_value());
    const vehicle::Params& t = start.value_or(kTractor);
    EXPECT_EQ(t.chassis.mass_kg, kTractor.chassis.mass_kg);
    EXPECT_EQ(t.chassis.ballast.mass_kg, 100.0) << "free to move";
    EXPECT_LT(t.chassis.cg_ahead_of_rear_axle_m, kTractor.chassis.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(t.rear_tire.traction.limit, 0.86);
    EXPECT_EQ(t.chassis.hitch_height_m, 0.40);
    EXPECT_EQ(t.driveline.load_downshift_gears, 0);
    EXPECT_LT(rules::pto_power_w(t), 0.92 * rules::pto_power_w(kTractor));
    EXPECT_FALSE(fully_upgraded(catalogue, career));
}

// Fully upgraded, the tractor is at the class's limits like the rivals'
// reference one, and beyond it: wider tires (0.95 against the reference's
// 0.93), two gears down under load against one, and the governor at the
// class's engine speed.
TEST(Garage, FullyUpgradedIsBeyondTheRivalsTractor) {
    const Catalogue catalogue = reference_catalogue();
    Career career = new_career(catalogue);
    career.levels = {3, 3, 3, 2, 2, 2};
    EXPECT_TRUE(fully_upgraded(catalogue, career));
    const std::optional<vehicle::Params> top = equipped(kTractor, catalogue, career);
    ASSERT_TRUE(top.has_value());
    const vehicle::Params& t = top.value_or(kTractor);
    EXPECT_EQ(t.chassis.mass_kg, kTractor.chassis.mass_kg);
    EXPECT_EQ(t.chassis.cg_ahead_of_rear_axle_m, kTractor.chassis.cg_ahead_of_rear_axle_m);
    // The governor moves where the engine gives its power, not its peak
    // (sampled over a wider speed range, so not to the last bit).
    EXPECT_NEAR(rules::pto_power_w(t), rules::pto_power_w(kTractor),
                1e-4 * rules::pto_power_w(kTractor));
    EXPECT_GT(t.engine.rated_omega_rad_s, kTractor.engine.rated_omega_rad_s);
    EXPECT_EQ(t.rear_tire.traction.limit, 0.95);
    EXPECT_EQ(t.driveline.load_downshift_gears, 2);
    EXPECT_EQ(t.driveline.load_shifts_per_pull, 1);
    EXPECT_TRUE(rules::check(kClass, t).ok());
}

// Each level costs its price from the one below, and only with the money.
TEST(Garage, BuysTheNextLevelWithTheMoney) {
    const Catalogue catalogue = reference_catalogue();
    Career career = new_career(catalogue);
    EXPECT_EQ(buy(career, catalogue, 0), std::nullopt);
    EXPECT_EQ(career.levels[0], 1);
    EXPECT_EQ(career.money_eur, 100);
    EXPECT_EQ(buy(career, catalogue, 0), BuyError::kNotEnoughMoney);
    EXPECT_EQ(career.levels[0], 1);
    EXPECT_EQ(career.money_eur, 100);
    EXPECT_EQ(buy(career, catalogue, 6), BuyError::kNoSuchUpgrade);
    career.money_eur = 10000;
    EXPECT_EQ(buy(career, catalogue, 3), std::nullopt);
    EXPECT_EQ(buy(career, catalogue, 3), std::nullopt);
    EXPECT_EQ(buy(career, catalogue, 3), BuyError::kTopLevel);
    EXPECT_EQ(career.levels[3], 2);
    EXPECT_EQ(career.money_eur, 10000 - 300 - 500);
}

// Fixed weights sit at the back: freeing fewer of the tractor's weights
// moves its centre of gravity back, the whole mass the same; freeing all
// of them gives the tractor back, bit for bit.
TEST(Garage, FreeWeightMovesTheCentreOfGravity) {
    const chassis::Params& built = kTractor.chassis;
    ASSERT_GT(built.ballast.mass_kg, 100.0);
    const chassis::Params fixed = chassis::with_free_weight(built, 100.0);
    EXPECT_EQ(fixed.mass_kg, built.mass_kg);
    EXPECT_DOUBLE_EQ(fixed.ballast.mass_kg, 100.0);
    EXPECT_LT(fixed.cg_ahead_of_rear_axle_m, built.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(chassis::validate(fixed), std::nullopt);
    // About the rear axle, a point mass's inertia is m x^2: moving the fixed
    // weights from where they were built to the rearmost place changes the
    // tractor's by exactly theirs (pitch and yaw alike).
    const chassis::Ballast& b = built.ballast;
    const double from_m = b.rear_ahead_of_rear_axle_m +
                          b.position * (b.front_ahead_of_rear_axle_m - b.rear_ahead_of_rear_axle_m);
    const double to_m = b.rear_ahead_of_rear_axle_m;
    const double fixed_kg = b.mass_kg - 100.0;
    const auto about_axle = [&](double about_cg, const chassis::Params& p) {
        return about_cg + p.mass_kg * p.cg_ahead_of_rear_axle_m * p.cg_ahead_of_rear_axle_m;
    };
    const double moved = fixed_kg * (to_m * to_m - from_m * from_m);
    EXPECT_NEAR(
        about_axle(fixed.pitch_inertia_kg_m2, fixed) - about_axle(built.pitch_inertia_kg_m2, built),
        moved, 1e-9 * built.pitch_inertia_kg_m2);
    EXPECT_NEAR(
        about_axle(fixed.yaw_inertia_kg_m2, fixed) - about_axle(built.yaw_inertia_kg_m2, built),
        moved, 1e-9 * built.yaw_inertia_kg_m2);
    // Moving the free weights still works, over a smaller range.
    const double reach_m = chassis::with_ballast_at(built, 1.0).cg_ahead_of_rear_axle_m -
                           chassis::with_ballast_at(built, 0.0).cg_ahead_of_rear_axle_m;
    const double fixed_reach_m = chassis::with_ballast_at(fixed, 1.0).cg_ahead_of_rear_axle_m -
                                 chassis::with_ballast_at(fixed, 0.0).cg_ahead_of_rear_axle_m;
    EXPECT_NEAR(fixed_reach_m, reach_m * 100.0 / built.ballast.mass_kg, 1e-9);
    // Unchanged, bit for bit, with all its weights free or nonsense.
    const double all_kg = built.ballast.mass_kg;
    EXPECT_EQ(chassis::with_free_weight(built, all_kg).cg_ahead_of_rear_axle_m,
              built.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(chassis::with_free_weight(built, all_kg + 1.0).cg_ahead_of_rear_axle_m,
              built.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(chassis::with_free_weight(built, -1.0).cg_ahead_of_rear_axle_m,
              built.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(chassis::with_free_weight(built, std::nan("")).cg_ahead_of_rear_axle_m,
              built.cg_ahead_of_rear_axle_m);
}

TEST(Garage, RefusesNonsense) {
    const auto error_with = [](auto change) {
        Catalogue c = reference_catalogue();
        change(c);
        return validate(c, kTractor, kClass);
    };
    EXPECT_EQ(error_with([](Catalogue& c) { c.count = kMaxUpgrades + 1; }), ParamError::kTooMany);
    EXPECT_EQ(error_with([](Catalogue& c) { c.start_money_eur = -1; }),
              ParamError::kStartMoneyNegative);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].level_count = 0; }),
              ParamError::kNoLevels);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].levels[2].value = std::nan(""); }),
              ParamError::kNotFinite);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].levels[2].cost_eur = -5; }),
              ParamError::kCostNegative);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].levels[0].cost_eur = 5; }),
              ParamError::kFirstLevelNotFree);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[2].part = Part::kEngineTorque; }),
              ParamError::kPartTwice);
    // More free weight than the tractor has weights.
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[2].levels[3].value = 450.0; }),
              ParamError::kValueInvalid);
    // Past the class: more power, a higher hitch.
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[0].levels[3].value = 1.05; }),
              ParamError::kOutsideTheClass);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[3].levels[2].value = 0.55; }),
              ParamError::kOutsideTheClass);
    EXPECT_EQ(error_with([](Catalogue& c) {
                  c.upgrades[5].levels[2].value = units::rpm_to_rad_s(2550.0);
              }),
              ParamError::kOutsideTheClass)
        << "2750 rpm unloaded";
    // Not a tractor at all.
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[4].levels[2].value = 1.5; }),
              ParamError::kValueInvalid);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[4].levels[2].value = 8.0; }),
              ParamError::kValueInvalid);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[0].levels[0].value = -1.0; }),
              ParamError::kValueInvalid);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].levels[0].value = -0.5; }),
              ParamError::kValueInvalid);
    EXPECT_EQ(error_with(
                  [](Catalogue& c) { c.upgrades[5].levels[1].value = units::rpm_to_rad_s(700.0); }),
              ParamError::kValueInvalid)
        << "below idle";
}

// The governor sets the rated speed, the unloaded speed with it, up to the
// class's 2700 rpm; the rest of the engine stays.
TEST(Garage, TheGovernorRaisesTheEngineSpeeds) {
    const Catalogue catalogue = reference_catalogue();
    Career career = new_career(catalogue);
    const std::optional<vehicle::Params> stock = equipped(kTractor, catalogue, career);
    career.levels[5] = 2;
    const std::optional<vehicle::Params> raised = equipped(kTractor, catalogue, career);
    ASSERT_TRUE(stock.has_value() && raised.has_value());
    const engine::Params& before = stock.value_or(kTractor).engine;
    const engine::Params& after = raised.value_or(kTractor).engine;
    EXPECT_EQ(before.rated_omega_rad_s, kTractor.engine.rated_omega_rad_s);
    EXPECT_EQ(after.rated_omega_rad_s, units::rpm_to_rad_s(2500.0));
    EXPECT_NEAR(after.high_idle_omega_rad_s, units::rpm_to_rad_s(2700.0), 1e-9);
    EXPECT_EQ(after.idle_omega_rad_s, before.idle_omega_rad_s);
    EXPECT_TRUE(rules::check(kClass, raised.value_or(kTractor)).ok());
}

TEST(Garage, PaysByPlace) {
    const std::array<std::int64_t, 3> prizes = {700, 500, 400};
    EXPECT_EQ(prize_eur(prizes, {.puller = 0, .place = 1}), 700);
    EXPECT_EQ(prize_eur(prizes, {.puller = 0, .place = 3}), 400);
    EXPECT_EQ(prize_eur(prizes, {.puller = 0, .place = 4}), 0);
    EXPECT_EQ(prize_eur(prizes, {.puller = 0, .place = 1, .disqualified = true}), 0);
    EXPECT_EQ(prize_eur(prizes, {.puller = 0, .place = 0}), 0);
}

}  // namespace
}  // namespace tp::garage
