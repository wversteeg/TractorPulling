#include "tp/core/soil/soil.hpp"
#include "tp/core/tire/tire.hpp"
#include "tp/data/tire/tire_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

// The reference track: moist, rolled clay and sand.
constexpr tp::soil::Composition kSoil{.clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};
constexpr double kLoad_n = 11000.0;
constexpr double kGround_m_s = 1.5;

// Traction and its slope for one tire on soil, plus its motion resistance at
// that slip: what the tractor evaluates per rear tire each step. Sweeps
// across the slip curve.
void BM_TireTraction(benchmark::State& state) {
    const auto params = tp::data::load_tire(std::string{TP_DATA_DIR} + "/tires/pulling_rear.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    const tp::soil::Surface surface = tp::soil::surface(kSoil);
    double wheel_m_s = 1.6;
    for (auto _ : state) {
        tp::tire::Traction traction =
            tp::tire::traction(params.value(), wheel_m_s, kGround_m_s, kLoad_n, surface);
        benchmark::DoNotOptimize(traction);
        benchmark::DoNotOptimize(tp::tire::driven_rolling_resistance(params.value().size, kLoad_n,
                                                                     traction.slip_ratio, surface));
        wheel_m_s = wheel_m_s > 4.0 ? 1.6 : wheel_m_s + 0.01;
    }
}
BENCHMARK(BM_TireTraction);

}  // namespace
