#include "tp/core/driver/driver.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/time/fixed_step_clock.hpp"
#include "tp/core/turn/turn.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/files.hpp"
#include "tp/data/garage/garage_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>

namespace tp::data {
namespace {

constexpr double kDt_s = 0.001;

std::filesystem::path data(const std::string& file) {
    return std::filesystem::path{TP_DATA_DIR} / file;
}

template <typename T>
T loaded(Result<T> result) {
    if (!result) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result).value();
}

// Where the front tires carried least, in three stretches of a pull: the
// launch, the middle and the end, where the sled is heavy.
struct FrontLoads {
    double launch_n = std::numeric_limits<double>::max();
    double middle_n = std::numeric_limits<double>::max();
    double end_n = std::numeric_limits<double>::max();
    /// The front tires' load on average over the middle stretch [N].
    double middle_mean_n = 0.0;
    run::Result result;
};

// The middle ends before the push-down (from 60 m on the Sport 2.5 t's
// sled); the end is well into it.
constexpr double kLaunchUntil_m = 10.0;
constexpr double kMiddleFrom_m = 35.0;
constexpr double kMiddleUntil_m = 58.0;
constexpr double kEndFrom_m = 90.0;

// A pull on `track_file` with `tractor_file` tuned to `ballast_position`
// (0 rearmost, 1 foremost) and the class's sled, driven as `plan` says.
FrontLoads pull(const std::string& tractor_file, const std::string& sled_file,
                const std::string& track_file, double ballast_position, const driver::Plan& plan) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/" + tractor_file)));
    vehicle::Tuning tuning = vehicle::as_built(built);
    tuning.ballast_position = ballast_position;
    const sled::Params sled = loaded(load_sled(data("sleds/" + sled_file)));
    const Track track = loaded(load_track(data("tracks/" + track_file)));
    const session::Setup setup = run_setup(vehicle::tuned(built, tuning), sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    run::State state = run::start(*model);
    driver::State driver;
    FrontLoads out;
    double middle_sum_n = 0.0;
    int middle_steps = 0;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        const pull::Output step =
            run::step(state, *model, setup.rules,
                      driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        const double pulled_m = state.pull.sled.position_m - model->sled_start_m();
        const double front_n = step.tractor.axle_loads.front_n;
        if (pulled_m < kLaunchUntil_m) {
            out.launch_n = std::min(out.launch_n, front_n);
        } else if (pulled_m >= kMiddleFrom_m && pulled_m < kMiddleUntil_m) {
            out.middle_n = std::min(out.middle_n, front_n);
            middle_sum_n += front_n;
            ++middle_steps;
        } else if (pulled_m >= kEndFrom_m) {
            out.end_n = std::min(out.end_n, front_n);
        }
    }
    out.result = state.result.value_or(run::Result{});
    out.middle_mean_n = middle_steps > 0 ? middle_sum_n / middle_steps : 0.0;
    return out;
}

// The Sport 2.5 t class's driver: 5th, the clutch eased back on the launch
// while the revs are low, and a gear down under load near the end.
const driver::Plan kSportDriver{.gear = 5,
                                .select_gear_s = 0.5,
                                .launch_s = 3.0,
                                .clutch_slip_in_s = 3.0,
                                .downshift_share = 0.9,
                                .launch_hold_share = 0.8,
                                .launch_hold_max_s = 8.0};

// Where the weights sit: 0 rearmost, 1 foremost.
constexpr double kBack = 0.0;
constexpr double kQuarter = 0.25;
constexpr double kThreeQuarters = 0.75;
constexpr double kForward = 1.0;
// The front tires carry this much or more: down [N]; on average this much
// or more over a stretch: light, but down rather than up.
constexpr double kDown_n = 1000.0;
constexpr double kLight_n = 300.0;

FrontLoads sport(const std::string& track_file, double ballast_position) {
    return pull("sport_2500.toml", "sport_2500.toml", track_file, ballast_position, kSportDriver);
}

// The Sport tractor with the weights a quarter forward, as pullers set it
// on a good track: the front comes up on the launch, rides light through
// the middle while the sled is light, and comes up again as the push-down
// comes on.
TEST(WheelieRuns, ASportTractorLiftsOnTheLaunchAndAgainAsTheSledGetsHeavy) {
    const FrontLoads f = sport("ideal_clay_sand.toml", kQuarter);
    EXPECT_EQ(f.launch_n, 0.0) << "up on the launch";
    EXPECT_GT(f.middle_mean_n, kLight_n) << "down, light, in the middle";
    EXPECT_EQ(f.end_n, 0.0) << "up again at the end";
}

// The weights all back keep the front up the whole pull, on the wheelie
// bar (touching down at most for a moment), which carries load the rear
// tires lose: it pulls shorter than a quarter forward.
TEST(WheelieRuns, TheWeightsAllBackKeepItUpAllTheWay) {
    const FrontLoads back = sport("ideal_clay_sand.toml", kBack);
    EXPECT_EQ(back.launch_n, 0.0);
    EXPECT_LT(back.middle_mean_n, kLight_n / 2.0) << "up in the middle too";
    EXPECT_EQ(back.end_n, 0.0);
    EXPECT_LT(back.result.distance_m, sport("ideal_clay_sand.toml", kQuarter).result.distance_m);
}

// The chain, hooked low on the pan, pulls the hitch down and lifts the
// front: with the weights three-quarters or all forward the front stays
// down through the middle and comes up only at the end, as the push-down
// comes on (the launch still lifts it: 350 hp). On a good track the rear
// tires have the grip to spare, so the weights forward pull further than
// a quarter forward, which rides up and down through the middle.
TEST(WheelieRuns, TheWeightsForwardKeepTheFrontDown) {
    const FrontLoads quarter = sport("ideal_clay_sand.toml", kQuarter);
    for (const double position : {kThreeQuarters, kForward}) {
        const FrontLoads f = sport("ideal_clay_sand.toml", position);
        EXPECT_EQ(f.launch_n, 0.0) << position;
        EXPECT_GT(f.middle_n, kDown_n) << position;
        EXPECT_EQ(f.end_n, 0.0) << position;
        EXPECT_GT(f.result.distance_m, quarter.result.distance_m) << position;
    }
    EXPECT_EQ(quarter.middle_n, 0.0) << "up for moments in the middle";
}

// A pull that ends with the front up and the chain pulling hard leaves the
// chain stretched once the tractor stands, which would hold the front up.
// With the clutch pressed the tractor rolls back on it, free of the
// engine: the chain goes slack and the front comes down, so backing off
// after the pull starts from the ground, not from a wheelie.
TEST(WheelieRuns, PressingTheClutchAfterThePullLetsTheFrontDown) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/sport_2500.toml")));
    vehicle::Tuning tuning = vehicle::as_built(built);
    tuning.ballast_position = 0.0;
    const sled::Params sled = loaded(load_sled(data("sleds/sport_2500.toml")));
    const Track track = loaded(load_track(data("tracks/firm_loam.toml")));
    session::Setup setup = run_setup(vehicle::tuned(built, tuning), sled, track, 0);
    setup.sled.pushdown_start_m = setup.rules.full_pull_line_m;
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    // The pull as a whole turn has it: hooked at the line, the flag green.
    constexpr turn::Parking kParking{.x_m = -15.0, .y_m = -10.0};
    turn::State state = turn::from_the_parking(*model, kParking);
    state.run = run::start(*model);
    state.stage = turn::Stage::kPulling;
    // Off the line quicker than kSportDriver, the push-down not before the
    // line: a full pull.
    driver::Plan plan = kSportDriver;
    plan.select_gear_s = 0.0;
    plan.launch_s = 0.5;
    driver::State driver;
    pull::Output out;
    while (state.stage == turn::Stage::kPulling) {
        const double t_s = static_cast<double>(state.run.steps) * kDt_s;
        out = turn::step(state, *model, setup.rules, kParking,
                         driver::input(plan, driver, t_s, state.run.pull.tractor, setup.tractor),
                         kDt_s);
    }
    ASSERT_EQ(state.stage, turn::Stage::kReleasing);
    ASSERT_EQ(state.run.result.value_or(run::Result{}).ending, run::Ending::kFullPull);
    const int gear = state.run.pull.tractor.driveline.gear;
    ASSERT_GT(out.chain.tension_n, 12000.0) << "the chain pulling hard at the line";
    ASSERT_EQ(out.tractor.axle_loads.front_n, 0.0) << "the front up";
    // Off the gas, the clutch pressed: the tractor and the sled roll to a
    // stop, the chain between them.
    for (int i = 0; i < 6000; ++i) {
        out = turn::step(state, *model, setup.rules, kParking,
                         {.throttle = 0.0, .clutch_pedal = 1.0, .gear = gear}, kDt_s);
    }
    EXPECT_EQ(out.chain.tension_n, 0.0) << "the chain slack";
    EXPECT_GT(out.tractor.axle_loads.front_n, 0.0) << "the front down";
    EXPECT_LT(std::abs(state.run.pull.tractor.chassis.pitch_rad), 0.01);
}

// The standard class's farm tractor, its weights halfway and its clutch
// slipped in, keeps its front on the ground on every track: wheelies are
// for the built tractors of the Sport classes and up.
TEST(WheelieRuns, TheStandardTractorKeepsItsFrontDown) {
    const driver::Plan farm{
        .gear = 5, .select_gear_s = 0.5, .launch_s = 3.0, .clutch_slip_in_s = 3.0};
    for (const char* track :
         {"ideal_clay_sand.toml", "firm_loam.toml", "covered_heavy_clay.toml"}) {
        const FrontLoads f = pull("farm_stock_light.toml", "light_class.toml", track, 0.5, farm);
        EXPECT_GT(f.launch_n, 0.0) << track;
        EXPECT_GT(f.middle_n, 0.0) << track;
        EXPECT_GT(f.end_n, 0.0) << track;
    }
}

// How long a class's reference tractor (fully built) takes from moving off
// to `metre` on the class's sled on ideal clay-sand, in `gear`, launched as
// the events' drivers do [s]; infinity if it never gets there.
double seconds_to(const std::string& klass, int gear, double metre) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/" + klass + ".toml")));
    const sled::Params sled = loaded(load_sled(data("sleds/" + klass + ".toml")));
    const Track track = loaded(load_track(data("tracks/ideal_clay_sand.toml")));
    const session::Setup setup = run_setup(built, sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    driver::Plan plan = kSportDriver;
    plan.gear = gear;
    run::State state = run::start(*model);
    driver::State driver;
    constexpr double kMovingFrom_m = 0.1;
    double moved_s = -1.0;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, setup.rules,
                  driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        const double pulled_m = state.pull.sled.position_m - model->sled_start_m();
        if (moved_s < 0.0 && pulled_m > kMovingFrom_m) {
            moved_s = t_s;
        }
        if (pulled_m >= metre) {
            return t_s - moved_s;
        }
    }
    return std::numeric_limits<double>::infinity();
}

// The Sport engines and puller tires are set from videos of the classes'
// top tractors on ordinary tracks: a 2.6 t tractor pulls 100 m in about
// 20 s from moving off, a 4.5 t one about 115 m in a pull-off in about
// 15 s. The fully built tractors in their pulling gears reach 80 m about
// that fast.
TEST(SportPace, TheTopTractorsPullAsFastAsInTheVideos) {
    const double sport_25_s = seconds_to("sport_2500", 6, 80.0);
    const double sport_45_s = seconds_to("sport_4500", 7, 80.0);
    EXPECT_GT(sport_25_s, 14.0);
    EXPECT_LT(sport_25_s, 18.5);
    EXPECT_GT(sport_45_s, 11.0);
    EXPECT_LT(sport_45_s, 14.5);
    EXPECT_LT(sport_45_s, sport_25_s);
}

// How a pull rides: how often the nose went up (past kNoseUp_deg after
// being down below kNoseDown_deg) and how long it was up [s].
struct Ride {
    int lifts = 0;
    double up_s = 0.0;
    run::Result result;
};

constexpr double kNoseUp_deg = 6.0;
constexpr double kNoseDown_deg = 2.0;

Ride ride(const vehicle::Params& tractor, const std::string& track_file, int gear) {
    vehicle::Tuning tuning = vehicle::as_built(tractor);
    tuning.ballast_position = kForward;
    const sled::Params sled = loaded(load_sled(data("sleds/sport_2500.toml")));
    const Track track = loaded(load_track(data("tracks/" + track_file)));
    const session::Setup setup = run_setup(vehicle::tuned(tractor, tuning), sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    driver::Plan plan = kSportDriver;
    plan.gear = gear;
    run::State state = run::start(*model);
    driver::State driver;
    Ride out;
    bool up = false;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, setup.rules,
                  driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        const double pitch_deg = state.pull.tractor.chassis.pitch_rad * 180.0 / std::numbers::pi;
        if (!up && pitch_deg > kNoseUp_deg) {
            up = true;
            ++out.lifts;
        } else if (up && pitch_deg < kNoseDown_deg) {
            up = false;
        }
        out.up_s += up ? kDt_s : 0.0;
    }
    out.result = state.result.value_or(run::Result{});
    return out;
}

// A Sport 2.5 t as a promo puller has it: every part two stages up (100 kg
// of its weights free), the free weights forward, in 4th on the grippy
// firm loam. The weights not yet free are bolted to the front bracket
// (Ballast::fixed_position), so the nose comes up on the launch and at the
// end and stays down between. Fixed at the back, as they were, it hopped
// onto its wheelie bar and back again all along the track.
TEST(WheelieRuns, ASportTractorWithLittleWeightFreeDoesNotHopOnTheLoam) {
    const vehicle::Params reference = loaded(load_tractor(data("tractors/sport_2500.toml")));
    const GarageDefinition g =
        loaded(load_garage(data("garages/sport_2500.toml").generic_string(), read_from_disk));
    garage::Career career = garage::new_career(g.catalogue);
    constexpr int kStages = 2;
    for (std::size_t i = 0; i < g.catalogue.count; ++i) {
        career.levels.at(i) =
            std::min(kStages, static_cast<int>(g.catalogue.upgrades.at(i).level_count) - 1);
    }
    const std::optional<vehicle::Params> equipped =
        garage::equipped(reference, g.catalogue, career);
    ASSERT_TRUE(equipped.has_value());
    const vehicle::Params promo = equipped.value_or(reference);
    ASSERT_LT(promo.chassis.ballast.mass_kg, reference.chassis.ballast.mass_kg);
    constexpr int kGear = 4;
    const Ride front = ride(promo, "firm_loam.toml", kGear);
    EXPECT_LE(front.lifts, 2) << "the launch and the end";
    EXPECT_LT(front.up_s, 3.0);

    vehicle::Params old_reference = reference;
    old_reference.chassis.ballast.fixed_position = 0.0;
    const std::optional<vehicle::Params> old = garage::equipped(old_reference, g.catalogue, career);
    ASSERT_TRUE(old.has_value());
    const Ride back = ride(old.value_or(old_reference), "firm_loam.toml", kGear);
    EXPECT_GE(back.lifts, 4) << "fixed at the back it hopped";
    EXPECT_GT(back.up_s, 2.0 * front.up_s);
    EXPECT_GT(front.result.distance_m, back.result.distance_m);
}

// How a Supersport 3.6 t pull carries its nose: how often the front comes
// off the ground and back down, how far it is up, its load through the
// middle, and whether it is down at the end.
struct Nose {
    int lifts = 0;
    int touchdowns = 0;
    double up_m = 0.0;
    double middle_mean_n = 0.0;
    bool down_at_the_end = false;
    double distance_m = 0.0;
};

// The class's tractor as built (tractors/supersport_3600.toml: the 7 l,
// puller tires, the middle final drive) on the clay-sand in 5th, its weights
// at `ballast_position` and the hitch at `hitch_m`, on a sled six steps
// heavier than the class's so that it stops short of the line, as at the
// end of a pull-off; the launch held while the turbo comes up
// ("optoeren").
Nose supersport_nose(double ballast_position, double hitch_m) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/supersport_3600.toml")));
    vehicle::Tuning tuning = vehicle::as_built(built);
    tuning.ballast_position = ballast_position;
    tuning.hitch_height_m = hitch_m;
    constexpr int kHeavier = 6;
    const sled::Params sled =
        sled::heavier(loaded(load_sled(data("sleds/supersport_3600.toml"))), kHeavier);
    const Track track = loaded(load_track(data("tracks/ideal_clay_sand.toml")));
    const session::Setup setup = run_setup(vehicle::tuned(built, tuning), sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    const driver::Plan plan{.gear = 5,
                            .select_gear_s = 0.5,
                            .launch_s = 3.0,
                            .clutch_slip_in_s = 3.0,
                            .launch_hold_share = 0.8,
                            .launch_hold_max_s = 8.0};
    run::State state = run::start(*model);
    driver::State driver;
    Nose out;
    bool up = false;
    double last_m = 0.0;
    double middle_sum_n = 0.0;
    int middle_steps = 0;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        const pull::Output step =
            run::step(state, *model, setup.rules,
                      driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        const double pulled_m = state.pull.sled.position_m - model->sled_start_m();
        const double front_n = step.tractor.axle_loads.front_n;
        const bool now_up = front_n <= 0.0;
        out.lifts += now_up && !up ? 1 : 0;
        out.touchdowns += !now_up && up ? 1 : 0;
        out.up_m += now_up ? std::max(pulled_m - last_m, 0.0) : 0.0;
        if (pulled_m >= kMiddleFrom_m && pulled_m < kMiddleUntil_m) {
            middle_sum_n += front_n;
            ++middle_steps;
        }
        up = now_up;
        last_m = pulled_m;
    }
    out.down_at_the_end = !up;
    out.middle_mean_n = middle_steps > 0 ? middle_sum_n / middle_steps : 0.0;
    out.distance_m = state.result.value_or(run::Result{}).distance_m;
    return out;
}

// A good Supersport 3.6 t pull (videos of NTTO pulls): the nose comes up on
// the launch, touches down softly, rides light, and is down at the end.
// With the weights halfway and the hitch in its top hole the tractor does
// that, and pulls furthest. The weights back keep the nose up all the way,
// on the wheelie bar, which takes load the rear tires lose; the hitch in
// its lowest hole holds the front down, and a front held down does not
// give the rear tires the most grip either: both pull clearly shorter.
TEST(WheelieRuns, ASupersportTractorLiftsOnceTouchesDownSoftlyAndEndsDown) {
    constexpr double kTopHole_m = 0.50;
    constexpr double kLowestHole_m = 0.40;
    constexpr double kHalfway = 0.5;
    const Nose good = supersport_nose(kHalfway, kTopHole_m);
    EXPECT_GE(good.lifts, 1) << "up on the launch";
    EXPECT_GE(good.touchdowns, 1) << "down softly after the launch";
    EXPECT_LT(good.up_m, 0.5 * good.distance_m) << "not up all the way";
    EXPECT_GT(good.middle_mean_n, 0.0);
    EXPECT_LT(good.middle_mean_n, kDown_n * 3.0) << "light through the middle";
    EXPECT_TRUE(good.down_at_the_end);

    const Nose back = supersport_nose(0.0, kTopHole_m);
    // All but the launch: the nose is up within a few tenths of a metre.
    constexpr double kLaunch_m = 0.3;
    EXPECT_GT(back.up_m, back.distance_m - kLaunch_m) << "up all the way, on the bar";
    const Nose held_down = supersport_nose(kHalfway, kLowestHole_m);
    EXPECT_LT(held_down.up_m, 1.0) << "the front held down";
    constexpr double kClearly_m = 5.0;
    EXPECT_GT(good.distance_m, back.distance_m + kClearly_m);
    EXPECT_GT(good.distance_m, held_down.distance_m + kClearly_m);
}

}  // namespace
}  // namespace tp::data
