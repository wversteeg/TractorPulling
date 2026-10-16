#include "tp/core/fixed_step_clock.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace tp {
namespace {

constexpr std::int64_t kMs = 1'000'000;

FixedStepClock make_clock(FixedStepClock::Config config = {}) {
    const auto clock = FixedStepClock::create(config);
    if (!clock) {
        ADD_FAILURE() << "invalid FixedStepClock config";
        std::abort();
    }
    return *clock;
}

TEST(FixedStepClock, DefaultsToOneKilohertz) {
    const auto clock = make_clock();
    EXPECT_EQ(clock.step_ns(), kMs);
    EXPECT_DOUBLE_EQ(clock.step_s(), 0.001);
    EXPECT_EQ(clock.accumulated_ns(), 0);
    EXPECT_DOUBLE_EQ(clock.alpha(), 0.0);
}

TEST(FixedStepClock, RejectsInvalidConfig) {
    EXPECT_FALSE(FixedStepClock::create({.step_ns = 0}).has_value());
    EXPECT_FALSE(FixedStepClock::create({.step_ns = -kMs}).has_value());
    EXPECT_FALSE(FixedStepClock::create({.max_steps_per_advance = 0}).has_value());
    EXPECT_FALSE(FixedStepClock::create({.step_ns = std::numeric_limits<std::int64_t>::max() / 2,
                                         .max_steps_per_advance = 2})
                     .has_value());
}

TEST(FixedStepClock, OneStepPerStepOfElapsedTime) {
    auto clock = make_clock();
    const auto result = clock.advance(kMs);
    EXPECT_EQ(result.steps, 1U);
    EXPECT_EQ(result.dropped_ns, 0);
    EXPECT_EQ(clock.accumulated_ns(), 0);
}

TEST(FixedStepClock, CarriesPartialStepsAcrossCalls) {
    auto clock = make_clock();
    EXPECT_EQ(clock.advance(400'000).steps, 0U);
    EXPECT_EQ(clock.advance(400'000).steps, 0U);
    EXPECT_EQ(clock.advance(400'000).steps, 1U);
    EXPECT_EQ(clock.accumulated_ns(), 200'000);
    EXPECT_DOUBLE_EQ(clock.alpha(), 0.2);
}

TEST(FixedStepClock, IgnoresNonPositiveElapsedTime) {
    auto clock = make_clock();
    EXPECT_EQ(clock.advance(0).steps, 0U);
    EXPECT_EQ(clock.advance(-kMs).steps, 0U);
    EXPECT_EQ(clock.accumulated_ns(), 0);
}

// Frames of uneven length that add up to exactly one second must give exactly
// 1000 steps: integer time cannot drift the way summed doubles do.
TEST(FixedStepClock, OneSecondOfUnevenFramesIsExactlyOneThousandSteps) {
    auto clock = make_clock();
    constexpr std::array<std::int64_t, 4> kFramesNs = {16'666'667, 16'666'666, 16'666'667, 333'333};
    std::int64_t elapsed_ns = 0;
    std::uint64_t steps = 0;
    std::size_t i = 0;
    while (elapsed_ns < kNanosecondsPerSecond) {
        const std::int64_t frame_ns =
            std::min(kFramesNs.at(i++ % kFramesNs.size()), kNanosecondsPerSecond - elapsed_ns);
        elapsed_ns += frame_ns;
        steps += clock.advance(frame_ns).steps;
    }
    EXPECT_EQ(steps, 1000U);
    EXPECT_EQ(clock.accumulated_ns(), 0);
}

TEST(FixedStepClock, CapsStepsAndReportsDroppedTime) {
    auto clock = make_clock({.max_steps_per_advance = 100});
    const auto result = clock.advance(kNanosecondsPerSecond);
    EXPECT_EQ(result.steps, 100U);
    EXPECT_EQ(result.dropped_ns, 900 * kMs);
    EXPECT_EQ(clock.accumulated_ns(), 0);
}

TEST(FixedStepClock, CapKeepsCarriedPartialStep) {
    auto clock = make_clock({.max_steps_per_advance = 10});
    EXPECT_EQ(clock.advance(kMs / 2).steps, 0U);
    const auto result = clock.advance(50 * kMs);
    EXPECT_EQ(result.steps, 10U);
    EXPECT_EQ(result.dropped_ns, 40 * kMs);
    EXPECT_EQ(clock.accumulated_ns(), kMs / 2);
}

TEST(FixedStepClock, HugeElapsedTimeDoesNotOverflow) {
    auto clock = make_clock();
    EXPECT_EQ(clock.advance(kMs / 2).steps, 0U);
    const auto result = clock.advance(std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(result.steps, 100U);
    EXPECT_EQ(clock.accumulated_ns(), kMs / 2);
}

TEST(FixedStepClock, AlphaStaysInUnitInterval) {
    auto clock = make_clock();
    for (std::int64_t frame_ns = 1; frame_ns < 3 * kMs; frame_ns += 7'919) {
        static_cast<void>(clock.advance(frame_ns));
        EXPECT_GE(clock.alpha(), 0.0);
        EXPECT_LT(clock.alpha(), 1.0);
    }
}

TEST(FixedStepClock, ResetDiscardsAccumulatedTime) {
    auto clock = make_clock();
    EXPECT_EQ(clock.advance(kMs / 2).steps, 0U);
    clock.reset();
    EXPECT_EQ(clock.accumulated_ns(), 0);
    EXPECT_EQ(clock.advance(kMs / 2).steps, 0U);
}

}  // namespace
}  // namespace tp
