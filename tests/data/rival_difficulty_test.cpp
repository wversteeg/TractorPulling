#include "tp/core/field/live_season.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>

namespace tp::field {
namespace {

// The game's event (ten pullers) at the season's first round, as shipped;
// a few seeds, each a different field.
constexpr std::array<std::uint64_t, 3> kSeeds{1, 2, 3};
constexpr int kGamePullers = 10;
constexpr std::int64_t kSlice = 1'000'000;

data::SeasonDefinition season_named(const char* file) {
    auto season = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / file);
    if (!season) {
        throw std::runtime_error(season.error().message);
    }
    return season.value();
}

data::SeasonDefinition club_season() {
    return season_named("club_season.toml");
}

// Where the player ends at the first event with this career's tractor,
// driving as the reference driver does and always in their best gear.
// The pulling gears the player tries: the field's (`lowest` 0), or these.
struct Gears {
    int lowest = 0;
    int highest = 0;
};

int place(const data::SeasonDefinition& d, const garage::Career& career, std::uint64_t seed,
          Gears gears = {}) {
    const data::EventDefinition& e = d.event;
    const std::optional<LiveSeason> created =
        LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull,
                           d.points, kGamePullers, static_cast<int>(d.rounds.size()), seed);
    if (!created) {
        throw std::runtime_error("the season was refused");
    }
    const auto season = std::make_unique<LiveSeason>(*created);
    Puller player = season->player();
    const std::optional<vehicle::Params> tractor =
        garage::equipped(e.reference.tractor, d.garage.catalogue, career);
    if (!tractor) {
        throw std::runtime_error("the career's tractor was refused");
    }
    player.tractor = *tractor;
    if (!season->start_event(data::venue(e.sled, d.rounds.front().track))) {
        throw std::runtime_error("the event was refused");
    }
    LiveEvent& event = *season->event();
    for (int guard = 0; guard < 10'000; ++guard) {
        switch (event.turn()) {
            case LiveEvent::Turn::kRivals:
                event.advance(kSlice);
                break;
            case LiveEvent::Turn::kPlayerPull: {
                const track::Params& track = event.venue().track;
                const int lowest = gears.lowest > 0 ? gears.lowest : e.variation.lowest_gear;
                const int highest = gears.lowest > 0 ? gears.highest : e.variation.highest_gear;
                player.driver.gear =
                    best_gear(player, event.sled(), track, lowest, highest, event.pull_rules())
                        .value_or(e.reference.driver.gear);
                event.record_player(run_pull(player, event.sled(), track, event.pull_rules())
                                        .value_or(run::Result{}),
                                    player.driver.gear);
                break;
            }
            case LiveEvent::Turn::kPlayerDecision:
                event.decide_player(event.suggested_keep());
                break;
            case LiveEvent::Turn::kFinished:
                for (const event::Standing& s : event.standings()) {
                    if (s.puller == LiveSeason::kPlayer) {
                        return s.place;
                    }
                }
                throw std::runtime_error("the player has no place");
            case LiveEvent::Turn::kFailed:
                throw std::runtime_error("the event failed");
        }
    }
    throw std::runtime_error("the event did not finish");
}

// Every part `steps` levels up from the start, or as far as it goes.
garage::Career levels_up(const garage::Catalogue& catalogue, int steps) {
    garage::Career career = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const int top = static_cast<int>(catalogue.upgrades[i].level_count) - 1;
        career.levels[i] = std::min(steps, top);
    }
    return career;
}

constexpr int kFullyUpgraded = static_cast<int>(garage::kMaxLevels);

// The field spans the garage, one step above it: its weakest rivals about
// a new career's tractor with a level or two bought, its strongest about a
// fully upgraded one. So a new tractor starts at the back, climbs with
// every level, and a fully upgraded one is on the podium, though not
// always on top.
struct Places {
    int best = 0;
    int worst = 0;
};

constexpr Places kNew{.best = 7, .worst = 10};
constexpr Places kOneLevelUp{.best = 5, .worst = 8};
// The sled operator judges by the strongest, so the field finishes close
// together: halfway may reach second on a good day.
constexpr Places kHalfway{.best = 2, .worst = 6};
constexpr Places kOnThePodium{.best = 1, .worst = 3};
// Halfway: every part at its second level of three or four.
constexpr int kHalfwayLevels = 2;

TEST(RivalDifficulty, ANewTractorStartsAtTheBackAndAFullyUpgradedOneIsOnThePodium) {
    const data::SeasonDefinition d = club_season();
    const garage::Catalogue& catalogue = d.garage.catalogue;
    const garage::Career full = levels_up(catalogue, kFullyUpgraded);
    ASSERT_TRUE(garage::fully_upgraded(catalogue, full));
    const auto expect_within = [](int place, Places places, std::uint64_t seed, const char* who) {
        EXPECT_GE(place, places.best) << who << ", seed " << seed;
        EXPECT_LE(place, places.worst) << who << ", seed " << seed;
    };
    for (const std::uint64_t seed : kSeeds) {
        expect_within(place(d, garage::new_career(catalogue), seed), kNew, seed, "new");
        expect_within(place(d, levels_up(catalogue, 1), seed), kOneLevelUp, seed, "one level up");
        expect_within(place(d, levels_up(catalogue, kHalfwayLevels), seed), kHalfway, seed,
                      "halfway");
        expect_within(place(d, full, seed), kOnThePodium, seed, "fully upgraded");
    }
}

// Every part bought to `share` of its levels (0 new, 1 fully upgraded),
// rounded to the nearest level.
garage::Career share_up(const garage::Catalogue& catalogue, double share) {
    garage::Career career = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const int top = static_cast<int>(catalogue.upgrades[i].level_count) - 1;
        career.levels[i] = static_cast<int>(std::lround(share * top));
    }
    return career;
}

// Each career level's field, in share of the class garage's stages bought
// (docs/career.md). The player picks their best of the pulling gears, as
// in the game.
constexpr Gears kPullingGears{.lowest = 5, .highest = 7};

struct Expectation {
    double share = 0.0;
    Places places;
};

void expect_level(const char* file, std::initializer_list<Expectation> expectations,
                  Gears gears = kPullingGears) {
    const data::SeasonDefinition d = season_named(file);
    for (const Expectation& x : expectations) {
        for (const std::uint64_t seed : kSeeds) {
            const int at = place(d, share_up(d.garage.catalogue, x.share), seed, gears);
            EXPECT_GE(at, x.places.best) << file << ", share " << x.share << ", seed " << seed;
            EXPECT_LE(at, x.places.worst) << file << ", share " << x.share << ", seed " << seed;
        }
    }
}

// The standard class, the career's warming-up at one level: a new tractor
// finishes near the back, and about two thirds of the garage wins.
TEST(RivalDifficulty, ANewStandardTractorIsNearTheBackAndTwoThirdsOfItsGarageWins) {
    expect_level("standard_3500.toml", {{.share = 0.0, .places = {.best = 7, .worst = 10}},
                                        {.share = 0.45, .places = {.best = 2, .worst = 4}},
                                        {.share = 0.66, .places = {.best = 1, .worst = 1}}});
}

// The Sport classes pull in tall, close gears (4th to 8th), launching with
// the clutch eased back while the revs are low: a gear too tall for the
// track bogs or stalls, one too low spins out, and the powershift saves a
// tall gear at the end. The player picks their best of them.
constexpr Gears kSportGears{.lowest = 4, .highest = 8};

// The Sport 2.5 t class's two levels (promo and top), its own tractor and
// garage: a new Sport tractor finishes at the back of promo and about half
// its garage wins it; promo's winner starts at the back at top, and only a
// nearly or fully built tractor wins there.
TEST(RivalDifficulty, ANewSportTractorIsAtTheBackOfPromoAndHalfItsGarageWinsIt) {
    expect_level("sport_2500_promo.toml",
                 {{.share = 0.0, .places = {.best = 7, .worst = 10}},
                  {.share = 0.45, .places = {.best = 1, .worst = 1}}},
                 kSportGears);
}

TEST(RivalDifficulty, OnlyAFullyBuiltSportTractorWinsAtTop) {
    expect_level("sport_2500_top.toml",
                 {{.share = 0.45, .places = {.best = 8, .worst = 10}},
                  {.share = 0.8, .places = {.best = 2, .worst = 7}},
                  {.share = 1.0, .places = {.best = 1, .worst = 1}}},
                 kSportGears);
}

// The Sport 4.5 t class's three levels, its own tractor and garage: a new
// tractor is at the back at belofte and about 45 % of its garage wins it;
// belofte's winner starts at the back of promo and about three quarters
// win it; promo's winner is at the back at top, where about 90 % of the
// garage reaches the podium and a fully built tractor wins.
TEST(RivalDifficulty, ANewSport45TractorIsLastAtBelofteAndAbout45PercentWinsIt) {
    expect_level("sport_4500_belofte.toml",
                 {{.share = 0.0, .places = {.best = 7, .worst = 10}},
                  {.share = 0.3, .places = {.best = 2, .worst = 7}},
                  {.share = 0.45, .places = {.best = 1, .worst = 1}}},
                 kSportGears);
}

TEST(RivalDifficulty, Sport45BeloftesWinnerIsAtTheBackOfPromoAndThreeQuartersWinIt) {
    expect_level("sport_4500_promo.toml",
                 {{.share = 0.45, .places = {.best = 6, .worst = 10}},
                  {.share = 0.66, .places = {.best = 1, .worst = 4}},
                  {.share = 0.75, .places = {.best = 1, .worst = 1}}},
                 kSportGears);
}

TEST(RivalDifficulty, OnlyAFullyBuiltSport45TractorIsSureToWinAtTop) {
    expect_level("sport_4500_top.toml",
                 {{.share = 0.75, .places = {.best = 7, .worst = 10}},
                  {.share = 0.9, .places = {.best = 1, .worst = 4}},
                  {.share = 1.0, .places = {.best = 1, .worst = 1}}},
                 kSportGears);
}

}  // namespace
}  // namespace tp::field
