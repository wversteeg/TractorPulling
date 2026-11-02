#include "tp/core/engine/engine.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr double kCruise_rad_s = 220.0;

// One 1 kHz engine step of the reference engine running under steady load at
// full throttle: the per-step cost the simulation pays for the engine. It
// starts at cruise speed, because a load applied at idle stalls the engine
// and a stalled engine is much cheaper to step.
void BM_EngineStep(benchmark::State& state) {
    const auto params =
        tp::data::load_engine(std::string{TP_DATA_DIR} + "/engines/farm_stock_light.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    tp::engine::State engine{.omega_rad_s = kCruise_rad_s, .boost = 0.5, .running = true};
    const tp::engine::Input input{.throttle = 1.0, .load_torque_nm = 250.0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::engine::step(engine, params.value(), input, kDt_s));
        benchmark::ClobberMemory();
    }
    if (!engine.running) {
        state.SkipWithError("engine stalled: the benchmark would time a stopped engine");
    }
}
BENCHMARK(BM_EngineStep);

}  // namespace
