#include "tp/core/career/career.hpp"

#include "tp/core/event/event.hpp"

namespace tp::career {

std::optional<ParamError> validate(const Ladder& ladder) noexcept {
    if (ladder.rungs < 1 || ladder.rungs > kMaxRungs) {
        return ParamError::kRungCountOutOfRange;
    }
    // A place no field has would open nothing.
    if (ladder.promotion_place < 1 || ladder.promotion_place > event::kMaxPullers) {
        return ParamError::kPromotionPlaceOutOfRange;
    }
    if (ladder.min_seasons < 1 || ladder.min_seasons > kMaxMinSeasons) {
        return ParamError::kMinSeasonsOutOfRange;
    }
    return std::nullopt;
}

bool valid(const Progress& progress, const Ladder& ladder) noexcept {
    return progress.rung >= 0 && progress.rung <= progress.unlocked &&
           progress.unlocked < ladder.rungs && progress.seasons >= 0;
}

std::optional<Progress> starting_at(const Ladder& ladder, int rung) noexcept {
    const Progress start{.rung = rung, .unlocked = rung, .seasons = 0, .title = false};
    if (!valid(start, ladder)) {
        return std::nullopt;
    }
    return start;
}

bool finish_season(Progress& progress, const Ladder& ladder, int championship_place) noexcept {
    const bool earned = championship_place >= 1 && championship_place <= ladder.promotion_place;
    ++progress.seasons;
    progress.title = progress.title || earned;
    const int above = progress.rung + 1;
    if (!progress.title || progress.seasons < ladder.min_seasons || above >= ladder.rungs ||
        above <= progress.unlocked) {
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
    return progress.unlocked > progress.rung;
}

bool move_up(Progress& progress) noexcept {
    if (!may_move_up(progress)) {
        return false;
    }
    ++progress.rung;
    progress.seasons = 0;
    progress.title = false;
    return true;
}

}  // namespace tp::career
