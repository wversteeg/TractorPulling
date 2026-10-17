#include "tp/data/tire/tire_loader.hpp"

#include "tp/core/math/curve.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// Slip ratios are dimensionless: no unit conversion.
constexpr double kSlipToSi = 1.0;

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(tire::ParamError error) {
    using tire::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"tire", "all values must be finite"};
        case ParamError::kRadiusNotPositive:
            return {"tire.radius_m", "must be > 0"};
        case ParamError::kCurveDoesNotSpanSlip:
            return {"tire.traction.slip", "must run from 0 to 1"};
        case ParamError::kCurveNotZeroAtZeroSlip:
            return {"tire.traction.mu", "must be 0 at zero slip"};
        case ParamError::kCurveNegative:
            return {"tire.traction.mu", "must not be negative"};
        case ParamError::kNominalLoadNotPositive:
            return {"tire.nominal_load_n", "must be > 0"};
        case ParamError::kLoadSensitivityOutOfRange:
            return {"tire.load_sensitivity", "must be in [0, 1)"};
        case ParamError::kRollingResistanceOutOfRange:
            return {"tire.rolling_resistance_coefficient", "must be in [0, 1)"};
        case ParamError::kInertiaNotPositive:
            return {"tire.wheel_inertia_kg_m2", "must be > 0"};
        case ParamError::kMinReferenceSpeedNotPositive:
            return {"tire.min_reference_speed_m_s", "must be > 0"};
    }
    return {"tire", "invalid"};
}

Result<tire::Params> read_tire(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};

    const std::optional<math::Curve> traction =
        reader.curve("tire.traction", "slip", "mu", kSlipToSi);
    const double radius_m = reader.number("tire.radius_m");
    const double nominal_load_n = reader.number("tire.nominal_load_n");
    const double load_sensitivity = reader.number("tire.load_sensitivity");
    const double rolling_resistance = reader.number("tire.rolling_resistance_coefficient");
    const double wheel_inertia_kg_m2 = reader.number("tire.wheel_inertia_kg_m2");
    const double min_reference_m_s = reader.number("tire.min_reference_speed_m_s");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (!traction) {
        return detail::make_error(source, "tire.traction", "invalid");
    }

    const tire::Params params{
        .radius_m = radius_m,
        .traction_coefficient = *traction,
        .nominal_load_n = nominal_load_n,
        .load_sensitivity = load_sensitivity,
        .rolling_resistance_coefficient = rolling_resistance,
        .wheel_inertia_kg_m2 = wheel_inertia_kg_m2,
        .min_reference_speed_m_s = min_reference_m_s,
    };
    if (const auto error = tire::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<tire::Params> parse_tire(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<tire::Params>(toml_text, source_name, &read_tire);
}

Result<tire::Params> load_tire(const std::filesystem::path& path) {
    return detail::load_file_with<tire::Params>(path, &read_tire);
}

}  // namespace tp::data
