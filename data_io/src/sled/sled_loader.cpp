#include "tp/data/sled/sled_loader.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(sled::ParamError error) {
    using sled::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"sled", "all values must be finite"};
        case ParamError::kFrameMassNotPositive:
            return {"sled.frame_mass_kg", "must be > 0"};
        case ParamError::kBoxMassNegative:
            return {"sled.weight_box.mass_kg", "must not be negative"};
        case ParamError::kPanNotAheadOfWheels:
            return {"sled.pan_ahead_of_wheels_m", "must be > 0"};
        case ParamError::kFrameCgOutsideSled:
            return {"sled.frame_cg_ahead_of_wheels_m", "must lie between the wheels and the pan"};
        case ParamError::kBoxOutsideSled:
            return {"sled.weight_box", "must stay between the wheels and the pan"};
        case ParamError::kBoxTravelNotPositive:
            return {"sled.weight_box.travel_distance_m", "must be > 0"};
        case ParamError::kHookInvalid:
            return {"sled.hook", "position and height must be > 0"};
        case ParamError::kPushdownStartNegative:
            return {"sled.pushdown.start_m", "must not be negative"};
        case ParamError::kPushdownRampNotPositive:
            return {"sled.pushdown.ramp_m", "must be > 0"};
        case ParamError::kPanFrictionOutOfRange:
            return {"sled.pan_friction_coefficient", "must be in (0, 2]"};
        case ParamError::kRollingResistanceOutOfRange:
            return {"sled.wheel_rolling_resistance_coefficient", "must be in [0, 1)"};
        case ParamError::kChainInvalid:
            return {"sled.chain", "length and stiffness must be > 0, damping >= 0"};
        case ParamError::kPullOffNegative:
            return {"sled.pull_off", "all values must be >= 0"};
    }
    return {"sled", "invalid"};
}

Result<sled::Params> read_sled(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const sled::Params params{
        .frame_mass_kg = reader.number("sled.frame_mass_kg"),
        .frame_cg_ahead_of_wheels_m = reader.number("sled.frame_cg_ahead_of_wheels_m"),
        .pan_ahead_of_wheels_m = reader.number("sled.pan_ahead_of_wheels_m"),
        .hook_ahead_of_wheels_m = reader.number("sled.hook.ahead_of_wheels_m"),
        .hook_height_m = reader.number("sled.hook.height_m"),
        .box_mass_kg = reader.number("sled.weight_box.mass_kg"),
        .box_start_ahead_of_wheels_m = reader.number("sled.weight_box.start_ahead_of_wheels_m"),
        .box_end_ahead_of_wheels_m = reader.number("sled.weight_box.end_ahead_of_wheels_m"),
        .box_travel_distance_m = reader.number("sled.weight_box.travel_distance_m"),
        .pushdown_start_m = reader.number("sled.pushdown.start_m"),
        .pushdown_ramp_m = reader.number("sled.pushdown.ramp_m"),
        .pan_friction_coefficient = reader.number("sled.pan_friction_coefficient"),
        .wheel_rolling_resistance_coefficient =
            reader.number("sled.wheel_rolling_resistance_coefficient"),
        .chain =
            {
                .length_m = reader.number("sled.chain.length_m"),
                .stiffness_n_per_m = reader.number("sled.chain.stiffness_n_per_m"),
                .damping_n_s_per_m = reader.number("sled.chain.damping_n_s_per_m"),
            },
        .pull_off =
            {
                .extra_box_mass_kg = reader.number("sled.pull_off.extra_box_mass_kg"),
                .pushdown_earlier_m = reader.number("sled.pull_off.pushdown_earlier_m"),
                .extra_pan_friction = reader.number("sled.pull_off.extra_pan_friction"),
            },
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = sled::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<sled::Params> parse_sled(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<sled::Params>(toml_text, source_name, &read_sled);
}

Result<sled::Params> load_sled(const std::filesystem::path& path) {
    return detail::load_file_with<sled::Params>(path, &read_sled);
}

}  // namespace tp::data
