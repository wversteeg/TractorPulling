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

/// The save format's version. Older ones are read too: version 1 (before
/// the garage) as a career that has not begun, version 2 (before the
/// career's classes) as a career on its first rung, version 3 (before the
/// levels) at the first level of its classes, version 4 (before the
/// garage's choices) with every choice at its first alternative, version 5
/// (before each class kept its own tractor) with no other class's tractor,
/// version 6 (before the dealers) with each tractor as the dealer's first
/// model in its first paint (SavedLook). Any other is refused.
inline constexpr std::int64_t kSeasonSaveVersion = 7;

/// An upgrade's level, by the garage's name for it (data::UpgradeText), so
/// that a save outlives a garage file whose order changes.
struct SavedUpgrade {
    std::string name;
    int level = 0;
};

/// A garage choice (data::ChoiceText), by the names of its alternatives: the
/// ones bought (not the first, which comes with the tractor) and the one
/// fitted.
struct SavedChoice {
    std::string name;
    std::string fitted;
    std::vector<std::string> owned;
};

/// The model a tractor was bought as and its paint, by the dealer's names
/// (data::ModelText, data::PaintText); a model of "" is not bought yet.
struct SavedLook {
    std::string model;
    std::string paint;
};

/// A tractor the player has in another class than the one they pull in
/// now, kept for when they go back to it: its upgrades and choices, by
/// name (as SavedCareer's).
struct SavedTractor {
    std::string class_name;
    std::vector<SavedUpgrade> upgrades;
    std::vector<SavedChoice> choices;
    /// None in a save from before version 7: the tractor the player had
    /// before there were dealers.
    std::optional<SavedLook> look;
};

/// The player's career: their money, what they have bought, and how far up
/// the career's ladder (data::CareerRung) they are, by the classes' and
/// levels' names so that a save outlives a career file whose order
/// changes. It carries from season to season.
struct SavedCareer {
    std::int64_t money_eur = 0;
    std::vector<SavedUpgrade> upgrades;
    /// None in a save from before version 5.
    std::vector<SavedChoice> choices;
    /// The class and level the player pulls at; "" for the career's first
    /// class (a version 2 save, or a season played without a career file)
    /// or the class's first level (a version 3 save).
    std::string class_name;
    std::string level_name;
    /// The highest class and level a season has opened, "" as above.
    std::string unlocked_class;
    std::string unlocked_level;
    /// The seasons finished at the player's level, and whether one was won
    /// (career::Progress); none in a save from before version 4.
    int seasons_on_level = 0;
    bool title_on_level = false;
    /// The player's tractors in the other classes they have pulled in (each
    /// class its own tractor, the money one wallet); none before version 6.
    std::vector<SavedTractor> other_tractors;
    /// The tractor's model and paint; none before version 7, as
    /// SavedTractor's.
    std::optional<SavedLook> look;
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
/// (version 3 on) a career without its class and the class unlocked, and
/// (version 4 on) without their levels, its seasons there (0 or more) or
/// whether one was won, (version 5 on) a choice without a name, the
/// alternative fitted or the list of those owned, and (version 7) a
/// tractor with its model but not its paint, or the other way round (with
/// neither it is one from before the dealers, as in a version 6 save).
[[nodiscard]] Result<SeasonSave> parse_season_save(std::string_view toml_text,
                                                   std::string_view source_name);

}  // namespace tp::data
