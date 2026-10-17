#include "tp/core/chassis/chassis.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr double kDrawbar_n = 8000.0;

// The pitch part of one 1 kHz step: axle loads from the state, then pitch
// with both contacts evaluated. Pulling steadily, so the front tires are in
// contact and the drag moments apply.
void BM_ChassisPitchStep(benchmark::State& state) {
    const auto params =
        tp::data::load_tractor(std::string{TP_DATA_DIR} + "/tractors/farm_stock_light.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    const tp::chassis::Params& chassis = params.value().chassis;
    tp::chassis::State body{.velocity_m_s = 2.0};
    // The rear tires carry the pull at the ground.
    const double axle_torque_nm = kDrawbar_n * chassis.rear_axle_height_m;
    for (auto _ : state) {
        benchmark::DoNotOptimize(tp::chassis::axle_loads(chassis, body));
        tp::chassis::advance_pitch(body, chassis, axle_torque_nm, kDrawbar_n, kDt_s);
        benchmark::ClobberMemory();
    }
    if (tp::chassis::axle_loads(chassis, body).front_n <= 0.0) {
        state.SkipWithError("front lifted: the benchmark would time something else");
    }
}
BENCHMARK(BM_ChassisPitchStep);

}  // namespace
