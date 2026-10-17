#include "tp/core/tire/tire.hpp"
#include "tp/data/tire/tire_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

// Traction and its slope for one tire, sweeping across the slip curve.
void BM_TireTraction(benchmark::State& state) {
    const auto params = tp::data::load_tire(std::string{TP_DATA_DIR} + "/tires/pulling_rear.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    double wheel_m_s = 1.6;
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::tire::traction(params.value(), wheel_m_s, 1.5, 11000.0));
        wheel_m_s = wheel_m_s > 4.0 ? 1.6 : wheel_m_s + 0.01;
    }
}
BENCHMARK(BM_TireTraction);

}  // namespace
