#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace tp::field {
namespace {

// Sport 4.5 t's turbos (garages/sport_4500.toml): the small, quick turbo
// the stock tractor comes with and the 68 mm one, each on the fully built
// tractor as built, in its best gear, against the class's sled on each of
// the season's tracks. The 68 mm turbo is the upgrade, and it makes a
// significant difference: over the season it pulls on average well further,
// clearly further on most tracks (on the grippiest both make a full pull),
// and the small one is never clearly further. A rule of thumb, not track by
// track: on a loose track, where the tires spin before either engine runs
// out, the two may come within a metre either way. (The season visits some
// tracks twice; each visit counts.)
TEST(TurboChoice, TheSixtyEightMillimetreTurboIsTheUpgrade) {
    const auto loaded =
        data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / "sport_4500_top.toml");
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    const data::SeasonDefinition& d = loaded.value();
    const data::EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    ASSERT_EQ(catalogue.choice_count, 1U);
    ASSERT_EQ(catalogue.choices[0].alternative_count, 2U);

    garage::Career full = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        full.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
    }
    full.owned[0] = 0b10;
    // A clear win: more than the spot on the track decides [m].
    constexpr double kClearly_m = 1.0;
    // A significant difference: what the 68 mm turbo gains on average over
    // the visits where a difference can show [m], a place or more in a close
    // field.
    constexpr double kSignificant_m = 2.0;
    double gained_m = 0.0;
    int showing = 0;
    int small_wins = 0;
    int large_wins = 0;
    int visits = 0;
    std::string distances;
    for (const data::SeasonRound& round : d.rounds) {
        const Venue venue = data::venue(e.sled, round.track);
        const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
        std::array<double, 2> distance_m{};
        for (std::size_t turbo = 0; turbo < distance_m.size(); ++turbo) {
            full.fitted[0] = static_cast<int>(turbo);
            const std::optional<vehicle::Params> tractor =
                garage::equipped(e.reference.tractor, catalogue, full);
            ASSERT_TRUE(tractor.has_value());
            Puller puller = e.reference;
            puller.tractor = tractor.value_or(e.reference.tractor);
            const std::optional<GearTrial> trial =
                best_gear_trial(puller, venue.sled, venue.track, e.variation.lowest_gear,
                                e.variation.highest_gear, rules);
            ASSERT_TRUE(trial.has_value());
            distance_m[turbo] = trial.has_value() ? trial->distance_m : 0.0;
        }
        ++visits;
        // Both at the run's limit, the track shows no difference.
        if (std::ranges::min(distance_m) < rules.track_length_m) {
            gained_m += distance_m[1] - distance_m[0];
            ++showing;
        }
        small_wins += distance_m[0] > distance_m[1] + kClearly_m ? 1 : 0;
        large_wins += distance_m[1] > distance_m[0] + kClearly_m ? 1 : 0;
        distances += round.name + ": small " + std::to_string(distance_m[0]) + " m, 68 mm " +
                     std::to_string(distance_m[1]) + " m\n";
    }
    EXPECT_EQ(small_wins, 0) << distances;
    EXPECT_GT(2 * large_wins, visits) << distances;
    ASSERT_GT(showing, 0);
    EXPECT_GT(gained_m / showing, kSignificant_m) << distances;
}

}  // namespace
}  // namespace tp::field
