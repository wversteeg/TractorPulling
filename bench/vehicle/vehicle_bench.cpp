#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 4;
constexpr double kDrawbar_n = 8000.0;

// One full 1 kHz step of the whole tractor pulling steadily: engine, clutch,
// driveline, tires and chassis, with the chain solve. The per-step cost of
// the simulation so far.
void BM_VehicleStep(benchmark::State& state) {
    const auto params =
        tp::data::load_tractor(std::string{TP_DATA_DIR} + "/tractors/farm_stock_light.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    const auto model = tp::vehicle::Model::create(params.value());
    if (!model) {
        state.SkipWithError("inconsistent tractor");
        return;
    }
    // Get to a steady pull before timing.
    tp::vehicle::State vehicle = tp::vehicle::initial_state(*model);
    for (int i = 0; i < 20000; ++i) {
        const double t_s = i * kDt_s;
        const tp::vehicle::Input input{
            .throttle = 1.0,
            .clutch_pedal = std::clamp(1.0 - (t_s - 2.0) / 3.0, 0.0, 1.0),
            .gear = kPullingGear};
        tp::vehicle::step(vehicle, *model, input, kDrawbar_n, kDt_s);
    }
    const tp::vehicle::Input pulling{.throttle = 1.0, .clutch_pedal = 0.0, .gear = kPullingGear};
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::vehicle::step(vehicle, *model, pulling, kDrawbar_n, kDt_s));
        benchmark::ClobberMemory();
    }
    if (!vehicle.engine.running || !vehicle.clutch.locked) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_VehicleStep);

}  // namespace
