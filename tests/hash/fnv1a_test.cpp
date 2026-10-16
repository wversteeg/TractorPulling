#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace tp::hash {
namespace {

TEST(Fnv1a, StartsAtOffsetBasis) {
    EXPECT_EQ(Fnv1a{}.value(), Fnv1a::kOffsetBasis);
}

// Reference value computed independently: FNV-1a 64 over the bytes
// 08 07 06 05 04 03 02 01, i.e. the integer fed least significant byte first.
TEST(Fnv1a, MatchesReferenceAndIsLittleEndian) {
    Fnv1a hash;
    hash.add(std::uint64_t{0x0102030405060708ULL});
    EXPECT_EQ(hash.value(), 0x0c6d4496e17859d5ULL);
}

TEST(Fnv1a, HashesExactBitPatterns) {
    Fnv1a positive_zero;
    Fnv1a negative_zero;
    positive_zero.add(0.0);
    negative_zero.add(-0.0);
    EXPECT_NE(positive_zero.value(), negative_zero.value());
}

TEST(Fnv1a, DependsOnOrder) {
    Fnv1a ab;
    Fnv1a ba;
    ab.add(1.0);
    ab.add(2.0);
    ba.add(2.0);
    ba.add(1.0);
    EXPECT_NE(ab.value(), ba.value());
}

}  // namespace
}  // namespace tp::hash
