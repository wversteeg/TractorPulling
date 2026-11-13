#include "tp/core/driver/driver.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace tp::data {
namespace {

const std::filesystem::path kDataDir{TP_DATA_DIR};
constexpr double kDt_s = 0.001;
// The reference tractor's pulling gear on the 15 t sled, launched as the
// events' drivers do: the clutch eased back while the engine is below
// 80 % of its rated speed.
constexpr driver::Plan kDriver{.gear = 5,
                               .select_gear_s = 0.5,
                               .launch_s = 3.0,
                               .clutch_slip_in_s = 3.0,
                               .launch_hold_share = 0.8,
                               .launch_hold_max_s = 8.0};

// The reference tractor on the class's sled, with its own push-down, in its
// pulling gear: only the ground differs between tracks.
run::Result pull_on(const std::string& track_file) {
    const auto tractor = load_tractor(kDataDir / "tractors/farm_stock_light.toml");
    const auto sled = load_sled(kDataDir / "sleds/light_class.toml");
    const auto track = load_track(kDataDir / "tracks" / track_file);
    EXPECT_TRUE(tractor.ok() && sled.ok() && track.ok());
    if (!tractor.ok() || !sled.ok() || !track.ok()) {
        return {};
    }
    const auto model = pull::Model::create(
        {.tractor = tractor.value(), .sled = sled.value(), .track = track.value().ground});
    EXPECT_TRUE(model.has_value());
    if (!model) {
        return {};
    }
    run::State state = run::start(*model);
    driver::State driver;
    for (int i = 0; !state.result; ++i) {
        const vehicle::Input input =
            driver::input(kDriver, driver, i * kDt_s, state.pull.tractor, tractor.value());
        run::step(state, *model, track.value().rules, input, kDt_s);
    }
    return state.result.value_or(run::Result{});
}

// Every shipped track is pullable: in its pulling gear the reference
// tractor gets past the push-down on all of them, stalling the engine or,
// where the track grips less, spinning the wheels; the tracks differ by
// metres, not tens.
TEST(ShippedTracks, EveryTrackIsPullable) {
    double shortest_m = 1e9;
    double longest_m = 0.0;
    for (const char* name :
         {"ideal_clay_sand.toml", "dry_hard_clay.toml", "covered_heavy_clay.toml",
          "sandy_clay.toml", "firm_loam.toml", "watered_start.toml"}) {
        const run::Result result = pull_on(name);
        EXPECT_TRUE(result.ending == run::Ending::kStalled ||
                    result.ending == run::Ending::kSpunOut)
            << name;
        EXPECT_GT(result.distance_m, 80.0) << name;
        EXPECT_LT(result.distance_m, 100.0) << name;
        shortest_m = std::min(shortest_m, result.distance_m);
        longest_m = std::max(longest_m, result.distance_m);
    }
    EXPECT_LT(longest_m - shortest_m, 12.0);
}

// How a track was prepared (its grip) decides how a pull ends: on the
// grippiest, firm loam, and on the reference the engine gives out first;
// on the heavy clay the tires dig in and bite, so the engine stalls before
// they spin out; on the loose sandy clay, which grips least, the wheels
// spin out. (With the push-down putting at most part of the sled on the
// pan, the drag levels out where the farm engine is pulled down.)
TEST(ShippedTracks, TheTracksGripDecidesHowAPullEnds) {
    const run::Result grippy = pull_on("firm_loam.toml");
    const run::Result reference = pull_on("ideal_clay_sand.toml");
    const run::Result heavy = pull_on("covered_heavy_clay.toml");
    const run::Result loose = pull_on("sandy_clay.toml");
    EXPECT_EQ(grippy.ending, run::Ending::kStalled);
    EXPECT_EQ(reference.ending, run::Ending::kStalled);
    EXPECT_EQ(heavy.ending, run::Ending::kStalled);
    EXPECT_EQ(loose.ending, run::Ending::kSpunOut);
    EXPECT_GE(grippy.distance_m, reference.distance_m);
    EXPECT_LT(loose.distance_m, reference.distance_m);
}

// The class's sled at `metre` of its travel on a track, the chain pulling
// 20 kN: what it drags [N].
double sled_drag_n(const std::string& track_file, double metre) {
    const auto sled = load_sled(kDataDir / "sleds/light_class.toml");
    const auto track = load_track(kDataDir / "tracks" / track_file);
    EXPECT_TRUE(sled.ok() && track.ok());
    if (!sled.ok() || !track.ok()) {
        return 0.0;
    }
    const auto ground = track::Model::create(track.value().ground);
    EXPECT_TRUE(ground.has_value());
    if (!ground) {
        return 0.0;
    }
    const soil::Surface surface = ground->surface_at(metre);
    constexpr double kChainPull_n = 20000.0;
    return sled::loads(sled.value(), {.distance_m = metre}, {.forward_n = kChainPull_n},
                       {.pan = surface, .wheels = surface})
        .resistance_n;
}

// The heavy clay, moist after the rain, is softer and stickier than the
// reference: the pan ploughs deeper and the wheels sink further, so the
// sled drags harder all the way. (A pull's distance hardly shows it: on
// the 15 t sled the push-down stops the light class, and the clay's grip
// makes up the rest.)
TEST(ShippedTracks, MoistHeavyClayDragsHarder) {
    for (int metre = 0; metre <= 90; metre += 10) {
        EXPECT_GT(sled_drag_n("covered_heavy_clay.toml", metre),
                  sled_drag_n("ideal_clay_sand.toml", metre))
            << metre << " m";
    }
}

}  // namespace
}  // namespace tp::data
