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

// Side grip for one tire, cut by its traction, its side force and the
// patch's relaxation: what the tractor adds per axle each step.
void BM_TireSideGrip(benchmark::State& state) {
    const auto params = tp::data::load_tire(std::string{TP_DATA_DIR} + "/tires/pulling_rear.toml");
    if (!params) {
        state.SkipWithError(params.error().message);
        return;
    }
    const tp::tire::Params& tire = params.value();
    const tp::soil::Surface surface = tp::soil::surface(kSoil);
    double deflection_m = 0.0;
    double lateral_m_s = -0.05;
    for (auto _ : state) {
        benchmark::DoNotOptimize(lateral_m_s);
        const tp::tire::SideGrip grip = tp::tire::with_traction(
            tp::tire::side_grip(tire.size, tire.cornering, kLoad_n, surface), 0.5 * kLoad_n,
            tp::tire::full_bite(tire, kLoad_n, surface) * kLoad_n, 0.1);
        benchmark::DoNotOptimize(tp::tire::side_force_n(grip, deflection_m));
        deflection_m =
            tp::tire::relax_deflection_m(grip, tire.cornering, deflection_m, lateral_m_s,
                                         kGround_m_s, tire.min_reference_speed_m_s, 0.001);
    }
}
BENCHMARK(BM_TireSideGrip);

}  // namespace
