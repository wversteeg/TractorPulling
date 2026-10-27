#pragma once

#include "tp/core/field/live_season.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/session/session.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/season/season_save.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace tp::godot_binding {

class TractorPullRun;

/// Which of the player's pulls a run was set up for: the season object,
/// the event in it and the log's size then. A run is recorded only for the
/// pull it was set up for.
struct PullKey {
    std::uint64_t season_id = 0;
    int event = 0;
    std::int64_t log_size = 0;

    bool operator==(const PullKey&) const = default;
};

/// A season as the game plays it, for GDScript: field::LiveSeason behind
/// Godot types. The player (puller 0, "You") and the season's named rivals
/// pull its events in turn. The scene advances the rivals' work in slices,
/// runs the player's pulls in a TractorPullRun (setup_event_pull) and hands
/// their results and decisions back here. Distances are SI.
class TractorPullEvent : public godot::RefCounted {
    GDCLASS(TractorPullEvent, godot::RefCounted)

public:
    /// Loads a season (res:// paths work) and draws its field for `seed`, a
    /// new season. The player's career (money and upgrades) carries over
    /// from the season before; the first season starts one (the garage's
    /// start money, nothing bought). Returns "" or what is wrong, naming the
    /// file and field.
    godot::String start_season(const godot::String& season_path, std::int64_t seed);
    /// As start_season(), with a new career: the garage's start money and
    /// nothing bought, whatever the career so far.
    godot::String start_career(const godot::String& season_path, std::int64_t seed);
    /// The season so far as save text (data::SeasonSave): the season file,
    /// its seed, the player's tuning and career and every event scored. An event being
    /// pulled is not in it: picked up again, it starts from its beginning.
    /// "" before a season.
    [[nodiscard]] godot::String get_save() const;
    /// Picks up a season saved by get_save() for `season_path`: the season
    /// drawn again for the saved seed, each saved event scored as it was
    /// (field::LiveSeason::add_pulled_event), and the tuning and career
    /// set (a save from before the garage keeps the career); the next
    /// event is the one after them. Returns "" or what is wrong (a broken
    /// save, another season's, or events that no longer fit it), leaving no
    /// season; the save's career is kept even then, for the new season that
    /// follows, as it outlives seasons.
    godot::String resume_season(const godot::String& season_path, const godot::String& save);
    /// {events, scored, index, over, names}: the season's event count, the
    /// events scored, the index of the next event to start, whether all are
    /// scored, and every puller's name (the player's first). {} before a
    /// season.
    [[nodiscard]] godot::Dictionary get_season() const;

    /// Starts the season's next event; "" or what is wrong.
    godot::String start_event();
    /// {name, index, count, pullers, full_pull_m, turn, pull_kind,
    /// pull_off_round} of the event being pulled: `turn` is "rivals",
    /// "player_pull", "player_decision", "finished" or "failed";
    /// `pull_kind` ("test", "competition", "repull", "pull_off") and
    /// `pull_off_round` the player's pull, while it is due. {} before one.
    [[nodiscard]] godot::Dictionary get_event() const;

    /// Runs the rivals' work for at most `max_steps` 1 ms steps; the steps
    /// run (0 while the player is due).
    std::int64_t advance(std::int64_t max_steps);

    /// The event's pulls and decisions from `from` on, oldest first, each
    /// {puller, name, kind, pull_off_round, reached_m, disqualified,
    /// full_pull, gear, change, steps, full_pull_m} for a pull (`full_pull`
    /// judged against the line in force before the pull, `full_pull_m` the
    /// line after it: a test pull past 100 m that moves it to 110 m is a full
    /// pull; `change` the sled operator's: "none", "line_extended",
    /// "lighter", "heavier"; `steps` how many), or {deciding, name, keep}
    /// for a test puller's keep-or-re-pull.
    [[nodiscard]] godot::Array get_log(std::int64_t from) const;
    [[nodiscard]] std::int64_t get_log_size() const;
    /// The event's standings so far, best first: {place, puller, name,
    /// distance_m, full_pull, disqualified, pull_off_rounds,
    /// pull_off_distance_m}.
    [[nodiscard]] godot::Array get_standings() const;

    /// Records the player's pull `run` has finished (it must be the pull
    /// setup_event_pull() set up): the operator's verdict as a log entry
    /// (get_log()), or {} if no pull of the player's is due or the run is
    /// not finished.
    godot::Dictionary record_pull(const godot::Ref<TractorPullRun>& run);
    /// What a sensible test puller would do, while the player decides.
    [[nodiscard]] bool get_suggested_keep() const;
    /// The player's keep (true) or re-pull (false), while it is due.
    void decide(bool keep);

    /// Scores the finished event in the championship and pays the player
    /// their prize (get_event_prize()); false if it is not finished or
    /// already scored.
    bool score_event();
    /// What the event being pulled pays the player for their place so far
    /// [EUR] (garage::prize_eur); 0 before an event.
    [[nodiscard]] std::int64_t get_event_prize() const;

    /// The garage: {money_eur, can_buy, fully_upgraded, upgrades}, each
    /// upgrade {name, level, levels, label, next_label, next_cost_eur} (the
    /// next ones "" and 0 at the top level). `can_buy` is false while the
    /// player has pulled in the event being pulled: a tractor changes
    /// between events, not during one. {} before a season.
    [[nodiscard]] godot::Dictionary get_garage() const;
    /// Buys the next level of upgrade `index`; "" or why not ("top level",
    /// "not enough money", "not during an event", "no such upgrade").
    godot::String buy_upgrade(std::int64_t index);
    /// The championship so far, best first: {place, puller, name, points,
    /// wins}.
    [[nodiscard]] godot::Array get_season_standings() const;

    /// The player's tractor as they set it before a pull (vehicle::Tuning),
    /// kept for the season: {ballast, ballast_position, rear_share,
    /// pressure, pressure_bar, min_bar, max_bar}. `ballast` and `pressure`
    /// say whether the tractor has weights to move and rear tires whose
    /// pressure can be set; `ballast_position` runs from 0 (rearmost) to 1
    /// (foremost), and `rear_share` is the share of the weight then on the
    /// rear axle at rest (0 to 1). {} before a season.
    [[nodiscard]] godot::Dictionary get_tuning() const;
    /// Sets the player's tuning, each held to its range (a value that is not
    /// finite changes nothing); their next pulls and practice pulls are on
    /// the tuned tractor.
    void set_tuning(double ballast_position, double pressure_bar);

    /// The player's next pull, for TractorPullRun: nothing unless one is
    /// due.
    [[nodiscard]] std::optional<session::Setup> player_setup() const;
    /// A practice pull at the event: the player's tractor on the event's
    /// sled as it is now; nothing before an event.
    [[nodiscard]] std::optional<session::Setup> practice_setup() const;
    /// The player's pull due now, for setup_event_pull() to remember.
    [[nodiscard]] PullKey pull_key() const;

protected:
    static void _bind_methods();

private:
    [[nodiscard]] godot::String name_of(int puller) const;
    /// `line_before_m`: the full-pull line in force before the pull.
    [[nodiscard]] godot::Dictionary entry(const field::LogEntry& entry, double line_before_m) const;
    /// The full-pull line each event starts with.
    [[nodiscard]] double first_line_m() const;
    /// The player's tractor as their career has it (garage::equipped),
    /// before the tuning.
    [[nodiscard]] vehicle::Params player_tractor() const;

    std::optional<data::SeasonDefinition> definition_;
    /// The season file and seed start_season() was given, for get_save().
    std::string season_path_;
    std::uint64_t seed_ = 0;
    std::vector<std::string> names_;
    vehicle::Tuning tuning_;
    /// Kept from season to season; set by the first start_season().
    std::optional<garage::Career> career_;
    // Large plain data: on the heap.
    std::unique_ptr<field::LiveSeason> season_;
};

}  // namespace tp::godot_binding
