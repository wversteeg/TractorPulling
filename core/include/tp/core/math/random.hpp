#pragma once

#include <cstdint>

namespace tp::math {

/// SplitMix64: a small, fast generator with a 64-bit state. Integer
/// arithmetic only, so the same seed gives the same numbers on every
/// platform, unlike the standard library's distributions.
class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}

    constexpr std::uint64_t next() noexcept {
        state_ += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }

    /// Uniform in [0, 1), from the top 53 bits: exact in a double.
    constexpr double uniform() noexcept {
        constexpr double kTwoToMinus53 = 1.0 / 9007199254740992.0;
        return static_cast<double>(next() >> 11U) * kTwoToMinus53;
    }

    /// Uniform in [low, high).
    constexpr double uniform(double low, double high) noexcept {
        return low + (high - low) * uniform();
    }

    /// Uniform integer in [0, bound), bound > 0. The modulo bias is below
    /// 2^-58 for the small bounds used here.
    constexpr std::uint64_t below(std::uint64_t bound) noexcept { return next() % bound; }

private:
    std::uint64_t state_;
};

}  // namespace tp::math
