#include "tp/core/controls/controls.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 6;
// Millions of timed steps would carry the sled past the push-down and the
// end of the track; out of reach, the pull stays steady and the cost per
// step is the same.
constexpr double kOutOfReach_m = 1.0e9;
constexpr double kNeverTimesOut_s = 1.0e9;
constexpr int kStepsToSteady = 12000;

std::string data_file(const char* name) {
    return std::string{TP_DATA_DIR} + "/" + name;
}

// One simulated millisecond as the game runs it: the easy layout with its
// clutch assist, then a step of the run (the pull, the lane and the run's
// end checks), on the shipped tractor, sled and track. This is what the
// speed budget is set on (docs/performance.md).
void BM_GameStep(benchmark::State& state) {
    const auto tractor = tp::data::load_tractor(data_file("tractors/farm_stock_light.toml"));
    const auto sled = tp::data::load_sled(data_file("sleds/light_class.toml"));
    const auto track = tp::data::load_track(data_file("tracks/ideal_clay_sand.toml"));
    const auto easy = tp::data::load_easy_controls(data_file("controls/easy.toml"));
    if (!tractor || !sled || !track || !easy) {
        state.SkipWithError("cannot load the shipped tractor, sled, track or easy layout");
        return;
    }
    tp::sled::Params steady_sled = tp::sled::with_conditions(sled.value(), track.value().sled);
    steady_sled.pushdown_start_m = kOutOfReach_m;
    steady_sled.box_travel_distance_m = kOutOfReach_m;
    const auto model = tp::pull::Model::create(
        {.tractor = tractor.value(), .sled = steady_sled, .track = track.value().ground});
    if (!model) {
        state.SkipWithError("inconsistent tractor, sled or track");
        return;
    }
    tp::run::Rules rules = track.value().rules;
    rules.track_length_m = kOutOfReach_m;
    rules.full_pull_line_m = kOutOfReach_m;
    rules.max_duration_s = kNeverTimesOut_s;

    const tp::vehicle::Params& params = model->tractor().params();
    tp::run::State run = tp::run::start(*model);
    tp::controls::AssistState assist;
    const tp::controls::EasyInput pulling{.throttle = 1.0, .gear = kPullingGear};
    const auto game_step = [&] {
        const tp::vehicle::Input input =
            tp::controls::easy(pulling, easy.value(), assist, params, run.pull.tractor, kDt_s);
        return tp::run::step(run, *model, rules, input, kDt_s);
    };
    // Get to a steady pull, the clutch fully in, before timing.
    for (int i = 0; i < kStepsToSteady; ++i) {
        game_step();
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(game_step());
        benchmark::ClobberMemory();
    }
    if (run.result || assist.engagement < 1.0 || run.pull.sled.velocity_m_s <= 0.0) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_GameStep);

}  // namespace
