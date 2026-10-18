#include "tp/core/season/season.hpp"

#include <algorithm>

namespace tp::season {

std::optional<ParamError> validate(const Points& points) noexcept {
    if (points.places < 1 || points.places > event::kMaxPullers) {
        return ParamError::kPlacesOutOfRange;
    }
    for (int i = 0; i < points.places; ++i) {
        const int here = points.by_place[static_cast<std::size_t>(i)];
        if (here < 0 || (i > 0 && here > points.by_place[static_cast<std::size_t>(i) - 1])) {
            return ParamError::kPointsOutOfOrder;
        }
    }
    return std::nullopt;
}

int points_for(const Points& points, int place) noexcept {
    if (place < 1 || place > points.places) {
        return 0;
    }
    return points.by_place[static_cast<std::size_t>(place) - 1];
}

std::optional<Championship> Championship::create(const Points& points, int pullers) noexcept {
    if (validate(points) || pullers < 1 || pullers > event::kMaxPullers) {
        return std::nullopt;
    }
    return Championship{points, pullers};
}

Championship::Championship(const Points& points, int pullers) noexcept
    : points_(points), pullers_(pullers) {
    for (int i = 0; i < pullers; ++i) {
        totals_[static_cast<std::size_t>(i)].puller = i;
    }
}

bool Championship::add(std::span<const event::Standing> event_standings) noexcept {
    if (events_ >= kMaxEvents ||
        !std::ranges::all_of(event_standings, [&](const event::Standing& s) {
            return s.puller >= 0 && s.puller < pullers_;
        })) {
        return false;
    }
    for (const event::Standing& s : event_standings) {
        Standing& total = totals_[static_cast<std::size_t>(s.puller)];
        const int scored = points_for(points_, s.place);
        total.points += scored;
        total.event_points[static_cast<std::size_t>(events_)] = scored;
        total.wins += s.place == 1 ? 1 : 0;
    }
    ++events_;
    return true;
}

std::span<const Standing> Championship::standings() noexcept {
    const std::span<Standing> list{ranked_.data(), static_cast<std::size_t>(pullers_)};
    std::copy_n(totals_.begin(), pullers_, list.begin());
    const auto ahead = [](const Standing& a, const Standing& b) {
        return a.points != b.points ? a.points > b.points : a.wins > b.wins;
    };
    // Equal pullers keep their field order, the same on every platform.
    std::ranges::sort(list, [&](const Standing& a, const Standing& b) {
        if (ahead(a, b) || ahead(b, a)) {
            return ahead(a, b);
        }
        return a.puller < b.puller;
    });
    for (std::size_t i = 0; i < list.size(); ++i) {
        const bool tied = i > 0 && !ahead(list[i - 1], list[i]);
        list[i].place = tied ? list[i - 1].place : static_cast<int>(i) + 1;
    }
    return list;
}

}  // namespace tp::season
