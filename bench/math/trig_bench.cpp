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

}  // namespace
