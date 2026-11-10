#include "tp/core/field/field.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
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
#include <stdexcept>
#include <string>

namespace tp::field {
namespace {

// The Sport 4.5 t class at top, its tractor fully built (the 68 mm turbo
// fitted), set up before a pull on one of the season's tracks: how far it
// gets in its best gear on the sled the operator has set when the player
// is due (the field's first event on that track), until the sled stops.
class SportSetup {
public:
    SportSetup() : season_(load("sport_4500_top.toml")), built_(built(season_)) {}

    double reach_m(const std::string& round, double ballast_position,
                   double pressure_bar = 1.0) const {
        const data::SeasonRound& r = round_named(round);
        const data::EventDefinition& e = season_.event;
        Venue venue = data::venue(e.sled, r.track);
        const auto known = sleds_.find(round);
        venue.sled = known != sleds_.end()
                         ? known->second
                         : sleds_.emplace(round, operators_sled(venue)).first->second;
        vehicle::Tuning tuning = vehicle::as_built(built_);
        tuning.ballast_position = ballast_position;
        tuning.rear_tire_pressure_bar = pressure_bar;
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
// More than the spot on the track decides [m].
constexpr double kClearly_m = 3.0;

// On a grippy track the weights all back stand the tractor on its wheelie
// bar, which carries load the rear tires lose: halfway pulls further.
TEST(TrackSetup, OnAGrippyTrackTheWeightsAllBackStandItOnTheBar) {
    const SportSetup sport;
    for (const char* track : {"Firm loam", "Summer, dry hard clay"}) {
        EXPECT_LT(sport.reach_m(track, kBack), sport.reach_m(track, kHalfway) - kClearly_m)
            << track;
    }
}

// On the heavy clay the tires dig in and the sled ploughs: with the
// weights back the nose climbs, so they go forward of the quarter.
TEST(TrackSetup, OnHeavyClayTheWeightsGoForward) {
    const SportSetup sport;
    const char* track = "Rainy day, covered heavy clay";
    const double forward_m =
        std::max(sport.reach_m(track, kHalfway), sport.reach_m(track, kThreeQuarters));
    EXPECT_GT(forward_m, sport.reach_m(track, kBack) + kClearly_m);
    EXPECT_GT(forward_m, sport.reach_m(track, kQuarter));
}

// On the loose sandy clay the grip runs out first: the weights all back
// put the most load on the rear tires.
TEST(TrackSetup, OnLooseSandTheWeightsGoBack) {
    const SportSetup sport;
    const char* track = "Sandy clay";
    EXPECT_GT(sport.reach_m(track, kBack), sport.reach_m(track, kHalfway) + kClearly_m);
}

// Soft tires' bigger footprint pays on the heavy clay; on the firm loam
// their lugs squirm and cost metres.
TEST(TrackSetup, SoftTiresPayOnHeavyClayAndCostOnFirmLoam) {
    const SportSetup sport;
    constexpr double kSoft_bar = 0.6;
    const char* heavy = "Rainy day, covered heavy clay";
    EXPECT_GT(sport.reach_m(heavy, kHalfway, kSoft_bar), sport.reach_m(heavy, kHalfway) + 2.0);
    EXPECT_LT(sport.reach_m("Firm loam", kHalfway, kSoft_bar),
              sport.reach_m("Firm loam", kHalfway) - kClearly_m);
}

}  // namespace
}  // namespace tp::field
