#include "tp/core/crew/crew.hpp"
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
#include <limits>
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

// Every step of a pull: how far the sled has come, what its pan drags (its
// friction and the knives: what a hard spot bites into) and the rear
// wheels' slip.
struct Sample {
    double pulled_m = 0.0;
    double pan_drag_n = 0.0;
    double slip_ratio = 0.0;
};

// The Sport 2.5 t tractor pulling its class's sled on `track`, its driver
// in 5th without a downshift, so the engine alone answers the drag.
std::vector<Sample> pull(const Track& track, double start_y_m = 0.0, int pull_offs = 0) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/sport_2500.toml")));
    const sled::Params sled = loaded(load_sled(data("sleds/sport_2500.toml")));
    const session::Setup setup = run_setup(built, sled, track, pull_offs);
    const std::optional<pull::Model> model = pull::Model::create({.tractor = setup.tractor,
                                                                  .sled = setup.sled,
                                                                  .track = setup.ground,
                                                                  .start_y_m = start_y_m});
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
        samples.push_back(
            {.pulled_m = state.pull.sled.position_m - model->sled_start_m(),
             .pan_drag_n = step.sled.pan_friction * step.sled.pan_n + step.sled.knives_n,
             .slip_ratio = step.tractor.slip_ratio});
    }
    return samples;
}

// Over the middle of the pull, within a metre: the most the pan's drag rose
// (as a share) and the wheels' slip rose.
struct Jumps {
    double drag_rise = 0.0;
    double slip_rise = 0.0;
};

constexpr double kFrom_m = 15.0;
constexpr double kUntil_m = 80.0;
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
        if (before.pan_drag_n > 0.0) {
            out.drag_rise = std::max(out.drag_rise, now.pan_drag_n / before.pan_drag_n - 1.0);
        }
        out.slip_rise = std::max(out.slip_rise, now.slip_ratio - before.slip_ratio);
    }
    return out;
}

// The dry hard clay with its hard spots alone: its weak spots taken away.
Track only_hard_spots() {
    Track track = loaded(load_track(data("tracks/dry_hard_clay.toml")));
    track.ground.weak_spots.reset();
    return track;
}

// `track` without any spots: neither hard nor weak.
Track even(Track track) {
    track.ground.hard_spots.reset();
    track.ground.weak_spots.reset();
    return track;
}

// On the dry hard clay the pan's drag jumps at each hard spot; the same
// track without its hard spots drags evenly, rising only with the box's
// travel. (On the 15 t sled the pan carries a small share of the sled's
// weight until the box comes forward, so a spot shows in the pan's drag
// more than in the whole sled's or the engine's speed.)
TEST(HardSpotRuns, ThePansDragJumpsAtHardSpots) {
    const Track spotted = only_hard_spots();
    ASSERT_TRUE(spotted.ground.hard_spots.has_value());
    const Jumps with = jumps(pull(spotted));
    const Jumps without = jumps(pull(even(spotted)));
    EXPECT_GT(with.drag_rise, 0.15) << "a jump of a sixth or more";
    EXPECT_LT(without.drag_rise, 0.05) << "only the box's travel";
}

// Where the tires lose grip at a weak spot the wheels slip more for a
// moment; the drag stays even. The shipped tracks have few and mild weak
// spots, so this one has them closer and deeper, to show them clearly.
TEST(HardSpotRuns, TheWheelsSlipMoreAtWeakSpots) {
    Track weak = even(only_hard_spots());
    weak.ground.weak_spots = track::WeakSpots{.every_m = 10.0,
                                              .length_m = 2.0,
                                              .min_width_m = 12.0,
                                              .max_width_m = 12.0,
                                              .grip_loss = 0.3,
                                              .seed = 3};
    const Jumps with = jumps(pull(weak));
    const Jumps without = jumps(pull(even(weak)));
    EXPECT_LT(with.drag_rise, 0.05) << "the sled drags as before";
    EXPECT_GT(with.slip_rise, 2.0 * without.slip_rise)
        << with.slip_rise << " vs " << without.slip_rise;
}

// The crew that knows the track puts the sled where the spots cost least:
// over several events' layouts on the dry hard clay its line pulls further
// on average than the middle and than the worst line across the lane, and
// close to the best of them. The stakes are small (the spots are mild,
// and on the 15 t sled the push-down ends the pull wherever the tractor
// is): a tenth or two of a metre on a heavy pull-off sled.
TEST(HardSpotRuns, TheCrewsLineCrossesTheFewestSpots) {
    const vehicle::Params built = loaded(load_tractor(data("tractors/sport_2500.toml")));
    const Track file = loaded(load_track(data("tracks/dry_hard_clay.toml")));
    const double max_m = run::max_start_offset_m(built, file.rules);
    constexpr int kPullOffs = 8;
    constexpr int kEvents = 8;
    constexpr int kLines = 3;  // each side of the middle
    double crews_m = 0.0;
    double middle_m = 0.0;
    double worst_m = 0.0;
    double best_m = 0.0;
    for (std::uint64_t seed = 1; seed <= kEvents; ++seed) {
        Track track = file;
        track.ground = track::reseeded(file.ground, seed);
        const std::optional<track::Model> model = track::Model::create(track.ground);
        if (!model) {
            FAIL() << "the track was refused";
        }
        const crew::Report knows = crew::report(*model, {.pulls_seen = 100, .pull_off = true},
                                                track.rules.full_pull_line_m, seed);
        const auto reached = [&](double y_m) {
            return pull(track, y_m, kPullOffs).back().pulled_m;
        };
        double worst_here_m = std::numeric_limits<double>::max();
        double best_here_m = 0.0;
        for (int i = -kLines; i <= kLines; ++i) {
            const double here_m = reached(max_m * i / kLines);
            worst_here_m = std::min(worst_here_m, here_m);
            best_here_m = std::max(best_here_m, here_m);
            if (i == 0) {
                middle_m += here_m / kEvents;
            }
        }
        crews_m += reached(crew::best_line_m(knows, built, max_m)) / kEvents;
        worst_m += worst_here_m / kEvents;
        best_m += best_here_m / kEvents;
    }
    EXPECT_GT(crews_m, middle_m + 0.05);
    EXPECT_GT(crews_m, worst_m + 0.2);
    EXPECT_GT(crews_m, best_m - 0.15);
}

}  // namespace
}  // namespace tp::data
