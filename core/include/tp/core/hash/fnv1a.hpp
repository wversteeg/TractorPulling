#pragma once

#include <bit>
#include <cstdint>

namespace tp::hash {

/// 64-bit FNV-1a over the exact bit patterns of simulation state.
///
/// Used for replay and determinism tests: two runs agree only if every value
/// is bit-identical. Values are fed in little-endian byte order regardless of
/// the host, so hashes compare across platforms.
class Fnv1a {
public:
    static constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;
    static constexpr std::uint64_t kPrime = 0x100000001b3ULL;

    constexpr void add(std::uint64_t value) noexcept {
        constexpr int kBitsPerByte = 8;
        constexpr std::uint64_t kByteMask = 0xffU;
        for (int i = 0; i < 8; ++i) {
            hash_ ^= (value >> (i * kBitsPerByte)) & kByteMask;
            hash_ *= kPrime;
        }
    }
    constexpr void add(double value) noexcept { add(std::bit_cast<std::uint64_t>(value)); }
    constexpr void add(bool value) noexcept { add(static_cast<std::uint64_t>(value)); }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return hash_; }

private:
    std::uint64_t hash_ = kOffsetBasis;
};

}  // namespace tp::hash
