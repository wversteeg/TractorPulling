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
    // The fully built tractor; `rear_traction`, if given, sets its rear
    // tires' traction limit in place of the built ones'.
    explicit SportSetup(const char* season_file = "sport_4500_top.toml",
                        std::optional<double> rear_traction = std::nullopt)
        : season_(load(season_file)), built_(built(season_)) {
        if (rear_traction) {
            built_.rear_tire.traction.limit = *rear_traction;
        }
    }

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
        constexpr int kMostSteps = 32;
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

    // The built rear tires' traction limit.
    [[nodiscard]] double rear_traction() const { return built_.rear_tire.traction.limit; }

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

// Where the weights pull furthest on `track` (of the five places tried),
// on a sled heavy enough that the tractor stops short of the line.
double best_weights(const SportSetup& sport, const std::string& track) {
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
    return best_at;
}

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
// forward on the grippy tracks (the firm loam, the dry hard clay, the moist
// heavy clay), where the tires hold and the chain, low on the sled, pulls
// the nose up; back on the loose sandy clay, where the grip runs out before
// the front can rise and the rear tires need every kilo; in between on the
// clay-sand and on the wet start, which is wet only at the start: enough
// weight back to get the sled going, not so much that the grippy rest of
// the track lifts the nose. Over the tracks the best places use at least
// three quarters of the weights' range.
TEST(TrackSetup, EachTrackHasItsOwnBestWeights) {
    const SportSetup sport;
    const std::map<std::string, std::vector<double>> best_on{
        {"Spring opener, watered start", {kHalfway, kThreeQuarters}},
        {"Sandy clay", {kQuarter, kHalfway}},
        {"Clay-sand", {kHalfway, kThreeQuarters}},
        {"Firm loam", {kThreeQuarters, kForward}},
        {"Summer, dry hard clay", {kThreeQuarters, kForward}},
        {"Rainy day, covered heavy clay", {kThreeQuarters, kForward}},
    };
    double furthest_back = kForward;
    double furthest_forward = kBack;
    for (const std::string& track : rounds_on_their_own_tracks(sport)) {
        const double best_at = best_weights(sport, track);
        furthest_back = std::min(furthest_back, best_at);
        furthest_forward = std::max(furthest_forward, best_at);
        ASSERT_TRUE(best_on.contains(track)) << track;
        const std::vector<double>& expected = best_on.at(track);
        EXPECT_NE(std::ranges::find(expected, best_at), expected.end())
            << track << ": best with the weights at " << best_at;
    }
    constexpr double kMostOfTheRange = 0.75;
    EXPECT_GE(furthest_forward - furthest_back, kMostOfTheRange)
        << "best from " << furthest_back << " to " << furthest_forward;
}

// Less grippy rear tires want the weights further back: on the firm loam
// the built puller tires pull furthest with them forward, tires that grip
// less (a lower traction limit) with them no further than halfway.
TEST(TrackSetup, LessGrippyTiresWantTheWeightsFurtherBack) {
    constexpr double kLessGrip = 1.1;
    const char* firm = "Firm loam";
    const SportSetup grippy;
    const SportSetup less{"sport_4500_top.toml", kLessGrip};
    ASSERT_GT(grippy.rear_traction(), kLessGrip);
    EXPECT_EQ(best_weights(grippy, firm), kForward);
    EXPECT_LE(best_weights(less, firm), kHalfway);
}

// What the wrong weights cost depends on the track. On the loose sandy
// clay forward costs metres, the rear tires short of load, and all back
// stands the tractor on its wheelie bar, which costs as much. On the grippy
// firm loam and the dry hard clay it is the other way round: forward pays,
// the weights halfway let the nose climb.
TEST(TrackSetup, TheTrackDecidesWhatTheWrongWeightsCost) {
    const SportSetup sport;
    const auto reach = [&](const char* track, double ballast) {
        return sport.reach_m(track, ballast, 1.0, sport.steps_short_of_line(track));
    };
    const char* loose = "Sandy clay";
    EXPECT_GT(reach(loose, kQuarter), reach(loose, kForward) + kClearly_m);
    EXPECT_GT(reach(loose, kQuarter), reach(loose, kBack) + kClearly_m);
    for (const char* grippy : {"Firm loam", "Summer, dry hard clay"}) {
        EXPECT_GT(reach(grippy, kForward), reach(grippy, kHalfway) + kClearly_m) << grippy;
    }
}

// Each track has its own best tire pressure, and both sides of it cost:
// too soft lets the lugs squirm, too hard shrinks the footprint. With the
// weights where the track wants them: soft pays on the heavy clay, where
// the tires dig in; on the firm loam and the dry hard clay the reference,
// where the softest costs metres. On a sled heavy enough that the tractor
// stops short of the line.
TEST(TrackSetup, EachTrackHasItsOwnBestTirePressure) {
    const SportSetup sport;
    constexpr double kSoft_bar = 0.5;
    constexpr double kReference_bar = 1.0;
    constexpr double kHard_bar = 1.6;
    constexpr double kSoftCosts_m = 2.0;
    const auto reach = [&](const char* track, double bar) {
        return sport.reach_m(track, best_weights(sport, track), bar,
                             sport.steps_short_of_line(track));
    };
    const char* heavy = "Rainy day, covered heavy clay";
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kReference_bar));
    EXPECT_GT(reach(heavy, kSoft_bar), reach(heavy, kHard_bar) + 0.5);
    for (const char* firm : {"Firm loam", "Summer, dry hard clay"}) {
        EXPECT_GT(reach(firm, kReference_bar), reach(firm, kSoft_bar) + kSoftCosts_m) << firm;
        EXPECT_GT(reach(firm, kReference_bar), reach(firm, kHard_bar)) << firm;
    }
}

// The lowest hitch hole takes most of the sled's pull off the rear tires:
// at every level of the class, with the weights well forward (as the chain,
// low on the sled, pulls the nose up), the top hole pulls clearly further
// on the clay-sand, the sandy clay, the wet start and the dry hard clay.
// On the grippiest tracks, the firm loam and the moist heavy clay, the
// tires have grip to spare and the chain lifts the nose: a hole or two
// below the top may pull further, and the lowest may gain up to about two
// metres, so the hitch matters little there either way. So the
// crew chief may always say to raise a hitch in its lowest hole (U-23): it
// costs that much at most. (With the weights all back it is another
// matter: the top hole stands the tractor on its wheelie bar, and a lower
// one keeps the nose down and pulls further, so the crew chief says it only
// with the weights at least halfway.) On a sled heavy enough that the
// tractor stops short of the line, where a setting shows.
TEST(TrackSetup, TheLowestHitchHoleCostsButOnTheGrippiestTracks) {
    constexpr double kAsBuilt_bar = 1.0;
    // Further than a pull-to-pull difference; at most this much shorter [m].
    constexpr double kFurther_m = 0.5;
    constexpr double kAtMostShorter_m = 2.0;
    // The grippiest tracks, by their ground: a renamed round is still one.
    std::vector<soil::Composition> grippiest;
    for (const char* file : {"firm_loam.toml", "covered_heavy_clay.toml"}) {
        const auto track = data::load_track(std::filesystem::path{TP_DATA_DIR} / "tracks" / file);
        ASSERT_TRUE(track.ok()) << track.error().message;
        grippiest.push_back(track.value().ground.soil);
    }
    // Short of the pull-off's line: the run's limit hides the rest [m].
    constexpr double kShortOfTheLine_m = 115.0;
    constexpr int kStepsAtATime = 2;
    constexpr int kMostSteps = 32;
    int on_grippiest_rounds = 0;
    for (const char* season :
         {"sport_4500_belofte.toml", "sport_4500_promo.toml", "sport_4500_top.toml"}) {
        const SportSetup sport{season};
        for (const std::string& round : rounds_on_their_own_tracks(sport)) {
            for (const double ballast : {kThreeQuarters, kForward}) {
                int steps = 0;
                double top_m = sport.reach_m(round, ballast, kAsBuilt_bar, steps);
                while (top_m > kShortOfTheLine_m && steps < kMostSteps) {
                    steps += kStepsAtATime;
                    top_m = sport.reach_m(round, ballast, kAsBuilt_bar, steps);
                }
                const double lowest_m =
                    sport.reach_m(round, ballast, kAsBuilt_bar, steps, sport.lowest_hitch_m());
                const soil::Composition& soil = sport.round(round).track.ground.soil;
                const bool on_grippiest =
                    std::ranges::any_of(grippiest, [&soil](const soil::Composition& g) {
                        return soil.clay_fraction == g.clay_fraction &&
                               soil.moisture == g.moisture && soil.compaction == g.compaction;
                    });
                if (!on_grippiest) {
                    EXPECT_GT(top_m, lowest_m + kFurther_m)
                        << season << ", " << round << ", weights at " << ballast;
                } else {
                    ++on_grippiest_rounds;
                    EXPECT_GT(top_m, lowest_m - kAtMostShorter_m)
                        << season << ", " << round << ", weights at " << ballast;
                }
            }
        }
    }
    EXPECT_GT(on_grippiest_rounds, 0) << "the grippiest tracks were never tried";
}

}  // namespace
}  // namespace tp::field
