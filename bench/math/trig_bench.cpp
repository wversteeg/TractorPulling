#include "tp/core/math/trig.hpp"

#include <benchmark/benchmark.h>

namespace {

void BM_SinCos(benchmark::State& state) {
    double angle_rad = 0.1;
    for (auto _ : state) {
        benchmark::DoNotOptimize(angle_rad);
        tp::math::SinCos sc = tp::math::sin_cos(angle_rad);
        benchmark::DoNotOptimize(sc);
    }
}
BENCHMARK(BM_SinCos);

// A heading in the second quadrant: the reduction path.
void BM_SinCosAny(benchmark::State& state) {
    double angle_rad = 2.0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(angle_rad);
        tp::math::SinCos sc = tp::math::sin_cos_any(angle_rad);
        benchmark::DoNotOptimize(sc);
    }
}
BENCHMARK(BM_SinCosAny);

}  // namespace
