#pragma once

#include "tp/core/season/season.hpp"
#include "tp/data/event/event_loader.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"
#include "tp/data/track/track_loader.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// One event of a season: where it is pulled.
struct SeasonRound {
    std::string name;
    Track track;
};

/// Everything a season file describes (see data/seasons/): the class's
/// event (field, rules, sled), the points table and the rounds. The same
/// field pulls every round.
struct SeasonDefinition {
    EventDefinition event;
    season::Points points;
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
