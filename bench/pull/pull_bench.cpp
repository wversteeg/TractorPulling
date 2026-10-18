#include "tp/core/pull/pull.hpp"
#include "tp/core/soil/soil.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <optional>
#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 6;
// Millions of timed steps would carry the sled past the push-down and stall
// the engine; out of reach, the pull stays steady and the cost per step is
// the same.
constexpr double kOutOfReach_m = 1.0e9;
// The reference track: moist, rolled clay and sand.
constexpr tp::soil::Composition kSoil{.clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};

// One full 1 kHz step of a pull: the chain, the whole tractor and the sled.
// The per-step cost of the simulation.
void BM_PullStep(benchmark::State& state) {
    const auto tractor =
        tp::data::load_tractor(std::string{TP_DATA_DIR} + "/tractors/farm_stock_light.toml");
    const auto sled = tp::data::load_sled(std::string{TP_DATA_DIR} + "/sleds/light_class.toml");
    if (!tractor || !sled) {
        state.SkipWithError(!tractor ? tractor.error().message : sled.error().message);
        return;
    }
    tp::sled::Params steady_sled = sled.value();
    steady_sled.pushdown_start_m = kOutOfReach_m;
    steady_sled.box_travel_distance_m = kOutOfReach_m;
    const auto model = tp::pull::Model::create(
        {.tractor = tractor.value(),
         .sled = steady_sled,
         .track = {
             .soil = kSoil, .moisture_along_m = std::nullopt, .compaction_along_m = std::nullopt}});
    if (!model) {
        state.SkipWithError("inconsistent tractor or sled");
        return;
    }
    // Get to a steady pull before timing.
    tp::pull::State pull = tp::pull::initial_state(*model);
    for (int i = 0; i < 12000; ++i) {
        const double t_s = i * kDt_s;
        const tp::vehicle::Input input{
            .throttle = 1.0,
            .clutch_pedal = std::clamp(1.0 - (t_s - 2.0) / 3.0, 0.0, 1.0),
            .gear = kPullingGear};
        tp::pull::step(pull, *model, input, kDt_s);
    }
    const tp::vehicle::Input pulling{.throttle = 1.0, .clutch_pedal = 0.0, .gear = kPullingGear};
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::pull::step(pull, *model, pulling, kDt_s));
        benchmark::ClobberMemory();
    }
    if (!pull.tractor.engine.running || pull.sled.velocity_m_s <= 0.0) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_PullStep);

}  // namespace
