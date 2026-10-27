#include "tp/data/garage/garage_loader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

constexpr std::array<std::pair<std::string_view, garage::Part>, 5> kParts = {{
    {"engine_torque", garage::Part::kEngineTorque},
    {"rear_traction", garage::Part::kRearTraction},
    {"ballast_kg", garage::Part::kBallastMass},
    {"hitch_height_m", garage::Part::kHitchHeight},
    {"load_downshift_gears", garage::Part::kLoadDownshift},
}};

std::optional<garage::Part> part_named(std::string_view name) {
    for (const auto& [key, part] : kParts) {
        if (key == name) {
            return part;
        }
    }
    return std::nullopt;
}

// One level: {value, cost_eur, label}; nothing if one is missing.
std::optional<std::pair<garage::Level, std::string>> read_level(const toml::node& node) {
    const toml::table* level = node.as_table();
    if (level == nullptr) {
        return std::nullopt;
    }
    const std::optional<double> value = level->at_path("value").value<double>();
    const std::optional<std::int64_t> cost = level->at_path("cost_eur").value<std::int64_t>();
    const std::optional<std::string> label = level->at_path("label").value<std::string>();
    if (!value || !cost || !label || label->empty()) {
        return std::nullopt;
    }
    return std::pair{garage::Level{.value = *value, .cost_eur = *cost}, *label};
}

// The levels of upgrade `name` into `upgrade` and `text`; the error if any.
std::optional<LoadError> read_levels(const toml::array& levels, const std::string& name,
                                     std::string_view source, garage::Upgrade& upgrade,
                                     UpgradeText& text) {
    if (levels.empty() || levels.size() > garage::kMaxLevels) {
        return detail::make_error(
            source, "garage.upgrades.levels",
            name + ": must list 1 to " + std::to_string(garage::kMaxLevels) + " levels");
    }
    for (const toml::node& node : levels) {
        const auto level = read_level(node);
        if (!level) {
            return detail::make_error(source, "garage.upgrades.levels",
                                      name + ": every level needs a value, cost_eur and label");
        }
        if (level->first.cost_eur < 0) {
            return detail::make_error(source, "garage.upgrades.levels",
                                      name + ": a cost must not be negative");
        }
        upgrade.levels[upgrade.level_count] = level->first;
        ++upgrade.level_count;
        text.level_labels.push_back(level->second);
    }
    if (upgrade.levels[0].cost_eur != 0) {
        return detail::make_error(
            source, "garage.upgrades.levels",
            name + ": the first level is the tractor as it starts, cost_eur = 0");
    }
    return std::nullopt;
}

// One upgrade, appended to `out`; the error if any.
std::optional<LoadError> read_upgrade(const toml::node& node, std::string_view source,
                                      GarageDefinition& out) {
    const toml::table* table = node.as_table();
    const std::optional<std::string> part_name =
        table != nullptr ? table->at_path("part").value<std::string>() : std::nullopt;
    const std::optional<std::string> name =
        table != nullptr ? table->at_path("name").value<std::string>() : std::nullopt;
    if (!part_name || !name || name->empty()) {
        return detail::make_error(source, "garage.upgrades",
                                  "every upgrade needs a part and a name");
    }
    const std::optional<garage::Part> part = part_named(*part_name);
    if (!part) {
        return detail::make_error(source, "garage.upgrades.part",
                                  "unknown part '" + *part_name +
                                      "' (engine_torque, rear_traction, ballast_kg, "
                                      "hitch_height_m or load_downshift_gears)");
    }
    const std::span<const garage::Upgrade> before = out.catalogue.offer();
    if (std::ranges::any_of(before, [&](const garage::Upgrade& u) { return u.part == *part; })) {
        return detail::make_error(source, "garage.upgrades.part",
                                  "'" + *part_name + "' is listed twice");
    }
    const toml::array* levels = table->at_path("levels").as_array();
    if (levels == nullptr) {
        return detail::make_error(
            source, "garage.upgrades.levels",
            *name + ": must list 1 to " + std::to_string(garage::kMaxLevels) + " levels");
    }
    garage::Upgrade upgrade{.part = *part, .levels = {}, .level_count = 0};
    UpgradeText text{.name = *name, .level_labels = {}};
    if (std::optional<LoadError> error = read_levels(*levels, *name, source, upgrade, text)) {
        return error;
    }
    out.catalogue.upgrades[out.catalogue.count] = upgrade;
    ++out.catalogue.count;
    out.texts.push_back(std::move(text));
    return std::nullopt;
}

Result<GarageDefinition> read_garage(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    GarageDefinition out;
    out.catalogue.start_money_eur = reader.integer("garage.start_money_eur");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (out.catalogue.start_money_eur < 0) {
        return detail::make_error(source, "garage.start_money_eur", "must not be negative");
    }
    const toml::array* upgrades = root.at_path("garage.upgrades").as_array();
    if (upgrades == nullptr || upgrades->empty() || upgrades->size() > garage::kMaxUpgrades) {
        return detail::make_error(
            source, "garage.upgrades",
            "must list 1 to " + std::to_string(garage::kMaxUpgrades) + " upgrades");
    }
    for (const toml::node& node : *upgrades) {
        if (std::optional<LoadError> error = read_upgrade(node, source, out)) {
            return *error;
        }
    }
    return out;
}

}  // namespace

Result<GarageDefinition> parse_garage(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<GarageDefinition>(toml_text, source_name, read_garage);
}

Result<GarageDefinition> load_garage(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<GarageDefinition>(path, read, read_garage);
}

}  // namespace tp::data
