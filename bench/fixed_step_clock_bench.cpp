#include "tp/core/fixed_step_clock.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

namespace {

// One 60 Hz frame of 16.67 ms, the typical per-frame call from a front end.
constexpr std::int64_t kFrameNs = 16'666'667;

void BM_FixedStepClockAdvance(benchmark::State& state) {
    auto clock = tp::FixedStepClock::create({});
    if (!clock) {
        state.SkipWithError("invalid FixedStepClock config");
        return;
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(clock->advance(kFrameNs));
    }
}
BENCHMARK(BM_FixedStepClockAdvance);

}  // namespace
