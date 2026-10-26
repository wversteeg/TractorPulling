#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/season/season.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::field {

/// The seed of a season's event `number` (0 the first): it draws the
/// event's running order, and one more than it the event's gears and
/// pulls. The tools' season and the game's draw the same.
[[nodiscard]] std::uint64_t event_seed(std::uint64_t season_seed, int number) noexcept;

/// A season as the game plays it: a field drawn for the season, in which
/// the player is puller kPlayer with the class's reference tractor and
/// driver, the events in turn as LiveEvents (each with its own draws, as
/// the tools' season has them), and the championship.
///
/// A season can be saved between events and picked up again: each scored
/// event's standings stay (event_standings()), and add_pulled_event() scores
/// them in a new LiveSeason drawn for the same seed, which then goes on as
/// the first would have: the field and every event's draws come from the
/// seed alone.
///
/// Plain data, no allocation. Large (about 145 KB, a LiveEvent inside):
/// create() returns it on the stack, so keep the long-lived copy on the
/// heap.
class LiveSeason {
public:
    static constexpr int kPlayer = 0;

    /// `pullers` pullers (the player and at least one rival) for `events`
    /// events; nothing if the field, the points or the rules are refused.
    [[nodiscard]] static std::optional<LiveSeason> create(
        const event::Rules& rules, const Puller& reference, const Variation& variation,
        const rules::ClassRules& class_rules, const PullToPull& pull_to_pull,
        const season::Points& points, int pullers, int events, std::uint64_t seed) noexcept;

    /// Starts the next event, at `venue`: false once the season is over, while
    /// an event is unscored, or if the event is refused.
    bool start_event(const Venue& venue) noexcept;
    /// The event being pulled, from start_event() until the next one.
    [[nodiscard]] LiveEvent* event() noexcept { return event_ ? &*event_ : nullptr; }
    /// Scores the event once it is finished, in the championship; false if
    /// there is none to score.
    bool score_event() noexcept;
    /// Scores the next event as pulled earlier, from its standings (a saved
    /// season's: each puller once, places 1 to the field's size), without
    /// pulling it. Only the places and
    /// disqualifications count, as in score_event(). False while an event is
    /// unscored, once the season is over, or for standings that are not the
    /// field's.
    bool add_pulled_event(std::span<const event::Standing> standings) noexcept;
    /// The final standings of scored event `number` (0 the first), best
    /// first; empty for an event not scored. For an event added with
    /// add_pulled_event() only the puller, place and disqualification are
    /// set: its distances read 0.
    [[nodiscard]] std::span<const event::Standing> event_standings(int number) const noexcept;

    /// The events scored so far, and in the season.
    [[nodiscard]] int events_scored() const noexcept { return championship_.events(); }
    [[nodiscard]] int event_count() const noexcept { return events_; }
    /// The events started: the one being pulled is the last of them.
    [[nodiscard]] int events_started() const noexcept { return started_; }
    [[nodiscard]] bool over() const noexcept { return events_scored() >= events_; }

    [[nodiscard]] int puller_count() const noexcept { return field_.count; }
    [[nodiscard]] const Puller& player() const noexcept {
        return field_.pullers[static_cast<std::size_t>(kPlayer)];
    }
    /// The championship so far, best first.
    [[nodiscard]] std::span<const season::Standing> standings() noexcept {
        return championship_.standings();
    }

private:
    LiveSeason(const event::Rules& rules, const Variation& variation,
               const PullToPull& pull_to_pull, const Field& field,
               const season::Championship& championship, int events, std::uint64_t seed) noexcept;

    event::Rules rules_;
    Variation variation_;
    PullToPull pull_to_pull_;
    Field field_;
    season::Championship championship_;
    int events_ = 0;
    std::uint64_t seed_ = 0;
    /// Events started; the one being pulled is started_ - 1.
    int started_ = 0;
    bool scored_ = true;
    /// Each scored event's standings, for event_standings().
    std::array<std::array<event::Standing, event::kMaxPullers>, season::kMaxEvents> results_{};
    std::optional<LiveEvent> event_;
};

}  // namespace tp::field
