#include "tire_fields.hpp"

namespace tp::data::detail {

tire::Size read_tire_size(Reader& reader, std::string_view table) {
    return {
        .radius_m = reader.number(field_path(table, "radius_m")),
        .section_width_m = reader.number(field_path(table, "section_width_m")),
        .section_height_m = reader.number(field_path(table, "section_height_m")),
        .deflection_m = reader.number(field_path(table, "deflection_m")),
    };
}

TireFieldError describe(tire::ParamError error) {
    using tire::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"", "all values must be finite"};
        case ParamError::kRadiusNotPositive:
            return {"radius_m", "must be > 0"};
        case ParamError::kSectionNotPositive:
            return {"section_width_m", "section width and height must be > 0"};
        case ParamError::kDeflectionOutOfRange:
            return {"deflection_m", "must be at least 0 and less than the section height"};
        case ParamError::kTractionLimitOutOfRange:
            return {"traction.limit", "must be in (0, 2]"};
        case ParamError::kTractionRateNotPositive:
            return {"traction", "mobility_rate and slip_rate must be > 0"};
        case ParamError::kInertiaNotPositive:
            return {"wheel_inertia_kg_m2", "must be > 0"};
        case ParamError::kMinReferenceSpeedNotPositive:
            return {"min_reference_speed_m_s", "must be > 0"};
    }
    return {"", "invalid"};
}

std::string field_path(std::string_view table, std::string_view field) {
    std::string path{table};
    if (!field.empty()) {
        path += '.';
        path += field;
    }
    return path;
}

}  // namespace tp::data::detail
