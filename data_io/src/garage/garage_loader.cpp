#include "tp/data/garage/garage_loader.hpp"

#include "tp/core/units/conversions.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/files.hpp"
#include "tp/data/tire/tire_loader.hpp"

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

// A part's name in the file, and what turns its values into the core's
// units (rpm into rad/s).
struct PartName {
    std::string_view key;
    garage::Part part = garage::Part::kEngineTorque;
    double to_si = 1.0;
};

constexpr std::array<PartName, 6> kParts = {{
    {.key = "engine_torque", .part = garage::Part::kEngineTorque, .to_si = 1.0},
    {.key = "rear_traction", .part = garage::Part::kRearTraction, .to_si = 1.0},
    {.key = "ballast_kg", .part = garage::Part::kBallastMass, .to_si = 1.0},
    {.key = "hitch_height_m", .part = garage::Part::kHitchHeight, .to_si = 1.0},
    {.key = "load_downshift_gears", .part = garage::Part::kLoadDownshift, .to_si = 1.0},
    {.key = "rated_rpm", .part = garage::Part::kRatedSpeed, .to_si = units::rpm_to_rad_s(1.0)},
}};

std::optional<PartName> part_named(std::string_view name) {
    for (const PartName& p : kParts) {
        if (p.key == name) {
            return p;
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
                                     double to_si, std::string_view source,
                                     garage::Upgrade& upgrade, UpgradeText& text) {
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
        upgrade.levels[upgrade.level_count] = {.value = level->first.value * to_si,
                                               .cost_eur = level->first.cost_eur};
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

// The choice and alternative upgrade `name` belongs to, if it names them
// (`choice` and `alternative`, by their names), into `upgrade`.
std::optional<LoadError> read_owner(const toml::table& table, const std::string& name,
                                    std::string_view source, const GarageDefinition& out,
                                    garage::Upgrade& upgrade) {
    const std::optional<std::string> choice = table.at_path("choice").value<std::string>();
    const std::optional<std::string> alternative =
        table.at_path("alternative").value<std::string>();
    if (!choice && !alternative) {
        return std::nullopt;
    }
    if (!choice || !alternative) {
        return detail::make_error(source, "garage.upgrades",
                                  name + ": names a choice and its alternative, or neither");
    }
    const auto c = std::ranges::find_if(out.choice_texts,
                                        [&](const ChoiceText& t) { return t.name == *choice; });
    if (c == out.choice_texts.end()) {
        return detail::make_error(source, "garage.upgrades.choice",
                                  name + ": no choice '" + *choice + "'");
    }
    const auto a = std::ranges::find(c->alternative_labels, *alternative);
    if (a == c->alternative_labels.end()) {
        return detail::make_error(
            source, "garage.upgrades.alternative",
            name + ": '" + *choice + "' has no alternative '" + *alternative + "'");
    }
    upgrade.choice = static_cast<int>(c - out.choice_texts.begin());
    upgrade.alternative = static_cast<int>(a - c->alternative_labels.begin());
    return std::nullopt;
}

// Where a choice's alternatives go, and the name of each in the file.
struct SlotName {
    std::string_view key;
    garage::Slot slot = garage::Slot::kEngine;
};

constexpr std::array<SlotName, 3> kSlots = {{
    {.key = "engine", .slot = garage::Slot::kEngine},
    {.key = "rear_tire", .slot = garage::Slot::kRearTire},
    {.key = "driveline", .slot = garage::Slot::kDriveline},
}};

// The component file `path` for `slot` into `alternative`; its own error if
// it does not load.
std::optional<LoadError> read_component(garage::Slot slot, const std::string& path,
                                        const ReadFile& read, garage::Alternative& alternative) {
    switch (slot) {
        case garage::Slot::kEngine: {
            auto engine = load_engine(path, read);
            if (!engine) {
                return engine.error();
            }
            alternative.engine = engine.value();
            break;
        }
        case garage::Slot::kRearTire: {
            auto tire = load_tire(path, read);
            if (!tire) {
                return tire.error();
            }
            alternative.rear_tire = tire.value();
            break;
        }
        case garage::Slot::kDriveline: {
            auto driveline = load_driveline(path, read);
            if (!driveline) {
                return driveline.error();
            }
            alternative.driveline = driveline.value();
            break;
        }
    }
    return std::nullopt;
}

// One alternative of choice `name`: {file, cost_eur, label}, appended to
// `choice` and `text`.
std::optional<LoadError> read_alternative(const toml::node& node, const std::string& name,
                                          std::string_view source, std::string_view base,
                                          const ReadFile& read, garage::Choice& choice,
                                          ChoiceText& text) {
    const toml::table* table = node.as_table();
    const std::optional<std::string> file =
        table != nullptr ? table->at_path("file").value<std::string>() : std::nullopt;
    const std::optional<std::int64_t> cost =
        table != nullptr ? table->at_path("cost_eur").value<std::int64_t>() : std::nullopt;
    const std::optional<std::string> label =
        table != nullptr ? table->at_path("label").value<std::string>() : std::nullopt;
    if (!file || !cost || !label || label->empty()) {
        return detail::make_error(source, "garage.choices.alternatives",
                                  name + ": every alternative needs a file, cost_eur and label");
    }
    if (*cost < 0) {
        return detail::make_error(source, "garage.choices.alternatives",
                                  name + ": a cost must not be negative");
    }
    if (std::ranges::find(text.alternative_labels, *label) != text.alternative_labels.end()) {
        return detail::make_error(source, "garage.choices.alternatives",
                                  name + ": '" + *label + "' is listed twice");
    }
    garage::Alternative alternative;
    alternative.cost_eur = *cost;
    if (std::optional<LoadError> error =
            read_component(choice.slot, resolve(base, *file), read, alternative)) {
        return error;
    }
    choice.alternatives[choice.alternative_count] = alternative;
    ++choice.alternative_count;
    text.alternative_labels.push_back(*label);
    return std::nullopt;
}

// One choice, appended to `out`; the error if any.
std::optional<LoadError> read_choice(const toml::node& node, std::string_view source,
                                     std::string_view base, const ReadFile& read,
                                     GarageDefinition& out) {
    const toml::table* table = node.as_table();
    const std::optional<std::string> slot_name =
        table != nullptr ? table->at_path("slot").value<std::string>() : std::nullopt;
    const std::optional<std::string> name =
        table != nullptr ? table->at_path("name").value<std::string>() : std::nullopt;
    if (!slot_name || !name || name->empty()) {
        return detail::make_error(source, "garage.choices", "every choice needs a slot and a name");
    }
    const auto* const slot =
        std::ranges::find_if(kSlots, [&](const SlotName& s) { return s.key == *slot_name; });
    if (slot == kSlots.end()) {
        return detail::make_error(
            source, "garage.choices.slot",
            "unknown slot '" + *slot_name + "' (engine, rear_tire or driveline)");
    }
    if (std::ranges::any_of(out.choice_texts,
                            [&](const ChoiceText& t) { return t.name == *name; })) {
        return detail::make_error(source, "garage.choices.name", "'" + *name + "' is listed twice");
    }
    const toml::array* alternatives = table->at_path("alternatives").as_array();
    if (alternatives == nullptr || alternatives->size() < 2 ||
        alternatives->size() > garage::kMaxAlternatives) {
        return detail::make_error(source, "garage.choices.alternatives",
                                  *name + ": must list 2 to " +
                                      std::to_string(garage::kMaxAlternatives) + " alternatives");
    }
    garage::Choice choice;
    choice.slot = slot->slot;
    ChoiceText text{.name = *name, .alternative_labels = {}};
    for (const toml::node& alternative : *alternatives) {
        if (std::optional<LoadError> error =
                read_alternative(alternative, *name, source, base, read, choice, text)) {
            return error;
        }
    }
    if (choice.alternatives[0].cost_eur != 0) {
        return detail::make_error(
            source, "garage.choices.alternatives",
            *name + ": the first alternative is the one the tractor comes with, cost_eur = 0");
    }
    out.catalogue.choices[out.catalogue.choice_count] = choice;
    ++out.catalogue.choice_count;
    out.choice_texts.push_back(std::move(text));
    return std::nullopt;
}

// The choices, if the garage has any, into `out`.
std::optional<LoadError> read_choices(const toml::table& root, std::string_view source,
                                      std::string_view base, const ReadFile& read,
                                      GarageDefinition& out) {
    const toml::node_view<const toml::node> node = root.at_path("garage.choices");
    if (!node) {
        return std::nullopt;
    }
    const toml::array* choices = node.as_array();
    if (choices == nullptr || choices->size() > garage::kMaxChoices) {
        return detail::make_error(
            source, "garage.choices",
            "must list at most " + std::to_string(garage::kMaxChoices) + " choices");
    }
    for (const toml::node& choice : *choices) {
        if (std::optional<LoadError> error = read_choice(choice, source, base, read, out)) {
            return error;
        }
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
    const std::optional<PartName> part = part_named(*part_name);
    if (!part) {
        return detail::make_error(source, "garage.upgrades.part",
                                  "unknown part '" + *part_name +
                                      "' (engine_torque, rear_traction, ballast_kg, "
                                      "hitch_height_m, load_downshift_gears or rated_rpm)");
    }
    const toml::array* levels = table->at_path("levels").as_array();
    if (levels == nullptr) {
        return detail::make_error(
            source, "garage.upgrades.levels",
            *name + ": must list 1 to " + std::to_string(garage::kMaxLevels) + " levels");
    }
    garage::Upgrade upgrade{.part = part->part, .levels = {}, .level_count = 0};
    if (std::optional<LoadError> error = read_owner(*table, *name, source, out, upgrade)) {
        return error;
    }
    if (std::ranges::any_of(out.texts, [&](const UpgradeText& t) { return t.name == *name; })) {
        return detail::make_error(source, "garage.upgrades.name",
                                  "'" + *name + "' is listed twice");
    }
    UpgradeText text{.name = *name, .level_labels = {}};
    if (std::optional<LoadError> error =
            read_levels(*levels, *name, part->to_si, source, upgrade, text)) {
        return error;
    }
    // The same part twice only for different alternatives of one choice
    // (each engine's own build): never two that could count at once.
    const std::span<const garage::Upgrade> before = out.catalogue.offer();
    if (std::ranges::any_of(before, [&](const garage::Upgrade& u) {
            return u.part == upgrade.part &&
                   (u.choice == garage::kNoChoice || u.choice != upgrade.choice ||
                    u.alternative == upgrade.alternative);
        })) {
        return detail::make_error(source, "garage.upgrades.part",
                                  "'" + *part_name +
                                      "' is listed twice (allowed only for different "
                                      "alternatives of one choice)");
    }
    out.catalogue.upgrades[out.catalogue.count] = upgrade;
    ++out.catalogue.count;
    out.texts.push_back(std::move(text));
    return std::nullopt;
}

Result<GarageDefinition> read_garage(const toml::table& root, std::string_view source,
                                     std::string_view base, const ReadFile& read) {
    detail::Reader reader{root, source};
    GarageDefinition out;
    out.catalogue.start_money_eur = reader.integer("garage.start_money_eur");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (out.catalogue.start_money_eur < 0) {
        return detail::make_error(source, "garage.start_money_eur", "must not be negative");
    }
    if (std::optional<LoadError> error = read_choices(root, source, base, read, out)) {
        return *error;
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

Result<GarageDefinition> parse_garage(std::string_view toml_text, std::string_view source_name,
                                      std::string_view base_directory, const ReadFile& read) {
    return detail::parse_then(toml_text, source_name, read_garage, base_directory, read);
}

Result<GarageDefinition> parse_garage(std::string_view toml_text, std::string_view source_name) {
    return parse_garage(toml_text, source_name, "", read_from_disk);
}

Result<GarageDefinition> load_garage(std::string_view path, const ReadFile& read) {
    const std::string name{path};
    const std::optional<std::string> text = read(name);
    if (!text) {
        return LoadError{name + ": cannot open file"};
    }
    return parse_garage(*text, name, directory_of(name), read);
}

}  // namespace tp::data
