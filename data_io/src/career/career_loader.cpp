#include "tp/data/career/career_loader.hpp"

#include "tp/core/event/event.hpp"
#include "tp/data/season/season_loader.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

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
    if (classes == nullptr || classes->empty() ||
        classes->size() > static_cast<std::size_t>(career::kMaxClasses)) {
        return detail::make_error(
            source, "career.classes",
            "must list 1 to " + std::to_string(career::kMaxClasses) + " classes");
    }
    definition.ladder = {.classes = static_cast<int>(classes->size()),
                         .promotion_place = static_cast<int>(std::clamp<std::int64_t>(
                             promotion_place, 0, std::numeric_limits<int>::max()))};
    if (career::validate(definition.ladder)) {
        return detail::make_error(
            source, "career.promotion_place",
            "must be a place from 1 to " + std::to_string(event::kMaxPullers));
    }
    for (const toml::node& node : *classes) {
        const toml::table* line = node.as_table();
        const std::optional<std::string> name =
            line != nullptr ? line->at_path("name").value<std::string>() : std::nullopt;
        const std::optional<std::string> season =
            line != nullptr ? line->at_path("season").value<std::string>() : std::nullopt;
        if (!name || name->empty() || !season) {
            return detail::make_error(source, "career.classes",
                                      "every class needs a name and a season");
        }
        // The save names the player's class: two of one name would be one.
        const bool taken = std::ranges::any_of(
            definition.classes, [&](const CareerClass& other) { return other.name == *name; });
        if (taken) {
            return detail::make_error(source, "career.classes",
                                      "the class name \"" + *name + "\" is used twice");
        }
        const std::string path = resolve(base, *season);
        if (auto loaded = load_season(path, read); !loaded) {
            return loaded.error();
        }
        definition.classes.push_back({.name = *name, .season = path});
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
