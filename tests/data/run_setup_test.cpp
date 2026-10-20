#include "tp/data/session/run_setup.hpp"

#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>

namespace tp::data {
namespace {

const std::filesystem::path kDataDir{TP_DATA_DIR};

struct Loaded {
    vehicle::Params tractor;
    sled::Params class_sled;
    Track track;
};

// The shipped tractor and sled on a track whose push-down differs from the
// sled's own.
std::optional<Loaded> load() {
    const auto tractor = load_tractor(kDataDir / "tractors/farm_stock_light.toml");
    const auto sled = load_sled(kDataDir / "sleds/light_class.toml");
    const auto track = load_track(kDataDir / "tracks/dry_hard_clay.toml");
    if (!tractor || !sled || !track) {
        return std::nullopt;
    }
    return Loaded{.tractor = tractor.value(), .class_sled = sled.value(), .track = track.value()};
}

// The sled as the operator sets it on this track, the track's ground and
// rules, in the pro layout, and a valid setup.
TEST(RunSetup, IsTheClassSledOnTheTrack) {
    const std::optional<Loaded> loaded = load();
    if (!loaded) {
        FAIL() << "cannot load the shipped data";
    }
    const auto& [tractor, class_sled, track] = *loaded;
    const session::Setup setup = run_setup(tractor, class_sled, track);
    const std::optional<double>& pushdown_start_m = track.sled.pushdown_start_m;
    if (!pushdown_start_m) {
        FAIL() << "the track sets no push-down";
    }
    EXPECT_EQ(setup.sled.pushdown_start_m, *pushdown_start_m);
    EXPECT_NE(setup.sled.pushdown_start_m, class_sled.pushdown_start_m);
    EXPECT_EQ(setup.sled.box_mass_kg, class_sled.box_mass_kg);
    EXPECT_EQ(setup.rules.lane_width_m, track.rules.lane_width_m);
    EXPECT_EQ(setup.rules.track_length_m, track.rules.track_length_m);
    EXPECT_EQ(setup.tractor.chassis.mass_kg, tractor.chassis.mass_kg);
    EXPECT_EQ(setup.layout, session::Layout::kPro);
    EXPECT_FALSE(session::validate(setup));
}

// Each pull-off makes the sled one step heavier, as between pull-offs.
TEST(RunSetup, MakesTheSledHeavierForEachPullOff) {
    const std::optional<Loaded> loaded = load();
    if (!loaded) {
        FAIL() << "cannot load the shipped data";
    }
    const auto& [tractor, class_sled, track] = *loaded;
    const sled::Params today = run_setup(tractor, class_sled, track).sled;
    const sled::Params second = run_setup(tractor, class_sled, track, 2).sled;
    const sled::Params expected = sled::heavier(sled::heavier(today));
    EXPECT_EQ(second.box_mass_kg, expected.box_mass_kg);
    EXPECT_EQ(second.knives.count, expected.knives.count);
    EXPECT_EQ(second.knives.depth_m, expected.knives.depth_m);
    EXPECT_EQ(second.pushdown_start_m, expected.pushdown_start_m);
    // Heavier: more weight in the box or more knives, never less of either.
    EXPECT_GE(second.box_mass_kg, today.box_mass_kg);
    EXPECT_GE(second.knives.count, today.knives.count);
    EXPECT_TRUE(second.box_mass_kg > today.box_mass_kg || second.knives.count > today.knives.count);
}

}  // namespace
}  // namespace tp::data
