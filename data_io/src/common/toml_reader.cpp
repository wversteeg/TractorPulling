#include "toml_reader.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>

namespace tp::data::detail {

namespace {

std::string describe(math::CurveError error, std::string_view x_key) {
    using math::CurveError;
    switch (error) {
        case CurveError::kTooFewPoints:
            return "needs at least 2 points";
        case CurveError::kTooManyPoints:
            return "has more than 16 points";
        case CurveError::kNotFinite:
            return "values must be finite";
        case CurveError::kNotStrictlyIncreasing:
            return std::string{x_key} + " must be strictly increasing";
    }
    return "invalid";
}

std::string join(std::string_view table, std::string_view key) {
    std::string path{table};
    path.append(".").append(key);
    return path;
}

}  // namespace

std::optional<std::string> read_text(const std::filesystem::path& path) {
    const std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string describe(const toml::parse_error& error, std::string_view source) {
    std::ostringstream message;
    message << source << ':' << error.source().begin.line << ':' << error.source().begin.column
            << ": " << error.description();
    return message.str();
}

LoadError make_error(std::string_view source, std::string_view field, std::string_view reason) {
    std::string message{source};
    message.append(": ").append(field).append(": ").append(reason);
    return LoadError{std::move(message)};
}

double Reader::number(std::string_view path) {
    const std::optional<double> value = root_->at_path(path).value<double>();
    if (!value) {
        fail(path, "missing or not a number");
        return 0.0;
    }
    return *value;
}

std::int64_t Reader::integer(std::string_view path) {
    const std::optional<std::int64_t> value = root_->at_path(path).value_exact<std::int64_t>();
    if (!value) {
        fail(path, "missing or not an integer");
        return 0;
    }
    return *value;
}

std::string Reader::text(std::string_view path) {
    const std::optional<std::string> value = root_->at_path(path).value_exact<std::string>();
    if (!value) {
        fail(path, "missing or not a string");
        return {};
    }
    return *value;
}

std::vector<double> Reader::numbers(std::string_view path) {
    const toml::array* array = root_->at_path(path).as_array();
    if (array == nullptr) {
        fail(path, "missing or not an array");
        return {};
    }
    std::vector<double> values;
    values.reserve(array->size());
    for (const toml::node& node : *array) {
        const std::optional<double> value = node.value<double>();
        if (!value) {
            fail(path, "must contain only numbers");
            return {};
        }
        values.push_back(*value);
    }
    return values;
}

std::optional<math::Curve> Reader::curve(std::string_view table, std::string_view x_key,
                                         std::string_view y_key, double x_to_si) {
    const std::vector<double> xs = numbers(join(table, x_key));
    const std::vector<double> ys = numbers(join(table, y_key));
    if (error_) {
        return std::nullopt;
    }
    if (xs.size() != ys.size()) {
        fail(table,
             std::string{x_key} + " and " + std::string{y_key} + " must have the same length");
        return std::nullopt;
    }
    std::vector<math::CurvePoint> points;
    points.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i) {
        points.push_back({xs[i] * x_to_si, ys[i]});
    }
    if (const auto error = math::Curve::validate(points)) {
        fail(table, describe(*error, x_key));
        return std::nullopt;
    }
    return math::Curve::create(points);
}

void Reader::fail(std::string_view field, std::string_view reason) {
    if (!error_) {
        error_ = make_error(source_, field, reason);
    }
}

}  // namespace tp::data::detail
