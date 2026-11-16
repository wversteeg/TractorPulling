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

// The slot the file names `name`; nothing if there is none. (A loop, as
// part_named: std::array's iterator is a pointer in libstdc++ but a class
// in libc++, so a find's result has no one spelling for both.)
std::optional<garage::Slot> slot_named(std::string_view name) {
    for (const SlotName& s : kSlots) {
        if (s.key == name) {
            return s.slot;
        }
    }
    return std::nullopt;
}

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
    const std::optional<garage::Slot> slot = slot_named(*slot_name);
    if (!slot) {
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
    choice.slot = *slot;
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

// What alternative `a` of choice `c` is made for, if it says
// (`suits = { "<choice>" = "<alternative>" }`), into `out`'s catalogue.
std::optional<LoadError> read_suits(const toml::table& table, std::size_t c, std::size_t a,
                                    std::string_view source, GarageDefinition& out) {
    const toml::node* node = table.get("suits");
    if (node == nullptr) {
        return std::nullopt;
    }
    const ChoiceText& self = out.choice_texts[c];
    const toml::table* suits = node->as_table();
    const auto bad = [&](const std::string& why) {
        return detail::make_error(source, "garage.choices.alternatives.suits",
                                  self.name + ", " + self.alternative_labels[a] + ": " + why);
    };
    if (suits == nullptr || suits->size() != 1) {
        return bad("must name one choice and its alternative");
    }
    const auto entry = suits->cbegin();
    const std::string choice_name{entry->first.str()};
    const std::optional<std::string> label = entry->second.value<std::string>();
    const auto other = std::ranges::find_if(
        out.choice_texts, [&](const ChoiceText& t) { return t.name == choice_name; });
    if (other == out.choice_texts.end() ||
        other == out.choice_texts.begin() + static_cast<std::ptrdiff_t>(c)) {
        return bad("no other choice '" + choice_name + "'");
    }
    if (!label) {
        return bad("'" + choice_name + "' must be one of its alternatives' labels (a string)");
    }
    const auto alternative = std::ranges::find(other->alternative_labels, *label);
    if (alternative == other->alternative_labels.end()) {
        return bad("'" + choice_name + "' has no alternative '" + *label + "'");
    }
    garage::Alternative& made = out.catalogue.choices[c].alternatives[a];
    made.suits_choice = static_cast<int>(other - out.choice_texts.begin());
    made.suits_alternative = static_cast<int>(alternative - other->alternative_labels.begin());
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
    // What each alternative is made for, once every choice is known.
    for (std::size_t c = 0; c < choices->size(); ++c) {
        const toml::array* alternatives =
            (*choices)[c].as_table()->at_path("alternatives").as_array();
        for (std::size_t a = 0; a < alternatives->size(); ++a) {
            if (std::optional<LoadError> error =
                    read_suits(*(*alternatives)[a].as_table(), c, a, source, out)) {
                return error;
            }
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
    UpgradeText text{.name = *name, .level_labels = {}, .former_names = {}};
    if (std::optional<LoadError> error =
            read_levels(*levels, *name, part->to_si, source, upgrade, text)) {
        return error;
    }
    if (const toml::node* formerly = table->get("formerly")) {
        const toml::array* names = formerly->as_array();
        if (names == nullptr) {
            return detail::make_error(source, "garage.upgrades.formerly",
                                      *name + ": must list the upgrade's former names");
        }
        for (const toml::node& former : *names) {
            const std::optional<std::string> former_name = former.value<std::string>();
            if (!former_name || former_name->empty()) {
                return detail::make_error(source, "garage.upgrades.formerly",
                                          *name + ": must list the upgrade's former names");
            }
            text.former_names.push_back(*former_name);
        }
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

// "#rrggbb".
bool is_colour(std::string_view text) {
    constexpr std::size_t kLength = 7;
    return text.size() == kLength && text.front() == '#' &&
           std::ranges::all_of(text.substr(1), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
           });
}

// The text at `key` in `table`, if there is one and it is not empty.
std::optional<std::string> text_at(const toml::table* table, std::string_view key) {
    std::optional<std::string> text =
        table != nullptr ? table->at_path(key).value<std::string>() : std::nullopt;
    if (text && text->empty()) {
        return std::nullopt;
    }
    return text;
}

// One model the dealer sells, appended to `out`.
std::optional<LoadError> read_model(const toml::node& node, std::string_view source,
                                    GarageDefinition& out) {
    const toml::table* table = node.as_table();
    const std::optional<std::string> name = text_at(table, "name");
    const std::optional<std::string> style = text_at(table, "style");
    const std::optional<std::string> about = text_at(table, "about");
    const std::optional<std::int64_t> cost =
        table != nullptr ? table->at_path("cost_eur").value<std::int64_t>() : std::nullopt;
    if (!name || !style || !about || !cost) {
        return detail::make_error(source, "garage.dealer.models",
                                  "every model needs a name, style, cost_eur and about");
    }
    if (*cost < 0) {
        return detail::make_error(source, "garage.dealer.models.cost_eur",
                                  *name + ": must not be negative");
    }
    if (std::ranges::any_of(out.model_texts, [&](const ModelText& t) { return t.name == *name; })) {
        return detail::make_error(source, "garage.dealer.models.name",
                                  "'" + *name + "' is listed twice");
    }
    garage::Dealer& dealer = out.catalogue.dealer;
    dealer.model_cost_eur[dealer.model_count] = *cost;
    ++dealer.model_count;
    out.model_texts.push_back({.name = *name, .style = *style, .about = *about});
    return std::nullopt;
}

// One paint the dealer offers, appended to `out`.
std::optional<LoadError> read_paint(const toml::node& node, std::string_view source,
                                    GarageDefinition& out) {
    const toml::table* table = node.as_table();
    PaintText paint{.name = text_at(table, "name").value_or(""),
                    .paint = text_at(table, "paint").value_or(""),
                    .trim = text_at(table, "trim").value_or(""),
                    .rims = text_at(table, "rims").value_or("")};
    if (paint.name.empty() || !is_colour(paint.paint) || !is_colour(paint.trim) ||
        !is_colour(paint.rims)) {
        return detail::make_error(
            source, "garage.dealer.paints",
            "every paint needs a name and its paint, trim and rims as \"#rrggbb\"");
    }
    if (std::ranges::any_of(out.paint_texts,
                            [&](const PaintText& t) { return t.name == paint.name; })) {
        return detail::make_error(source, "garage.dealer.paints.name",
                                  "'" + paint.name + "' is listed twice");
    }
    ++out.catalogue.dealer.paint_count;
    out.paint_texts.push_back(std::move(paint));
    return std::nullopt;
}

// The list at `path`, if it holds 1 to `most` entries.
const toml::array* list_at(const toml::table& root, std::string_view path, std::size_t most) {
    const toml::array* list = root.at_path(path).as_array();
    return list != nullptr && !list->empty() && list->size() <= most ? list : nullptr;
}

// The dealer, if the garage has one, into `out`.
std::optional<LoadError> read_dealer(const toml::table& root, std::string_view source,
                                     GarageDefinition& out) {
    if (!root.at_path("garage.dealer")) {
        return std::nullopt;
    }
    const std::optional<std::int64_t> paint_cost =
        root.at_path("garage.dealer.paint_cost_eur").value<std::int64_t>();
    if (!paint_cost || *paint_cost < 0) {
        return detail::make_error(source, "garage.dealer.paint_cost_eur",
                                  "must be a cost of 0 or more");
    }
    out.catalogue.dealer.paint_cost_eur = *paint_cost;
    const toml::array* models = list_at(root, "garage.dealer.models", garage::kMaxModels);
    if (models == nullptr) {
        return detail::make_error(
            source, "garage.dealer.models",
            "must list 1 to " + std::to_string(garage::kMaxModels) + " models");
    }
    for (const toml::node& model : *models) {
        if (std::optional<LoadError> error = read_model(model, source, out)) {
            return error;
        }
    }
    const toml::array* paints = list_at(root, "garage.dealer.paints", garage::kMaxPaints);
    if (paints == nullptr) {
        return detail::make_error(
            source, "garage.dealer.paints",
            "must list 1 to " + std::to_string(garage::kMaxPaints) + " paints");
    }
    for (const toml::node& paint : *paints) {
        if (std::optional<LoadError> error = read_paint(paint, source, out)) {
            return error;
        }
    }
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
    if (std::optional<LoadError> error = read_dealer(root, source, out)) {
        return *error;
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
