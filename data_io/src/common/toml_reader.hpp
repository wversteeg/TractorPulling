#pragma once

// Internal helpers shared by the loaders: typed field access with error
// collection, curve tables, and file/text parsing with uniform messages.

#include "tp/core/math/curve.hpp"
#include "tp/data/result.hpp"

#include <toml++/toml.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data::detail {

[[nodiscard]] LoadError make_error(std::string_view source, std::string_view field,
                                   std::string_view reason);

// Reads typed values by dotted path and remembers the first failure, so a
// loader can read every field and check for errors once.
class Reader {
public:
    Reader(const toml::table& root, std::string_view source) : root_(&root), source_(source) {}

    double number(std::string_view path);
    std::int64_t integer(std::string_view path);
    std::string text(std::string_view path);
    std::vector<double> numbers(std::string_view path);

    // A curve stored as two parallel arrays, `<table>.<x_key>` and
    // `<table>.<y_key>`. x values are multiplied by `x_to_si` (e.g. rpm to
    // rad/s) before validation.
    std::optional<math::Curve> curve(std::string_view table, std::string_view x_key,
                                     std::string_view y_key, double x_to_si);

    void fail(std::string_view field, std::string_view reason);
    [[nodiscard]] const std::optional<LoadError>& error() const noexcept { return error_; }
    [[nodiscard]] std::string_view source() const noexcept { return source_; }

private:
    const toml::table* root_;
    std::string_view source_;
    std::optional<LoadError> error_;
};

[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path);
[[nodiscard]] std::string describe(const toml::parse_error& error, std::string_view source);

template <typename T>
using TableLoader = Result<T> (*)(const toml::table& root, std::string_view source);

// Parses `toml_text` and hands the table to `load`; syntax errors become
// "source:line:column: description".
template <typename T>
Result<T> parse_with(std::string_view toml_text, std::string_view source, TableLoader<T> load) {
    try {
        const toml::table root = toml::parse(toml_text, source);
        return load(root, source);
    } catch (const toml::parse_error& e) {
        return LoadError{describe(e, source)};
    }
}

template <typename T>
Result<T> load_file_with(const std::filesystem::path& path, TableLoader<T> load) {
    const std::optional<std::string> text = read_text(path);
    if (!text) {
        return LoadError{path.string() + ": cannot open file"};
    }
    return parse_with<T>(*text, path.string(), load);
}

}  // namespace tp::data::detail
