#include "tp/core/clutch/clutch.hpp"
#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr double kDrivelineInertia_kg_m2 = 10.0;
constexpr double kDrivelineResistance_nm = 100.0;

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

// One full 1 kHz step of engine, clutch and a driveline stand-in, driving
// steadily with the clutch locked: the cost of the powertrain so far.
void BM_EngineClutchStep(benchmark::State& state) {
    const auto engine_params =
        tp::data::load_engine(std::string{TP_DATA_DIR} + "/engines/farm_stock_light.toml");
    const auto clutch_params =
        tp::data::load_clutch(std::string{TP_DATA_DIR} + "/clutches/farm_stock_light.toml");
    if (!engine_params || !clutch_params) {
        state.SkipWithError("cannot load reference engine or clutch");
        return;
    }
    constexpr double kCruise_rad_s = 220.0;
    tp::engine::State engine{.omega_rad_s = kCruise_rad_s, .boost = 0.5, .running = true};
    tp::clutch::State clutch{.locked = true};
    double driveline_omega_rad_s = kCruise_rad_s;
    for (auto _ : state) {
        const tp::engine::Prediction prediction =
            tp::engine::predict(engine, engine_params.value(), 1.0, kDt_s);
        const tp::coupling::ShaftPrediction driveline{
            .free_omega_next_rad_s =
                driveline_omega_rad_s - kDt_s / kDrivelineInertia_kg_m2 * kDrivelineResistance_nm,
            .omega_per_torque = kDt_s / kDrivelineInertia_kg_m2};
        const tp::clutch::Output out =
            tp::clutch::step(clutch, clutch_params.value(), {.pedal = 0.0},
                             tp::engine::shaft_prediction(prediction), driveline);
        tp::engine::advance(engine, engine_params.value(), prediction, out.torque_nm);
        driveline_omega_rad_s =
            driveline.free_omega_next_rad_s + driveline.omega_per_torque * out.torque_nm;
        benchmark::ClobberMemory();
    }
    if (!engine.running || !clutch.locked) {
        state.SkipWithError("left steady driving: the benchmark would time something else");
    }
}
BENCHMARK(BM_EngineClutchStep);

}  // namespace
