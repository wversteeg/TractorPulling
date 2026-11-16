#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/track/track_loader.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace tp::field {
namespace {

// The Supersport 3.6 t garage's choices (garages/supersport_3600.toml), in
// the order of its file.
constexpr std::size_t kEngine = 0;
constexpr std::size_t kRearTires = 1;
constexpr std::size_t kFinalDrive = 2;
constexpr int kSevenLitre = 0;
constexpr int kFarmTires = 0;
constexpr int kPullerTires = 1;
constexpr int kShort = 0;
constexpr int kTall = 1;
// The gearbox's pulling gears (drivelines/supersport_3600_*.toml): the
// player picks the best of them for the track.
constexpr int kLowestPullingGear = 3;
constexpr int kHighestPullingGear = 6;
// Heavier than the class's sled by this many steps, so that no build
// reaches the end of the track [steps].
constexpr int kHeavier = 4;

struct Tires {
    int tires = 0;
    int final_drive = 0;
};

// How far the fully built Supersport 3.6 t tractor, its 7 l engine on
// `tires`, pulls the class's sled made kHeavier steps heavier on `track`,
// in its best pulling gear, driven as the class's reference driver.
double pulled_m(const std::string& track_file, Tires tires) {
    const auto loaded = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" /
                                          "supersport_3600_top.toml");
    EXPECT_TRUE(loaded.ok()) << loaded.error().message;
    const data::SeasonDefinition& d = loaded.value();
    const data::EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    garage::Career full = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        full.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
    }
    full.owned.fill(0xFF);
    full.fitted.at(kEngine) = kSevenLitre;
    full.fitted.at(kRearTires) = tires.tires;
    full.fitted.at(kFinalDrive) = tires.final_drive;
    const std::optional<vehicle::Params> tractor =
        garage::equipped(e.reference.tractor, catalogue, full);
    EXPECT_TRUE(tractor.has_value());
    Puller puller = e.reference;
    puller.tractor = tractor.value_or(e.reference.tractor);

    const auto track = data::load_track(std::filesystem::path{TP_DATA_DIR} / "tracks" / track_file);
    EXPECT_TRUE(track.ok()) << track.error().message;
    const Venue venue = data::venue(e.sled, track.value());
    const std::optional<GearTrial> trial = best_gear_trial(
        puller, sled::heavier(venue.sled, kHeavier), venue.track, kLowestPullingGear,
        kHighestPullingGear, trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
    EXPECT_TRUE(trial.has_value());
    return trial.has_value() ? trial->distance_m : 0.0;
}

constexpr Tires kPullerOnTall{.tires = kPullerTires, .final_drive = kTall};
constexpr Tires kFarmOnShort{.tires = kFarmTires, .final_drive = kShort};
// A clear win: more than the spot on the track decides [m].
constexpr double kClearly_m = 2.0;

// Fully built, each tire on the final drive that suits it: the puller tires
// pull furthest on a firm track, where they spin up to their bite; on the
// watered start, slick under the tires at the launch, the farm tires' lugs
// grip at little slip and they win there. Neither combination is the best
// everywhere.
TEST(TireChoice, PullerTiresWinOnAFirmTrackAndFarmTiresOnAWetStart) {
    for (const char* firm : {"firm_loam.toml", "ideal_clay_sand.toml"}) {
        const double puller_m = pulled_m(firm, kPullerOnTall);
        const double farm_m = pulled_m(firm, kFarmOnShort);
        EXPECT_GT(puller_m, farm_m + kClearly_m) << firm;
    }
    const double puller_m = pulled_m("watered_start.toml", kPullerOnTall);
    const double farm_m = pulled_m("watered_start.toml", kFarmOnShort);
    EXPECT_GT(farm_m, puller_m + kClearly_m);
}

}  // namespace
}  // namespace tp::field
