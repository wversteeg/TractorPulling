#include "tp/core/field/field.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/core/field/play.hpp"
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
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>

namespace tp::field {
namespace {

// The game's event (fifteen pullers) at the season's first round, as
// shipped; a few seeds, each a different field.
constexpr std::array<std::uint64_t, 3> kSeeds{1, 2, 3};
constexpr int kGamePullers = 15;
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
    if (!season->start_event(data::venue(e.sled, d.rounds.front().track), player.tractor)) {
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
// always on top: the strongest few are close, and the spot on the track
// (field::PullToPull) decides between them. On the 15 t sled the push-down
// stops every tractor within a few metres of each other, so the field
// finishes closer than it did on a light sled, and a step in the garage
// is worth more places in the middle of the field.
struct Places {
    int best = 0;
    int worst = 0;
};

constexpr Places kNew{.best = 12, .worst = 15};
// One level up: the middle of the field, the top four on a good day.
constexpr Places kOneLevelUp{.best = 4, .worst = 13};
// The sled operator sets the sled for the field's average, so the field
// finishes close together: halfway may reach second on a good day.
constexpr Places kHalfway{.best = 2, .worst = 10};
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

// Past this share of the garage the player owns every choice's
// alternatives (Sport 4.5 t's 68 mm turbo).
constexpr double kOwnsTheChoicesFrom = 0.5;

// Every part bought to `share` of its levels (0 new, 1 fully upgraded),
// rounded to the nearest level; from kOwnsTheChoicesFrom, every
// alternative owned too.
garage::Career share_up(const garage::Catalogue& catalogue, double share) {
    garage::Career career = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const int top = static_cast<int>(catalogue.upgrades[i].level_count) - 1;
        career.levels[i] = static_cast<int>(std::lround(share * top));
    }
    if (share >= kOwnsTheChoicesFrom) {
        career.owned.fill(std::numeric_limits<std::uint8_t>::max());
    }
    return career;
}

// `career` with the alternative of its one choice (if it has one) that
// takes the tractor furthest on the season's first track, in its best
// gear: the player fits the turbo for the day.
garage::Career fitted_for_the_day(const data::SeasonDefinition& d, garage::Career career,
                                  Gears gears) {
    const data::EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    if (catalogue.choice_count != 1) {
        return career;
    }
    const Venue venue = data::venue(e.sled, d.rounds.front().track);
    const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
    double furthest_m = -1.0;
    int best = 0;
    for (std::size_t a = 0; a < catalogue.choices[0].alternative_count; ++a) {
        if (!garage::owns(career, 0, a)) {
            continue;
        }
        career.fitted[0] = static_cast<int>(a);
        Puller puller = e.reference;
        puller.tractor =
            garage::equipped(e.reference.tractor, catalogue, career).value_or(e.reference.tractor);
        const std::optional<GearTrial> trial =
            best_gear_trial(puller, venue.sled, venue.track, gears.lowest, gears.highest, rules);
        if (trial && trial->distance_m > furthest_m) {
            furthest_m = trial->distance_m;
            best = static_cast<int>(a);
        }
    }
    career.fitted[0] = best;
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
            const int at =
                place(d, fitted_for_the_day(d, share_up(d.garage.catalogue, x.share), gears), seed,
                      gears);
            EXPECT_GE(at, x.places.best) << file << ", share " << x.share << ", seed " << seed;
            EXPECT_LE(at, x.places.worst) << file << ", share " << x.share << ", seed " << seed;
        }
    }
}

// The standard class, the career's warming-up at one level: a new tractor
// finishes near the back, and about two thirds of the garage fights for
// the win (the top four). The sled set for the field's average, not its
// strongest, lets the weakest gain a place now and then.
TEST(RivalDifficulty, ANewStandardTractorIsNearTheBackAndTwoThirdsOfItsGarageWins) {
    expect_level("standard_3500.toml", {{.share = 0.0, .places = {.best = 11, .worst = 15}},
                                        {.share = 0.45, .places = {.best = 2, .worst = 7}},
                                        {.share = 0.66, .places = {.best = 1, .worst = 4}}});
}

// The Sport classes pull in tall, close gears (3rd to 8th), launching with
// the clutch eased back while the revs are low: a gear too tall for the
// track bogs or stalls, one too low spins out, and the powershift saves a
// tall gear at the end. The player picks their best of them.
constexpr Gears kSportGears{.lowest = 3, .highest = 8};

// The Sport 2.5 t class's two levels (promo and top), its own tractor and
// garage: a new Sport tractor finishes at the back of promo and about half
// its garage fights for the win; promo's winner starts at the back at top,
// and only a nearly or fully built tractor wins there.
TEST(RivalDifficulty, ANewSportTractorIsAtTheBackOfPromoAndHalfItsGarageWinsIt) {
    expect_level("sport_2500_promo.toml",
                 {{.share = 0.0, .places = {.best = 10, .worst = 15}},
                  {.share = 0.45, .places = {.best = 1, .worst = 3}}},
                 kSportGears);
}

TEST(RivalDifficulty, OnlyAFullyBuiltSportTractorWinsAtTop) {
    expect_level("sport_2500_top.toml",
                 {{.share = 0.45, .places = {.best = 12, .worst = 15}},
                  {.share = 0.8, .places = {.best = 2, .worst = 10}},
                  {.share = 1.0, .places = {.best = 1, .worst = 2}}},
                 kSportGears);
}

// The Sport 4.5 t class's three levels, its own tractor and garage: a new
// tractor is at the back at belofte and about half its garage wins it;
// belofte's winner starts in the back half of promo and about three quarters
// win it; promo's winner is at the back at top, where about 90 % of the
// garage is in the front half, winning now and then, and only a fully
// built tractor is sure of the podium. Belofte's first
// event is the watered start, where the sled is lightened until the field
// gets it going and the right gear decides much: a third of the garage,
// in its best gear, may win there.
TEST(RivalDifficulty, ANewSport45TractorIsLastAtBelofteAndAbout45PercentWinsIt) {
    expect_level("sport_4500_belofte.toml",
                 {{.share = 0.0, .places = {.best = 13, .worst = 15}},
                  {.share = 0.3, .places = {.best = 1, .worst = 10}},
                  {.share = 0.45, .places = {.best = 1, .worst = 4}},
                  {.share = 0.55, .places = {.best = 1, .worst = 2}}},
                 kSportGears);
}

TEST(RivalDifficulty, Sport45BeloftesWinnerIsAtTheBackOfPromoAndThreeQuartersWinIt) {
    expect_level("sport_4500_promo.toml",
                 {{.share = 0.45, .places = {.best = 5, .worst = 13}},
                  {.share = 0.66, .places = {.best = 1, .worst = 3}},
                  {.share = 0.75, .places = {.best = 1, .worst = 2}}},
                 kSportGears);
}

TEST(RivalDifficulty, OnlyAFullyBuiltSport45TractorIsSureToWinAtTop) {
    expect_level("sport_4500_top.toml",
                 {{.share = 0.75, .places = {.best = 8, .worst = 15}},
                  {.share = 0.9, .places = {.best = 1, .worst = 8}},
                  {.share = 1.0, .places = {.best = 1, .worst = 2}}},
                 kSportGears);
}

// A new tractor is at the back, but it pulls: on every track of its class's
// first season, in its best gear, it gets the 15 t sled well going. (On
// its farm drawbar and worn tires a new standard tractor once could not
// move it at all; the class's sled starts its box at the back for that.)
TEST(RivalDifficulty, ANewTractorGetsTheSledGoingOnEveryTrack) {
    constexpr double kWellGoing_m = 40.0;
    for (const char* file : {"standard_3500.toml", "club_season.toml", "sport_2500_promo.toml",
                             "sport_4500_belofte.toml"}) {
        const data::SeasonDefinition d = season_named(file);
        const data::EventDefinition& e = d.event;
        const std::optional<vehicle::Params> fresh = garage::equipped(
            e.reference.tractor, d.garage.catalogue, garage::new_career(d.garage.catalogue));
        if (!fresh) {
            FAIL() << file << ": the new tractor was refused";
        }
        std::set<std::tuple<double, double, double, double>> grounds;
        for (const data::SeasonRound& round : d.rounds) {
            const track::Params& ground = round.track.ground;
            if (!grounds
                     .insert({ground.soil.clay_fraction, ground.soil.moisture,
                              ground.soil.compaction, ground.grip_scale})
                     .second) {
                continue;
            }
            const Venue venue = data::venue(e.sled, round.track);
            Puller puller = e.reference;
            puller.tractor = *fresh;
            const std::optional<GearTrial> trial = best_gear_trial(
                puller, venue.sled, venue.track, e.variation.lowest_gear, e.variation.highest_gear,
                trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
            if (!trial) {
                FAIL() << file << ", " << round.name << ": the trial was refused";
            }
            EXPECT_GT(trial->distance_m, kWellGoing_m) << file << ", " << round.name;
        }
    }
}

}  // namespace
}  // namespace tp::field
