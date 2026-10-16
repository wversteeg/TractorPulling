#pragma once

#include <string>
#include <utility>
#include <variant>

namespace tp::data {

struct LoadError {
    /// Human-readable, names the source and the offending field.
    std::string message;
};

/// Either a loaded value or the reason loading failed. std::expected would
/// do, but it is C++23.
template <typename T>
class Result {
public:
    // Implicit on purpose, so loaders can `return value;` or `return error;`.
    Result(T value) : value_(std::move(value)) {}          // NOLINT(google-explicit-constructor)
    Result(LoadError error) : value_(std::move(error)) {}  // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(value_); }
    explicit operator bool() const noexcept { return ok(); }

    /// Precondition: ok().
    [[nodiscard]] const T& value() const { return std::get<T>(value_); }
    /// Precondition: !ok().
    [[nodiscard]] const LoadError& error() const { return std::get<LoadError>(value_); }

private:
    std::variant<T, LoadError> value_;
};

}  // namespace tp::data
