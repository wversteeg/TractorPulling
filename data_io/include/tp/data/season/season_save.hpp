#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// The save format's version. Version 1 (before the garage) is read too,
/// as a career that has not begun, and version 2 (before the career's
/// classes) as a career in its first class; any other is refused.
inline constexpr std::int64_t kSeasonSaveVersion = 3;

/// An upgrade's level, by the garage's name for it (data::UpgradeText), so
/// that a save outlives a garage file whose order changes.
struct SavedUpgrade {
    std::string name;
    int level = 0;
};

/// The player's career: their money, what they have bought, and how far up
/// the career's classes (data::CareerClass) they are, by the classes' names
/// so that a save outlives a career file whose order changes. It carries
/// from season to season.
struct SavedCareer {
    std::int64_t money_eur = 0;
    std::vector<SavedUpgrade> upgrades;
    /// The class the player pulls in; "" for the career's first (a version
    /// 2 save, or a season played without a career file).
    std::string class_name;
    /// The highest class a season has opened; "" for the career's first.
    std::string unlocked_class;
};

/// A season saved between events, to pick up on the next launch: which
/// season file, the seed its field and draws came from, the player's
/// tuning and career, and each event pulled so far, by its final standings.
/// Starting the same season with the same seed and scoring those events
/// (field::LiveSeason::add_pulled_event) gives the season as it was; an
/// event left halfway is not saved, and starts again from its beginning.
struct SeasonSave {
    /// The season file, as the game names it (e.g. res://data/seasons/...).
    std::string season;
    std::uint64_t seed = 0;
    vehicle::Tuning tuning;
    /// Nothing in a version 1 save: the career starts anew.
    std::optional<SavedCareer> career;
    /// Each event's standings: only the puller, place and disqualification
    /// are kept, which is what the championship scores.
    std::vector<std::vector<event::Standing>> events;
};

/// The save as TOML text. The tuning must be finite (as parse_season_save
/// reads it back).
[[nodiscard]] std::string write_season_save(const SeasonSave& save);

/// A save from TOML text; `source_name` names it in error messages. Refuses
/// another version, a missing field, a tuning that is not a number in range
/// (a ballast position from 0 to 1, a pressure of 0 or more), standings
/// without a puller or a place, (version 2 on) a career without its money
/// (0 or more) or with an upgrade without a name or a level (0 or more),
/// and (version 3) a career without its class and the class unlocked.
[[nodiscard]] Result<SeasonSave> parse_season_save(std::string_view toml_text,
                                                   std::string_view source_name);

}  // namespace tp::data
