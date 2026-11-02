#pragma once

namespace tp::math {

/// Arguments below this give exactly 0 (the result would be subnormal) [-].
inline constexpr double kExpMinArgument = -708.0;
/// Arguments above this give +infinity.
inline constexpr double kExpMaxArgument = 709.0;

/// e^x to within about 2 ulp of the true value.
///
/// std::exp is not required to round identically on every platform's math
/// library, which would break the bit-identical replay hash. This splits x
/// into k ln 2 + r exactly (Cody-Waite), evaluates e^r with a fixed Taylor
/// polynomial using + and × only (no FMA contraction, see
/// TP_DETERMINISM_FLAGS) and scales by 2^k through the exponent bits: the
/// same bits everywhere.
[[nodiscard]] double exp(double x) noexcept;

}  // namespace tp::math
