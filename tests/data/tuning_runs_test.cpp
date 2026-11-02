#include "tp/core/field/field.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::field {
namespace {

// The shipped light-class tractor, driven as the reference driver drives:
// fifth gear, the clutch slipped in over 3 s.
const driver::Plan kDriver{
    .gear = 5, .select_gear_s = 0.5, .launch_s = 3.0, .clutch_slip_in_s = 3.0};

std::filesystem::path data(const std::string& file) {
    return std::filesystem::path{TP_DATA_DIR} / file;
}

template <typename T>
T loaded(data::Result<T> result) {
    if (!result) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result).value();
}

// How far the tuned tractor pulls the class sled on `track` (a file in
// data/tracks/).
double reach_m(const vehicle::Tuning& tuning, const std::string& track_file) {
    const vehicle::Params tractor =
        loaded(data::load_tractor(data("tractors/farm_stock_light.toml")));
    const sled::Params sled = loaded(data::load_sled(data("sleds/light_class.toml")));
    const data::Track track = loaded(data::load_track(data("tracks/" + track_file)));
    const Venue venue = data::venue(sled, track);
    const std::optional<run::Result> result =
        run_pull({.tractor = vehicle::tuned(tractor, tuning), .driver = kDriver}, venue.sled,
                 venue.track, track.rules);
    if (!result) {
        throw std::runtime_error("the run was refused");
    }
    return result->distance_m;
}

vehicle::Tuning tuning(double ballast_position, double pressure_bar) {
    return {.ballast_position = ballast_position, .rear_tire_pressure_bar = pressure_bar};
}

// The settings matter in a whole pull, as the README says: the weights at
// the rear pull further than at the front.
TEST(TuningRuns, RearWeightsPullFurther) {
    EXPECT_GT(reach_m(tuning(0.0, 1.0), "ideal_clay_sand.toml"),
              reach_m(tuning(1.0, 1.0), "ideal_clay_sand.toml"));
}

// And what a pressure costs depends on the track: on dry hard clay soft
// tires' lugs squirm and lose metres; on the soft covered heavy clay their
// bigger footprint sinks less and makes up most of it.
TEST(TuningRuns, SoftTiresCostLessOnSoftGround) {
    const double soft_clay_m = reach_m(tuning(0.5, 0.7), "covered_heavy_clay.toml");
    const double clay_m = reach_m(tuning(0.5, 1.0), "covered_heavy_clay.toml");
    const double dry_m = reach_m(tuning(0.5, 1.0), "dry_hard_clay.toml");
    const double soft_dry_m = reach_m(tuning(0.5, 0.7), "dry_hard_clay.toml");
    EXPECT_GT(dry_m, soft_dry_m + 1.0) << dry_m << " m, " << soft_dry_m << " m";
    EXPECT_GT(soft_clay_m - clay_m, soft_dry_m - dry_m + 1.0)
        << "clay " << soft_clay_m << " m against " << clay_m << " m";
}

}  // namespace
}  // namespace tp::field
