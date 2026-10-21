#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/math/random.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::field {

/// One class's event as a game plays it: scripted pullers and one player.
/// The program is play_event's, draw for draw (the same seed gives the
/// rivals the same gears, timing and grip), except that the player's pulls
/// and keep-or-re-pull decisions come from outside.
///
/// The rivals' work (each driver trying the gears to pick one for the day,
/// then their pulls) runs in slices of at most a given number of 1 ms
/// steps, so a game spreads it over its frames: call advance() while
/// turn() is kRivals. When the player is due, turn() says so; the game runs
/// the player's pull on sled() by pull_rules() and hands the result to
/// record_player(), or their decision to decide_player().
///
/// Plain data, no allocation; large (every puller and a pull in progress),
/// so keep it on the heap.
class LiveEvent {
public:
    enum class Turn : std::uint8_t {
        /// Scripted work to do: call advance().
        kRivals,
        /// The player pulls next (see pending_pull()).
        kPlayerPull,
        /// The player, a test puller, decides to keep their test pull or
        /// pull again.
        kPlayerDecision,
        kFinished,
        /// A pull could not be set up (an inconsistent tractor or sled).
        kFailed,
    };

    /// The field (its pullers in `field`) pulls in `running_order`; puller
    /// `player` is the player, whose tractor `field` holds. Nothing if
    /// play_event would refuse these.
    [[nodiscard]] static std::optional<LiveEvent> create(const event::Rules& rules,
                                                         const Venue& venue, const Field& field,
                                                         const Variation& variation,
                                                         const PullToPull& pull_to_pull,
                                                         std::span<const int> running_order,
                                                         int player, std::uint64_t seed) noexcept;

    [[nodiscard]] Turn turn() const noexcept;

    /// Runs the rivals' work for at most `max_steps` simulation steps,
    /// stopping early when the player is due or the event is over; returns
    /// the steps run. A pull may be cut across calls.
    std::int64_t advance(std::int64_t max_steps) noexcept;

    /// The player's pull, while turn() is kPlayerPull.
    [[nodiscard]] std::optional<event::Pull> pending_pull() const noexcept;
    /// The sled and run rules for the next pull: the event's sled as it is
    /// now, and a run that goes on past the line until the sled stops.
    [[nodiscard]] const sled::Params& sled() const noexcept { return event_.sled(); }
    /// Where the event is pulled: the track's ground for the player's run.
    [[nodiscard]] const Venue& venue() const noexcept { return venue_; }
    [[nodiscard]] run::Rules pull_rules() const noexcept;

    /// Records the player's pull (`gear` the gear they pulled in): out of
    /// bounds counts 0 m, as for every puller. Only while turn() is
    /// kPlayerPull.
    event::Outcome record_player(const run::Result& result, int gear) noexcept;
    /// The player's keep-or-re-pull, while turn() is kPlayerDecision;
    /// suggested_keep() is what a sensible puller would choose.
    [[nodiscard]] bool suggested_keep() const noexcept { return event_.suggested_keep(); }
    void decide_player(bool keep) noexcept;

    [[nodiscard]] int player() const noexcept { return player_; }
    [[nodiscard]] int puller_count() const noexcept { return field_.count; }
    /// The gear each puller chose for the day (the player's is the field's
    /// guess for them, unused).
    [[nodiscard]] int gear(int puller) const noexcept;
    /// Every pull and decision so far, in order.
    [[nodiscard]] std::span<const LogEntry> log() const noexcept {
        return {log_.data(), log_size_};
    }
    /// The standings so far, best first.
    [[nodiscard]] std::span<const event::Standing> standings() noexcept {
        return event_.standings();
    }
    [[nodiscard]] double full_pull_m() const noexcept { return event_.full_pull_m(); }

private:
    /// A scripted pull in progress: a gear trial or a pull at the event.
    struct Job {
        pull::Model model;
        run::State state;
        driver::Plan plan;
        run::Rules rules;
    };

    LiveEvent(const event::Rules& rules, const Venue& venue, const Field& field,
              const Variation& variation, const PullToPull& pull_to_pull, const event::Event& event,
              int player, std::uint64_t seed) noexcept;

    /// Starts `job_` for `puller` on `sled`; false if the model is invalid.
    bool start_job(const Puller& puller, const sled::Params& sled,
                   const run::Rules& rules) noexcept;
    /// Starts the next piece of the rivals' work: a gear trial or a pull
    /// in `job_`, or what needs no steps (the player's gear guess, a
    /// rival's decision).
    void start_next() noexcept;
    /// Steps `job` for at most `max_steps`; the steps run.
    static std::int64_t step_job(Job& job, std::int64_t max_steps) noexcept;
    /// After a gear trial: the next gear, or the puller's guess.
    void finish_trial(double reached_m) noexcept;
    /// The trial puller's gear for the day from the best, then the next
    /// puller, or the event once all have one.
    void choose(int best) noexcept;
    /// After a rival's pull at the event.
    void finish_pull(const run::Result& result) noexcept;
    /// Settles what needs no steps: the rivals' decisions.
    void settle() noexcept;
    LogEntry* next_entry() noexcept;

    event::Rules rules_;
    Venue venue_;
    Field field_;
    Variation variation_;
    PullToPull pull_to_pull_;
    event::Event event_;
    int player_ = 0;
    math::SplitMix64 guesses_;
    math::SplitMix64 pulls_;
    /// Choosing gears: the puller and gear being tried, and the best so far.
    bool choosing_ = true;
    int trial_puller_ = 0;
    int trial_gear_ = 0;
    int best_gear_ = 0;
    double best_m_ = -1.0;
    std::array<int, event::kMaxPullers> gears_{};
    /// The rival's pull in progress at the event, and the puller as they are
    /// on it.
    std::optional<Job> job_;
    std::optional<event::Pull> job_pull_;
    int job_gear_ = 0;
    bool failed_ = false;
    std::array<LogEntry, kMaxLog> log_{};
    std::size_t log_size_ = 0;
};

}  // namespace tp::field
