#include "tp/data/run/track_loader.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(run::ParamError error) {
    using run::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"track", "all values must be finite"};
        case ParamError::kTrackLengthNotPositive:
            return {"track.length_m", "must be > 0"};
        case ParamError::kMaxDurationNotPositive:
            return {"track.max_duration_s", "must be > 0"};
    }
    return {"track", "invalid"};
}

Result<run::Rules> read_track(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const run::Rules rules{
        .track_length_m = reader.number("track.length_m"),
        .max_duration_s = reader.number("track.max_duration_s"),
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = run::validate(rules)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return rules;
}

}  // namespace

Result<run::Rules> parse_track(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<run::Rules>(toml_text, source_name, &read_track);
}

Result<run::Rules> load_track(const std::filesystem::path& path) {
    return detail::load_file_with<run::Rules>(path, &read_track);
}

}  // namespace tp::data
