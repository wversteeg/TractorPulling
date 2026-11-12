#include "tp/core/field/field.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace tp::field {
namespace {

// The Sport 4.5 t class at top, its tractor fully built (the 68 mm turbo
// fitted), set up before a pull on one of the season's tracks: how far it
// gets in its best gear on the sled the operator has set when the player
// is due (the field's first event on that track), until the sled stops.
class SportSetup {
public:
    explicit SportSetup(const char* season_file = "sport_4500_top.toml")
        : season_(load(season_file)), built_(built(season_)) {}

    double reach_m(const std::string& round, double ballast_position, double pressure_bar = 1.0,
                   int heavier_steps = 0, std::optional<double> hitch_m = std::nullopt) const {
        const data::SeasonRound& r = round_named(round);
        const data::EventDefinition& e = season_.event;
        Venue venue = data::venue(e.sled, r.track);
        const auto known = sleds_.find(round);
        venue.sled = known != sleds_.end()
                         ? known->second
                         : sleds_.emplace(round, operators_sled(venue)).first->second;
        venue.sled = sled::heavier(venue.sled, heavier_steps);
        vehicle::Tuning tuning = vehicle::as_built(built_);
        tuning.ballast_position = ballast_position;
        tuning.rear_tire_pressure_bar = pressure_bar;
        if (hitch_m) {
            tuning.hitch_height_m = *hitch_m;
        }
        Puller puller = e.reference;
        puller.tractor = vehicle::tuned(built_, tuning);
        const std::optional<GearTrial> trial = best_gear_trial(
            puller, venue.sled, venue.track, e.variation.lowest_gear, e.variation.highest_gear,
            trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
        if (!trial) {
            throw std::runtime_error("the pull was refused");
        }
        return trial->distance_m;
    }

    // The tractor's lowest hitch hole [m], and the season's rounds.
    [[nodiscard]] double lowest_hitch_m() const { return built_.chassis.hitch_min_height_m; }
    [[nodiscard]] const std::vector<data::SeasonRound>& rounds() const { return season_.rounds; }

private:
    static data::SeasonDefinition load(const char* file) {
        auto season = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / file);
        if (!season) {
            throw std::runtime_error(season.error().message);
        }
        return season.value();
    }

    static vehicle::Params built(const data::SeasonDefinition& d) {
        const garage::Catalogue& catalogue = d.garage.catalogue;
        garage::Career full = garage::new_career(catalogue);
        for (std::size_t i = 0; i < catalogue.count; ++i) {
            full.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
        }
        full.owned.fill(std::numeric_limits<std::uint8_t>::max());
        full.fitted[0] = 1;
        const std::optional<vehicle::Params> tractor =
            garage::equipped(d.event.reference.tractor, catalogue, full);
        if (!tractor) {
            throw std::runtime_error("the built tractor was refused");
        }
        return *tractor;
    }

    const data::SeasonRound& round_named(const std::string& name) const {
        for (const data::SeasonRound& r : season_.rounds) {
            if (r.name == name) {
                return r;
            }
        }
        throw std::runtime_error("no round " + name);
    }

    sled::Params operators_sled(const Venue& venue) const {
        const data::EventDefinition& e = season_.event;
        constexpr int kPullers = 15;
        constexpr std::uint64_t kSeed = 1;
        constexpr std::int64_t kSlice = 1'000'000;
        const std::optional<LiveSeason> created = LiveSeason::create(
            e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull, season_.points,
            kPullers, static_cast<int>(season_.rounds.size()), kSeed);
        if (!created) {
            throw std::runtime_error("the season was refused");
        }
        const auto season = std::make_unique<LiveSeason>(*created);
        if (!season->start_event(venue, built_)) {
            throw std::runtime_error("the event was refused");
        }
        LiveEvent& event = *season->event();
        while (event.turn() == LiveEvent::Turn::kRivals) {
            event.advance(kSlice);
        }
        return event.sled();
    }

    data::SeasonDefinition season_;
    vehicle::Params built_;
    // The operator's sled for each round already worked out.
    mutable std::map<std::string, sled::Params> sleds_;
};

constexpr double kBack = 0.0;
constexpr double kQuarter = 0.25;
constexpr double kHalfway = 0.5;
constexpr double kThreeQuarters = 0.75;
constexpr double kForward = 1.0;
// More than the spot on the track decides [m].
constexpr double kClearly_m = 3.0;

// The season's rounds on different tracks, by their ground (the soil's
// make-up and the track's grip), not their names: a track may come round
// twice in a season under another name.
std::vector<std::string> rounds_on_their_own_tracks(const SportSetup& sport) {
    std::vector<std::string> names;
    std::set<std::tuple<double, double, double, double>> grounds;
    for (const data::SeasonRound& round : sport.rounds()) {
        const track::Params& ground = round.track.ground;
        if (grounds
                .insert({ground.soil.clay_fraction, ground.soil.moisture, ground.soil.compaction,
                         ground.grip_scale})
                .second) {
            names.push_back(round.name);
        }
    }
    return names;
}

// The weights a quarter forward pull furthest on every track: the front
// rides light, the rear tires carry the most they can without the
// wheelie bar taking load off them.
TEST(TrackSetup, AQuarterForwardPullsFurthestOnEveryTrack) {
    const SportSetup sport;
    for (const std::string& track : rounds_on_their_own_tracks(sport)) {
        const double quarter_m = sport.reach_m(track, kQuarter);
        for (const double elsewhere : {kBack, kHalfway, kForward}) {
            EXPECT_GT(quarter_m, sport.reach_m(track, elsewhere)) << track << ", " << elsewhere;
        }
    }
}

// What the wrong weights cost depends on the track. All back stands the
// tractor on its wheelie bar, which costs most on the wet start, where the
// tires need every kilo; all forward lifts load off the rear tires, which
// costs most on the loose sandy clay, where grip runs out first.
TEST(TrackSetup, TheTrackDecidesWhatTheWrongWeightsCost) {
    const SportSetup sport;
    const char* wet = "Spring opener, watered start";
    EXPECT_GT(sport.reach_m(wet, kQuarter), sport.reach_m(wet, kBack) + kClearly_m);
    const char* sand = "Sandy clay";
    EXPECT_GT(sport.reach_m(sand, kQuarter), sport.reach_m(sand, kForward) + kClearly_m);
    const char* loam = "Firm loam";
    EXPECT_LT(sport.reach_m(loam, kQuarter), sport.reach_m(loam, kBack) + kClearly_m)
        << "on the grippy loam the weights matter less";
}

// Each track has its own best tire pressure, and both sides of it cost:
// too soft lets the lugs squirm, too hard shrinks the footprint. Soft
// pays on the heavy clay, where the tires dig in; a little soft on the
// loose sandy clay; on the clay-sand the reference, where the softest
// costs metres. On a sled heavier than the operator's, where the best
// setting does not make a full pull.
TEST(TrackSetup, EachTrackHasItsOwnBestTirePressure) {
    const SportSetup sport;
    constexpr int kHeavier = 3;
    constexpr double kSoft_bar = 0.5;
    constexpr double kLittleSoft_bar = 0.7;
    constexpr double kReference_bar = 1.0;
    constexpr double kHard_bar = 1.6;
    const auto reach = [&](const char* track, double bar) {
        return sport.reach_m(track, kQuarter, bar, kHeavier);
    };
    const char* heavy = "Rainy day, covered heavy clay";
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kReference_bar));
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kHard_bar) + 0.5);
    const char* sand = "Sandy clay";
    EXPECT_GT(reach(sand, kLittleSoft_bar), reach(sand, kReference_bar) + 0.5);
    EXPECT_GT(reach(sand, kLittleSoft_bar), reach(sand, kSoft_bar));
    const char* clay_sand = "Clay-sand";
    EXPECT_GT(reach(clay_sand, kReference_bar), reach(clay_sand, kSoft_bar) + 1.5);
    EXPECT_GT(reach(clay_sand, kReference_bar), reach(clay_sand, kHard_bar));
}

// The lowest hitch hole takes most of the sled's pull off the rear tires:
// at every level of the class, with the weights back, halfway or well
// forward, the top hole pulls clearly further on every track but the
// moist heavy clay, where a low hitch keeps the nose down and may gain up
// to a metre; there the hitch matters little either way. So the crew
// chief may always say to raise a hitch in its lowest hole (U-23): it
// costs a metre at most. (The holes between are another matter: with the
// weights all back on a grippy track the top hole stands the tractor on
// its wheelie bar, and one a little lower pulls further.) On a sled heavy
// enough that the tractor stops short of the line, where a setting shows.
TEST(TrackSetup, TheLowestHitchHoleNeverPullsFurthest) {
    constexpr double kAsBuilt_bar = 1.0;
    // Further than a pull-to-pull difference; at most this much shorter [m].
    constexpr double kFurther_m = 0.5;
    constexpr double kAtMostShorter_m = 1.5;
    const std::string heavy_clay = "covered heavy clay";
    // Short of the pull-off's line: the run's limit hides the rest [m].
    constexpr double kShortOfTheLine_m = 115.0;
    constexpr int kStepsAtATime = 2;
    constexpr int kMostSteps = 16;
    for (const char* season :
         {"sport_4500_belofte.toml", "sport_4500_promo.toml", "sport_4500_top.toml"}) {
        const SportSetup sport{season};
        for (const std::string& round : rounds_on_their_own_tracks(sport)) {
            for (const double ballast : {kBack, kHalfway, kThreeQuarters}) {
                int steps = 0;
                double top_m = sport.reach_m(round, ballast, kAsBuilt_bar, steps);
                while (top_m > kShortOfTheLine_m && steps < kMostSteps) {
                    steps += kStepsAtATime;
                    top_m = sport.reach_m(round, ballast, kAsBuilt_bar, steps);
                }
                const auto at_hole = [&](double hole_m) {
                    return sport.reach_m(round, ballast, kAsBuilt_bar, steps, hole_m);
                };
                const double lowest_m = at_hole(sport.lowest_hitch_m());
                if (round.find(heavy_clay) == std::string::npos) {
                    EXPECT_GT(top_m, lowest_m + kFurther_m)
                        << season << ", " << round << ", weights at " << ballast;
                } else {
                    EXPECT_GT(top_m, lowest_m - kAtMostShorter_m)
                        << season << ", " << round << ", weights at " << ballast;
                }
            }
        }
    }
}

}  // namespace
}  // namespace tp::field
