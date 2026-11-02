#include "tp/data/season/season_loader.hpp"

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

// The prize money by place: whole euros, 0 or more, never more for a lower
// place; nothing if not.
std::optional<std::vector<std::int64_t>> whole_euros(const std::vector<double>& prizes) {
    // Below 2^53 every whole double is exact; more than this is nonsense.
    constexpr double kMaxPrize_eur = 1e15;
    std::vector<std::int64_t> out;
    for (std::size_t i = 0; i < prizes.size(); ++i) {
        const bool whole = prizes[i] == std::floor(prizes[i]);
        const bool falling = i == 0 || prizes[i] <= prizes[i - 1];
        if (!whole || prizes[i] < 0.0 || prizes[i] > kMaxPrize_eur || !falling) {
            return std::nullopt;
        }
        out.push_back(static_cast<std::int64_t>(prizes[i]));
    }
    return out;
}

Result<SeasonDefinition> read_season(const toml::table& root, std::string_view source,
                                     std::string_view base, const ReadFile& read) {
    detail::Reader reader{root, source};
    const std::string event_path = reader.text("season.event");
    const std::int64_t seed = reader.integer("season.seed");
    const std::vector<double> table = reader.numbers("season.points");
    const std::vector<double> prizes = reader.numbers("season.prize_money_eur");
    const std::string garage_path = reader.text("season.garage");
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

    const std::optional<std::vector<std::int64_t>> prize_money_eur = whole_euros(prizes);
    if (!prize_money_eur) {
        return detail::make_error(source, "season.prize_money_eur",
                                  "must be whole euros, >= 0, never more for a lower place");
    }

    const toml::array* events = root.at_path("season.events").as_array();
    if (events == nullptr || events->empty() ||
        events->size() > static_cast<std::size_t>(season::kMaxEvents)) {
        return detail::make_error(source, "season.events", "must list 1 to 24 events");
    }
    auto event = load_event(resolve(base, event_path), read);
    if (!event) {
        return event.error();
    }
    auto garage = load_garage(resolve(base, garage_path), read);
    if (!garage) {
        return garage.error();
    }
    if (garage::validate(garage.value().catalogue, event.value().reference.tractor,
                         event.value().class_rules)) {
        return detail::make_error(source, "season.garage",
                                  garage_path +
                                      ": some level gives a tractor that is not valid or that "
                                      "the class does not allow");
    }
    SeasonDefinition definition{.event = event.value(),
                                .points = points,
                                .prize_money_eur = *prize_money_eur,
                                .garage = std::move(garage).value(),
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
        auto track = load_track(resolve(base, *track_path), read);
        if (!track) {
            return track.error();
        }
        definition.rounds.push_back({.name = *name, .track = track.value()});
    }
    return definition;
}

}  // namespace

Result<SeasonDefinition> parse_season(std::string_view toml_text, std::string_view source_name,
                                      std::string_view base_directory, const ReadFile& read) {
    try {
        const toml::table root = toml::parse(toml_text, source_name);
        return read_season(root, source_name, base_directory, read);
    } catch (const toml::parse_error& e) {
        return LoadError{detail::describe(e, source_name)};
    }
}

Result<SeasonDefinition> parse_season(std::string_view toml_text, std::string_view source_name,
                                      const std::filesystem::path& base_directory) {
    return parse_season(toml_text, source_name, base_directory.generic_string(), read_from_disk);
}

Result<SeasonDefinition> load_season(std::string_view path, const ReadFile& read) {
    const std::string name{path};
    const std::optional<std::string> text = read(name);
    if (!text) {
        return LoadError{name + ": cannot open file"};
    }
    return parse_season(*text, name, directory_of(name), read);
}

Result<SeasonDefinition> load_season(const std::filesystem::path& path) {
    return load_season(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
