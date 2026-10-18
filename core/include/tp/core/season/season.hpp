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

/// A puller's place in the championship.
struct Standing {
    int puller = 0;
    /// 1 is the champion; equal points and wins share a place.
    int place = 0;
    int points = 0;
    int wins = 0;
    std::array<int, kMaxEvents> event_points{};
};

/// A season's championship: each event's standings score points by place;
/// the most points win, and more event wins break a tie.
class Championship {
public:
    [[nodiscard]] static std::optional<Championship> create(const Points& points,
                                                            int pullers) noexcept;

    /// Scores an event. False once the season is full or for a puller
    /// outside the field.
    bool add(std::span<const event::Standing> event_standings) noexcept;

    [[nodiscard]] int events() const noexcept { return events_; }
    [[nodiscard]] int pullers() const noexcept { return pullers_; }
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
