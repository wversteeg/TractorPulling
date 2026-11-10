#include "tp/core/run/run.hpp"
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
constexpr int kPullingGear = 6;

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
    for (int i = 0; !state.result; ++i) {
        const double t_s = i * kDt_s;
        const vehicle::Input input{
            .throttle = t_s < 0.5 ? 0.0 : 1.0,
            .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
            .gear = t_s < 0.5 ? driveline::kNeutral : kPullingGear};
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
// grippiest, firm loam, and the reference the engine gives out first; on
// the heavy clay the tires dig in and bite, so the engine stalls before
// they spin out; on the loose sandy clay, which grips least, the wheels
// spin out.
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

// The heavy clay, moist after the rain, is softer and stickier than the
// reference: the pan ploughs deeper and the same tractor gets less far.
TEST(ShippedTracks, MoistHeavyClayPullsShorter) {
    EXPECT_LT(pull_on("covered_heavy_clay.toml").distance_m,
              pull_on("ideal_clay_sand.toml").distance_m);
}

}  // namespace
}  // namespace tp::data
