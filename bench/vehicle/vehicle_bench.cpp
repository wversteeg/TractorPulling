#include "tp/core/soil/soil.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cmath>
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

// One step while a brake holds its side at rest, in gear: first gear at
// full throttle against a heavy drawbar, full pedal on the left, so the
// left side stands still and the right side spins. On top of the full
// chain solve, the hold takes extra solves to find the holding torque: the
// step's dearest case. Each iteration steps from the same state, copied in
// (a few hundred bytes).
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
    constexpr int kFirstGear = 1;
    constexpr double kHeavyDrawbar_n = 20000.0;
    const auto input = [](double t_s) {
        return tp::vehicle::Input{.throttle = 1.0,
                                  .clutch_pedal = std::clamp(1.0 - (t_s - 2.0) / 3.0, 0.0, 1.0),
                                  .gear = kFirstGear,
                                  .brake_left = t_s >= 6.0 ? 1.0 : 0.0};
    };
    // Drive until a step holds the left side with less than full torque,
    // and keep the state just before it.
    tp::vehicle::State vehicle = tp::vehicle::initial_state(*model);
    tp::vehicle::State before_hold;
    double hold_t_s = -1.0;
    for (int i = 0; i < 20000 && hold_t_s < 0.0; ++i) {
        const double t_s = i * kDt_s;
        const tp::vehicle::State before = vehicle;
        const tp::vehicle::Output out = tp::vehicle::step(vehicle, *model, input(t_s), ground,
                                                          {.pull_n = kHeavyDrawbar_n}, kDt_s);
        const double left_rad_s = vehicle.driveline.output_omega_rad_s - vehicle.differential_rad_s;
        if (t_s >= 6.0 && vehicle.engine.running &&
            std::abs(left_rad_s) <= tp::vehicle::kBrakeHoldTolerance_rad_s &&
            std::abs(out.rear_left.brake_torque_nm) < params.value().brakes.max_torque_nm) {
            before_hold = before;
            hold_t_s = t_s;
        }
    }
    if (hold_t_s < 0.0) {
        state.SkipWithError("no held step in gear: the benchmark would time something else");
        return;
    }
    const tp::vehicle::Input held = input(hold_t_s);
    for (auto _ : state) {
        tp::vehicle::State copy = before_hold;
        benchmark::DoNotOptimize(
            tp::vehicle::step(copy, *model, held, ground, {.pull_n = kHeavyDrawbar_n}, kDt_s));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VehicleStepBrakeHeld);

}  // namespace
