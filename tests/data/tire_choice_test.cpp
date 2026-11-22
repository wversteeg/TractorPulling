#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/track/track_loader.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::field {
namespace {

// The Supersport 3.6 t garage's choices (garages/supersport_3600.toml), in
// the order of its file.
constexpr std::size_t kEngine = 0;
constexpr std::size_t kRearTires = 1;
constexpr std::size_t kFinalDrive = 2;
constexpr int kSevenLitre = 0;
constexpr int kSixLitre = 1;
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

// The Supersport 3.6 t class at top, loaded once for the test.
data::SeasonDefinition supersport() {
    auto loaded = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" /
                                    "supersport_3600_top.toml");
    if (!loaded.ok()) {
        throw std::runtime_error(loaded.error().message);
    }
    return std::move(loaded).value();
}

data::Track track(const std::string& file) {
    auto loaded = data::load_track(std::filesystem::path{TP_DATA_DIR} / "tracks" / file);
    if (!loaded.ok()) {
        throw std::runtime_error(loaded.error().message);
    }
    return std::move(loaded).value();
}

// The turbo fitted: the garage's biggest, or the stock one.
enum class Turbo : std::uint8_t { kBiggest, kStock };

// How far the fully built tractor of `d`'s class, its `engine` (the 7 l)
// on `tires` with `turbo`, pulls the class's sled made kHeavier steps
// heavier on `on`, in its best pulling gear, driven as the class's
// reference driver.
double pulled_m(const data::SeasonDefinition& d, const data::Track& on, Tires tires,
                int engine = kSevenLitre, Turbo turbo = Turbo::kBiggest) {
    const data::EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    garage::Career full = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        full.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
    }
    full.owned.fill(std::numeric_limits<std::uint8_t>::max());
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        if (turbo == Turbo::kStock &&
            catalogue.upgrades.at(i).part == garage::Part::kBoostPressure) {
            full.levels.at(i) = 0;
        }
    }
    full.fitted.at(kEngine) = engine;
    full.fitted.at(kRearTires) = tires.tires;
    full.fitted.at(kFinalDrive) = tires.final_drive;
    const std::optional<vehicle::Params> tractor =
        garage::equipped(e.reference.tractor, catalogue, full);
    EXPECT_TRUE(tractor.has_value());
    Puller puller = e.reference;
    puller.tractor = tractor.value_or(e.reference.tractor);
    const Venue venue = data::venue(e.sled, on);
    const std::optional<GearTrial> trial = best_gear_trial(
        puller, sled::heavier(venue.sled, kHeavier), venue.track, kLowestPullingGear,
        kHighestPullingGear, trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
    EXPECT_TRUE(trial.has_value());
    return trial.has_value() ? trial->distance_m : 0.0;
}

constexpr Tires kPullerOnTall{.tires = kPullerTires, .final_drive = kTall};
constexpr Tires kPullerOnShort{.tires = kPullerTires, .final_drive = kShort};
constexpr Tires kFarmOnShort{.tires = kFarmTires, .final_drive = kShort};
// A clear win: more than the spot on the track decides [m].
constexpr double kClearly_m = 2.0;

// Fully built, each tire on the final drive the crew pairs it with (the
// one it suits): the puller tires pull furthest on a firm track, where
// they spin up to their bite; on the watered start, slick under the tires
// at the launch, the farm tires' lugs grip at little slip and they beat
// the puller tires on the tall drive there. Neither pairing is the best
// everywhere. The tires alone are another matter: on the watered start the
// puller tires on the short drive go further still, so a player who owns
// both drives is never worse off on the puller tires. The farm tires are
// the stock ones, cheaper: they may win somewhere, but need not.
TEST(TireChoice, PullerTiresWinOnAFirmTrackAndFarmTiresOnAWetStart) {
    const data::SeasonDefinition d = supersport();
    for (const char* firm : {"firm_loam.toml", "ideal_clay_sand.toml"}) {
        const data::Track on = track(firm);
        EXPECT_GT(pulled_m(d, on, kPullerOnTall), pulled_m(d, on, kFarmOnShort) + kClearly_m)
            << firm;
    }
    const data::Track wet = track("watered_start.toml");
    const double farm_m = pulled_m(d, wet, kFarmOnShort);
    EXPECT_GT(farm_m, pulled_m(d, wet, kPullerOnTall) + kClearly_m);
    EXPECT_GT(pulled_m(d, wet, kPullerOnShort), farm_m + kClearly_m);
}

// Power counts: the puller tires spin at 70 km/h and more in the pulling
// gears, held by the engine's power, not the governor. On a firm track the
// fully built 6.06 l pulls clearly further on the garage's biggest turbo
// than on the stock one, and gains more from it than the 7.0 l, held to
// 2700 rpm, does.
TEST(TireChoice, TheBiggestTurboPullsFurthestAndMostOnTheSixLitre) {
    const data::SeasonDefinition d = supersport();
    for (const char* firm : {"firm_loam.toml", "ideal_clay_sand.toml"}) {
        const data::Track on = track(firm);
        const double six_m = pulled_m(d, on, kPullerOnTall, kSixLitre);
        const double six_stock_m = pulled_m(d, on, kPullerOnTall, kSixLitre, Turbo::kStock);
        const double seven_m = pulled_m(d, on, kPullerOnTall);
        const double seven_stock_m = pulled_m(d, on, kPullerOnTall, kSevenLitre, Turbo::kStock);
        std::printf("PROBE %s six %.2f/%.2f seven %.2f/%.2f\n", firm, six_m, six_stock_m, seven_m,
                    seven_stock_m);
        EXPECT_GT(six_m, six_stock_m + kClearly_m) << firm;
        EXPECT_GT(six_m - six_stock_m, seven_m - seven_stock_m) << firm;
    }
}

}  // namespace
}  // namespace tp::field
