#include "parse_number.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace tp::tools {
namespace {

// Decimals are read as std::from_chars reads them, whichever C++ library
// does the work: the shorthands strtod also takes are turned away.
TEST(ParseNumber, ReadsDecimalsAsFromCharsDoes) {
    EXPECT_EQ(parse_number<double>("8"), 8.0);
    EXPECT_EQ(parse_number<double>("0.5"), 0.5);
    EXPECT_EQ(parse_number<double>("-0.5"), -0.5);
    EXPECT_EQ(parse_number<double>(".5"), 0.5);
    EXPECT_EQ(parse_number<double>("1e3"), 1000.0);
    EXPECT_EQ(parse_number<double>("2.5E-1"), 0.25);
    for (const std::string_view bad :
         {"", " 8", "8 ", "\t8", "+8", "0x8", "0X1p3", "abc", "0.5x", "1,5", "1e400", "-1e400",
          "1e-400", "inf", "-inf", "nan", "infinity"}) {
        EXPECT_EQ(parse_number<double>(bad), std::nullopt) << "'" << bad << "'";
    }
}

TEST(ParseNumber, ReadsWholeNumbers) {
    EXPECT_EQ(parse_number<int>("6"), 6);
    EXPECT_EQ(parse_number<int>("-3"), -3);
    EXPECT_EQ(parse_number<std::uint64_t>("2026"), 2026U);
    for (const std::string_view bad : {"", " 6", "+6", "6.0", "6x", "0x6", "99999999999"}) {
        EXPECT_EQ(parse_number<int>(bad), std::nullopt) << "'" << bad << "'";
    }
    EXPECT_EQ(parse_number<std::uint64_t>("-1"), std::nullopt);
}

}  // namespace
}  // namespace tp::tools
