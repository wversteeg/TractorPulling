#include "tp/data/track/track_loader.hpp"

#include <cmath>
#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// Profile positions are in metres already.
constexpr double kMetresToSi = 1.0;

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

FieldError describe(soil::ParamError error) {
    using soil::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"track.soil", "all values must be finite"};
        case ParamError::kClayFractionOutOfRange:
            return {"track.soil.clay_fraction", "must be in [0, 1]"};
        case ParamError::kMoistureOutOfRange:
            return {"track.soil.moisture", "must be in [0, 0.5]"};
        case ParamError::kCompactionOutOfRange:
            return {"track.soil.compaction", "must be in [0, 1]"};
    }
    return {"track.soil", "invalid"};
}

FieldError describe(track::ParamError error) {
    using track::ParamError;
    switch (error) {
        case ParamError::kSoil:
            return {"track.soil", "invalid"};
        case ParamError::kMoistureProfile:
            return {"track.profile.moisture.value", "every value must be in [0, 0.5]"};
        case ParamError::kCompactionProfile:
            return {"track.profile.compaction.value", "every value must be in [0, 1]"};
    }
    return {"track", "invalid"};
}

// An optional profile table: x_m and value arrays, or nothing.
std::optional<math::Curve> profile(detail::Reader& reader, std::string_view table) {
    if (!reader.has(table)) {
        return std::nullopt;
    }
    return reader.curve(table, "x_m", "value", kMetresToSi);
}

bool valid_distance(const std::optional<double>& distance_m) {
    return !distance_m || (std::isfinite(*distance_m) && *distance_m >= 0.0);
}

Result<Track> read_track(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const Track track{
        .rules =
            {
                .track_length_m = reader.number("track.length_m"),
                .max_duration_s = reader.number("track.max_duration_s"),
            },
        .ground =
            {
                .soil =
                    {
                        .clay_fraction = reader.number("track.soil.clay_fraction"),
                        .moisture = reader.number("track.soil.moisture"),
                        .compaction = reader.number("track.soil.compaction"),
                    },
                .moisture_along_m = profile(reader, "track.profile.moisture"),
                .compaction_along_m = profile(reader, "track.profile.compaction"),
            },
        .sled =
            {
                .pushdown_start_m = reader.optional_number("track.pushdown.start_m"),
            },
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = run::validate(track.rules)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    if (const auto error = soil::validate(track.ground.soil)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    if (const auto error = track::validate(track.ground)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    if (!valid_distance(track.sled.pushdown_start_m)) {
        return detail::make_error(source, "track.pushdown.start_m", "must be >= 0");
    }
    return track;
}

}  // namespace

Result<Track> parse_track(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<Track>(toml_text, source_name, &read_track);
}

Result<Track> load_track(const std::filesystem::path& path) {
    return detail::load_file_with<Track>(path, &read_track);
}

}  // namespace tp::data
