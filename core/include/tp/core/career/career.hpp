#pragma once

#include "tp/core/season/season.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tp::career {

/// The most classes a career's ladder holds.
inline constexpr int kMaxClasses = 12;

/// The classes a career climbs, lowest first, and what opens the next one:
/// a season in a class finished at `promotion_place` or better (1: the
/// championship won; a shared place counts). Which class is which (its
/// season, garage and rules) is the data's; the core only counts them.
struct Ladder {
    int classes = 0;
    int promotion_place = 1;
};

enum class ParamError : std::uint8_t {
    kClassCountOutOfRange,
    kPromotionPlaceOutOfRange,
};

[[nodiscard]] std::optional<ParamError> validate(const Ladder& ladder) noexcept;

/// How far a career has climbed: the class the player pulls in, and the
/// highest class a season has opened. A class opened stays open, so a
/// player who stays to defend a title may still move up later. Plain data:
/// it is saved with the career.
struct Progress {
    int class_index = 0;
    int unlocked = 0;
};

/// Whether `progress` fits `ladder`: from the first class up to the one
/// unlocked, which is a class of the ladder.
[[nodiscard]] bool valid(const Progress& progress, const Ladder& ladder) noexcept;

/// Counts a finished season in the progress's class, in which the player
/// finished the championship at `championship_place` (1 the champion): it
/// opens the class above when the place earns it and that class is not open
/// yet. True if it opened a class.
bool finish_season(Progress& progress, const Ladder& ladder, int championship_place) noexcept;

/// As finish_season() for `player`'s place in a finished season's
/// championship (season::Championship::standings()); false if the player
/// is not in it.
bool finish_season(Progress& progress, const Ladder& ladder,
                   std::span<const season::Standing> championship, int player) noexcept;

/// Whether a class above the player's is open.
[[nodiscard]] bool may_move_up(const Progress& progress) noexcept;

/// Moves the player one class up, if may_move_up(); true if it did.
bool move_up(Progress& progress) noexcept;

}  // namespace tp::career
