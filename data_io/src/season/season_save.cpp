#include "tp/data/season/season_save.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

// The seed is a 64-bit unsigned number, which TOML's signed integers cannot
// all hold: it is kept as decimal text.
std::optional<std::uint64_t> parse_seed(std::string_view text) {
    constexpr std::uint64_t kBase = 10;
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / kBase) {
            return std::nullopt;
        }
        value = value * kBase + digit;
    }
    return value;
}

// A TOML basic string: quotes, backslashes and control characters escaped.
std::string toml_string(std::string_view text) {
    constexpr int kHexDigits = 4;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << '"';
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out << '\\' << c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out << "\\u" << std::hex << std::setw(kHexDigits) << std::setfill('0')
                << static_cast<unsigned>(c) << std::dec;
        } else {
            out << c;
        }
    }
    out << '"';
    return out.str();
}

// A (finite) number with every digit it needs to read back as itself.
std::string exact(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    std::string text = out.str();
    // TOML tells floats from integers by a point or an exponent.
    if (text.find_first_of(".eE") == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::optional<event::Standing> read_standing(const toml::node& node) {
    const toml::table* line = node.as_table();
    if (line == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int64_t> puller = line->at_path("puller").value<std::int64_t>();
    const std::optional<std::int64_t> place = line->at_path("place").value<std::int64_t>();
    const std::optional<bool> disqualified = line->at_path("disqualified").value<bool>();
    if (!puller || !place || !disqualified || *puller < 0 || *place < 1 ||
        *puller >= event::kMaxPullers || *place > event::kMaxPullers) {
        return std::nullopt;
    }
    return event::Standing{.puller = static_cast<int>(*puller),
                           .place = static_cast<int>(*place),
                           .disqualified = *disqualified};
}

// Version 2's career into `career`; the error if it is broken.
std::optional<LoadError> read_career(const toml::table& root, std::string_view source,
                                     SavedCareer& career) {
    detail::Reader reader{root, source};
    career.money_eur = reader.integer("career.money_eur");
    if (reader.error()) {
        return reader.error();
    }
    if (career.money_eur < 0) {
        return detail::make_error(source, "career.money_eur", "must be 0 or more");
    }
    const toml::node_view<const toml::node> upgrades = root.at_path("career.upgrades");
    if (!upgrades) {
        return std::nullopt;
    }
    const toml::array* list = upgrades.as_array();
    if (list == nullptr) {
        return detail::make_error(source, "career.upgrades", "must be an array of tables");
    }
    for (const toml::node& node : *list) {
        const toml::table* line = node.as_table();
        const std::optional<std::string> name =
            line != nullptr ? line->at_path("name").value<std::string>() : std::nullopt;
        const std::optional<std::int64_t> level =
            line != nullptr ? line->at_path("level").value<std::int64_t>() : std::nullopt;
        constexpr std::int64_t kMaxLevel = 1000;
        if (!name || !level || *level < 0 || *level > kMaxLevel) {
            return detail::make_error(source, "career.upgrades",
                                      "every upgrade needs a name and a level (0 or more)");
        }
        career.upgrades.push_back({.name = *name, .level = static_cast<int>(*level)});
    }
    return std::nullopt;
}

Result<SeasonSave> read_save(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const std::int64_t version = reader.integer("version");
    SeasonSave save;
    save.season = reader.text("season");
    const std::string seed = reader.text("seed");
    save.tuning.ballast_position = reader.number("tuning.ballast_position");
    save.tuning.rear_tire_pressure_bar = reader.number("tuning.rear_tire_pressure_bar");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    constexpr std::int64_t kBeforeTheGarage = 1;
    if (version != kSeasonSaveVersion && version != kBeforeTheGarage) {
        return detail::make_error(source, "version",
                                  "must be " + std::to_string(kBeforeTheGarage) + " or " +
                                      std::to_string(kSeasonSaveVersion));
    }
    if (version == kSeasonSaveVersion) {
        SavedCareer career;
        if (const std::optional<LoadError> error = read_career(root, source, career)) {
            return *error;
        }
        save.career = std::move(career);
    }
    // The tuning is the tractor's to hold to its ranges; a save only needs
    // to hold numbers that can be tuning at all.
    if (!std::isfinite(save.tuning.ballast_position) || save.tuning.ballast_position < 0.0 ||
        save.tuning.ballast_position > 1.0) {
        return detail::make_error(source, "tuning.ballast_position", "must be from 0 to 1");
    }
    if (!std::isfinite(save.tuning.rear_tire_pressure_bar) ||
        save.tuning.rear_tire_pressure_bar < 0.0) {
        return detail::make_error(source, "tuning.rear_tire_pressure_bar",
                                  "must be a finite number of 0 or more");
    }
    const std::optional<std::uint64_t> parsed = parse_seed(seed);
    if (!parsed) {
        return detail::make_error(source, "seed", "must be a whole number of 0 or more");
    }
    save.seed = *parsed;

    const toml::node_view<const toml::node> events = root.at_path("events");
    if (!events) {
        return save;
    }
    const toml::array* list = events.as_array();
    if (list == nullptr) {
        return detail::make_error(source, "events", "must be an array of tables");
    }
    for (const toml::node& node : *list) {
        const toml::table* event = node.as_table();
        const toml::array* standings =
            event != nullptr ? event->at_path("standings").as_array() : nullptr;
        if (standings == nullptr || standings->empty()) {
            return detail::make_error(source, "events", "every event needs its standings");
        }
        std::vector<event::Standing>& lines = save.events.emplace_back();
        for (const toml::node& line : *standings) {
            const std::optional<event::Standing> standing = read_standing(line);
            if (!standing) {
                return detail::make_error(
                    source, "events.standings",
                    "every line needs a puller (0 or more), a place (1 or more) and disqualified");
            }
            lines.push_back(*standing);
        }
    }
    return save;
}

}  // namespace

std::string write_season_save(const SeasonSave& save) {
    std::string out = "# A saved season (tp::data::SeasonSave): the events pulled so far.\n";
    out += "version = " + std::to_string(kSeasonSaveVersion) + "\n";
    out += "season = " + toml_string(save.season) + "\n";
    out += "seed = \"" + std::to_string(save.seed) + "\"\n";
    out += "\n[tuning]\n";
    out += "ballast_position = " + exact(save.tuning.ballast_position) + "\n";
    out += "rear_tire_pressure_bar = " + exact(save.tuning.rear_tire_pressure_bar) + "\n";
    const SavedCareer career = save.career.value_or(SavedCareer{});
    out += "\n[career]\n";
    out += "money_eur = " + std::to_string(career.money_eur) + "\n";
    out += "upgrades = [\n";
    for (const SavedUpgrade& upgrade : career.upgrades) {
        out += "    { name = " + toml_string(upgrade.name) +
               ", level = " + std::to_string(upgrade.level) + " },\n";
    }
    out += "]\n";
    for (const std::vector<event::Standing>& standings : save.events) {
        out += "\n[[events]]\nstandings = [\n";
        for (const event::Standing& s : standings) {
            out += "    { puller = " + std::to_string(s.puller) +
                   ", place = " + std::to_string(s.place) +
                   ", disqualified = " + (s.disqualified ? "true" : "false") + " },\n";
        }
        out += "]\n";
    }
    return out;
}

Result<SeasonSave> parse_season_save(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<SeasonSave>(toml_text, source_name, read_save);
}

}  // namespace tp::data
