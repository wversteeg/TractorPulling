#pragma once

#include "tp/core/season/season.hpp"
#include "tp/data/event/event_loader.hpp"
#include "tp/data/files.hpp"
#include "tp/data/garage/garage_loader.hpp"
#include "tp/data/result.hpp"
#include "tp/data/track/track_loader.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// How big an event is, for the game to dress the venue: a small village
/// pull, a club event, or a big event with a full grandstand. The
/// simulation does not use it.
enum class EventSize : std::uint8_t { kSmall, kClub, kBig };

/// When an event is pulled, for the game's light: in the day, at sunset, or
/// in the evening under floodlights.
enum class TimeOfDay : std::uint8_t { kDay, kSunset, kEvening };

/// The names season files use for them ("small", "club", "big"; "day",
/// "sunset", "evening").
[[nodiscard]] std::string_view name_of(EventSize size) noexcept;
[[nodiscard]] std::string_view name_of(TimeOfDay time) noexcept;

/// One event of a season: where it is pulled, and how big and when.
struct SeasonRound {
    std::string name;
    Track track;
    EventSize size = EventSize::kClub;
    TimeOfDay time_of_day = TimeOfDay::kDay;
};

/// Everything a season file describes (see data/seasons/): the class's
/// event (field, rules, sled), the points table, the prize money and the
/// rounds, and the class's garage. The same field pulls every round.
struct SeasonDefinition {
    EventDefinition event;
    season::Points points;
    /// What each event pays by place, 1st first [EUR]; garage::prize_eur.
    std::vector<std::int64_t> prize_money_eur;
    /// Checked against the event's tractor and class (garage::validate).
    GarageDefinition garage;
    std::vector<SeasonRound> rounds;
    std::uint64_t seed = 0;
};

/// Loads and validates a season; paths are relative to the season file.
[[nodiscard]] Result<SeasonDefinition> load_season(const std::filesystem::path& path);

/// As load_season, reading this file and the files it names through `read`
/// (see files.hpp).
[[nodiscard]] Result<SeasonDefinition> load_season(std::string_view path, const ReadFile& read);

/// As load_season, from TOML text.
[[nodiscard]] Result<SeasonDefinition> parse_season(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    const std::filesystem::path& base_directory);

/// As parse_season, reading the files it names through `read`, relative to
/// `base_directory` ('/'-separated, see files.hpp).
[[nodiscard]] Result<SeasonDefinition> parse_season(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    std::string_view base_directory,
                                                    const ReadFile& read);

}  // namespace tp::data
