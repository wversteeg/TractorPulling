#include "tp/data/sled/sled_loader.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
        case ParamError::kYawInertiaNotPositive:
            return {"sled.frame_yaw_inertia_kg_m2", "must be > 0"};
        case ParamError::kBoxMassNegative:
            return {"sled.weight_box.mass_kg", "must not be negative"};
        case ParamError::kBoxOverMax:
            return {"sled.weight_box.mass_kg", "must not be over max_mass_kg"};
        case ParamError::kPanInvalid:
            return {"sled.pan",
                    "mass_kg 0 to frame_mass_kg, length_m > 0, pivot_height_m >= 0, "
                    "cylinders_ahead_of_wheels_m between the wheels and the pan"};
        case ParamError::kPanNotAheadOfWheels:
            return {"sled.pan_ahead_of_wheels_m", "must be > 0"};
        case ParamError::kFrameCgOutsideSled:
            return {"sled.frame_cg_ahead_of_wheels_m", "must lie between the wheels and the pan"};
        case ParamError::kBoxOutsideSled:
            return {"sled.weight_box", "must stay between the wheels and the pan"};
        case ParamError::kSledTipsBack:
            return {"sled.weight_box.start_ahead_of_wheels_m",
                    "too far behind the wheels: the sled would tip onto its tail"};
        case ParamError::kBoxTravelNotPositive:
            return {"sled.weight_box.travel_distance_m", "must be > 0"};
        case ParamError::kBoxGearboxInvalid:
            return {"sled.weight_box.gearbox", "first_m and step_m must be > 0, gears 1 to 16"};
        case ParamError::kBoxTravelNotAGear:
            return {"sled.weight_box.travel_distance_m",
                    "must be one of the gearbox's settings (first_m + n step_m)"};
        case ParamError::kHookInvalid:
            return {"sled.hook", "position and height must be > 0"};
        case ParamError::kPushdownStartNegative:
            return {"sled.pushdown.start_m", "must not be negative"};
        case ParamError::kPushdownRampNotPositive:
            return {"sled.pushdown.ramp_m", "must be > 0"};
        case ParamError::kPushdownShareOutOfRange:
            return {"sled.pushdown.most_pan_share", "must be > 0 and at most 1"};
        case ParamError::kPanFrictionOutOfRange:
            return {"sled.pan_friction_coefficient", "must be in (0, 2]"};
        case ParamError::kPanAreaNotPositive:
            return {"sled.pan_area_m2", "must be > 0"};
        case ParamError::kPanPloughNegative:
            return {"sled.pan_plough_coefficient", "must not be negative"};
        case ParamError::kKnivesOutOfRange:
            return {"sled.knives",
                    "count 0 to 24, depth 0 to 0.15 m, width >= 0 (> 0 with knives)"};
        case ParamError::kBermOutOfRange:
            return {"sled.berm",
                    "collected_share 0 to 1, spill_distance_m > 0, friction_coefficient >= 0"};
        case ParamError::kWheel:
            return {"sled.wheels", "invalid tire size"};
        case ParamError::kWheelCornering:
            return {
                "sled.wheels.cornering",
                "stiffness_per_rad, mobility_rate and relaxation_length_m must be > 0, limit in "
                "(0, 2]"};
        case ParamError::kWheelCountOutOfRange:
            return {"sled.wheels.count", "must be 1 to 8"};
        case ParamError::kWheelLossesOutOfRange:
            return {"sled.wheels.rolling_losses", "must be 0 to 0.2"};
        case ParamError::kChainInvalid:
            return {"sled.chain", "length and stiffness must be > 0, damping >= 0"};
        case ParamError::kFendersInvalid:
            return {"sled.fenders",
                    "behind_hook_m and half_span_m must be >= 0, depth_m > 0 with a span"};
        case ParamError::kPullOffStepNotHeavier:
            return {"sled.pull_off_step",
                    "must make the sled heavier: values >= 0, at least one > 0"};
    }
    return {"sled", "invalid"};
}

constexpr std::string_view kWheels = "sled.wheels";
constexpr std::string_view kGearbox = "sled.weight_box.gearbox";

Result<sled::Params> read_sled(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const std::int64_t wheel_count = reader.integer("sled.wheels.count");
    const std::int64_t knife_count = reader.integer("sled.knives.count");
    const std::int64_t step_knives = reader.integer("sled.pull_off_step.knives");
    // Optional: a sled without a gearbox lets its box take any travel.
    const bool has_gearbox = static_cast<bool>(root.at_path(kGearbox));
    const std::int64_t gears = has_gearbox ? reader.integer("sled.weight_box.gearbox.gears") : 0;
    sled::Params params{
        .frame_mass_kg = reader.number("sled.frame_mass_kg"),
        .frame_cg_ahead_of_wheels_m = reader.number("sled.frame_cg_ahead_of_wheels_m"),
        .frame_yaw_inertia_kg_m2 = reader.number("sled.frame_yaw_inertia_kg_m2"),
        .pan_ahead_of_wheels_m = reader.number("sled.pan_ahead_of_wheels_m"),
        .pan_mass_kg = reader.number("sled.pan.mass_kg"),
        .pan_length_m = reader.number("sled.pan.length_m"),
        .pivot_height_m = reader.number("sled.pan.pivot_height_m"),
        .cylinders_ahead_of_wheels_m = reader.number("sled.pushdown.cylinders_ahead_of_wheels_m"),
        .hook_ahead_of_wheels_m = reader.number("sled.hook.ahead_of_wheels_m"),
        .hook_height_m = reader.number("sled.hook.height_m"),
        .box_mass_kg = reader.number("sled.weight_box.mass_kg"),
        .box_max_mass_kg = reader.number("sled.weight_box.max_mass_kg"),
        .box_start_ahead_of_wheels_m = reader.number("sled.weight_box.start_ahead_of_wheels_m"),
        .box_end_ahead_of_wheels_m = reader.number("sled.weight_box.end_ahead_of_wheels_m"),
        .box_travel_distance_m = reader.number("sled.weight_box.travel_distance_m"),
        .box_gearbox =
            {
                .first_m = has_gearbox ? reader.number("sled.weight_box.gearbox.first_m") : 0.0,
                .step_m = has_gearbox ? reader.number("sled.weight_box.gearbox.step_m") : 0.0,
                .gears = 0,  // checked below
            },
        .pushdown_start_m = reader.number("sled.pushdown.start_m"),
        .pushdown_ramp_m = reader.number("sled.pushdown.ramp_m"),
        .pushdown_most_pan_share =
            reader.optional_number("sled.pushdown.most_pan_share").value_or(1.0),
        .pan_friction_coefficient = reader.number("sled.pan_friction_coefficient"),
        .pan_area_m2 = reader.number("sled.pan_area_m2"),
        .pan_plough_coefficient = reader.number("sled.pan_plough_coefficient"),
        .knives = {.count = 0,  // checked below
                   .depth_m = reader.number("sled.knives.depth_m"),
                   .width_m = reader.number("sled.knives.width_m")},
        .berm = {.collected_share = reader.number("sled.berm.collected_share"),
                 .spill_distance_m = reader.number("sled.berm.spill_distance_m"),
                 .friction_coefficient = reader.number("sled.berm.friction_coefficient")},
        .wheel = detail::read_tire_size(reader, kWheels),
        .wheel_cornering = detail::read_cornering(reader, "sled.wheels.cornering"),
        .wheel_count = 0,  // checked below
        .wheel_rolling_losses = reader.number("sled.wheels.rolling_losses"),
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
        // Optional: a sled without fenders lets a tractor back up to its hook.
        .fenders =
            {
                .behind_hook_m = reader.optional_number("sled.fenders.behind_hook_m").value_or(0.0),
                .half_span_m = reader.optional_number("sled.fenders.half_span_m").value_or(0.0),
                .depth_m = reader.optional_number("sled.fenders.depth_m").value_or(0.0),
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
    if (has_gearbox && (gears < 1 || gears > sled::kMaxBoxGears)) {
        return detail::make_error(source, "sled.weight_box.gearbox.gears", "must be 1 to 16");
    }
    params.box_gearbox.gears = static_cast<int>(gears);
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

constexpr std::string_view kBase = "sled.base";

// Copies every value of `over` into `into`, table by table: what `over`
// sets wins, what it leaves out keeps `into`'s value. A work list rather
// than recursion: the tables nest only a few deep, but no call depth is
// left to the file.
void overlay(toml::table& into, const toml::table& over) {
    std::vector<std::pair<toml::table*, const toml::table*>> work{{&into, &over}};
    while (!work.empty()) {
        const auto [to, from] = work.back();
        work.pop_back();
        for (const auto& [key, node] : *from) {
            const toml::table* from_table = node.as_table();
            toml::table* to_table = (*to)[key].as_table();
            if (from_table != nullptr && to_table != nullptr) {
                work.emplace_back(to_table, from_table);
            } else {
                to->insert_or_assign(key, node);
            }
        }
    }
}

// A sled file that names no base, read as it is.
Result<sled::Params> read_without_base(const toml::table& root, std::string_view source) {
    if (root.at_path(kBase)) {
        return detail::make_error(source, kBase, "needs the file it names: load the file instead");
    }
    return read_sled(root, source);
}

// A class's sled: the sled every class pulls (`sled.base`, a file next to
// this one), with this file's values over it, usually the weight box and
// the push-down.
Result<sled::Params> read_with_base(const toml::table& root, std::string_view source,
                                    const ReadFile& read) {
    const toml::node_view<const toml::node> base_node = root.at_path(kBase);
    if (!base_node) {
        return read_sled(root, source);
    }
    const std::optional<std::string> base_name = base_node.value<std::string>();
    if (!base_name) {
        return detail::make_error(source, kBase, "must be a file name");
    }
    const std::string base_path = resolve(directory_of(source), *base_name);
    const std::optional<std::string> base_text = read(base_path);
    if (!base_text) {
        return detail::make_error(source, kBase, "cannot open " + base_path);
    }
    toml::parse_result parsed = toml::parse(*base_text, base_path);
    if (!parsed) {
        return LoadError{detail::describe(parsed.error(), base_path)};
    }
    toml::table merged = std::move(parsed).table();
    if (merged.at_path(kBase)) {
        return detail::make_error(base_path, kBase, "a base sled names no base of its own");
    }
    overlay(merged, root);
    if (toml::table* sled = merged["sled"].as_table()) {
        sled->erase("base");
    }
    // The value may be this file's or the base's: the message names both.
    Result<sled::Params> out = read_sled(merged, source);
    if (!out) {
        return LoadError{out.error().message + " (this sled laid over the base sled " + base_path +
                         ")"};
    }
    return out;
}

}  // namespace

Result<sled::Params> parse_sled(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<sled::Params>(toml_text, source_name, &read_without_base);
}

Result<sled::Params> load_sled(std::string_view path, const ReadFile& read) {
    const std::string name{path};
    const std::optional<std::string> text = read(name);
    if (!text) {
        return LoadError{name + ": cannot open file"};
    }
    return detail::parse_then(*text, name, &read_with_base, read);
}

Result<sled::Params> load_sled(const std::filesystem::path& path) {
    return load_sled(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
