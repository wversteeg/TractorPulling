#include "tp/data/driveline/driveline_loader.hpp"

#include "tp/core/units/conversions.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// Rpm per "1000 rpm", for drag given per 1000 rpm in the data files.
constexpr double kRpmPerKiloRpm = 1000.0;

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(driveline::ParamError error) {
    using driveline::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"driveline", "all values must be finite"};
        case ParamError::kNoGears:
            return {"driveline.gearbox.ratios", "needs at least one gear"};
        case ParamError::kTooManyGears:
            return {"driveline.gearbox.ratios", "has more than 16 gears"};
        case ParamError::kRatioNotPositive:
            return {"driveline.gearbox.ratios", "must all be > 0"};
        case ParamError::kRatiosNotDecreasing:
            return {"driveline.gearbox.ratios", "must decrease from 1st to top gear"};
        case ParamError::kFinalDriveNotPositive:
            return {"driveline.final_drive_ratio", "must be > 0"};
        case ParamError::kEfficiencyOutOfRange:
            return {"driveline.efficiency", "must be in (0, 1]"};
        case ParamError::kInertiaNotPositive:
            return {"driveline", "input_inertia_kg_m2 and output_inertia_kg_m2 must be > 0"};
        case ParamError::kDragNegative:
            return {"driveline.drag", "must not be negative"};
        case ParamError::kLoadDownshiftOutOfRange:
            return {"driveline.powershift.load_downshift_gears",
                    "must be 0 or more and below the number of gears"};
    }
    return {"driveline", "invalid"};
}

Result<driveline::Params> read_driveline(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};

    const std::vector<double> ratios = reader.numbers("driveline.gearbox.ratios");
    driveline::Params params{
        .gear_ratios = {},
        .gear_count = ratios.size(),
        .final_drive_ratio = reader.number("driveline.final_drive_ratio"),
        .efficiency = reader.number("driveline.efficiency"),
        .input_inertia_kg_m2 = reader.number("driveline.input_inertia_kg_m2"),
        .output_inertia_kg_m2 = reader.number("driveline.output_inertia_kg_m2"),
        .drag_torque_nm = reader.number("driveline.drag.constant_nm"),
        .viscous_drag_nm_s_rad = reader.number("driveline.drag.viscous_nm_per_1000_rpm") /
                                 units::rpm_to_rad_s(kRpmPerKiloRpm),
    };
    // Optional: a gearbox without it shifts with the clutch pressed only.
    const std::optional<double> downshift =
        reader.optional_number("driveline.powershift.load_downshift_gears");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (downshift) {
        if (*downshift != std::floor(*downshift) || *downshift < 0.0 ||
            *downshift > static_cast<double>(driveline::kMaxGears)) {
            return detail::make_error(source, "driveline.powershift.load_downshift_gears",
                                      "must be a whole number of gears, 0 or more");
        }
        params.load_downshift_gears = static_cast<int>(*downshift);
    }
    std::copy_n(ratios.begin(), std::min(ratios.size(), driveline::kMaxGears),
                params.gear_ratios.begin());

    if (const auto error = driveline::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<driveline::Params> parse_driveline(std::string_view toml_text,
                                          std::string_view source_name) {
    return detail::parse_with<driveline::Params>(toml_text, source_name, &read_driveline);
}

Result<driveline::Params> load_driveline(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<driveline::Params>(path, read, &read_driveline);
}

Result<driveline::Params> load_driveline(const std::filesystem::path& path) {
    return load_driveline(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
