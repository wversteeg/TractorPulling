#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::field {

/// One line of a played event: a pull and where the sled stopped, with the
/// operator's verdict, or a test puller's decision.
struct LogEntry {
    /// A pull; unused for a decision.
    event::Pull pull;
    double reached_m = 0.0;
    /// A tractor tire touched a chalk line: the pull counts 0 m.
    bool disqualified = false;
    event::Outcome outcome;
    /// The gear pulled in.
    int gear = 0;
    /// After the pull: the full-pull line and the sled's setting.
    double full_pull_m = 0.0;
    int knives = 0;
    double pushdown_start_m = 0.0;
    /// The deciding test puller for a decision, else -1.
    int deciding = -1;
    bool keep = false;
};

inline constexpr std::size_t kMaxLog = 160;

/// Where a class's event is pulled.
struct Venue {
    track::Params track;
    /// The class's sled with the day's conditions.
    sled::Params sled;
    /// A pull stops after this at the latest [s].
    double max_duration_s = 0.0;
    /// Between the chalk lines [m].
    double lane_width_m = 0.0;
};

/// A played event: the event at its end and what happened, in order.
struct PlayedEvent {
    event::Event event;
    std::array<LogEntry, kMaxLog> log{};
    std::size_t log_size = 0;
    /// The gear each puller chose for the day.
    std::array<int, event::kMaxPullers> gears{};

    [[nodiscard]] std::span<const LogEntry> entries() const noexcept {
        return {log.data(), log_size};
    }
};

/// Plays one class's event with scripted pullers: each driver guesses a
/// gear for the day's track (between `variation`'s gears), every pull
/// differs a little (`pull_to_pull`), and test pullers decide as
/// suggested. Deterministic for `seed`. Runs whole pulls: for tools,
/// tests and seasons, not inside a frame.
[[nodiscard]] std::optional<PlayedEvent> play_event(const event::Rules& rules, const Venue& venue,
                                                    const Field& field, const Variation& variation,
                                                    const PullToPull& pull_to_pull,
                                                    std::span<const int> running_order,
                                                    std::uint64_t seed) noexcept;

}  // namespace tp::field
