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

// The career's three levels of the standard class (docs/career.md), one
// tractor and garage for all three, in share of the garage's stages: a new
// tractor finishes around last in belofte, and the tractor wins it once it
// pulls 6th, with about 45 % bought; belofte's winner starts at the back of
// promo and about 75 % wins it; promo's winner is near the back at top,
// and only a fully upgraded tractor wins there. The player picks their best
// of the pulling gears, as in the game.
constexpr Gears kPullingGears{.lowest = 5, .highest = 7};

struct Expectation {
    double share = 0.0;
    Places places;
};

void expect_level(const char* file, std::initializer_list<Expectation> expectations) {
    const data::SeasonDefinition d = season_named(file);
    for (const Expectation& x : expectations) {
        for (const std::uint64_t seed : kSeeds) {
            const int at = place(d, share_up(d.garage.catalogue, x.share), seed, kPullingGears);
            EXPECT_GE(at, x.places.best) << file << ", share " << x.share << ", seed " << seed;
            EXPECT_LE(at, x.places.worst) << file << ", share " << x.share << ", seed " << seed;
        }
    }
}

TEST(RivalDifficulty, ANewTractorIsAroundLastInBelofteAndPulling6thWinsIt) {
    expect_level("standard_3500_belofte.toml",
                 {{.share = 0.0, .places = {.best = 7, .worst = 10}},
                  {.share = 0.45, .places = {.best = 1, .worst = 1}}});
}

TEST(RivalDifficulty, BeloftesWinnerStartsAtTheBackOfPromoAndThreeQuartersWinIt) {
    expect_level("standard_3500_promo.toml", {{.share = 0.45, .places = {.best = 6, .worst = 10}},
                                              {.share = 0.66, .places = {.best = 1, .worst = 4}},
                                              {.share = 0.8, .places = {.best = 1, .worst = 1}}});
}

TEST(RivalDifficulty, PromosWinnerIsNearTheBackAtTopAndOnlyAFullyUpgradedTractorWins) {
    expect_level("standard_3500_top.toml", {{.share = 0.66, .places = {.best = 7, .worst = 10}},
                                            {.share = 0.8, .places = {.best = 2, .worst = 5}},
                                            {.share = 1.0, .places = {.best = 1, .worst = 1}}});
}

}  // namespace
}  // namespace tp::field
