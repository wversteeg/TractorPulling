#include "tp/core/field/good_buy.hpp"

#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::field {
namespace {

data::SeasonDefinition season_named(const std::string& file) {
    auto loaded = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / file);
    if (!loaded.ok()) {
        throw std::runtime_error(loaded.error().message);
    }
    return std::move(loaded).value();
}

// The crew chief's good buy for `career` on the season's first track, as
// the game asks for it (TractorPullEvent::get_good_buy).
std::optional<GoodBuy> advice_for(const data::SeasonDefinition& d, const garage::Career& career) {
    const data::EventDefinition& e = d.event;
    const Venue venue = data::venue(e.sled, d.rounds.front().track);
    const int gears = static_cast<int>(e.reference.tractor.driveline.gear_count);
    return good_buy(e.reference, d.garage.catalogue, career, venue.sled, venue.track,
                    e.variation.lowest_gear, std::min(e.variation.highest_gear + 1, gears),
                    trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
}

// How far `career`'s tractor pulls the good buy's weighing sled on the
// season's first track, in its best gear [m].
double weighed_m(const data::SeasonDefinition& d, const garage::Career& career) {
    const data::EventDefinition& e = d.event;
    const Venue venue = data::venue(e.sled, d.rounds.front().track);
    Puller puller = e.reference;
    puller.tractor =
        garage::equipped(e.reference.tractor, d.garage.catalogue, career).value_or(puller.tractor);
    const int gears = static_cast<int>(e.reference.tractor.driveline.gear_count);
    const auto trial =
        best_gear_trial(puller, sled::heavier(venue.sled, kGoodBuySteps), venue.track,
                        e.variation.lowest_gear, std::min(e.variation.highest_gear + 1, gears),
                        trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
    return trial ? trial->distance_m : 0.0;
}

// The crew chief's good buy for a new tractor with money to spend: one it
// can afford, and made it pulls as much further as the advice says.
TEST(GoodBuy, ANewTractorsGoodBuyPullsFurther) {
    const data::SeasonDefinition d = season_named("supersport_3600_belofte.toml");
    garage::Career career = garage::new_career(d.garage.catalogue);
    career.money_eur = 5000;
    const std::optional<GoodBuy> advice = advice_for(d, career);
    ASSERT_TRUE(advice.has_value());
    EXPECT_FALSE(advice->save_for);
    EXPECT_LE(advice->buy.cost_eur, career.money_eur);
    EXPECT_GE(advice->gain_m, kLeastGain_m);
    const garage::Career bought = with_buy(d.garage.catalogue, career, advice->buy);
    EXPECT_EQ(bought.money_eur, career.money_eur - advice->buy.cost_eur);
    EXPECT_NEAR(weighed_m(d, bought) - weighed_m(d, career), advice->gain_m, 1e-9);
}

// With no money the crew chief says what to save for: a buy the career
// cannot afford yet that gains.
TEST(GoodBuy, WithoutMoneyItSaysWhatToSaveFor) {
    const data::SeasonDefinition d = season_named("supersport_3600_belofte.toml");
    garage::Career career = garage::new_career(d.garage.catalogue);
    career.money_eur = 0;
    const std::optional<GoodBuy> advice = advice_for(d, career);
    ASSERT_TRUE(advice.has_value());
    EXPECT_TRUE(advice->save_for);
    EXPECT_GT(advice->buy.cost_eur, 0);
    EXPECT_GE(advice->gain_m, kLeastGain_m);
}

// Every part bought: nothing left to advise.
TEST(GoodBuy, NothingToAdviseWhenEverythingIsBought) {
    const data::SeasonDefinition d = season_named("supersport_3600_belofte.toml");
    const garage::Catalogue& catalogue = d.garage.catalogue;
    garage::Career career = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        career.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
    }
    career.owned.fill(std::numeric_limits<std::uint8_t>::max());
    EXPECT_FALSE(advice_for(d, career).has_value());
}

// A buy that cannot be made leaves the career as it is.
TEST(GoodBuy, ABuyThatCannotBeMadeChangesNothing) {
    const data::SeasonDefinition d = season_named("supersport_3600_belofte.toml");
    const garage::Career career = garage::new_career(d.garage.catalogue);
    const Buy no_such{.kind = BuyKind::kUpgrade, .index = garage::kMaxUpgrades - 1};
    const garage::Career after = with_buy(d.garage.catalogue, career, no_such);
    EXPECT_EQ(after.money_eur, career.money_eur);
    EXPECT_EQ(after.levels, career.levels);
}

}  // namespace
}  // namespace tp::field
