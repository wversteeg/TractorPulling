#pragma once

#include <array>
#include <cstddef>

namespace tp::math {

struct SinCos {
    double sin;
    double cos;
};

/// Largest |angle| sin_cos() is accurate for [rad]: π/4.
inline constexpr double kSinCosMaxAngle_rad = 0.785398163397448309616;

namespace detail {

// Taylor coefficients (-1)^n / k! for the odd (sine) and even (cosine)
// powers. Over |x| ≤ π/4 the first omitted terms, x¹⁹/19! and x¹⁸/18!, are
// below 1e-17.
inline constexpr std::array<double, 9> kSinCoefficients = {
    1.0,
    -1.0 / 6.0,
    1.0 / 120.0,
    -1.0 / 5040.0,
    1.0 / 362880.0,
    -1.0 / 39916800.0,
    1.0 / 6227020800.0,
    -1.0 / 1307674368000.0,
    1.0 / 355687428096000.0,
};
inline constexpr std::array<double, 9> kCosCoefficients = {
    1.0,
    -1.0 / 2.0,
    1.0 / 24.0,
    -1.0 / 720.0,
    1.0 / 40320.0,
    -1.0 / 3628800.0,
    1.0 / 479001600.0,
    -1.0 / 87178291200.0,
    1.0 / 20922789888000.0,
};

constexpr double horner(const std::array<double, 9>& coefficients, double x2) noexcept {
    double sum = coefficients.back();
    for (std::size_t i = coefficients.size() - 1; i-- > 0;) {
        sum = coefficients[i] + x2 * sum;
    }
    return sum;
}

}  // namespace detail

/// Sine and cosine for |angle_rad| ≤ kSinCosMaxAngle_rad, to within about
/// 1e-16 of the true values.
///
/// std::sin and std::cos are not required to round identically on every
/// platform's math library, which would break the bit-identical replay hash.
/// A fixed polynomial evaluated with + and × only (and no FMA contraction,
/// see TP_DETERMINISM_FLAGS) gives the same bits everywhere.
[[nodiscard]] constexpr SinCos sin_cos(double angle_rad) noexcept {
    const double x2 = angle_rad * angle_rad;
    return {.sin = angle_rad * detail::horner(detail::kSinCoefficients, x2),
            .cos = detail::horner(detail::kCosCoefficients, x2)};
}

}  // namespace tp::math
