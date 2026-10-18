#include "tp/core/math/curve.hpp"
#include "tp/core/track/track.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <optional>

namespace {

constexpr tp::soil::Composition kSoil{.clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};
constexpr std::array<tp::math::CurvePoint, 4> kWetPatch = {
    {{35.0, 0.14}, {40.0, 0.24}, {60.0, 0.24}, {65.0, 0.14}}};

// One surface lookup on a profiled track; a pull step does four (both
// tractor axles, the sled's pan and wheels). Sweeps along the track.
void BM_TrackSurfaceAt(benchmark::State& state) {
    const auto model =
        tp::track::Model::create({.soil = kSoil,
                                  .moisture_along_m = tp::math::Curve::create(kWetPatch),
                                  .compaction_along_m = std::nullopt});
    if (!model || !model->params().moisture_along_m) {
        state.SkipWithError("invalid track");
        return;
    }
    double x_m = 0.0;
    for (auto _ : state) {
        tp::soil::Surface surface = model->surface_at(x_m);
        benchmark::DoNotOptimize(surface);
        x_m = x_m > 100.0 ? 0.0 : x_m + 0.013;
    }
}
BENCHMARK(BM_TrackSurfaceAt);

}  // namespace
