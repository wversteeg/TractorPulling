#include "tp/data/event/event_loader.hpp"

#include "tp/core/driveline/driveline.hpp"
#include "tp/data/rules/class_rules_loader.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(event::ParamError error) {
    using event::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"event.rules", "all values must be finite"};
        case ParamError::kDistancesOutOfOrder:
            return {"event.rules",
                    "need 0 < qualifying_distance_m, sled_target_m <= full_pull_m <= "
                    "extended_full_pull_m <= max_distance_m"};
        case ParamError::kPlacesOutOfRange:
            return {"event.rules.pull_off_places", "must be 1 to 32"};
        case ParamError::kTurnsOutOfOrder:
            return {"event.rules", "need 0 <= decide_after_pulls < repull_after_pulls"};
        case ParamError::kStepNotPositive:
            return {"event.rules.step_m", "must be > 0"};
        case ParamError::kMaxStepsNotPositive:
            return {"event.rules.max_steps_per_change", "must be >= 1"};
        case ParamError::kSled:
            return {"event.sled", "invalid"};
        case ParamError::kRunningOrder:
            return {"event.pullers", "invalid"};
    }
    return {"event.rules", "invalid"};
}

FieldError describe(field::ParamError error) {
    using field::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"event.field", "all values must be finite"};
        case ParamError::kRangeOutOfOrder:
            return {"event.field",
                    "every range is [low, high] with low <= high, low > 0, track_reading within "
                    "[0, 1], downshift_share within [0, 1)"};
        case ParamError::kGearsOutOfRange:
            return {"event.field.gears", "must be [lowest, highest] within the gearbox"};
        case ParamError::kLoadDownshiftsOutOfRange:
            return {"event.field.load_downshift_gears",
                    "must be [fewest, most], fewer than the gearbox's gears"};
        case ParamError::kCountOutOfRange:
            return {"event.pullers", "must be 1 to 32"};
        case ParamError::kReference:
            return {"event.driver", "the reference driver is invalid for the tractor"};
    }
    return {"event.field", "invalid"};
}

field::Range range(detail::Reader& reader, std::string_view path) {
    const std::vector<double> values = reader.numbers(path);
    if (values.size() != 2) {
        reader.fail(path, "must be [low, high]");
        return {};
    }
    return {.low = values[0], .high = values[1]};
}

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

Result<EventDefinition> read_event(const toml::table& root, std::string_view source,
                                   std::string_view base, const ReadFile& read) {
    detail::Reader reader{root, source};
    const std::string class_path = reader.text("event.class");
    const std::string tractor_path = reader.text("event.tractor");
    const std::string sled_path = reader.text("event.sled");
    const std::string track_path = reader.text("event.track");
    const std::int64_t pullers = reader.integer("event.pullers");
    const std::int64_t seed = reader.integer("event.seed");
    const std::int64_t gear = reader.integer("event.driver.gear");
    const std::vector<double> gears = reader.numbers("event.field.gears");
    const std::int64_t game_pullers = reader.integer("event.game.pullers");
    std::vector<std::string> rival_names = reader.texts("event.game.rival_names");
    const event::Rules rules{
        .full_pull_m = reader.number("event.rules.full_pull_m"),
        .extended_full_pull_m = reader.number("event.rules.extended_full_pull_m"),
        .max_distance_m = reader.number("event.rules.max_distance_m"),
        .pull_off_places = static_cast<int>(reader.integer("event.rules.pull_off_places")),
        .qualifying_distance_m = reader.number("event.rules.qualifying_distance_m"),
        .decide_after_pulls = static_cast<int>(reader.integer("event.rules.decide_after_pulls")),
        .repull_after_pulls = static_cast<int>(reader.integer("event.rules.repull_after_pulls")),
        .sled_target_m = reader.number("event.rules.sled_target_m"),
        .step_m = reader.number("event.rules.step_m"),
        .max_steps_per_change =
            static_cast<int>(reader.integer("event.rules.max_steps_per_change")),
    };
    const driver::Plan driver{
        .gear = static_cast<int>(gear),
        .select_gear_s = reader.number("event.driver.select_gear_s"),
        .launch_s = reader.number("event.driver.launch_s"),
        .clutch_slip_in_s = reader.number("event.driver.clutch_slip_in_s"),
        .downshift_share = reader.number("event.driver.downshift_share"),
    };
    field::Variation variation{
        .torque_scale = range(reader, "event.field.torque_scale"),
        .mass_kg = range(reader, "event.field.mass_kg"),
        .traction_limit = range(reader, "event.field.traction_limit"),
        .lowest_gear = 0,
        .highest_gear = 0,
        .clutch_slip_in_s = range(reader, "event.field.clutch_slip_in_s"),
        .track_reading = range(reader, "event.field.track_reading"),
        .downshift_share = range(reader, "event.field.downshift_share"),
        .hitch_height_m = {},
        .fewest_load_downshift_gears = -1,
        .most_load_downshift_gears = -1,
    };
    const field::PullToPull pull_to_pull{
        .grip_share = reader.number("event.field.pull_to_pull_grip_share"),
        .clutch_slip_in_s = reader.number("event.field.pull_to_pull_clutch_slip_in_s"),
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (gears.size() != 2) {
        return detail::make_error(source, "event.field.gears", "must be [lowest, highest]");
    }
    variation.lowest_gear = static_cast<int>(gears[0]);
    variation.highest_gear = static_cast<int>(gears[1]);
    // Optional: without them every puller has the reference's drawbar and
    // powershift.
    if (reader.has("event.field.hitch_height_m")) {
        variation.hitch_height_m = range(reader, "event.field.hitch_height_m");
    }
    if (reader.has("event.field.load_downshift_gears")) {
        const std::vector<double> counts = reader.numbers("event.field.load_downshift_gears");
        const bool whole = std::ranges::all_of(counts, [](double c) {
            return c >= 0.0 && c == std::floor(c) && c <= static_cast<double>(driveline::kMaxGears);
        });
        if (counts.size() != 2 || !whole) {
            return detail::make_error(source, "event.field.load_downshift_gears",
                                      "must be [fewest, most], whole numbers from 0");
        }
        variation.fewest_load_downshift_gears = static_cast<int>(counts[0]);
        variation.most_load_downshift_gears = static_cast<int>(counts[1]);
    }
    if (const std::optional<LoadError>& late = reader.error()) {
        return *late;
    }
    // Written to also reject NaN.
    const bool grip_ok = pull_to_pull.grip_share >= 0.0 && pull_to_pull.grip_share < 1.0;
    const bool clutch_ok = pull_to_pull.clutch_slip_in_s >= 0.0;
    if (!grip_ok || !clutch_ok) {
        return detail::make_error(source, "event.field",
                                  "pull_to_pull values must be >= 0, the grip share < 1");
    }
    if (seed < 0) {
        return detail::make_error(source, "event.seed", "must not be negative");
    }
    if (pullers < 1 || pullers > event::kMaxPullers) {
        return detail::make_error(source, "event.pullers", "must be 1 to 32");
    }
    if (game_pullers < 2 || game_pullers > event::kMaxPullers) {
        return detail::make_error(source, "event.game.pullers", "must be 2 to 32");
    }
    const bool named =
        std::ranges::none_of(rival_names, [](const std::string& name) { return name.empty(); });
    if (std::cmp_not_equal(rival_names.size(), game_pullers - 1) || !named) {
        return detail::make_error(source, "event.game.rival_names",
                                  "one name, not empty, for every puller but the player");
    }

    std::optional<LoadError> error;
    const auto class_rules = component(load_class_rules(resolve(base, class_path), read), error);
    const auto tractor = component(load_tractor(resolve(base, tractor_path), read), error);
    const auto sled = component(load_sled(resolve(base, sled_path), read), error);
    const auto track = component(load_track(resolve(base, track_path), read), error);
    if (error) {
        return *error;
    }
    if (!class_rules || !tractor || !sled || !track) {
        return detail::make_error(source, "event", "invalid component");
    }
    if (const auto rules_error = event::validate(rules)) {
        const FieldError field_error = describe(*rules_error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    const field::Puller reference{.tractor = *tractor, .driver = driver};
    const int count = static_cast<int>(pullers);
    if (const auto field_error = field::validate(reference, variation, *class_rules, count)) {
        const FieldError described = describe(*field_error);
        return detail::make_error(source, described.field, described.reason);
    }
    return EventDefinition{
        .class_rules = *class_rules,
        .rules = rules,
        .reference = reference,
        .variation = variation,
        .pull_to_pull = pull_to_pull,
        .pullers = count,
        .seed = static_cast<std::uint64_t>(seed),
        .sled = *sled,
        .track = *track,
        .game = {.pullers = static_cast<int>(game_pullers), .rival_names = std::move(rival_names)}};
}

}  // namespace

Result<EventDefinition> parse_event(std::string_view toml_text, std::string_view source_name,
                                    std::string_view base_directory, const ReadFile& read) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_event(root, source_name, base_directory, read);
    } catch (const toml::parse_error& e) {
        return LoadError{detail::describe(e, source_name)};
    }
}

Result<EventDefinition> parse_event(std::string_view toml_text, std::string_view source_name,
                                    const std::filesystem::path& base_directory) {
    return parse_event(toml_text, source_name, base_directory.generic_string(), read_from_disk);
}

Result<EventDefinition> load_event(std::string_view path, const ReadFile& read) {
    const std::string name{path};
    const std::optional<std::string> text = read(name);
    if (!text) {
        return LoadError{name + ": cannot open file"};
    }
    return parse_event(*text, name, directory_of(name), read);
}

Result<EventDefinition> load_event(const std::filesystem::path& path) {
    return load_event(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
