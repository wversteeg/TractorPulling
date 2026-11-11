#include "tp/core/driver/driver.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

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

// Every step of a pull: how far the sled has come, what it drags, the
// engine's speed and the rear wheels' slip.
struct Sample {
    double pulled_m = 0.0;
    double resistance_n = 0.0;
    double engine_rad_s = 0.0;
    double slip_ratio = 0.0;
};

// The Sport 2.5 t tractor pulling its class's sled on `track`, its driver
// in 5th without a downshift, so the engine alone answers the drag.
std::vector<Sample> pull(const Track& track) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/sport_2500.toml")));
    const sled::Params sled = loaded(load_sled(data("sleds/sport_2500.toml")));
    const session::Setup setup = run_setup(built, sled, track, 0);
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
    std::vector<Sample> samples;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        const pull::Output step =
            run::step(state, *model, setup.rules,
                      driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        samples.push_back({.pulled_m = state.pull.sled.position_m - model->sled_start_m(),
                           .resistance_n = step.sled.resistance_n,
                           .engine_rad_s = state.pull.tractor.engine.omega_rad_s,
                           .slip_ratio = step.tractor.slip_ratio});
    }
    return samples;
}

// Over the middle of the pull, within a metre: the most the drag rose (as
// a share), the engine slowed and the wheels' slip rose.
struct Jumps {
    double drag_rise = 0.0;
    double engine_drop_rad_s = 0.0;
    double slip_rise = 0.0;
};

constexpr double kFrom_m = 15.0;
constexpr double kUntil_m = 60.0;
constexpr double kWindow_m = 1.0;

Jumps jumps(const std::vector<Sample>& samples) {
    Jumps out;
    std::size_t back = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Sample& now = samples[i];
        while (samples[back].pulled_m < now.pulled_m - kWindow_m) {
            ++back;
        }
        if (now.pulled_m < kFrom_m || now.pulled_m > kUntil_m) {
            continue;
        }
        const Sample& before = samples[back];
        out.drag_rise = std::max(out.drag_rise, now.resistance_n / before.resistance_n - 1.0);
        out.engine_drop_rad_s =
            std::max(out.engine_drop_rad_s, before.engine_rad_s - now.engine_rad_s);
        out.slip_rise = std::max(out.slip_rise, now.slip_ratio - before.slip_ratio);
    }
    return out;
}

// The dry hard clay without its weak spots, and without its hard spots.
Track only_hard_spots() {
    Track track = loaded(load_track(data("tracks/dry_hard_clay.toml")));
    track.ground.weak_spots.reset();
    return track;
}

Track even(Track track) {
    track.ground.hard_spots.reset();
    track.ground.weak_spots.reset();
    return track;
}

// On the dry hard clay the sled's drag jumps at each hard spot and the
// engine dips with it; the same track without its hard spots drags evenly,
// rising only with the box's travel, and the engine runs steadier.
TEST(HardSpotRuns, TheDragJumpsAndTheEngineDipsAtHardSpots) {
    const Track spotted = only_hard_spots();
    ASSERT_TRUE(spotted.ground.hard_spots.has_value());
    const Jumps with = jumps(pull(spotted));
    const Jumps without = jumps(pull(even(spotted)));
    EXPECT_GT(with.drag_rise, 0.15) << "a jump of a sixth or more";
    EXPECT_LT(without.drag_rise, 0.05) << "only the box's travel";
    EXPECT_GT(with.engine_drop_rad_s, 2.0 * without.engine_drop_rad_s);
}

// Where the tires lose grip at a weak spot the wheels slip more for a
// moment; the drag stays even. The shipped tracks have few and mild weak
// spots, so this one has them closer and deeper, to show them clearly.
TEST(HardSpotRuns, TheWheelsSlipMoreAtWeakSpots) {
    Track weak = even(only_hard_spots());
    weak.ground.weak_spots =
        track::WeakSpots{.every_m = 10.0, .length_m = 2.0, .grip_loss = 0.3, .seed = 3};
    const Jumps with = jumps(pull(weak));
    const Jumps without = jumps(pull(even(weak)));
    EXPECT_LT(with.drag_rise, 0.05) << "the sled drags as before";
    EXPECT_GT(with.slip_rise, 2.0 * without.slip_rise)
        << with.slip_rise << " vs " << without.slip_rise;
}

}  // namespace
}  // namespace tp::data
