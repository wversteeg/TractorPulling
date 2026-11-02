#include "tp/data/career/career_loader.hpp"

#include "tp/core/event/event.hpp"
#include "tp/data/season/season_loader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// A table's text field, or nothing.
std::optional<std::string> text_of(const toml::node& node, std::string_view key) {
    const toml::table* table = node.as_table();
    return table != nullptr ? table->at_path(key).value<std::string>() : std::nullopt;
}

// The levels of the class at `node` onto `definition`'s rungs; the error if
// they are broken.
std::optional<LoadError> read_levels(const toml::node& node, const std::string& class_name,
                                     std::string_view source, std::string_view base,
                                     const ReadFile& read, CareerDefinition& definition) {
    const toml::table* line = node.as_table();
    const toml::array* levels = line != nullptr ? line->at_path("levels").as_array() : nullptr;
    if (levels == nullptr || levels->empty()) {
        return detail::make_error(source, "career.classes",
                                  "the class \"" + class_name + "\" needs its levels");
    }
    const std::size_t first = definition.rungs.size();
    for (const toml::node& level : *levels) {
        const std::optional<std::string> name = text_of(level, "name");
        const std::optional<std::string> season = text_of(level, "season");
        if (!name || name->empty() || !season) {
            return detail::make_error(source, "career.classes.levels",
                                      "every level needs a name and a season");
        }
        // The save names the player's level: two of one name would be one.
        const bool taken = std::any_of(
            definition.rungs.begin() + static_cast<std::ptrdiff_t>(first), definition.rungs.end(),
            [&](const CareerRung& other) { return other.level_name == *name; });
        if (taken) {
            return detail::make_error(
                source, "career.classes.levels",
                "the level name \"" + *name + "\" is used twice in \"" + class_name + "\"");
        }
        const std::string path = resolve(base, *season);
        if (auto loaded = load_season(path, read); !loaded) {
            return loaded.error();
        }
        definition.rungs.push_back({.class_name = class_name, .level_name = *name, .season = path});
    }
    return std::nullopt;
}

Result<CareerDefinition> read_career(const toml::table& root, std::string_view source,
                                     std::string_view base, const ReadFile& read) {
    detail::Reader reader{root, source};
    CareerDefinition definition;
    definition.name = reader.text("career.name");
    const std::int64_t promotion_place = reader.integer("career.promotion_place");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    const toml::array* classes = root.at_path("career.classes").as_array();
    if (classes == nullptr || classes->empty()) {
        return detail::make_error(source, "career.classes", "must list its classes");
    }
    for (const toml::node& node : *classes) {
        const std::optional<std::string> name = text_of(node, "name");
        if (!name || name->empty()) {
            return detail::make_error(source, "career.classes", "every class needs a name");
        }
        const bool taken = std::ranges::any_of(
            definition.rungs, [&](const CareerRung& other) { return other.class_name == *name; });
        if (taken) {
            return detail::make_error(source, "career.classes",
                                      "the class name \"" + *name + "\" is used twice");
        }
        if (std::optional<LoadError> error =
                read_levels(node, *name, source, base, read, definition)) {
            return *error;
        }
    }
    definition.ladder = {.rungs = static_cast<int>(std::min<std::size_t>(
                             definition.rungs.size(), std::numeric_limits<int>::max())),
                         .promotion_place = static_cast<int>(std::clamp<std::int64_t>(
                             promotion_place, 0, std::numeric_limits<int>::max()))};
    if (const std::optional<career::ParamError> error = career::validate(definition.ladder)) {
        if (*error == career::ParamError::kRungCountOutOfRange) {
            return detail::make_error(
                source, "career.classes",
                "must hold 1 to " + std::to_string(career::kMaxRungs) + " levels in all");
        }
        return detail::make_error(
            source, "career.promotion_place",
            "must be a place from 1 to " + std::to_string(event::kMaxPullers));
    }
    return definition;
}

}  // namespace

Result<CareerDefinition> parse_career(std::string_view toml_text, std::string_view source_name,
                                      std::string_view base_directory, const ReadFile& read) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_career(root, source_name, base_directory, read);
    } catch (const toml::parse_error& e) {
        return LoadError{detail::describe(e, source_name)};
    }
}

Result<CareerDefinition> load_career(std::string_view path, const ReadFile& read) {
    const std::string name{path};
    const std::optional<std::string> text = read(name);
    if (!text) {
        return LoadError{name + ": cannot open file"};
    }
    return parse_career(*text, name, directory_of(name), read);
}

Result<CareerDefinition> load_career(const std::filesystem::path& path) {
    return load_career(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
