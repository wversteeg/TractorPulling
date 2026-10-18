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

// Hard, dry clay carries the tractor and the sled best; wet heavy clay is
// soft and slick, and the pan ploughs deep in it, so the same tractor gets
// least far there. Loose sand sits in between.
TEST(ShippedTracks, TheGroundDecidesHowFarAndHowAPullEnds) {
    const run::Result ideal = pull_on("ideal_clay_sand.toml");
    const run::Result hard = pull_on("dry_hard_clay.toml");
    const run::Result wet = pull_on("wet_heavy_clay.toml");
    const run::Result sand = pull_on("loose_sand.toml");

    EXPECT_EQ(ideal.ending, run::Ending::kStalled);
    EXPECT_EQ(hard.ending, run::Ending::kStalled);

    EXPECT_GT(hard.distance_m, ideal.distance_m);
    EXPECT_LT(sand.distance_m, ideal.distance_m);
    EXPECT_LT(wet.distance_m, sand.distance_m);
}

}  // namespace
}  // namespace tp::data
