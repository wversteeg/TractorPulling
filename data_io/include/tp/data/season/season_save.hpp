#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// The save format's version; a file of another is refused.
inline constexpr std::int64_t kSeasonSaveVersion = 1;

/// A season saved between events, to pick up on the next launch: which
/// season file, the seed its field and draws came from, the player's
/// tuning and each event pulled so far, by its final standings. Starting
/// the same season with the same seed and scoring those events
/// (field::LiveSeason::add_pulled_event) gives the season as it was; an
/// event left halfway is not saved, and starts again from its beginning.
struct SeasonSave {
    /// The season file, as the game names it (e.g. res://data/seasons/...).
    std::string season;
    std::uint64_t seed = 0;
    vehicle::Tuning tuning;
    /// Each event's standings: only the puller, place and disqualification
    /// are kept, which is what the championship scores.
    std::vector<std::vector<event::Standing>> events;
};

/// The save as TOML text.
[[nodiscard]] std::string write_season_save(const SeasonSave& save);

/// A save from TOML text; `source_name` names it in error messages. Refuses
/// another version, a missing field, and standings without a puller or a
/// place.
[[nodiscard]] Result<SeasonSave> parse_season_save(std::string_view toml_text,
                                                   std::string_view source_name);

}  // namespace tp::data
