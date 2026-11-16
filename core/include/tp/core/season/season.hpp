#pragma once

#include "tp/core/event/event.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::season {

inline constexpr int kMaxEvents = 24;

/// Points per place in an event, best first; places beyond the list score
/// nothing. Equal places score equal points.
struct Points {
    std::array<int, event::kMaxPullers> by_place{};
    int places = 0;
};

enum class ParamError : std::uint8_t {
    kPlacesOutOfRange,
    /// Negative, or more points for a lower place.
    kPointsOutOfOrder,
};

[[nodiscard]] std::optional<ParamError> validate(const Points& points) noexcept;

[[nodiscard]] int points_for(const Points& points, int place) noexcept;

/// The points a line of an event's standings scores: its place's, nothing
/// for a disqualified pull, whatever place it ends in, or a guest. In an
/// event with guests a competitor scores by their place among the
/// competitors: event_points().
[[nodiscard]] int points_scored(const Points& points, const event::Standing& standing) noexcept;

/// A competitor's place among the competitors: their place in
/// `event_standings` less the guests ahead of them (a guest who wins
/// leaves the best competitor first); 0 for a guest.
[[nodiscard]] int competition_place(std::span<const event::Standing> event_standings,
                                    const event::Standing& standing) noexcept;

/// What each line of an event's standings scores, in the same order: a
/// competitor by their competition_place(), nothing for a disqualified
/// pull or a guest.
[[nodiscard]] std::array<int, event::kMaxPullers> event_points(
    const Points& points, std::span<const event::Standing> event_standings) noexcept;

/// A puller's place in the championship.
struct Standing {
    int puller = 0;
    /// 1 is the champion; equal points and wins share a place.
    int place = 0;
    int points = 0;
    int wins = 0;
    std::array<int, kMaxEvents> event_points{};
};

/// A season's championship: each event's standings score points by place
/// among the competitors (event_points), a disqualified pull none and a
/// guest none, outside the championship; the most points win, and more
/// event wins (first among the competitors) break a tie.
class Championship {
public:
    [[nodiscard]] static std::optional<Championship> create(const Points& points,
                                                            int pullers) noexcept;

    /// Scores an event. False once the season is full or for a competitor
    /// outside the field; guests' lines are passed over.
    bool add(std::span<const event::Standing> event_standings) noexcept;

    [[nodiscard]] int events() const noexcept { return events_; }
    [[nodiscard]] int pullers() const noexcept { return pullers_; }
    /// The points per place it scores by.
    [[nodiscard]] const Points& points() const noexcept { return points_; }
    /// The championship so far, best first.
    [[nodiscard]] std::span<const Standing> standings() noexcept;

private:
    Championship(const Points& points, int pullers) noexcept;

    Points points_;
    int pullers_ = 0;
    int events_ = 0;
    std::array<Standing, event::kMaxPullers> totals_{};
    std::array<Standing, event::kMaxPullers> ranked_{};
};

}  // namespace tp::season
