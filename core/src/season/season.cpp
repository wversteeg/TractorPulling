#include "tp/core/season/season.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

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

int points_scored(const Points& points, const event::Standing& standing) noexcept {
    return standing.disqualified || standing.guest ? 0 : points_for(points, standing.place);
}

int competition_place(std::span<const event::Standing> event_standings,
                      const event::Standing& standing) noexcept {
    if (standing.guest) {
        return 0;
    }
    const auto guests_ahead = std::ranges::count_if(event_standings, [&](const event::Standing& s) {
        return s.guest && s.place < standing.place;
    });
    return standing.place - static_cast<int>(guests_ahead);
}

std::array<int, event::kMaxPullers> event_points(
    const Points& points, std::span<const event::Standing> event_standings) noexcept {
    std::array<int, event::kMaxPullers> out{};
    for (std::size_t i = 0; i < event_standings.size() && i < out.size(); ++i) {
        const event::Standing& s = event_standings[i];
        out[i] = s.disqualified || s.guest
                     ? 0
                     : points_for(points, competition_place(event_standings, s));
    }
    return out;
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
    if (events_ >= kMaxEvents || std::cmp_greater(event_standings.size(), event::kMaxPullers) ||
        !std::ranges::all_of(event_standings, [&](const event::Standing& s) {
            return s.guest || (s.puller >= 0 && s.puller < pullers_);
        })) {
        return false;
    }
    const std::array<int, event::kMaxPullers> scored = event_points(points_, event_standings);
    for (std::size_t i = 0; i < event_standings.size(); ++i) {
        const event::Standing& s = event_standings[i];
        if (s.guest) {
            continue;
        }
        Standing& total = totals_[static_cast<std::size_t>(s.puller)];
        total.points += scored[i];
        total.event_points[static_cast<std::size_t>(events_)] = scored[i];
        total.wins += competition_place(event_standings, s) == 1 && !s.disqualified ? 1 : 0;
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
