#pragma once

#include "tp/core/sled/sled.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::event {

inline constexpr int kMaxPullers = 32;
inline constexpr int kMaxPullOffRounds = 5;

/// A pulling class's competition rules (after the NTTO's) and the sled
/// operator's rule of thumb. Distances are where the sled stops, measured
/// from the start line [m].
struct Rules {
    /// The full-pull line.
    double full_pull_m = 0.0;
    /// The line moves here when a test pull stops between the two; the sled
    /// then stays as it is.
    double extended_full_pull_m = 0.0;
    /// A pull past the line stops here at the latest.
    double max_distance_m = 0.0;
    /// Places in the pull-off: every full pull, filled up with the best
    /// runs from `qualifying_distance_m` on.
    int pull_off_places = 0;
    double qualifying_distance_m = 0.0;
    /// A test puller who may keep the test pull decides once this many
    /// more pulls are done, and re-pulls after this many more (counting the
    /// test pull as the first, the NTTO's 3rd and 6th pull).
    int decide_after_pulls = 0;
    int repull_after_pulls = 0;
    /// A test pull stopping short of this makes the operator lighten the
    /// sled.
    double sled_target_m = 0.0;
    /// The operator's rule of thumb: what one sled step is worth [m].
    double step_m = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kDistancesOutOfOrder,
    kPlacesOutOfRange,
    kTurnsOutOfOrder,
    kStepNotPositive,
    kSled,
    kRunningOrder,
};

[[nodiscard]] std::optional<ParamError> validate(const Rules& rules) noexcept;

enum class PullKind : std::uint8_t {
    /// The first pull of the event, and the first after every change to the
    /// sled: the sled operator judges the sled by it.
    kTest,
    kCompetition,
    /// A test puller pulling again on the changed sled.
    kRepull,
    kPullOff,
};

/// The next pull: who pulls, and what kind of pull it is.
struct Pull {
    /// Index into the field.
    int puller = 0;
    PullKind kind = PullKind::kCompetition;
    /// 1 for the first pull-off, 0 in the main round.
    int pull_off_round = 0;
};

/// What the sled operator made of a test pull.
enum class SledChange : std::uint8_t {
    kNone,
    /// The test pull stopped between the full-pull line and the extended
    /// line: the line moves, the sled stays.
    kLineExtended,
    kLighter,
    kHeavier,
};

struct Outcome {
    SledChange change = SledChange::kNone;
    /// Sled steps for kLighter and kHeavier.
    int steps = 0;
};

/// A puller's place in the standings. The official distance is capped at
/// the full-pull line.
struct Standing {
    int puller = 0;
    /// 1 is the winner; equal results share a place.
    int place = 0;
    double distance_m = 0.0;
    bool full_pull = false;
    /// Pull-off rounds pulled, and the official distance in the last one.
    int pull_off_rounds = 0;
    double pull_off_distance_m = 0.0;
};

/// One class at an event, pull by pull: the running order, test pulls and
/// the sled operator, test pullers keeping or re-pulling, the full-pull
/// line, qualifying for the pull-off and the pull-offs.
///
/// A front end (or the tools' scripted pullers) asks next() who pulls, runs
/// that pull on sled() until the sled stops or reaches max_distance_m, and
/// hands record() where it stopped. When a test puller's decision is due,
/// next() waits for decide(). Plain data; no allocation.
class Event {
public:
    /// `running_order` lists the field's pullers (0 to count - 1), each
    /// once, in the order they pull.
    [[nodiscard]] static std::optional<Event> create(const Rules& rules,
                                                     const sled::Params& class_sled,
                                                     std::span<const int> running_order) noexcept;

    /// The next pull, or nothing when the event is over or a decision is
    /// due.
    [[nodiscard]] std::optional<Pull> next() const noexcept;

    /// The sled and full-pull line for the next pull.
    [[nodiscard]] const sled::Params& sled() const noexcept { return sled_; }
    [[nodiscard]] double full_pull_m() const noexcept { return line_m_; }

    /// Records where the sled stopped in the pull next() named, and what the
    /// operator does about it if it was a test pull.
    Outcome record(double reached_m) noexcept;

    /// The test puller whose keep-or-re-pull decision is due.
    [[nodiscard]] std::optional<int> decision_due() const noexcept;
    /// What a sensible puller would choose: keep a full pull, or a test pull
    /// no one has beaten since.
    [[nodiscard]] bool suggested_keep() const noexcept;
    void decide(bool keep) noexcept;

    [[nodiscard]] bool finished() const noexcept;

    [[nodiscard]] int puller_count() const noexcept { return count_; }
    /// The standings so far, best first.
    [[nodiscard]] std::span<const Standing> standings() noexcept;

private:
    struct Entry {
        bool pulled = false;
        double reached_m = 0.0;
        int pull_off_rounds = 0;
        double pull_off_reached_m = 0.0;
        bool tested = false;
        double test_reached_m = 0.0;
        bool may_keep = false;
        int test_pull_number = 0;
        bool decision_pending = false;
    };

    static constexpr std::size_t kMaxQueue = std::size_t{3} * kMaxPullers;

    Event(const Rules& rules, const sled::Params& class_sled,
          std::span<const int> running_order) noexcept;

    void insert(std::size_t position, const Pull& pull) noexcept;
    void schedule_repull(int puller) noexcept;
    Outcome judge_test(int puller, double reached_m) noexcept;
    /// After a pull or a decision: a decision now due, or the next round.
    void advance() noexcept;
    void start_pull_off() noexcept;
    [[nodiscard]] double official(double reached_m) const noexcept;

    Rules rules_;
    sled::Params sled_;
    double line_m_ = 0.0;
    int count_ = 0;
    std::array<int, kMaxPullers> order_{};
    std::array<Entry, kMaxPullers> entries_{};
    std::array<Pull, kMaxQueue> queue_{};
    std::size_t queue_size_ = 0;
    std::size_t head_ = 0;
    /// Pulls done so far.
    int completed_ = 0;
    int pull_off_round_ = 0;
    std::optional<int> pending_decision_;
    std::array<Standing, kMaxPullers> standings_{};
};

/// The field's pullers in a random order for `seed`.
[[nodiscard]] std::array<int, kMaxPullers> running_order(int count, std::uint64_t seed) noexcept;

}  // namespace tp::event
