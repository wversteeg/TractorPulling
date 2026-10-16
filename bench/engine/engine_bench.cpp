#include "tp/core/engine/engine.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;

// One 1 kHz engine step of the reference engine under steady load at full
// throttle: the per-step cost the simulation pays for the engine.
void BM_EngineStep(benchmark::State& state) {
    const auto params =
        tp::data::load_engine(std::string{TP_DATA_DIR} + "/engines/farm_stock_light.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    tp::engine::State engine = tp::engine::initial_state(params.value());
    const tp::engine::Input input{.throttle = 1.0, .load_torque_nm = 250.0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::engine::step(engine, params.value(), input, kDt_s));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EngineStep);

}  // namespace
