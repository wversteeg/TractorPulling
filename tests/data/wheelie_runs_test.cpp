#include "tp/core/driver/driver.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/time/fixed_step_clock.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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
    run::Result result;
};

constexpr double kLaunchUntil_m = 10.0;
constexpr double kMiddleFrom_m = 25.0;
constexpr double kMiddleUntil_m = 65.0;
constexpr double kEndFrom_m = 80.0;

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
        } else if (pulled_m >= kEndFrom_m) {
            out.end_n = std::min(out.end_n, front_n);
        }
    }
    out.result = state.result.value_or(run::Result{});
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

FrontLoads sport_no_shift(const std::string& track_file, double ballast_position) {
    driver::Plan plan = kSportDriver;
    plan.downshift_share = 0.0;
    return pull("sport_2500.toml", "sport_2500.toml", track_file, ballast_position, plan);
}

FrontLoads sport(const std::string& track_file, double ballast_position) {
    return pull("sport_2500.toml", "sport_2500.toml", track_file, ballast_position, kSportDriver);
}

// The Sport tractor with the weights towards the back, as pullers set it on
// a good track: the front comes up on the launch, comes down while the sled
// is light, and comes up again as the sled gets heavy and the gear goes
// down under load.
TEST(WheelieRuns, ASportTractorLiftsOnTheLaunchAndAgainAsTheSledGetsHeavy) {
    const FrontLoads f = sport("ideal_clay_sand.toml", 0.25);
    EXPECT_EQ(f.launch_n, 0.0) << "up on the launch";
    EXPECT_GT(f.middle_n, 1000.0) << "down in the middle";
    EXPECT_EQ(f.end_n, 0.0) << "up again at the end";
}

// Without a powershift step the engine is pulled down before the front
// comes up again at the end, unless the weights are all the way back: then
// the front comes up by itself as the sled gets heavy.
TEST(WheelieRuns, WithoutAPowershiftOnlyTheWeightsAllBackLiftItAtTheEnd) {
    const FrontLoads quarter = sport_no_shift("ideal_clay_sand.toml", 0.25);
    const FrontLoads back = sport_no_shift("ideal_clay_sand.toml", 0.0);
    EXPECT_EQ(quarter.launch_n, 0.0);
    EXPECT_GT(quarter.end_n, 0.0) << "light, but down";
    EXPECT_EQ(quarter.result.ending, run::Ending::kStalled);
    EXPECT_EQ(back.end_n, 0.0) << "up by itself";
}

// The weights forward keep the front down the whole pull: the puller
// trades the wheelie for less grip at the rear, and spins out sooner.
TEST(WheelieRuns, TheWeightsForwardKeepTheFrontDown) {
    const FrontLoads back = sport("ideal_clay_sand.toml", 0.25);
    const FrontLoads forward = sport("ideal_clay_sand.toml", 1.0);
    EXPECT_GT(forward.launch_n, 0.0);
    EXPECT_GT(forward.middle_n, 0.0);
    EXPECT_GT(forward.end_n, 0.0);
    EXPECT_EQ(forward.result.ending, run::Ending::kSpunOut);
    EXPECT_LT(forward.result.distance_m, back.result.distance_m);
}

// On a track that grips less the same tractor lifts on the launch only:
// near the end the tires give way before the front comes up, and the
// wheels spin out.
TEST(WheelieRuns, OnALowGripTrackItLiftsOnlyOnTheLaunch) {
    const FrontLoads f = sport("covered_heavy_clay.toml", 0.25);
    EXPECT_EQ(f.launch_n, 0.0);
    EXPECT_GT(f.end_n, 0.0);
    EXPECT_EQ(f.result.ending, run::Ending::kSpunOut);
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
