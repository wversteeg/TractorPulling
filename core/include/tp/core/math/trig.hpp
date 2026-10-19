#pragma once

#include <array>
#include <cstddef>
#include <numbers>

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

/// Sine and cosine for any |angle_rad| ≤ π (a heading), to within a few
/// 1e-16: the angle is brought within ±π/4 by the nearest multiple k of π/2,
/// then sin_cos() applies and the quadrant swaps the results. π/2 is split
/// in two parts (Cody and Waite), the first with trailing zero bits so that
/// k times it is exact. Within ±π/4 this is sin_cos() bit for bit.
[[nodiscard]] constexpr SinCos sin_cos_any(double angle_rad) noexcept {
    // π/2 to 33 bits, and the rest.
    constexpr double kHalfPiHigh = 1.57079632673412561417e+00;
    constexpr double kHalfPiLow = 6.07710050650619224932e-11;
    constexpr double kThreeQuarterPi = 3.0 * std::numbers::pi / 4.0;
    int quadrant = 0;
    if (angle_rad > kThreeQuarterPi) {
        quadrant = 2;
    } else if (angle_rad > kSinCosMaxAngle_rad) {
        quadrant = 1;
    } else if (angle_rad < -kThreeQuarterPi) {
        quadrant = -2;
    } else if (angle_rad < -kSinCosMaxAngle_rad) {
        quadrant = -1;
    }
    const double k = quadrant;
    const SinCos r = sin_cos((angle_rad - k * kHalfPiHigh) - k * kHalfPiLow);
    switch (quadrant) {
        case 1:
            return {.sin = r.cos, .cos = -r.sin};
        case -1:
            return {.sin = -r.cos, .cos = r.sin};
        case 2:
        case -2:
            return {.sin = -r.sin, .cos = -r.cos};
        default:
            return r;
    }
}

}  // namespace tp::math
