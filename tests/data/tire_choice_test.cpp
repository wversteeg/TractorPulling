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
#include <cstdio>
#include <cstdint>
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

// The class's reference driver in the fully built tractor of `d`'s class,
// its `engine` (the 7 l) on `tires` with `turbo`.
Puller built_puller(const data::SeasonDefinition& d, Tires tires, int engine, Turbo turbo) {
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
    return puller;
}

// How far that tractor pulls the class's sled made kHeavier steps heavier
// on `on`, in its best pulling gear.
double pulled_m(const data::SeasonDefinition& d, const data::Track& on, Tires tires,
                int engine = kSevenLitre, Turbo turbo = Turbo::kBiggest) {
    const data::EventDefinition& e = d.event;
    const Puller puller = built_puller(d, tires, engine, turbo);
    const Venue venue = data::venue(e.sled, on);
    const std::optional<GearTrial> trial = best_gear_trial(
        puller, sled::heavier(venue.sled, kHeavier), venue.track, kLowestPullingGear,
        kHighestPullingGear, trial_pull_rules(e.rules, venue, e.rules.full_pull_m));
    EXPECT_TRUE(trial.has_value());
    return trial.has_value() ? trial->distance_m : 0.0;
}

// Through the middle of the track, where the wheels spin fastest: the
// wheels' surface speed and the ground speed, averaged [km/h].
constexpr double kMiddleFrom_m = 40.0;
constexpr double kMiddleUntil_m = 60.0;
constexpr double kKmhPerM_s = 3.6;

struct Speeds {
    double wheel_kmh = 0.0;
    double ground_kmh = 0.0;
};

// The fully built 6.06 l on `tires` pulling the class's sled on `on` in its
// best pulling gear and hand throttle, as the reference driver drives it.
Speeds middle_speeds(const data::SeasonDefinition& d, const data::Track& on, Tires tires,
                     const sled::Adjustment& lighter = {}) {
    const data::EventDefinition& e = d.event;
    Puller puller = built_puller(d, tires, kSixLitre, Turbo::kBiggest);
    Venue venue = data::venue(e.sled, on);
    venue.sled = sled::adjusted(venue.sled, lighter);
    const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
    const std::optional<GearTrial> trial = best_gear_trial(
        puller, venue.sled, venue.track, kLowestPullingGear, kHighestPullingGear, rules);
    EXPECT_TRUE(trial.has_value());
    if (!trial) {
        return {};
    }
    puller.driver.gear = trial->gear;
    puller.driver.throttle = trial->throttle;
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
    Speeds sum;
    int steps = 0;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, rules,
                  driver::input(puller.driver, driver, t_s, state.pull.tractor,
                                model->tractor().params()),
                  kDt_s);
        const double pulled_m = state.pull.sled.position_m - model->sled_start_m();
        if (pulled_m >= kMiddleFrom_m && pulled_m < kMiddleUntil_m) {
            sum.wheel_kmh +=
                state.pull.tractor.driveline.output_omega_rad_s * radius_m * kKmhPerM_s;
            sum.ground_kmh += state.pull.tractor.chassis.velocity_m_s * kKmhPerM_s;
            ++steps;
        }
    }
    EXPECT_GT(steps, 0) << "the pull never reached the middle";
    return steps > 0
               ? Speeds{.wheel_kmh = sum.wheel_kmh / steps, .ground_kmh = sum.ground_kmh / steps}
               : Speeds{};
}

constexpr Tires kPullerOnTall{.tires = kPullerTires, .final_drive = kTall};
constexpr Tires kFarmOnShort{.tires = kFarmTires, .final_drive = kShort};
// A clear win: more than the spot on the track decides [m].
constexpr double kClearly_m = 2.0;
// Less than a clear win, more than none [m].
constexpr double kSomewhat_m = 1.0;

// The two puller drives are a choice by the track: on the loose and wet
// tracks (the watered start, the clay-sand) the tall drive keeps the
// tires spinning and pulls further; on the firm and heavy ones (the firm
// loam, the covered heavy clay, the dry hard clay), where the tires grip
// harder, the middle drive pulls clearly further.
TEST(TireChoice, TheTallDriveSuitsLooseTracksAndTheMiddleOneFirmOnes) {
    const data::SeasonDefinition d = supersport();
    constexpr Tires kPullerOnMiddle{.tires = kPullerTires, .final_drive = kMiddle};
    for (const char* loose : {"watered_start.toml", "ideal_clay_sand.toml"}) {
        const data::Track on = track(loose);
        EXPECT_GT(pulled_m(d, on, kPullerOnTall, kSixLitre),
                  pulled_m(d, on, kPullerOnMiddle, kSixLitre) + kSomewhat_m)
            << loose;
    }
    for (const char* firm : {"firm_loam.toml", "covered_heavy_clay.toml", "dry_hard_clay.toml"}) {
        const data::Track on = track(firm);
        EXPECT_GT(pulled_m(d, on, kPullerOnMiddle, kSixLitre),
                  pulled_m(d, on, kPullerOnTall, kSixLitre) + kClearly_m)
            << firm;
    }
}

// Through the middle of a pull on the class's sled the puller tires spin
// fast, as top tractors' do: at 65 km/h and more at the wheel on the loose
// tracks (on the tall drive), at 50 and more on the firm ones (on the
// middle drive, where they grip harder and the engine's power holds them
// back), the ground going by at a third to two thirds of that. The farm
// tires on their short drive turn at no more than 40 km/h.
TEST(TireChoice, PullerTiresSpinFastThroughTheMiddleOfThePull) {
    constexpr double kLooseWheel_kmh = 65.0;
    constexpr double kFirmWheel_kmh = 50.0;
    constexpr double kFarmWheel_kmh = 40.0;
    constexpr double kSpinning = 1.4;  // wheel over ground speed, at least
    const data::SeasonDefinition d = supersport();
    for (const char* loose : {"watered_start.toml", "ideal_clay_sand.toml"}) {
        const data::Track on = track(loose);
        const Speeds puller = middle_speeds(d, on, kPullerOnTall);
        EXPECT_GT(puller.wheel_kmh, kLooseWheel_kmh) << loose;
        EXPECT_GT(puller.wheel_kmh, kSpinning * puller.ground_kmh) << loose;
        EXPECT_LT(middle_speeds(d, on, kFarmOnShort).wheel_kmh, kFarmWheel_kmh) << loose;
    }
    for (const char* firm : {"firm_loam.toml", "dry_hard_clay.toml"}) {
        const data::Track on = track(firm);
        const Speeds puller = middle_speeds(d, on, {.tires = kPullerTires, .final_drive = kMiddle});
        EXPECT_GT(puller.wheel_kmh, kFirmWheel_kmh) << firm;
        EXPECT_GT(puller.wheel_kmh, kSpinning * puller.ground_kmh) << firm;
        EXPECT_LT(middle_speeds(d, on, kFarmOnShort).wheel_kmh, kFarmWheel_kmh) << firm;
    }
}

// Fully built, the puller tires on the final drive that suits the track
// (the tall or the middle one) pull further than the farm tires on theirs
// on every track: no top team pulls on farm tires. On the dry hard clay the
// farm tire's lugs ride on its crust (tire::lug_grip); on the softer tracks
// the puller tire's grip and the turbo's power carry it.
TEST(TireChoice, FarmTiresLoseToPullerTiresOnEveryTrack) {
    const data::SeasonDefinition d = supersport();
    // (No Supersport event is on the loose sandy clay: neither tire gets
    // the class's sled going there.)
    for (const char* name : {"watered_start.toml", "firm_loam.toml", "covered_heavy_clay.toml",
                             "dry_hard_clay.toml", "ideal_clay_sand.toml"}) {
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
// 7.0 l, held to 2700 rpm, does.
TEST(TireChoice, TheBiggestTurboPullsFurthestAndMostOnTheSixLitre) {
    const data::SeasonDefinition d = supersport();
    for (const char* firm : {"firm_loam.toml"}) {
        const data::Track on = track(firm);
        const double six_m = pulled_m(d, on, kPullerOnTall, kSixLitre);
        const double six_stock_m = pulled_m(d, on, kPullerOnTall, kSixLitre, Turbo::kStock);
        const double seven_m = pulled_m(d, on, kPullerOnTall);
        const double seven_stock_m = pulled_m(d, on, kPullerOnTall, kSevenLitre, Turbo::kStock);
        EXPECT_GT(six_m, six_stock_m + kClearly_m) << firm;
        EXPECT_GT(six_m - six_stock_m, seven_m - seven_stock_m) << firm;
    }
}

}  // namespace
}  // namespace tp::field
namespace tp::field {
namespace {
TEST(TmpClutch, DISABLED_Print) {
    for (const char* sf : {"club_season.toml", "sport_2500_promo.toml", "sport_2500_top.toml", "sport_4500_belofte.toml", "sport_4500_top.toml", "supersport_3600_belofte.toml", "supersport_3600_top.toml"}) {
        auto loaded = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / sf);
        if (!loaded.ok()) { std::printf("ERR %s %s\n", sf, loaded.error().message.c_str()); continue; }
        const data::SeasonDefinition d = std::move(loaded).value();
        const data::EventDefinition& e = d.event;
        for (const data::SeasonRound& round : d.rounds) {
            const Venue venue = data::venue(e.sled, round.track);
            Puller puller = e.reference;
            const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
            const auto trial = best_gear_trial(puller, venue.sled, venue.track, e.variation.lowest_gear, e.variation.highest_gear, rules);
            if (!trial) continue;
            puller.driver.gear = trial->gear; puller.driver.throttle = trial->throttle;
            const auto model = pull::Model::create({.tractor = puller.tractor, .sled = venue.sled, .track = venue.track});
            run::State state = run::start(*model);
            driver::State drv;
            double in_m = -1, in_s = -1;
            while (!state.result) {
                const double t_s = static_cast<double>(state.steps) * 0.001;
                const vehicle::Input in = driver::input(puller.driver, drv, t_s, state.pull.tractor, model->tractor().params());
                run::step(state, *model, rules, in, 0.001);
                if (in_m < 0 && t_s > puller.driver.launch_s && in.clutch_pedal == 0.0) { in_m = state.pull.sled.position_m - model->sled_start_m(); in_s = t_s - puller.driver.launch_s; }
            }
            std::printf("CLUTCH %-30s %-34s gear %d in at %5.1f m (%4.1f s), pulled %5.1f\n", sf, round.name.c_str(), trial->gear, in_m, in_s, state.result->distance_m);
        }
    }
}
}  // namespace
}  // namespace tp::field
namespace tp::field {
namespace {
TEST(TmpLaunch, DISABLED_Print) {
    const data::SeasonDefinition d = supersport();
    const data::EventDefinition& e = d.event;
    const data::Track on = track("dry_hard_clay.toml");
    Puller puller = built_puller(d, {.tires = kPullerTires, .final_drive = kMiddle}, kSixLitre, Turbo::kBiggest);
    puller.driver.gear = 6;
    Venue venue = data::venue(e.sled, on);
    venue.sled = sled::adjusted(venue.sled, {.box_mass_kg = 3000.0, .knives = 6});
    const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
    const auto model = pull::Model::create({.tractor = puller.tractor, .sled = venue.sled, .track = venue.track});
    run::State state = run::start(*model);
    driver::State drv;
    const double r = puller.tractor.rear_tire.size.radius_m;
    double next = 0.0;
    while (!state.result) {
        const double ts = static_cast<double>(state.steps) * 0.001;
        const vehicle::Input in = driver::input(puller.driver, drv, ts, state.pull.tractor, model->tractor().params());
        const pull::Output o = run::step(state, *model, rules, in, 0.001);
        const double x = state.pull.sled.position_m - model->sled_start_m();
        if (ts >= next && ts > 2.5) {
            std::printf("LAUNCH t%.2f x%5.1f pedal %.2f rpm %4.0f boost %.2f ground %4.1f wheel %4.1f tire %5.1fkN clutch %5.0fNm chain %5.1f sled %5.1f rear %5.1f\n", ts - puller.driver.launch_s, x, in.clutch_pedal, state.pull.tractor.engine.omega_rad_s * 9.549, state.pull.tractor.engine.boost, state.pull.tractor.chassis.velocity_m_s * 3.6, state.pull.tractor.driveline.output_omega_rad_s * r * 3.6, o.tractor.tire_force_n / 1000.0, o.tractor.clutch_torque_nm, o.chain.tension_n / 1000.0, o.sled.resistance_n / 1000.0, o.tractor.axle_loads.rear_n / 1000.0);
            next = ts + 0.5;
        }
        if (x > 40.0) break;
    }
}

TEST(TmpGears, DISABLED_Print) {
    const data::SeasonDefinition d = supersport();
    const data::EventDefinition& e = d.event;
    for (const char* f : {"dry_hard_clay.toml", "firm_loam.toml"}) {
        const data::Track on = track(f);
        for (const Tires t : {kPullerOnTall, Tires{.tires = kPullerTires, .final_drive = kMiddle}}) {
            for (int gear = 3; gear <= 6; ++gear) {
                Puller puller = built_puller(d, t, kSixLitre, Turbo::kBiggest);
                puller.driver.gear = gear;
                Venue venue = data::venue(e.sled, on);
                venue.sled = sled::adjusted(venue.sled, {.box_mass_kg = 3000.0, .knives = 6});
                const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
                const auto model = pull::Model::create({.tractor = puller.tractor, .sled = venue.sled, .track = venue.track});
                run::State state = run::start(*model);
                driver::State drv;
                const double r = puller.tractor.rear_tire.size.radius_m;
                std::printf("GEAR %-18s drive %d g%d:", f, t.final_drive, gear);
                double next = 0.5;
                while (!state.result) {
                    const double ts = static_cast<double>(state.steps) * 0.001;
                    const pull::Output o = run::step(state, *model, rules, driver::input(puller.driver, drv, ts, state.pull.tractor, model->tractor().params()), 0.001);
                    const double x = state.pull.sled.position_m - model->sled_start_m();
                    if (x >= next) {
                        std::printf(" @%.0f t%.1f %.0f/%.0f %.0fkN %.0frpm", next, ts - puller.driver.launch_s, state.pull.tractor.chassis.velocity_m_s * 3.6, state.pull.tractor.driveline.output_omega_rad_s * r * 3.6, o.sled.resistance_n / 1000.0, state.pull.tractor.engine.omega_rad_s * 9.549);
                        next += next < 1.0 ? 4.5 : 20.0;
                    }
                }
                std::printf(" -> %.1f m in %.1f s\n", state.result->distance_m, state.result->time_s - puller.driver.launch_s);
            }
        }
    }
}

TEST(TmpSpeeds, DISABLED_Print) {
    const data::SeasonDefinition d = supersport();
    for (const char* f : {"watered_start.toml", "ideal_clay_sand.toml", "covered_heavy_clay.toml", "firm_loam.toml", "dry_hard_clay.toml"}) {
        const data::Track on = track(f);
        for (const Tires t : {kPullerOnTall, Tires{.tires = kPullerTires, .final_drive = kMiddle}}) {
            // The operator's sled: the heaviest of the steps on which this tractor still pulls 90 m.
            const sled::Params cls = data::venue(d.event.sled, on).sled;
            int chosen = -99;
            for (int k = 8; k >= -12; --k) {
                const sled::Adjustment a = k >= 0 ? sled::Adjustment{.box_mass_kg = 500.0 * k, .knives = k}
                                                  : sled::Adjustment{.box_mass_kg = 300.0 * k, .knives = k};
                const Puller pl = built_puller(d, t, kSixLitre, Turbo::kBiggest);
                const Venue v = data::venue(d.event.sled, on);
                const auto tr = best_gear_trial(pl, sled::adjusted(v.sled, a), v.track, kLowestPullingGear, kHighestPullingGear, trial_pull_rules(d.event.rules, v, d.event.rules.full_pull_m));
                if (tr && tr->distance_m >= 90.0) { chosen = k; break; }
            }
            const sled::Adjustment a = chosen >= 0 ? sled::Adjustment{.box_mass_kg = 500.0 * chosen, .knives = chosen}
                                                   : sled::Adjustment{.box_mass_kg = 300.0 * chosen, .knives = chosen};
            const Speeds sp = middle_speeds(d, on, t, a);
            std::printf("SPEED %-24s drive %d sled step %3d wheel %5.1f ground %5.1f\n", f, t.final_drive, chosen, sp.wheel_kmh, sp.ground_kmh);
            (void)cls;
        }
    }
}
}  // namespace
}  // namespace tp::field
