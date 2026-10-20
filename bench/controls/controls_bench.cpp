#include "tp/core/controls/controls.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 6;

// The easy layout's cost per step, on top of the simulation: the steering
// split and the clutch assist, which reads the engine speed and turns its
// engagement into a pedal through the clutch's curve. Timed with the engine
// below the guard speed, so the assist is slipping the clutch.
void BM_EasyControls(benchmark::State& state) {
    const auto tractor =
        tp::data::load_tractor(std::string{TP_DATA_DIR} + "/tractors/farm_stock_light.toml");
    const auto easy =
        tp::data::load_easy_controls(std::string{TP_DATA_DIR} + "/controls/easy.toml");
    if (!tractor || !easy) {
        state.SkipWithError("cannot load the tractor or the easy layout");
        return;
    }
    const auto model = tp::vehicle::Model::create(tractor.value());
    if (!model) {
        state.SkipWithError("inconsistent tractor");
        return;
    }
    tp::vehicle::State vehicle = tp::vehicle::initial_state(*model);
    vehicle.driveline.gear = kPullingGear;
    vehicle.engine.omega_rad_s =
        0.55 * tractor.value().engine.rated_omega_rad_s;  // below the 60 % guard
    const tp::controls::AssistState assist{.engagement = 0.5};
    const tp::controls::EasyInput input{.throttle = 1.0, .gear = kPullingGear, .steering = 0.7};
    for (auto _ : state) {
        tp::controls::AssistState now = assist;
        benchmark::DoNotOptimize(
            tp::controls::easy(input, easy.value(), now, tractor.value(), vehicle, kDt_s));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EasyControls);

}  // namespace
