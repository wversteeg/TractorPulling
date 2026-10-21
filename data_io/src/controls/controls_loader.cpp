#include "tp/data/controls/controls_loader.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(controls::ParamError error) {
    using controls::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"easy", "all values must be finite"};
        case ParamError::kSteerShareOutOfRange:
            return {"easy.steer_share", "must be in (0, 1]"};
        case ParamError::kSteerDeadZoneOutOfRange:
            return {"easy.steer_dead_zone", "must be in [0, steer_share)"};
        case ParamError::kSteerRateNotPositive:
            return {"easy.steer_rate_per_s", "must be > 0"};
        case ParamError::kAssistTimeNotPositive:
            return {"easy.clutch_assist", "engage_time_s and release_time_s must be > 0"};
        case ParamError::kGuardOutOfRange:
            return {"easy.clutch_assist",
                    "guard_speed_share must be in (0, 1] and guard_band_share in (0, "
                    "guard_speed_share)"};
        case ParamError::kMinThrottleOutOfRange:
            return {"easy.clutch_assist.min_throttle", "must be in [0, 1)"};
    }
    return {"easy", "invalid"};
}

Result<controls::EasyParams> read_easy(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const controls::EasyParams params{
        .steer_share = reader.number("easy.steer_share"),
        .steer_dead_zone = reader.number("easy.steer_dead_zone"),
        .steer_rate_per_s = reader.number("easy.steer_rate_per_s"),
        .clutch_assist = {
            .engage_time_s = reader.number("easy.clutch_assist.engage_time_s"),
            .release_time_s = reader.number("easy.clutch_assist.release_time_s"),
            .guard_speed_share = reader.number("easy.clutch_assist.guard_speed_share"),
            .guard_band_share = reader.number("easy.clutch_assist.guard_band_share"),
            .min_throttle = reader.number("easy.clutch_assist.min_throttle"),
        }};
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = controls::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<controls::EasyParams> parse_easy_controls(std::string_view toml_text,
                                                 std::string_view source_name) {
    return detail::parse_with<controls::EasyParams>(toml_text, source_name, &read_easy);
}

Result<controls::EasyParams> load_easy_controls(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<controls::EasyParams>(path, read, &read_easy);
}

Result<controls::EasyParams> load_easy_controls(const std::filesystem::path& path) {
    return load_easy_controls(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
