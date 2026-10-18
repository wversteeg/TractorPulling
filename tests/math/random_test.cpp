#include "tp/core/math/random.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace tp::math {
namespace {

// The reference SplitMix64 sequence for seed 0 (Vigna's splitmix64.c): the
// same numbers on every platform.
TEST(SplitMix64, MatchesTheReferenceSequence) {
    SplitMix64 random{0};
    EXPECT_EQ(random.next(), 0xe220a8397b1dcdafULL);
    EXPECT_EQ(random.next(), 0x6e789e6aa1b965f4ULL);
    EXPECT_EQ(random.next(), 0x06c45d188009454fULL);
}

TEST(SplitMix64, UniformStaysInRange) {
    SplitMix64 random{12345};
    double sum = 0.0;
    constexpr int kSamples = 10000;
    for (int i = 0; i < kSamples; ++i) {
        const double u = random.uniform();
        ASSERT_GE(u, 0.0);
        ASSERT_LT(u, 1.0);
        sum += u;
        const double v = random.uniform(-2.0, 3.0);
        ASSERT_GE(v, -2.0);
        ASSERT_LT(v, 3.0);
        ASSERT_LT(random.below(7), 7U);
    }
    EXPECT_NEAR(sum / kSamples, 0.5, 0.02);
}

}  // namespace
}  // namespace tp::math
