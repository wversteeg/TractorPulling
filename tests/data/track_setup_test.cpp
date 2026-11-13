#include "tp/core/field/field.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/track/track_loader.hpp"

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

    // How many steps heavier than the operator's sled the round needs for
    // the tractor to stop short of the pull-off's line however its weights
    // sit, where a setting shows (the run's limit hides the rest).
    int steps_short_of_line(const std::string& round) const {
        const auto known = steps_.find(round);
        if (known != steps_.end()) {
            return known->second;
        }
        constexpr double kShortOfTheLine_m = 115.0;
        constexpr int kMostSteps = 16;
        int steps = 0;
        for (; steps < kMostSteps; ++steps) {
            double best_m = 0.0;
            for (const double ballast : {0.0, 0.25, 0.5, 0.75, 1.0}) {
                best_m = std::max(best_m, reach_m(round, ballast, 1.0, steps));
            }
            if (best_m < kShortOfTheLine_m) {
                break;
            }
        }
        return steps_.emplace(round, steps).first->second;
    }

    // The tractor's lowest hitch hole [m], and the season's rounds.
    [[nodiscard]] double lowest_hitch_m() const { return built_.chassis.hitch_min_height_m; }
    [[nodiscard]] const std::vector<data::SeasonRound>& rounds() const { return season_.rounds; }
    [[nodiscard]] const data::SeasonRound& round(const std::string& name) const {
        return round_named(name);
    }

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
    mutable std::map<std::string, int> steps_;
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

// Each track has its own best place for the weights. On a sled heavy
// enough that the tractor stops short of the line, where a setting shows:
// a quarter forward on the wet start, the loose sandy clay and the
// clay-sand, where the rear tires need the load but the front must stay
// light; further forward on the grippy firm loam and the dry hard clay,
// where the tires hold and the front would stand up; all forward on the
// moist heavy clay, where the tires dig in and bite and the nose wants to
// climb. (With the push-down putting half the sled at most on the pan,
// the strongest settings pull on past it, so the best one shows.)
TEST(TrackSetup, EachTrackHasItsOwnBestWeights) {
    const SportSetup sport;
    const std::map<std::string, std::vector<double>> best_on{
        {"Spring opener, watered start", {kQuarter}},
        {"Sandy clay", {kQuarter}},
        {"Clay-sand", {kQuarter}},
        {"Firm loam", {kHalfway, kThreeQuarters}},
        {"Summer, dry hard clay", {kHalfway, kThreeQuarters}},
        {"Rainy day, covered heavy clay", {kForward}},
    };
    for (const std::string& track : rounds_on_their_own_tracks(sport)) {
        const int steps = sport.steps_short_of_line(track);
        double best_m = 0.0;
        double best_at = -1.0;
        for (const double ballast : {kBack, kQuarter, kHalfway, kThreeQuarters, kForward}) {
            const double reached_m = sport.reach_m(track, ballast, 1.0, steps);
            if (reached_m > best_m) {
                best_m = reached_m;
                best_at = ballast;
            }
        }
        ASSERT_TRUE(best_on.contains(track)) << track;
        const std::vector<double>& expected = best_on.at(track);
        EXPECT_NE(std::ranges::find(expected, best_at), expected.end())
            << track << ": best with the weights at " << best_at;
    }
}

// What the wrong weights cost depends on the track. All back stands the
// tractor on its wheelie bar, which costs most on the wet start, where the
// tires need every kilo; all forward lifts load off the rear tires, which
// costs most on the loose sandy clay, where grip runs out first; on the
// moist heavy clay it is the other way round: forward pays.
TEST(TrackSetup, TheTrackDecidesWhatTheWrongWeightsCost) {
    const SportSetup sport;
    const auto reach = [&](const char* track, double ballast) {
        return sport.reach_m(track, ballast, 1.0, sport.steps_short_of_line(track));
    };
    const char* wet = "Spring opener, watered start";
    EXPECT_GT(reach(wet, kQuarter), reach(wet, kBack) + kClearly_m);
    const char* sand = "Sandy clay";
    EXPECT_GT(reach(sand, kQuarter), reach(sand, kForward) + kClearly_m);
    const char* heavy = "Rainy day, covered heavy clay";
    EXPECT_GT(reach(heavy, kForward), reach(heavy, kQuarter) + kClearly_m);
}

// Each track has its own best tire pressure, and both sides of it cost:
// too soft lets the lugs squirm, too hard shrinks the footprint. Soft
// pays on the heavy clay, where the tires dig in; on the clay-sand and the
// dry hard clay the reference, where the softest costs metres. On a sled
// heavy enough that the tractor stops short of the line.
TEST(TrackSetup, EachTrackHasItsOwnBestTirePressure) {
    const SportSetup sport;
    constexpr double kSoft_bar = 0.5;
    constexpr double kReference_bar = 1.0;
    constexpr double kHard_bar = 1.6;
    const auto reach = [&](const char* track, double bar) {
        return sport.reach_m(track, kQuarter, bar, sport.steps_short_of_line(track));
    };
    const char* heavy = "Rainy day, covered heavy clay";
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kReference_bar));
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kHard_bar) + 0.5);
    const char* clay_sand = "Clay-sand";
    EXPECT_GT(reach(clay_sand, kReference_bar), reach(clay_sand, kSoft_bar) + 1.5);
    EXPECT_GT(reach(clay_sand, kReference_bar), reach(clay_sand, kHard_bar));
    const char* dry = "Summer, dry hard clay";
    EXPECT_GT(reach(dry, kReference_bar), reach(dry, kSoft_bar) + kClearly_m);
}

// The lowest hitch hole takes most of the sled's pull off the rear tires:
// at every level of the class, with the weights halfway or well forward,
// the top hole pulls clearly further on every track but the
// moist heavy clay, where a low hitch keeps the nose down and may gain
// about a metre; there the hitch matters little either way. So the crew
// chief may always say to raise a hitch in its lowest hole (U-23): it
// costs about a metre at most. (With the weights all back it is another
// matter: the top hole stands the tractor on its wheelie bar, and a lower
// one keeps the nose down and pulls further, so the crew chief says it only
// with the weights at least halfway.) On a sled heavy
// enough that the tractor stops short of the line, where a setting shows.
TEST(TrackSetup, TheLowestHitchHoleCostsEverywhereButTheHeavyClay) {
    constexpr double kAsBuilt_bar = 1.0;
    // Further than a pull-to-pull difference; at most this much shorter [m].
    constexpr double kFurther_m = 0.5;
    constexpr double kAtMostShorter_m = 1.2;
    // The moist heavy clay, by its ground: a renamed round is still it.
    const auto heavy_clay =
        data::load_track(std::filesystem::path{TP_DATA_DIR} / "tracks" / "covered_heavy_clay.toml");
    ASSERT_TRUE(heavy_clay.ok()) << heavy_clay.error().message;
    const soil::Composition& clay = heavy_clay.value().ground.soil;
    // Short of the pull-off's line: the run's limit hides the rest [m].
    constexpr double kShortOfTheLine_m = 115.0;
    constexpr int kStepsAtATime = 2;
    constexpr int kMostSteps = 16;
    int on_heavy_clay_rounds = 0;
    for (const char* season :
         {"sport_4500_belofte.toml", "sport_4500_promo.toml", "sport_4500_top.toml"}) {
        const SportSetup sport{season};
        for (const std::string& round : rounds_on_their_own_tracks(sport)) {
            for (const double ballast : {kHalfway, kThreeQuarters}) {
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
                const soil::Composition& soil = sport.round(round).track.ground.soil;
                const bool on_heavy_clay = soil.clay_fraction == clay.clay_fraction &&
                                           soil.moisture == clay.moisture &&
                                           soil.compaction == clay.compaction;
                if (!on_heavy_clay) {
                    EXPECT_GT(top_m, lowest_m + kFurther_m)
                        << season << ", " << round << ", weights at " << ballast;
                } else {
                    ++on_heavy_clay_rounds;
                    EXPECT_GT(top_m, lowest_m - kAtMostShorter_m)
                        << season << ", " << round << ", weights at " << ballast;
                }
            }
        }
    }
    EXPECT_GT(on_heavy_clay_rounds, 0) << "the heavy clay's exception was never tried";
}

}  // namespace
}  // namespace tp::field
