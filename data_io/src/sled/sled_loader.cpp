#include "tp/data/sled/sled_loader.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"
#include "../tire/tire_fields.hpp"

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
        case ParamError::kPanAreaNotPositive:
            return {"sled.pan_area_m2", "must be > 0"};
        case ParamError::kPanPloughNegative:
            return {"sled.pan_plough_coefficient", "must not be negative"};
        case ParamError::kKnivesOutOfRange:
            return {"sled.knives",
                    "count 0 to 24, depth 0 to 0.15 m, width >= 0 (> 0 with knives)"};
        case ParamError::kWheel:
            return {"sled.wheels", "invalid tire size"};
        case ParamError::kWheelCountOutOfRange:
            return {"sled.wheels.count", "must be 1 to 8"};
        case ParamError::kChainInvalid:
            return {"sled.chain", "length and stiffness must be > 0, damping >= 0"};
        case ParamError::kPullOffStepNotHeavier:
            return {"sled.pull_off_step",
                    "must make the sled heavier: values >= 0, at least one > 0"};
    }
    return {"sled", "invalid"};
}

constexpr std::string_view kWheels = "sled.wheels";

Result<sled::Params> read_sled(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const std::int64_t wheel_count = reader.integer("sled.wheels.count");
    const std::int64_t knife_count = reader.integer("sled.knives.count");
    const std::int64_t step_knives = reader.integer("sled.pull_off_step.knives");
    sled::Params params{
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
        .pan_area_m2 = reader.number("sled.pan_area_m2"),
        .pan_plough_coefficient = reader.number("sled.pan_plough_coefficient"),
        .knives = {.count = 0,  // checked below
                   .depth_m = reader.number("sled.knives.depth_m"),
                   .width_m = reader.number("sled.knives.width_m")},
        .wheel = detail::read_tire_size(reader, kWheels),
        .wheel_count = 0,  // checked below
        .chain =
            {
                .length_m = reader.number("sled.chain.length_m"),
                .stiffness_n_per_m = reader.number("sled.chain.stiffness_n_per_m"),
                .damping_n_s_per_m = reader.number("sled.chain.damping_n_s_per_m"),
            },
        .pull_off_step =
            {
                .box_mass_kg = reader.number("sled.pull_off_step.box_mass_kg"),
                .knives = 0,  // checked below
                .knife_depth_m = reader.number("sled.pull_off_step.knife_depth_m"),
                .pushdown_earlier_m = reader.number("sled.pull_off_step.pushdown_earlier_m"),
            },
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (wheel_count < 1 || wheel_count > sled::kMaxWheels) {
        return detail::make_error(source, "sled.wheels.count", "must be 1 to 8");
    }
    params.wheel_count = static_cast<int>(wheel_count);
    if (knife_count < 0 || knife_count > sled::kMaxKnives) {
        return detail::make_error(source, "sled.knives.count", "must be 0 to 24");
    }
    params.knives.count = static_cast<int>(knife_count);
    if (step_knives < 0 || step_knives > sled::kMaxKnives) {
        return detail::make_error(source, "sled.pull_off_step.knives", "must be 0 to 24");
    }
    params.pull_off_step.knives = static_cast<int>(step_knives);
    if (const auto tire_error = tire::validate(params.wheel)) {
        const detail::TireFieldError field_error = detail::describe(*tire_error);
        return detail::make_error(source, detail::field_path(kWheels, field_error.field),
                                  field_error.reason);
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
