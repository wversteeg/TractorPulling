#include "tp/data/engine/engine_loader.hpp"

#include "tp/core/math/curve.hpp"
#include "tp/core/units/conversions.hpp"

#include <toml++/toml.hpp>

#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace tp::data {

namespace {

using units::rpm_to_rad_s;

// Rpm per "1000 rpm", for friction given per 1000 rpm in the data files.
constexpr double kRpmPerKiloRpm = 1000.0;

// Reads typed values by dotted path and remembers the first failure, so the
// loader can read every field and check for errors once.
class Reader {
public:
    Reader(const toml::table& root, std::string_view source) : root_(&root), source_(source) {}

    double number(std::string_view path) {
        const std::optional<double> value = root_->at_path(path).value<double>();
        if (!value) {
            fail(path, "missing or not a number");
            return 0.0;
        }
        return *value;
    }

    std::vector<double> numbers(std::string_view path) {
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

    void fail(std::string_view field, std::string_view reason) {
        if (!error_) {
            error_ = make_error(source_, field, reason);
        }
    }

    [[nodiscard]] const std::optional<LoadError>& error() const noexcept { return error_; }

    static LoadError make_error(std::string_view source, std::string_view field,
                                std::string_view reason) {
        std::string message{source};
        message.append(": ").append(field).append(": ").append(reason);
        return LoadError{std::move(message)};
    }

private:
    const toml::table* root_;
    std::string_view source_;
    std::optional<LoadError> error_;
};

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(engine::ParamError error) {
    using engine::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"engine", "all values must be finite"};
        case ParamError::kInertiaNotPositive:
            return {"engine.inertia_kg_m2", "must be > 0"};
        case ParamError::kStallNotPositive:
            return {"engine.stall_rpm", "must be > 0"};
        case ParamError::kStallNotBelowIdle:
            return {"engine.stall_rpm", "must be below idle_rpm"};
        case ParamError::kIdleNotBelowRated:
            return {"engine.idle_rpm", "must be below rated_rpm"};
        case ParamError::kRatedNotBelowHighIdle:
            return {"engine.rated_rpm", "must be below high_idle_rpm"};
        case ParamError::kFrictionNegative:
            return {"engine.friction", "must not be negative"};
        case ParamError::kNaTorqueFractionOutOfRange:
            return {"engine.turbo.na_torque_fraction", "must be in (0, 1]"};
        case ParamError::kTurboTimeConstantNotPositive:
            return {"engine.turbo.time_constant_s", "must be > 0"};
        case ParamError::kBoostRangeInvalid:
            return {"engine.turbo", "need 0 <= boost_onset_rpm < full_boost_rpm"};
        case ParamError::kTorqueCurveNegative:
            return {"engine.torque_curve.torque_nm", "must not be negative"};
        case ParamError::kTorqueCurveDoesNotCoverRange:
            return {"engine.torque_curve.rpm", "must span idle_rpm to high_idle_rpm"};
    }
    return {"engine", "invalid"};
}

std::string_view describe(math::CurveError error) {
    using math::CurveError;
    switch (error) {
        case CurveError::kTooFewPoints:
            return "needs at least 2 points";
        case CurveError::kTooManyPoints:
            return "has more than 16 points";
        case CurveError::kNotFinite:
            return "values must be finite";
        case CurveError::kNotStrictlyIncreasing:
            return "rpm must be strictly increasing";
    }
    return "invalid";
}

Result<engine::Params> read_engine(const toml::table& root, std::string_view source) {
    Reader reader{root, source};

    const std::vector<double> rpm = reader.numbers("engine.torque_curve.rpm");
    const std::vector<double> torque_nm = reader.numbers("engine.torque_curve.torque_nm");
    const double inertia_kg_m2 = reader.number("engine.inertia_kg_m2");
    const double idle_rpm = reader.number("engine.idle_rpm");
    const double rated_rpm = reader.number("engine.rated_rpm");
    const double high_idle_rpm = reader.number("engine.high_idle_rpm");
    const double stall_rpm = reader.number("engine.stall_rpm");
    const double friction_nm = reader.number("engine.friction.constant_nm");
    const double viscous_nm_per_krpm = reader.number("engine.friction.viscous_nm_per_1000_rpm");
    const double na_torque_fraction = reader.number("engine.turbo.na_torque_fraction");
    const double turbo_time_constant_s = reader.number("engine.turbo.time_constant_s");
    const double boost_onset_rpm = reader.number("engine.turbo.boost_onset_rpm");
    const double full_boost_rpm = reader.number("engine.turbo.full_boost_rpm");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }

    if (rpm.size() != torque_nm.size()) {
        return Reader::make_error(source, "engine.torque_curve",
                                  "rpm and torque_nm must have the same length");
    }
    std::vector<math::CurvePoint> points;
    points.reserve(rpm.size());
    for (std::size_t i = 0; i < rpm.size(); ++i) {
        points.push_back({rpm_to_rad_s(rpm[i]), torque_nm[i]});
    }
    const std::optional<math::Curve> curve = math::Curve::create(points);
    if (!curve) {
        const auto error = math::Curve::validate(points);
        return Reader::make_error(source, "engine.torque_curve",
                                  error ? describe(*error) : "invalid");
    }

    const engine::Params params{
        .full_load_torque_nm = *curve,
        .inertia_kg_m2 = inertia_kg_m2,
        .idle_omega_rad_s = rpm_to_rad_s(idle_rpm),
        .rated_omega_rad_s = rpm_to_rad_s(rated_rpm),
        .high_idle_omega_rad_s = rpm_to_rad_s(high_idle_rpm),
        .stall_omega_rad_s = rpm_to_rad_s(stall_rpm),
        .friction_torque_nm = friction_nm,
        .viscous_friction_nm_s_rad = viscous_nm_per_krpm / rpm_to_rad_s(kRpmPerKiloRpm),
        .na_torque_fraction = na_torque_fraction,
        .turbo_time_constant_s = turbo_time_constant_s,
        .boost_onset_omega_rad_s = rpm_to_rad_s(boost_onset_rpm),
        .full_boost_omega_rad_s = rpm_to_rad_s(full_boost_rpm),
    };
    if (const auto error = engine::validate(params)) {
        const FieldError field_error = describe(*error);
        return Reader::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<engine::Params> parse_engine(std::string_view toml_text, std::string_view source_name) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_engine(root, source_name);
    } catch (const toml::parse_error& e) {
        std::ostringstream message;
        message << source_name << ':' << e.source().begin.line << ':' << e.source().begin.column
                << ": " << e.description();
        return LoadError{message.str()};
    }
}

Result<engine::Params> load_engine(const std::filesystem::path& path) {
    const std::ifstream file{path, std::ios::binary};
    if (!file) {
        return LoadError{path.string() + ": cannot open file"};
    }
    std::ostringstream text;
    text << file.rdbuf();
    return parse_engine(text.str(), path.string());
}

}  // namespace tp::data
