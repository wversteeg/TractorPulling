#include "tp/core/math/exp.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

namespace tp::math {
namespace {

constexpr int kSamples = 20000;

double sample(int i) {
    // -60 to +10: the soil and tire model use e^-y for y up to about 50.
    return -60.0 + 70.0 * i / kSamples;
}

TEST(Exp, MatchesTheStandardLibrary) {
    for (int i = 0; i <= kSamples; ++i) {
        const double x = sample(i);
        const double expected = std::exp(x);
        ASSERT_NEAR(math::exp(x), expected, 4e-16 * expected) << x;
    }
}

TEST(Exp, IsExactWhereItMustBe) {
    EXPECT_EQ(math::exp(0.0), 1.0);
    EXPECT_EQ(math::exp(-1000.0), 0.0);
    EXPECT_EQ(math::exp(1000.0), std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(math::exp(std::nan(""))));
}

TEST(Exp, NeverDecreases) {
    double previous = 0.0;
    for (int i = 0; i <= kSamples; ++i) {
        const double value = math::exp(sample(i));
        ASSERT_GE(value, previous) << sample(i);
        previous = value;
    }
}

TEST(Exp, TurnsSumsIntoProducts) {
    for (int i = 0; i <= 100; ++i) {
        const double a = -5.0 + 0.1 * i;
        EXPECT_NEAR(math::exp(a) * math::exp(-a), 1.0, 1e-15) << a;
    }
}

std::uint64_t samples_hash() {
    hash::Fnv1a hash;
    for (int i = 0; i <= kSamples; ++i) {
        hash.add(math::exp(sample(i)));
    }
    return hash.value();
}

// Regression hash; must match on every compiler, build type and platform.
TEST(Exp, MatchesRegressionHash) {
    EXPECT_EQ(samples_hash(), 0x11899eb155c9f69bULL);
}

}  // namespace
}  // namespace tp::math
