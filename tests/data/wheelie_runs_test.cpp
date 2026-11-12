#include "tp/core/driver/driver.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/time/fixed_step_clock.hpp"
#include "tp/core/turn/turn.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
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

constexpr double kLaunchUntil_m = 10.0;
constexpr double kMiddleFrom_m = 55.0;
constexpr double kMiddleUntil_m = 85.0;
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
constexpr double kHalfway = 0.5;
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
// bar, which carries load the rear tires lose: it pulls shorter than a
// quarter forward.
TEST(WheelieRuns, TheWeightsAllBackKeepItUpAllTheWay) {
    const FrontLoads back = sport("ideal_clay_sand.toml", kBack);
    EXPECT_EQ(back.launch_n, 0.0);
    EXPECT_LT(back.middle_mean_n, kLight_n / 10.0) << "up in the middle too";
    EXPECT_EQ(back.end_n, 0.0);
    EXPECT_LT(back.result.distance_m, sport("ideal_clay_sand.toml", kQuarter).result.distance_m);
}

// Halfway, the front is down through the middle and comes up only at the
// end; all forward it stays down from the launch on (the launch still
// lifts it: 350 hp). Both trade grip at the rear for it and pull shorter
// than a quarter forward.
TEST(WheelieRuns, TheWeightsForwardKeepTheFrontDown) {
    const FrontLoads quarter = sport("ideal_clay_sand.toml", kQuarter);
    const FrontLoads halfway = sport("ideal_clay_sand.toml", kHalfway);
    const FrontLoads forward = sport("ideal_clay_sand.toml", kForward);
    EXPECT_GT(halfway.middle_n, kDown_n);
    EXPECT_EQ(halfway.end_n, 0.0);
    EXPECT_GT(forward.middle_n, kDown_n);
    EXPECT_GT(forward.end_n, kDown_n);
    EXPECT_LT(halfway.result.distance_m, quarter.result.distance_m);
    EXPECT_LT(forward.result.distance_m, quarter.result.distance_m);
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
    ASSERT_GT(out.chain.tension_n, 20000.0) << "the chain pulling hard at the line";
    ASSERT_EQ(out.tractor.axle_loads.front_n, 0.0) << "the front up";
    // Off the gas, the clutch pressed: the tractor and the sled roll to a
    // stop, the chain between them.
    for (int i = 0; i < 3000; ++i) {
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

}  // namespace
}  // namespace tp::data

