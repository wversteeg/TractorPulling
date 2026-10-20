#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace tp::tools {

// A number from the command line: all of `text` and nothing else, written
// as std::from_chars reads it. No leading spaces or '+', no hex, nothing
// after it, nothing out of range.
//
// Whole numbers go through std::from_chars. Decimals go through std::strtod,
// because libc++ (the Android NDK's C++ library) has long lacked
// std::from_chars for floating point; strtod also takes spaces, '+', hex and
// underflow, so those are turned away first. The tool never sets a locale,
// so strtod reads the decimal point as a dot.
template <typename T>
std::optional<T> parse_number(std::string_view text) {
    if constexpr (std::is_floating_point_v<T>) {
        constexpr std::string_view kNotFromChars = " \t\n\v\f\rxX";
        if (text.empty() || text.front() == '+' ||
            text.find_first_of(kNotFromChars) != std::string_view::npos) {
            return std::nullopt;
        }
        const std::string copy{text};
        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(copy.c_str(), &end);
        if (*end != '\0' || errno == ERANGE || !std::isfinite(value)) {
            return std::nullopt;
        }
        return static_cast<T>(value);
    } else {
        T value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()) {
            return std::nullopt;
        }
        return value;
    }
}

}  // namespace tp::tools
