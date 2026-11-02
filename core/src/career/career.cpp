#include "tp/core/career/career.hpp"

#include "tp/core/event/event.hpp"

namespace tp::career {

std::optional<ParamError> validate(const Ladder& ladder) noexcept {
    if (ladder.classes < 1 || ladder.classes > kMaxClasses) {
        return ParamError::kClassCountOutOfRange;
    }
    // A place no field has would open nothing.
    if (ladder.promotion_place < 1 || ladder.promotion_place > event::kMaxPullers) {
        return ParamError::kPromotionPlaceOutOfRange;
    }
    return std::nullopt;
}

bool valid(const Progress& progress, const Ladder& ladder) noexcept {
    return progress.class_index >= 0 && progress.class_index <= progress.unlocked &&
           progress.unlocked < ladder.classes;
}

bool finish_season(Progress& progress, const Ladder& ladder, int championship_place) noexcept {
    const bool earned = championship_place >= 1 && championship_place <= ladder.promotion_place;
    const int above = progress.class_index + 1;
    if (!earned || above >= ladder.classes || above <= progress.unlocked) {
        return false;
    }
    progress.unlocked = above;
    return true;
}

bool finish_season(Progress& progress, const Ladder& ladder,
                   std::span<const season::Standing> championship, int player) noexcept {
    for (const season::Standing& standing : championship) {
        if (standing.puller == player) {
            return finish_season(progress, ladder, standing.place);
        }
    }
    return false;
}

bool may_move_up(const Progress& progress) noexcept {
    return progress.unlocked > progress.class_index;
}

bool move_up(Progress& progress) noexcept {
    if (!may_move_up(progress)) {
        return false;
    }
    ++progress.class_index;
    return true;
}

}  // namespace tp::career
