#include "tp/data/clutch/clutch_loader.hpp"

#include "tp/core/math/curve.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// Pedal positions are already fractions of travel: no unit conversion.
constexpr double kPedalToSi = 1.0;

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(clutch::ParamError error) {
    using clutch::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"clutch", "all values must be finite"};
        case ParamError::kCapacityNotPositive:
            return {"clutch.static_torque_capacity_nm", "must be > 0"};
        case ParamError::kKineticRatioOutOfRange:
            return {"clutch.kinetic_ratio", "must be in (0, 1]"};
        case ParamError::kEngagementDoesNotSpanPedalTravel:
            return {"clutch.engagement.pedal", "must run from 0 to 1"};
        case ParamError::kEngagementOutOfRange:
            return {"clutch.engagement.fraction", "must be between 0 and 1"};
        case ParamError::kEngagementIncreasesWithPedal:
            return {"clutch.engagement.fraction", "must not increase with pedal travel"};
        case ParamError::kEngagementNotFullWhenReleased:
            return {"clutch.engagement.fraction", "must be 1 with the pedal released"};
        case ParamError::kEngagementNotZeroWhenPressed:
            return {"clutch.engagement.fraction", "must be 0 with the pedal pressed"};
    }
    return {"clutch", "invalid"};
}

Result<clutch::Params> read_clutch(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};

    const std::optional<math::Curve> engagement =
        reader.curve("clutch.engagement", "pedal", "fraction", kPedalToSi);
    const double capacity_nm = reader.number("clutch.static_torque_capacity_nm");
    const double kinetic_ratio = reader.number("clutch.kinetic_ratio");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (!engagement) {
        return detail::make_error(source, "clutch.engagement", "invalid");
    }

    const clutch::Params params{
        .engagement = *engagement,
        .static_torque_capacity_nm = capacity_nm,
        .kinetic_ratio = kinetic_ratio,
    };
    if (const auto error = clutch::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<clutch::Params> parse_clutch(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<clutch::Params>(toml_text, source_name, &read_clutch);
}

Result<clutch::Params> load_clutch(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<clutch::Params>(path, read, &read_clutch);
}

Result<clutch::Params> load_clutch(const std::filesystem::path& path) {
    return load_clutch(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
