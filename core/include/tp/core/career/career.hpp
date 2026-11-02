#pragma once

#include "tp/core/season/season.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tp::career {

/// The most rungs a career's ladder holds: twelve classes of three levels.
inline constexpr int kMaxRungs = 36;

/// The rungs a career climbs, lowest first, and what opens the next one: a
/// season on a rung finished at `promotion_place` or better (1: the
/// championship won; a shared place counts), once the player has pulled at
/// least `min_seasons` seasons on the rung. A rung is a class at one of its
/// levels (belofte, promo, top); which is which (its season, garage and
/// rules) is the data's, the core only counts them.
struct Ladder {
    int rungs = 0;
    int promotion_place = 1;
    int min_seasons = 1;
};

/// The most seasons a ladder may ask on one rung.
inline constexpr int kMaxMinSeasons = 10;

enum class ParamError : std::uint8_t {
    kRungCountOutOfRange,
    kPromotionPlaceOutOfRange,
    kMinSeasonsOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Ladder& ladder) noexcept;

/// How far a career has climbed: the rung the player pulls on, and the
/// highest rung a season has opened; the seasons finished on the player's
/// rung, and whether one of them was won. A rung opened stays open, so a
/// player who stays to defend a title may still move up later. Plain data:
/// it is saved with the career.
struct Progress {
    int rung = 0;
    int unlocked = 0;
    int seasons = 0;
    bool title = false;
};

/// Whether `progress` fits `ladder`: from the first rung up to the one
/// unlocked, which is a rung of the ladder.
[[nodiscard]] bool valid(const Progress& progress, const Ladder& ladder) noexcept;

/// A career that starts on `rung` with it the highest open, as if the
/// rungs below were climbed (for testing a class without pulling up to it);
/// nothing for a rung the ladder does not have.
[[nodiscard]] std::optional<Progress> starting_at(const Ladder& ladder, int rung) noexcept;

/// Counts a finished season on the progress's rung, in which the player
/// finished the championship at `championship_place` (1 the champion): a
/// title (the place earns it) is kept, and the rung above opens once there
/// is one and the rung has had its `min_seasons`; a title in the first
/// season opens it after the last of them, whatever their places. True if
/// it opened a rung.
bool finish_season(Progress& progress, const Ladder& ladder, int championship_place) noexcept;

/// As finish_season() for `player`'s place in a finished season's
/// championship (season::Championship::standings()); false if the player
/// is not in it.
bool finish_season(Progress& progress, const Ladder& ladder,
                   std::span<const season::Standing> championship, int player) noexcept;

/// Whether a rung above the player's is open.
[[nodiscard]] bool may_move_up(const Progress& progress) noexcept;

/// Moves the player one rung up, if may_move_up(), with no seasons on the
/// new rung yet; true if it did.
bool move_up(Progress& progress) noexcept;

}  // namespace tp::career
