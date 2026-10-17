#include "tp/core/clutch/clutch.hpp"
#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <benchmark/benchmark.h>

#include <string>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kPullingGear = 4;
// Stand-in vehicle: 3500 kg on 0.8 m tires, pulling 15 kN.
constexpr double kVehicleInertia_kg_m2 = 3500.0 * 0.8 * 0.8;
constexpr double kAxleLoad_nm = 15000.0 * 0.8;
constexpr double kCruiseEngine_rad_s = 235.0;

std::string data_path(const char* relative) {
    return std::string{TP_DATA_DIR} + "/" + relative;
}

void BM_DrivelineStep(benchmark::State& state) {
    const auto params = tp::data::load_driveline(data_path("drivelines/farm_stock_light.toml"));
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    tp::driveline::State driveline;
    tp::driveline::shift(driveline, params.value(), kPullingGear, true);
    driveline.input_omega_rad_s = kCruiseEngine_rad_s;
    driveline.output_omega_rad_s =
        kCruiseEngine_rad_s / tp::driveline::overall_ratio(params.value(), kPullingGear);
    for (auto _ : state) {
        const tp::driveline::Prediction p =
            tp::driveline::predict(driveline, params.value(), kAxleLoad_nm, kDt_s);
        benchmark::DoNotOptimize(tp::driveline::input_shaft(p));
        tp::driveline::advance(driveline, p, 100.0);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_DrivelineStep);

// One full 1 kHz step of engine, clutch and driveline pulling steadily in
// gear with the clutch locked: the cost of the whole powertrain.
void BM_PowertrainStep(benchmark::State& state) {
    const auto engine_params = tp::data::load_engine(data_path("engines/farm_stock_light.toml"));
    const auto clutch_params = tp::data::load_clutch(data_path("clutches/farm_stock_light.toml"));
    auto driveline_params = tp::data::load_driveline(data_path("drivelines/farm_stock_light.toml"));
    if (!engine_params || !clutch_params || !driveline_params) {
        state.SkipWithError("cannot load the reference powertrain");
        return;
    }
    tp::driveline::Params driveline_with_vehicle = driveline_params.value();
    driveline_with_vehicle.output_inertia_kg_m2 += kVehicleInertia_kg_m2;

    tp::engine::State engine{.omega_rad_s = kCruiseEngine_rad_s, .boost = 0.8, .running = true};
    tp::clutch::State clutch{.locked = true};
    tp::driveline::State driveline;
    tp::driveline::shift(driveline, driveline_with_vehicle, kPullingGear, true);
    driveline.input_omega_rad_s = kCruiseEngine_rad_s;
    driveline.output_omega_rad_s =
        kCruiseEngine_rad_s / tp::driveline::overall_ratio(driveline_with_vehicle, kPullingGear);

    for (auto _ : state) {
        const tp::engine::Prediction e =
            tp::engine::predict(engine, engine_params.value(), 1.0, kDt_s);
        const tp::driveline::Prediction d =
            tp::driveline::predict(driveline, driveline_with_vehicle, kAxleLoad_nm, kDt_s);
        const tp::clutch::Output out =
            tp::clutch::step(clutch, clutch_params.value(), {.pedal = 0.0},
                             tp::engine::shaft_prediction(e), tp::driveline::input_shaft(d));
        tp::engine::advance(engine, engine_params.value(), e, out.torque_nm);
        tp::driveline::advance(driveline, d, out.torque_nm);
        benchmark::ClobberMemory();
    }
    if (!engine.running || !clutch.locked) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_PowertrainStep);

}  // namespace
