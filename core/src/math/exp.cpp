#include "tp/core/math/exp.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace tp::math {

namespace {

constexpr double kInvLn2 = std::numbers::log2e;
// ln 2 split so that k · kLn2Hi is exact for |k| < 2^11: the high part is
// deliberately ln 2 with its low bits cut off, not std::numbers::ln2.
constexpr double kLn2Hi = 0x1.62e42fee00000p-1;  // NOLINT(modernize-use-std-numbers)
constexpr double kLn2Lo = 0x1.a39ef35793c76p-33;

// Taylor coefficients 1/n! for e^r, |r| ≤ ln 2 / 2: the first omitted term,
// r^14 / 14!, is below 1e-17.
constexpr std::array<double, 14> kCoefficients = {
    1.0,
    1.0,
    1.0 / 2.0,
    1.0 / 6.0,
    1.0 / 24.0,
    1.0 / 120.0,
    1.0 / 720.0,
    1.0 / 5040.0,
    1.0 / 40320.0,
    1.0 / 362880.0,
    1.0 / 3628800.0,
    1.0 / 39916800.0,
    1.0 / 479001600.0,
    1.0 / 6227020800.0,
};

constexpr int kExponentBias = 1023;
constexpr int kMantissaBits = 52;

double power_of_two(int k) noexcept {
    return std::bit_cast<double>(static_cast<std::uint64_t>(k + kExponentBias) << kMantissaBits);
}

}  // namespace

double exp(double x) noexcept {
    if (std::isnan(x)) {
        return x;
    }
    if (x < kExpMinArgument) {
        return 0.0;
    }
    if (x > kExpMaxArgument) {
        return std::numeric_limits<double>::infinity();
    }
    // floor is exact, so k is the same everywhere.
    const double k = std::floor(x * kInvLn2 + 0.5);
    const double r = (x - k * kLn2Hi) - k * kLn2Lo;
    double sum = kCoefficients.back();
    for (std::size_t i = kCoefficients.size() - 1; i-- > 0;) {
        sum = kCoefficients[i] + r * sum;
    }
    return sum * power_of_two(static_cast<int>(k));
}

}  // namespace tp::math
