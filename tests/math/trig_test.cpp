#include "tp/core/math/trig.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace tp::math {
namespace {

constexpr int kSamples = 10000;

double sample(int i) {
    return -kSinCosMaxAngle_rad + 2.0 * kSinCosMaxAngle_rad * i / kSamples;
}

TEST(SinCos, MatchesTheStandardLibraryOverItsRange) {
    for (int i = 0; i <= kSamples; ++i) {
        const double x = sample(i);
        const SinCos sc = sin_cos(x);
        ASSERT_NEAR(sc.sin, std::sin(x), 2e-16) << x;
        ASSERT_NEAR(sc.cos, std::cos(x), 2e-16) << x;
    }
}

TEST(SinCos, IsExactAtZero) {
    static_assert(sin_cos(0.0).sin == 0.0);
    static_assert(sin_cos(0.0).cos == 1.0);
}

TEST(SinCos, HasTheSymmetriesOfSineAndCosine) {
    for (int i = 0; i <= kSamples; ++i) {
        const double x = sample(i);
        ASSERT_EQ(sin_cos(-x).sin, -sin_cos(x).sin);
        ASSERT_EQ(sin_cos(-x).cos, sin_cos(x).cos);
    }
}

TEST(SinCos, StaysOnTheUnitCircle) {
    for (int i = 0; i <= kSamples; ++i) {
        const SinCos sc = sin_cos(sample(i));
        ASSERT_NEAR(sc.sin * sc.sin + sc.cos * sc.cos, 1.0, 4e-16);
    }
}

std::uint64_t samples_hash() {
    hash::Fnv1a hash;
    for (int i = 0; i <= kSamples; ++i) {
        const SinCos sc = sin_cos(sample(i));
        hash.add(sc.sin);
        hash.add(sc.cos);
    }
    return hash.value();
}

// Regression hash; must match on every compiler, build type and platform.
TEST(SinCos, MatchesRegressionHash) {
    EXPECT_EQ(samples_hash(), 0x85a6612d51307038ULL);
}

}  // namespace
}  // namespace tp::math
