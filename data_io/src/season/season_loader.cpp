#include "tp/data/season/season_loader.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

Result<SeasonDefinition> read_season(const toml::table& root, std::string_view source,
                                     const std::filesystem::path& base) {
    detail::Reader reader{root, source};
    const std::string event_path = reader.text("season.event");
    const std::int64_t seed = reader.integer("season.seed");
    const std::vector<double> table = reader.numbers("season.points");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (seed < 0) {
        return detail::make_error(source, "season.seed", "must not be negative");
    }

    season::Points points;
    if (table.empty() || table.size() > points.by_place.size()) {
        return detail::make_error(source, "season.points", "must list 1 to 32 places");
    }
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i] != std::floor(table[i])) {
            return detail::make_error(source, "season.points", "must be whole numbers");
        }
        points.by_place[i] = static_cast<int>(table[i]);
    }
    points.places = static_cast<int>(table.size());
    if (season::validate(points)) {
        return detail::make_error(source, "season.points",
                                  "must be >= 0, never more for a lower place");
    }

    const toml::array* events = root.at_path("season.events").as_array();
    if (events == nullptr || events->empty() ||
        events->size() > static_cast<std::size_t>(season::kMaxEvents)) {
        return detail::make_error(source, "season.events", "must list 1 to 24 events");
    }
    auto event = load_event(base / event_path);
    if (!event) {
        return event.error();
    }
    SeasonDefinition definition{.event = event.value(),
                                .points = points,
                                .rounds = {},
                                .seed = static_cast<std::uint64_t>(seed)};
    for (const toml::node& node : *events) {
        const toml::table* round = node.as_table();
        const std::optional<std::string> name =
            round != nullptr ? round->at_path("name").value<std::string>() : std::nullopt;
        const std::optional<std::string> track_path =
            round != nullptr ? round->at_path("track").value<std::string>() : std::nullopt;
        if (!name || !track_path) {
            return detail::make_error(source, "season.events",
                                      "every event needs a name and a track");
        }
        auto track = load_track(base / *track_path);
        if (!track) {
            return track.error();
        }
        definition.rounds.push_back({.name = *name, .track = track.value()});
    }
    return definition;
}

}  // namespace

Result<SeasonDefinition> parse_season(std::string_view toml_text, std::string_view source_name,
                                      const std::filesystem::path& base_directory) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_season(root, source_name, base_directory);
    } catch (const toml::parse_error& e) {
        return LoadError{detail::describe(e, source_name)};
    }
}

Result<SeasonDefinition> load_season(const std::filesystem::path& path) {
    const std::optional<std::string> text = detail::read_text(path);
    if (!text) {
        return LoadError{path.string() + ": cannot open file"};
    }
    return parse_season(*text, path.string(), path.parent_path());
}

}  // namespace tp::data
