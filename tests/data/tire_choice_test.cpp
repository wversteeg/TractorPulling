#include "tp/core/driver/driver.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/track/track_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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
constexpr int kMiddle = 1;
constexpr int kTall = 2;
// The gearbox's pulling gears (drivelines/supersport_3600_*.toml): the
// player picks the best of them for the track.
constexpr int kLowestPullingGear = 3;
constexpr int kHighestPullingGear = 6;
// Heavier than the class's sled by this many steps, so that no build
// reaches the end of the track on the soft tracks, or on the firm ones
// with kHeaviest [steps].
constexpr int kHeavier = 8;
constexpr int kHeaviest = 12;
// The stock tractor's sled at belofte made this many steps heavier: as
// heavy as the pull-offs get there [steps].
constexpr int kPullOff = 4;

struct Tires {
    int tires = 0;
    int final_drive = 0;
};

// The Supersport 3.6 t class at `level` (top unless given).
data::SeasonDefinition supersport(const char* level = "supersport_3600_top.toml") {
    auto loaded = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / level);
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
// The engine, grip, weights and hitch: fully built, or stock (nothing
// bought, the stock turbo with it).
enum class Build : std::uint8_t { kFull, kStock };

// The class's reference driver in the tractor of `d`'s class built as
// `build`, its `engine` (the 7 l) on `tires` with `turbo`.
Puller built_puller(const data::SeasonDefinition& d, Tires tires, int engine, Turbo turbo,
                    Build build = Build::kFull) {
    const data::EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    garage::Career full = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count && build == Build::kFull; ++i) {
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
    return puller;
}

// `puller`'s best pulling gear on `d`'s sled made `heavier` steps heavier
// on `on`, and how far it got there.
GearTrial trial(const data::SeasonDefinition& d, const data::Track& on, const Puller& puller,
                int heavier) {
    const data::EventDefinition& e = d.event;
    const Venue venue = data::venue(e.sled, on);
    const std::optional<GearTrial> best =
        best_gear_trial(puller, sled::heavier(venue.sled, heavier), venue.track, kLowestPullingGear,
                        kHighestPullingGear, trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
    EXPECT_TRUE(best.has_value());
    return best.value_or(GearTrial{});
}

// How far the fully built tractor pulls the class's sled made `heavier`
// steps heavier on `on`, in its best pulling gear.
double pulled_m(const data::SeasonDefinition& d, const data::Track& on, Tires tires,
                int engine = kSevenLitre, Turbo turbo = Turbo::kBiggest, int heavier = kHeavier) {
    return trial(d, on, built_puller(d, tires, engine, turbo), heavier).distance_m;
}

// A pull in one gear, as the class's rivals pull (they gear for wheel
// speed): the wheels' surface speed and the ground speed, averaged through
// the middle of the track, and how long the sled took from first moving
// to 100 m (0 if it stopped short).
constexpr double kMiddleFrom_m = 40.0;
constexpr double kMiddleUntil_m = 60.0;
constexpr double kKmhPerM_s = 3.6;
constexpr double kMoved_m = 0.1;
constexpr double kLine_m = 100.0;

struct GearRun {
    double wheel_kmh = 0.0;
    double ground_kmh = 0.0;
    double to_the_line_s = 0.0;
};

// The fully built 6.06 l on `tires` in `gear`, pulling the class's sled
// made `heavier` steps heavier on `on`, as the reference driver drives it.
GearRun gear_run(const data::SeasonDefinition& d, const data::Track& on, Tires tires, int gear,
                 int heavier) {
    const data::EventDefinition& e = d.event;
    Puller puller = built_puller(d, tires, kSixLitre, Turbo::kBiggest);
    puller.driver.gear = gear;
    Venue venue = data::venue(e.sled, on);
    venue.sled = sled::heavier(venue.sled, heavier);
    const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
    const auto model =
        pull::Model::create({.tractor = puller.tractor, .sled = venue.sled, .track = venue.track});
    EXPECT_TRUE(model.has_value());
    if (!model) {
        return {};
    }
    constexpr double kDt_s = 0.001;
    const double radius_m = puller.tractor.rear_tire.size.radius_m;
    run::State state = run::start(*model);
    driver::State driver;
    GearRun sum;
    double moved_s = -1.0;
    int steps = 0;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, rules,
                  driver::input(puller.driver, driver, t_s, state.pull.tractor,
                                model->tractor().params()),
                  kDt_s);
        const double pulled_m = state.pull.sled.position_m - model->sled_start_m();
        if (moved_s < 0.0 && pulled_m > kMoved_m) {
            moved_s = t_s;
        }
        if (sum.to_the_line_s == 0.0 && pulled_m >= kLine_m) {
            sum.to_the_line_s = t_s - moved_s;
        }
        if (pulled_m >= kMiddleFrom_m && pulled_m < kMiddleUntil_m) {
            sum.wheel_kmh +=
                state.pull.tractor.driveline.output_omega_rad_s * radius_m * kKmhPerM_s;
            sum.ground_kmh += state.pull.tractor.chassis.velocity_m_s * kKmhPerM_s;
            ++steps;
        }
    }
    if (steps > 0) {
        sum.wheel_kmh /= steps;
        sum.ground_kmh /= steps;
    }
    return sum;
}

constexpr Tires kPullerOnTall{.tires = kPullerTires, .final_drive = kTall};
constexpr Tires kFarmOnShort{.tires = kFarmTires, .final_drive = kShort};
// A clear win: more than the spot on the track decides [m].
constexpr double kClearly_m = 2.0;
// Less than a clear win, more than none [m].
constexpr double kSomewhat_m = 1.0;

// A top Supersport pull (NTTO, as measured): fully built on its puller
// tires, geared for wheel speed (the tall final drive in 6th, as the
// class's rivals pull), on a firm track the wheels spin at 65 km/h and more
// through the middle of the track while the tractor goes over the ground
// at 30 km/h and more, the engine's power holding the tires' slip; it is
// at the 100 m line 12 to 15 s after the sled first moved. The farm tires
// on their short drive turn at no more than 45 km/h.
TEST(TireChoice, ATopPullSpinsTheWheelsFastAndTakesThirteenSeconds) {
    constexpr int kTopGear = 6;
    constexpr int kFarmGear = 5;
    constexpr int kWellSet = 4;
    constexpr double kWheel_kmh = 65.0;
    constexpr double kGround_kmh = 30.0;
    constexpr double kQuickest_s = 12.0;
    constexpr double kSlowest_s = 15.0;
    constexpr double kFarmWheel_kmh = 45.0;
    const data::SeasonDefinition d = supersport();
    for (const char* firm : {"firm_loam.toml", "dry_hard_clay.toml"}) {
        const data::Track on = track(firm);
        const GearRun puller = gear_run(d, on, kPullerOnTall, kTopGear, kWellSet);
        EXPECT_GT(puller.wheel_kmh, kWheel_kmh) << firm;
        EXPECT_GT(puller.ground_kmh, kGround_kmh) << firm;
        EXPECT_GT(puller.to_the_line_s, kQuickest_s) << firm;
        EXPECT_LT(puller.to_the_line_s, kSlowest_s) << firm;
        EXPECT_LT(gear_run(d, on, kFarmOnShort, kFarmGear, 0).wheel_kmh, kFarmWheel_kmh) << firm;
    }
}

// Fully built, the puller tires on the final drive that suits the track
// (the tall or the middle one) pull further than the farm tires on theirs
// on every track: no top team pulls on farm tires. On the dry hard clay the
// farm tire's lugs ride on its crust (tire::lug_grip); on the softer tracks
// the puller tire's grip and the turbo's power carry it.
TEST(TireChoice, FarmTiresLoseToPullerTiresOnEveryTrack) {
    const data::SeasonDefinition d = supersport();
    for (const char* name : {"watered_start.toml", "firm_loam.toml", "covered_heavy_clay.toml",
                             "dry_hard_clay.toml", "ideal_clay_sand.toml", "sandy_clay.toml"}) {
        const data::Track on = track(name);
        const double puller_m =
            std::max(pulled_m(d, on, kPullerOnTall, kSixLitre),
                     pulled_m(d, on, {.tires = kPullerTires, .final_drive = kMiddle}, kSixLitre));
        EXPECT_GT(puller_m, pulled_m(d, on, kFarmOnShort, kSixLitre) + kSomewhat_m) << name;
    }
}

// Power counts: the puller tires spin on the engine's power, so on the
// firm loam the fully built 6.06 l pulls clearly further on the garage's
// biggest turbo than on the stock one, and gains more from it than the
// 7.0 l, held to 2700 rpm, does (on a sled heavy enough that neither
// reaches the end of the track).
TEST(TireChoice, TheBiggestTurboPullsFurthestAndMostOnTheSixLitre) {
    const data::SeasonDefinition d = supersport();
    for (const char* firm : {"firm_loam.toml"}) {
        const data::Track on = track(firm);
        const auto pulled = [&](int engine, Turbo turbo) {
            return pulled_m(d, on, kPullerOnTall, engine, turbo, kHeaviest);
        };
        const double six_m = pulled(kSixLitre, Turbo::kBiggest);
        const double six_stock_m = pulled(kSixLitre, Turbo::kStock);
        const double seven_m = pulled(kSevenLitre, Turbo::kBiggest);
        const double seven_stock_m = pulled(kSevenLitre, Turbo::kStock);
        EXPECT_GT(six_m, six_stock_m + kClearly_m) << firm;
        EXPECT_GT(six_m - six_stock_m, seven_m - seven_stock_m) << firm;
    }
}

// The puller tires' two final drives each have their place in a career.
// A stock engine cannot spin the puller tires up on the tall drive: on the
// middle one it pulls clearly further on the firm and the heavy tracks,
// as heavy as belofte's pull-offs get, with either engine.
TEST(FinalDrive, AStockEngineSpinsThePullerTiresUpOnTheMiddleDrive) {
    constexpr Tires kPullerOnMiddle{.tires = kPullerTires, .final_drive = kMiddle};
    const data::SeasonDefinition d = supersport("supersport_3600_belofte.toml");
    for (const char* name : {"firm_loam.toml", "covered_heavy_clay.toml"}) {
        const data::Track on = track(name);
        for (const int engine : {kSevenLitre, kSixLitre}) {
            const auto pulled = [&](Tires tires) {
                return trial(d, on, built_puller(d, tires, engine, Turbo::kStock, Build::kStock),
                             kPullOff)
                    .distance_m;
            };
            EXPECT_GT(pulled(kPullerOnMiddle), pulled(kPullerOnTall) + kClearly_m)
                << name << " engine " << engine;
        }
    }
}

// A fully built engine runs out of gears on the middle drive (it pulls
// best in its top gear) and pulls clearly further on the tall one on the
// wet and loose tracks, where the tires must spin fastest, with either
// engine.
TEST(FinalDrive, ABuiltEngineRunsOutOfGearsOnTheMiddleDrive) {
    constexpr Tires kPullerOnMiddle{.tires = kPullerTires, .final_drive = kMiddle};
    const data::SeasonDefinition d = supersport();
    for (const char* name : {"watered_start.toml", "ideal_clay_sand.toml"}) {
        const data::Track on = track(name);
        for (const int engine : {kSevenLitre, kSixLitre}) {
            const GearTrial middle =
                trial(d, on, built_puller(d, kPullerOnMiddle, engine, Turbo::kBiggest), kHeavier);
            const GearTrial tall =
                trial(d, on, built_puller(d, kPullerOnTall, engine, Turbo::kBiggest), kHeavier);
            EXPECT_EQ(middle.gear, kHighestPullingGear) << name << " engine " << engine;
            EXPECT_GT(tall.distance_m, middle.distance_m + kClearly_m)
                << name << " engine " << engine;
        }
    }
}

}  // namespace
}  // namespace tp::field
namespace tp::field {
namespace {}  // namespace
}  // namespace tp::field
