#include "tp/data/vehicle/tractor_loader.hpp"

#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/tire/tire_loader.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(chassis::ParamError error) {
    using chassis::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"tractor.chassis", "all values must be finite"};
        case ParamError::kMassNotPositive:
            return {"tractor.chassis.mass_kg", "must be > 0"};
        case ParamError::kWheelbaseNotPositive:
            return {"tractor.chassis.wheelbase_m", "must be > 0"};
        case ParamError::kCgOutsideWheelbase:
            return {"tractor.chassis.cg_ahead_of_rear_axle_m", "must lie between the axles"};
        case ParamError::kCgHeightNotPositive:
            return {"tractor.chassis.cg_height_m", "must be > 0"};
        case ParamError::kHitchHeightNegative:
            return {"tractor.chassis.hitch_height_m", "must not be negative"};
        case ParamError::kRollingResistanceOutOfRange:
            return {"tractor.chassis.front_rolling_resistance_coefficient", "must be in [0, 1)"};
    }
    return {"tractor.chassis", "invalid"};
}

// Loads a component the tractor names, or remembers why it could not.
template <typename T>
std::optional<T> component(const Result<T>& result, std::optional<LoadError>& error) {
    if (!result) {
        if (!error) {
            error = result.error();
        }
        return std::nullopt;
    }
    return result.value();
}

Result<vehicle::Params> read_tractor(const toml::table& root, std::string_view source,
                                     const std::filesystem::path& base) {
    detail::Reader reader{root, source};
    const std::string engine_path = reader.text("tractor.engine");
    const std::string clutch_path = reader.text("tractor.clutch");
    const std::string driveline_path = reader.text("tractor.driveline");
    const std::string tire_path = reader.text("tractor.rear_tire");
    const std::int64_t rear_tire_count = reader.integer("tractor.rear_tire_count");
    const chassis::Params chassis{
        .mass_kg = reader.number("tractor.chassis.mass_kg"),
        .wheelbase_m = reader.number("tractor.chassis.wheelbase_m"),
        .cg_ahead_of_rear_axle_m = reader.number("tractor.chassis.cg_ahead_of_rear_axle_m"),
        .cg_height_m = reader.number("tractor.chassis.cg_height_m"),
        .hitch_height_m = reader.number("tractor.chassis.hitch_height_m"),
        .front_rolling_resistance_coefficient =
            reader.number("tractor.chassis.front_rolling_resistance_coefficient"),
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = chassis::validate(chassis)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    if (rear_tire_count < 1 || rear_tire_count > vehicle::kMaxRearTires) {
        return detail::make_error(source, "tractor.rear_tire_count", "must be 1 to 8");
    }

    std::optional<LoadError> error;
    const auto engine = component(load_engine(base / engine_path), error);
    const auto clutch = component(load_clutch(base / clutch_path), error);
    const auto driveline = component(load_driveline(base / driveline_path), error);
    const auto rear_tire = component(load_tire(base / tire_path), error);
    if (error) {
        return *error;
    }
    if (!engine || !clutch || !driveline || !rear_tire) {
        return detail::make_error(source, "tractor", "invalid component");
    }
    return vehicle::Params{
        .engine = *engine,
        .clutch = *clutch,
        .driveline = *driveline,
        .rear_tire = *rear_tire,
        .rear_tire_count = static_cast<int>(rear_tire_count),
        .chassis = chassis,
    };
}

}  // namespace

Result<vehicle::Params> parse_tractor(std::string_view toml_text, std::string_view source_name,
                                      const std::filesystem::path& base_directory) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_tractor(root, source_name, base_directory);
    } catch (const toml::parse_error& e) {
        return LoadError{detail::describe(e, source_name)};
    }
}

Result<vehicle::Params> load_tractor(const std::filesystem::path& path) {
    const std::optional<std::string> text = detail::read_text(path);
    if (!text) {
        return LoadError{path.string() + ": cannot open file"};
    }
    return parse_tractor(*text, path.string(), path.parent_path());
}

}  // namespace tp::data
