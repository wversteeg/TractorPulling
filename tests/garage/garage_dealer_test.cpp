#include "tp/core/garage/garage.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "../rules/reference_class.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::garage {
namespace {

// A garage with nothing but its dealer: three models and four paints.
Catalogue dealer_catalogue() {
    Catalogue c;
    c.start_money_eur = 2500;
    c.dealer.model_cost_eur = {1500, 2000, 2500};
    c.dealer.model_count = 3;
    c.dealer.paint_count = 4;
    c.dealer.paint_cost_eur = 300;
    return c;
}

// A career starts without its tractor where the class has a dealer, and
// with it (as before the dealers) where it has none.
TEST(GarageDealer, ACareerWaitsForItsTractorWhereThereIsADealer) {
    const Catalogue catalogue = dealer_catalogue();
    const Career career = new_career(catalogue);
    EXPECT_EQ(career.model, kNoModel);
    EXPECT_TRUE(needs_tractor(catalogue, career));
    EXPECT_FALSE(needs_tractor(Catalogue{}, new_career(Catalogue{})));
}

// A model costs its price from the money, once; the tractor then comes in
// the paint picked.
TEST(GarageDealer, BuysAModelOnceWithTheMoney) {
    const Catalogue catalogue = dealer_catalogue();
    Career career = new_career(catalogue);
    EXPECT_EQ(buy_model(career, catalogue, 3, 0), BuyError::kNoSuchUpgrade);
    EXPECT_EQ(buy_model(career, catalogue, 0, 4), BuyError::kNoSuchUpgrade);
    EXPECT_EQ(buy_model(career, catalogue, 1, 2), std::nullopt);
    EXPECT_EQ(career.model, 1);
    EXPECT_EQ(career.paint, 2);
    EXPECT_EQ(career.money_eur, 500);
    EXPECT_FALSE(needs_tractor(catalogue, career));
    EXPECT_EQ(buy_model(career, catalogue, 0, 0), BuyError::kOwned) << "one tractor per class";
    EXPECT_EQ(career.money_eur, 500);
}

// A dearer model than the money waits; the cheapest is never dearer than
// all the money there is, so a season can always start.
TEST(GarageDealer, TheCheapestModelIsWithinReach) {
    const Catalogue catalogue = dealer_catalogue();
    Career career = new_career(catalogue);
    career.money_eur = 1800;
    EXPECT_EQ(model_price_eur(catalogue, 1, career.money_eur), 2000);
    EXPECT_EQ(buy_model(career, catalogue, 1, 0), BuyError::kNotEnoughMoney);
    EXPECT_EQ(career.model, kNoModel);
    EXPECT_EQ(model_price_eur(catalogue, 0, career.money_eur), 1500) << "within the money";
    career.money_eur = 400;
    EXPECT_EQ(model_price_eur(catalogue, 0, career.money_eur), 400);
    EXPECT_EQ(model_price_eur(catalogue, 2, career.money_eur), 2500);
    EXPECT_EQ(buy_model(career, catalogue, 0, 3), std::nullopt);
    EXPECT_EQ(career.money_eur, 0);
    EXPECT_EQ(model_price_eur(catalogue, 5, 1000), 0) << "no such model";
}

// Another paint costs the dealer's paint price; the same paint, or a
// tractor not bought, cannot be painted.
TEST(GarageDealer, RepaintsForThePaintPrice) {
    const Catalogue catalogue = dealer_catalogue();
    Career career = new_career(catalogue);
    EXPECT_EQ(repaint(career, catalogue, 1), BuyError::kOwned) << "not bought yet";
    ASSERT_EQ(buy_model(career, catalogue, 0, 0), std::nullopt);
    EXPECT_EQ(repaint(career, catalogue, 0), BuyError::kOwned) << "painted so already";
    EXPECT_EQ(repaint(career, catalogue, 4), BuyError::kNoSuchUpgrade);
    EXPECT_EQ(repaint(career, catalogue, 3), std::nullopt);
    EXPECT_EQ(career.paint, 3);
    EXPECT_EQ(career.money_eur, 1000 - 300);
    career.money_eur = 299;
    EXPECT_EQ(repaint(career, catalogue, 1), BuyError::kNotEnoughMoney);
    EXPECT_EQ(career.paint, 3);
}

TEST(GarageDealer, RefusesNonsense) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const rules::ClassRules class_rules = rules::testing::ntto_standard_3500();
    const auto error_with = [&](auto change) {
        Catalogue c = dealer_catalogue();
        change(c);
        return validate(c, tractor, class_rules);
    };
    EXPECT_EQ(error_with([](Catalogue&) {}), std::nullopt);
    EXPECT_EQ(error_with([](Catalogue& c) { c.dealer.model_count = kMaxModels + 1; }),
              ParamError::kDealer);
    EXPECT_EQ(error_with([](Catalogue& c) { c.dealer.paint_count = kMaxPaints + 1; }),
              ParamError::kDealer);
    EXPECT_EQ(error_with([](Catalogue& c) { c.dealer.paint_count = 0; }), ParamError::kDealer)
        << "models without paints";
    EXPECT_EQ(error_with([](Catalogue& c) { c.dealer.model_cost_eur[2] = -1; }),
              ParamError::kDealer);
    EXPECT_EQ(error_with([](Catalogue& c) { c.dealer.paint_cost_eur = -1; }), ParamError::kDealer);
}

}  // namespace
}  // namespace tp::garage
