#include "tp/core/engine/engine.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/rules/class_rules.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "../rules/reference_class.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::garage {
namespace {

const vehicle::Params kTractor = vehicle::testing::reference_params();
const rules::ClassRules kClass = rules::testing::ntto_standard_3500();

Alternative engine_alternative(const engine::Params& e, std::int64_t cost_eur) {
    Alternative a;
    a.engine = e;
    a.cost_eur = cost_eur;
    return a;
}

Alternative tire_alternative(const tire::Params& t, std::int64_t cost_eur) {
    Alternative a;
    a.rear_tire = t;
    a.cost_eur = cost_eur;
    return a;
}

Alternative driveline_alternative(const driveline::Params& d, std::int64_t cost_eur) {
    Alternative a;
    a.driveline = d;
    a.cost_eur = cost_eur;
    return a;
}

// A turbo that boosts sooner: the same engine, its boost from lower down.
engine::Params early_turbo() {
    engine::Params e = kTractor.engine;
    e.boost_onset_omega_rad_s *= 0.8;
    e.full_boost_omega_rad_s *= 0.8;
    e.turbo_time_constant_s *= 0.5;
    return e;
}

// The engine choice (the tractor's own turbo, or an early one at 1500
// euros), each with its own engine build.
Catalogue choice_catalogue() {
    Catalogue c;
    c.start_money_eur = 2000;
    c.choices[0].slot = Slot::kEngine;
    c.choices[0].alternatives[0] = engine_alternative(kTractor.engine, 0);
    c.choices[0].alternatives[1] = engine_alternative(early_turbo(), 1500);
    c.choices[0].alternative_count = 2;
    c.choice_count = 1;
    c.upgrades[0] = {.part = Part::kEngineTorque,
                     .levels = {{{.value = 0.9, .cost_eur = 0}, {.value = 1.0, .cost_eur = 400}}},
                     .level_count = 2,
                     .choice = 0,
                     .alternative = 0};
    c.upgrades[1] = {.part = Part::kEngineTorque,
                     .levels = {{{.value = 0.8, .cost_eur = 0}, {.value = 1.0, .cost_eur = 400}}},
                     .level_count = 2,
                     .choice = 0,
                     .alternative = 1};
    c.count = 2;
    return c;
}

const engine::Params& engine_of(const std::optional<vehicle::Params>& t) {
    return t.has_value() ? t->engine : kTractor.engine;
}

TEST(GarageChoice, TheCatalogueFitsTheClass) {
    EXPECT_EQ(validate(choice_catalogue(), kTractor, kClass), std::nullopt);
}

// A new career has the first alternative fitted, and the upgrade belonging
// to the other does not count.
TEST(GarageChoice, ANewCareerHasTheFirstAlternative) {
    const Catalogue catalogue = choice_catalogue();
    const Career career = new_career(catalogue);
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
    EXPECT_TRUE(owns(career, 0, 0));
    EXPECT_FALSE(owns(career, 0, 1));
    EXPECT_TRUE(counts(catalogue, career, 0));
    EXPECT_FALSE(counts(catalogue, career, 1));
    const std::optional<vehicle::Params> t = equipped(kTractor, catalogue, career);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(engine_of(t).boost_onset_omega_rad_s, kTractor.engine.boost_onset_omega_rad_s);
    EXPECT_EQ(engine_of(t).full_load_torque_nm.max_y(),
              0.9 * kTractor.engine.full_load_torque_nm.max_y());
}

// Bought, an alternative is fitted; it can be taken off and fitted again
// for nothing, and stays owned.
TEST(GarageChoice, BoughtOnceFittedAsOftenAsLiked) {
    const Catalogue catalogue = choice_catalogue();
    Career career = new_career(catalogue);
    EXPECT_EQ(fit(career, catalogue, 0, 1), FitError::kNotOwned);
    EXPECT_EQ(buy_alternative(career, catalogue, 0, 1), std::nullopt);
    EXPECT_EQ(career.money_eur, 500);
    EXPECT_TRUE(owns(career, 0, 1));
    EXPECT_EQ(fitted(catalogue, career, 0), 1U);
    EXPECT_EQ(buy_alternative(career, catalogue, 0, 1), BuyError::kOwned);
    EXPECT_EQ(buy_alternative(career, catalogue, 0, 0), BuyError::kOwned);
    EXPECT_EQ(fit(career, catalogue, 0, 0), std::nullopt);
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
    EXPECT_EQ(fit(career, catalogue, 0, 1), std::nullopt);
    EXPECT_EQ(fitted(catalogue, career, 0), 1U);
    EXPECT_EQ(career.money_eur, 500);
    EXPECT_EQ(fit(career, catalogue, 0, 2), FitError::kNoSuchAlternative);
    EXPECT_EQ(fit(career, catalogue, 1, 0), FitError::kNoSuchAlternative);
    EXPECT_EQ(buy_alternative(career, catalogue, 1, 0), BuyError::kNoSuchUpgrade);
}

TEST(GarageChoice, BuyingNeedsTheMoney) {
    const Catalogue catalogue = choice_catalogue();
    Career career = new_career(catalogue);
    career.money_eur = 1499;
    EXPECT_EQ(buy_alternative(career, catalogue, 0, 1), BuyError::kNotEnoughMoney);
    EXPECT_FALSE(owns(career, 0, 1));
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
    EXPECT_EQ(career.money_eur, 1499);
}

// The fitted alternative replaces the engine and its own build works on
// it; a build counts only while its alternative is fitted, and keeps its
// level while it is not.
TEST(GarageChoice, UpgradesWorkOnTheFittedAlternative) {
    const Catalogue catalogue = choice_catalogue();
    Career career = new_career(catalogue);
    career.money_eur = 10000;
    ASSERT_EQ(buy_alternative(career, catalogue, 0, 1), std::nullopt);
    const double top_nm = kTractor.engine.full_load_torque_nm.max_y();
    const std::optional<vehicle::Params> early = equipped(kTractor, catalogue, career);
    EXPECT_EQ(engine_of(early).boost_onset_omega_rad_s, early_turbo().boost_onset_omega_rad_s);
    EXPECT_NEAR(engine_of(early).full_load_torque_nm.max_y(), 0.8 * top_nm, 1e-9);
    EXPECT_FALSE(counts(catalogue, career, 0));
    ASSERT_EQ(buy(career, catalogue, 1), std::nullopt);
    EXPECT_NEAR(engine_of(equipped(kTractor, catalogue, career)).full_load_torque_nm.max_y(),
                top_nm, 1e-9);
    ASSERT_EQ(fit(career, catalogue, 0, 0), std::nullopt);
    EXPECT_FALSE(counts(catalogue, career, 1));
    EXPECT_EQ(career.levels[1], 1);
    EXPECT_NEAR(engine_of(equipped(kTractor, catalogue, career)).full_load_torque_nm.max_y(),
                0.9 * top_nm, 1e-9);
    EXPECT_EQ(engine_of(equipped(kTractor, catalogue, career)).boost_onset_omega_rad_s,
              kTractor.engine.boost_onset_omega_rad_s);
}

// A career that says it fits an alternative it does not own (or one there
// is not) has the first fitted.
TEST(GarageChoice, AnAlternativeNotOwnedIsNotFitted) {
    const Catalogue catalogue = choice_catalogue();
    Career career = new_career(catalogue);
    career.fitted[0] = 1;
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
    career.owned[0] = 0xFF;
    career.fitted[0] = 3;
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
    career.fitted[0] = -1;
    EXPECT_EQ(fitted(catalogue, career, 0), 0U);
}

TEST(GarageChoice, FullyUpgradedOwnsEveryAlternative) {
    const Catalogue catalogue = choice_catalogue();
    Career career = new_career(catalogue);
    career.levels = {1, 1};
    EXPECT_FALSE(fully_upgraded(catalogue, career));
    career.owned[0] = 0b10;
    EXPECT_TRUE(fully_upgraded(catalogue, career));
}

TEST(GarageChoice, RefusesNonsense) {
    const auto error_with = [](auto change) {
        Catalogue c = choice_catalogue();
        change(c);
        return validate(c, kTractor, kClass);
    };
    EXPECT_EQ(error_with([](Catalogue& c) { c.choice_count = kMaxChoices + 1; }),
              ParamError::kTooMany);
    EXPECT_EQ(error_with([](Catalogue& c) { c.choices[0].alternative_count = 0; }),
              ParamError::kNoAlternatives);
    EXPECT_EQ(error_with([](Catalogue& c) { c.choices[0].alternatives[1].cost_eur = -1; }),
              ParamError::kCostNegative);
    EXPECT_EQ(error_with([](Catalogue& c) { c.choices[0].alternatives[0].cost_eur = 1; }),
              ParamError::kFirstLevelNotFree);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].alternative = 2; }),
              ParamError::kNoSuchAlternative);
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].choice = 1; }),
              ParamError::kNoSuchAlternative);
    // Two engine builds that would count together.
    EXPECT_EQ(error_with([](Catalogue& c) { c.upgrades[1].choice = kNoChoice; }),
              ParamError::kPartTwice);
    EXPECT_EQ(error_with([](Catalogue& c) {
                  c.upgrades[0].choice = 0;
                  c.upgrades[0].alternative = 1;
              }),
              ParamError::kPartTwice);
    // An alternative without the slot's component, or one that is no engine.
    EXPECT_EQ(error_with([](Catalogue& c) { c.choices[0].alternatives[1].engine.reset(); }),
              ParamError::kValueInvalid);
    EXPECT_EQ(error_with([](Catalogue& c) { c.choices[0].slot = Slot::kRearTire; }),
              ParamError::kValueInvalid);
    // Every alternative is judged by the class, not only the first: an
    // engine over the class's PTO power with the build at its top.
    EXPECT_EQ(error_with([](Catalogue& c) {
                  c.choices[0].alternatives[1].engine =
                      engine::with_torque_scale(early_turbo(), 1.3);
              }),
              ParamError::kOutsideTheClass);
}

// The rear tire and driveline slots replace their components.
TEST(GarageChoice, TiresAndDrivelineSwap) {
    Catalogue c;
    tire::Params grippy = kTractor.rear_tire;
    grippy.traction.limit += 0.05;
    driveline::Params taller = kTractor.driveline;
    taller.final_drive_ratio *= 0.9;
    c.choices[0].slot = Slot::kRearTire;
    c.choices[0].alternatives[0] = tire_alternative(kTractor.rear_tire, 0);
    c.choices[0].alternatives[1] = tire_alternative(grippy, 100);
    c.choices[0].alternative_count = 2;
    c.choices[1].slot = Slot::kDriveline;
    c.choices[1].alternatives[0] = driveline_alternative(kTractor.driveline, 0);
    c.choices[1].alternatives[1] = driveline_alternative(taller, 100);
    c.choices[1].alternative_count = 2;
    c.choice_count = 2;
    EXPECT_EQ(validate(c, kTractor, kClass), std::nullopt);
    Career career = new_career(c);
    career.money_eur = 200;
    ASSERT_EQ(buy_alternative(career, c, 0, 1), std::nullopt);
    ASSERT_EQ(buy_alternative(career, c, 1, 1), std::nullopt);
    const std::optional<vehicle::Params> t = equipped(kTractor, c, career);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t.value_or(kTractor).rear_tire.traction.limit, grippy.traction.limit);
    EXPECT_EQ(t.value_or(kTractor).driveline.final_drive_ratio, taller.final_drive_ratio);
}

}  // namespace
}  // namespace tp::garage
