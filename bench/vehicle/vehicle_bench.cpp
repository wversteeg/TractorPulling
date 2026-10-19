#include "tp/core/soil/soil.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 4;
constexpr double kDrawbar_n = 8000.0;
// The reference track: moist, rolled clay and sand.
constexpr tp::soil::Composition kSoil{.clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};

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
    const tp::soil::Surface surface = tp::soil::surface(kSoil);
    const tp::vehicle::Ground ground{.rear = surface, .front = surface};
    // Get to a steady pull before timing.
    tp::vehicle::State vehicle = tp::vehicle::initial_state(*model);
    for (int i = 0; i < 20000; ++i) {
        const double t_s = i * kDt_s;
        const tp::vehicle::Input input{
            .throttle = 1.0,
            .clutch_pedal = std::clamp(1.0 - (t_s - 2.0) / 3.0, 0.0, 1.0),
            .gear = kPullingGear};
        tp::vehicle::step(vehicle, *model, input, ground, {.pull_n = kDrawbar_n}, kDt_s);
    }
    const tp::vehicle::Input pulling{.throttle = 1.0, .clutch_pedal = 0.0, .gear = kPullingGear};
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            tp::vehicle::step(vehicle, *model, pulling, ground, {.pull_n = kDrawbar_n}, kDt_s));
        benchmark::ClobberMemory();
    }
    if (!vehicle.engine.running || !vehicle.clutch.locked) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_VehicleStep);

// One step while a brake holds its side at rest: rolling in neutral, full
// pedal on the left. The hold takes extra solves (Newton steps on the held
// side's end speed), so this is the step's dearest case. Each iteration
// steps from the same state, copied in: a few hundred bytes.
void BM_VehicleStepBrakeHeld(benchmark::State& state) {
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
    const tp::soil::Surface surface = tp::soil::surface(kSoil);
    const tp::vehicle::Ground ground{.rear = surface, .front = surface};
    constexpr double kRolling_m_s = 2.0;
    tp::vehicle::State held = tp::vehicle::initial_state(*model);
    held.chassis.velocity_m_s = kRolling_m_s;
    held.driveline.output_omega_rad_s = kRolling_m_s / params.value().rear_tire.size.radius_m;
    const tp::vehicle::Input braking{
        .clutch_pedal = 1.0, .gear = tp::driveline::kNeutral, .brake_left = 1.0};
    // Roll until the left side is held.
    tp::vehicle::Output out;
    for (int i = 0; i < 300; ++i) {
        out = tp::vehicle::step(held, *model, braking, ground, {}, kDt_s);
    }
    if (out.rear_left.brake_torque_nm >= params.value().brakes.max_torque_nm) {
        state.SkipWithError("the left side is not held: the benchmark would time something else");
        return;
    }
    for (auto _ : state) {
        tp::vehicle::State vehicle = held;
        benchmark::DoNotOptimize(tp::vehicle::step(vehicle, *model, braking, ground, {}, kDt_s));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VehicleStepBrakeHeld);

}  // namespace
