#include "tp/core/clutch/clutch.hpp"
#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/data/clutch/clutch_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

// Clutch solve alone, alternating between locked and slipping so both
// branches are measured.
void BM_ClutchStep(benchmark::State& state) {
    const auto params =
        tp::data::load_clutch(std::string{TP_DATA_DIR} + "/clutches/farm_stock_light.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    tp::clutch::State clutch;
    const tp::coupling::ShaftPrediction engine{.free_omega_next_rad_s = 200.0,
                                               .omega_per_torque = 0.001 / 1.2};
    tp::coupling::ShaftPrediction driveline{.omega_per_torque = 0.001 / 10.0};
    double pedal = 0.0;
    for (auto _ : state) {
        driveline.free_omega_next_rad_s = pedal * 200.0;
        benchmark::DoNotOptimize(
            tp::clutch::step(clutch, params.value(), {.pedal = pedal}, engine, driveline));
        pedal = pedal > 0.9 ? 0.0 : pedal + 0.01;
    }
}
BENCHMARK(BM_ClutchStep);

}  // namespace
