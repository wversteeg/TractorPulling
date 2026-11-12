#pragma once

#include "tp/core/career/career.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/session/session.hpp"
#include "tp/core/vehicle/tuning.hpp"
#include "tp/data/career/career_loader.hpp"
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
    /// Loads a career (data/careers/: its classes, each at its levels, each
    /// level a rung with its season) for the methods below, on its first
    /// rung; no season is started. Returns "" or what is wrong, naming the
    /// file and field.
    godot::String load_career(const godot::String& career_path);
    /// {name, rungs, rung, class_name, level_name, unlocked, next_class,
    /// next_level, next_is_new_class, can_move_up, seasons, min_seasons,
    /// title} of the loaded career:
    /// every rung's "class · level" lowest first, the index of the player's
    /// rung and its class and level, the index of the highest rung open,
    /// the rung above's class and level ("" at the top) and whether it is
    /// another class, whether move_up() would move the player now, the
    /// seasons finished on the player's rung, the ladder's minimum there,
    /// and whether one was won. {} without a career.
    [[nodiscard]] godot::Dictionary get_career() const;
    /// A new career: the first rung, the garage's start money and nothing
    /// bought, and its season drawn for `seed`. "" or what is wrong; the
    /// career so far stays then.
    godot::String new_career(std::int64_t seed);
    /// As new_career(), starting on rung `rung` with it the highest open
    /// (career::starting_at), for testing a class without climbing to it.
    godot::String new_career_at(std::int64_t rung, std::int64_t seed);
    /// A new season on the player's rung, drawn for `seed`; money and
    /// upgrades carry over. "" or what is wrong.
    godot::String new_season(std::int64_t seed);
    /// As resume_season(), for the loaded career: the save's class and
    /// level (by name; a class the career no longer has is its first rung, a
    /// level its class's first) and its season.
    godot::String resume_career(const godot::String& save);
    /// Moves the player up to the rung their titles have opened (career::
    /// move_up) and starts its season, drawn for `seed`; money and upgrades
    /// carry over, by the upgrades' names. Only between seasons: with the
    /// season over, or before the player's first pull in it. "" or why not.
    godot::String move_up(std::int64_t seed);
    /// Loads a season (res:// paths work) and draws its field for `seed`, a
    /// new season. The player's career (money and upgrades) carries over
    /// from the season before; the first season starts one (the garage's
    /// start money, nothing bought). Returns "" or what is wrong, naming the
    /// file and field.
    godot::String start_season(const godot::String& season_path, std::int64_t seed);
    /// As start_season(), with a new career: the garage's start money and
    /// nothing bought, whatever the career so far. If the season cannot be
    /// started, the career so far stays.
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
    /// {name, index, count, pullers, full_pull_m, standard_full_pull_m,
    /// pull_off_places, turn, pull_kind, pull_off_round, track} of the event being
    /// pulled:
    /// `full_pull_m` is the line in force (110 m after a test pull past
    /// 100 m, the pull-off line in a pull-off), `standard_full_pull_m` the
    /// class's own, where the banner stands; `turn` is "rivals",
    /// "player_pull", "player_decision", "finished" or "failed";
    /// `pull_kind` ("test", "competition", "repull", "pull_off") and
    /// `pull_off_round` the player's pull, while it is due; `track` {grip,
    /// cone_index_kpa, tip} the track at the start line and how to set up
    /// for it. {} before one.
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
    /// pull_off_distance_m, pull_off_distances_m (each round's, the first
    /// first)}.
    [[nodiscard]] godot::Array get_standings() const;

    /// Records the player's pull `run` has finished (it must be the pull
    /// setup_event_pull() set up): the operator's verdict as a log entry
    /// (get_log()), or {} if no pull of the player's is due or the run is
    /// not finished.
    godot::Dictionary record_pull(const godot::Ref<TractorPullRun>& run);
    /// The player's pull stopped short of the restart line: instead of
    /// recording it, they pull again event::kDeferPlaces later (field::
    /// LiveEvent::defer_player). The log entry for the move, or {} if the
    /// player is not due or could not move back (the last to pull, a
    /// pull-off).
    godot::Dictionary move_back();
    /// What a sensible test puller would do, while the player decides.
    [[nodiscard]] bool get_suggested_keep() const;
    /// The player's keep (true) or re-pull (false), while it is due.
    void decide(bool keep);

    /// Scores the finished event in the championship and pays the player
    /// their prize (get_event_prize()); false if it is not finished or
    /// already scored. The last event of a season in the career's class
    /// counts the season (career::finish_season): a title opens the class
    /// above.
    bool score_event();
    /// What the event being pulled pays the player for their place so far
    /// [EUR] (garage::prize_eur); 0 before an event.
    [[nodiscard]] std::int64_t get_event_prize() const;

    /// The garage: {money_eur, can_buy, fully_upgraded, power_hp, upgrades,
    /// choices}, each upgrade {name, part, level, levels, label, labels,
    /// costs, next_label, next_cost_eur, counts, for} (`part` as the garage
    /// files name it, `labels` and `costs` every level's, the next ones ""
    /// and 0 at the top level; `for` the alternative it belongs to, "" for
    /// none, and `counts` whether that is fitted), each choice {name, slot,
    /// fitted, alternatives}, each alternative {label, cost_eur, owned};
    /// `power_hp` the tractor's peak in metric hp. `can_buy` is false while the
    /// player has pulled in the event being pulled: a tractor changes
    /// between events, not during one. {} before a season.
    [[nodiscard]] godot::Dictionary get_garage() const;
    /// The engine's dyno curve for upgrade `index`'s screen: {now, next},
    /// each {points, hp}: (rpm, kW) at full throttle from idle to high idle
    /// and the peak in metric hp; `next` with the upgrade's next level
    /// bought, left out at its top. {} for no such upgrade.
    [[nodiscard]] godot::Dictionary get_upgrade_dyno(std::int64_t index) const;
    /// Each alternative of choice `choice` as if fitted, with the career's
    /// builds: its dyno curve {points, hp} (an engine slot's; the others'
    /// engine is the tractor's).
    [[nodiscard]] godot::Array get_choice_dynos(std::int64_t choice) const;
    /// Buys the next level of upgrade `index`; "" or why not ("top level",
    /// "not enough money", "not during an event", "no such upgrade").
    godot::String buy_upgrade(std::int64_t index);
    /// Buys alternative `alternative` of choice `choice` and fits it; "" or
    /// why not ("owned already", "not enough money", "not during an
    /// event", "no such part").
    godot::String buy_alternative(std::int64_t choice, std::int64_t alternative);
    /// Fits an alternative owned, for nothing; "" or why not ("not bought
    /// yet", "not during an event", "no such part").
    godot::String fit_alternative(std::int64_t choice, std::int64_t alternative);
    /// For testing: adds `amount_eur` to the player's money (a negative
    /// amount takes it away, to no less than 0); false without a career.
    bool add_money(std::int64_t amount_eur);
    /// The championship so far, best first: {place, puller, name, points,
    /// wins}.
    [[nodiscard]] godot::Array get_season_standings() const;

    /// The player's tractor as they set it before a pull (vehicle::Tuning),
    /// kept for the season: {ballast, ballast_kg, ballast_position, rear_share,
    /// pressure, pressure_bar, min_bar, max_bar, hitch, hitch_m,
    /// min_hitch_m, max_hitch_m}. `ballast`, `pressure` and `hitch` say
    /// whether the tractor has weights to move, rear tires whose pressure
    /// can be set and an adjustable pulling hitch (from its lowest hole to
    /// the top one its drawbar has); `ballast_position` runs from 0
    /// (rearmost) to 1 (foremost), and `rear_share` is the share of the
    /// weight then on the rear axle at rest (0 to 1). {} before a season.
    [[nodiscard]] godot::Dictionary get_tuning() const;
    /// Sets the player's tuning, each held to its range (a value that is not
    /// finite changes nothing); their next pulls and practice pulls are on
    /// the tuned tractor. `hitch_m` 0 (the default) puts the hitch in its
    /// top hole.
    void set_tuning(double ballast_position, double pressure_bar, double hitch_m);

    /// What the player's crew reports of the track's hard and weak spots
    /// before their next pull (field::LiveEvent::player_crew_report),
    /// sharper with every pull already pulled and in the pull-off:
    /// {spots, uncertainty, pulls_seen, pull_off, suggested_y_m, gear, max_y_m,
    /// start_y_m, lane_width_m, full_pull_m, rear_track_m,
    /// rear_tire_width_m, pan_half_width_m, along_error_m,
    /// across_error_m}; the last two say how far off a reported spot may
    /// be. Each spot is {kind ("hard" or
    /// "weak"), at_m (its middle, from the start line), y_m (its middle,
    /// to the left of the lane's middle), width_m, length_m, strength (a
    /// hard spot's extra drag, a weak spot's grip lost, as shares)}.
    /// `suggested_y_m` is where the crew would put the sled
    /// (crew::best_line_m), `max_y_m` how far off the middle it may go
    /// either way, `start_y_m` where the player has put it; `gear` the gear
    /// the player's tractor, as tuned when the event started, went
    /// furthest in at the event's gear trials
    /// (field::LiveEvent::player_best_gear; 0 before them): set up
    /// differently on the pit board, it may pull best in another. {}
    /// before an event.
    [[nodiscard]] godot::Dictionary get_crew_report() const;
    /// Where the player puts the sled across the lane for their next pulls
    /// and practice pulls, to the left of the middle [m]; held to the
    /// lane's limits for the tractor (run::max_start_offset_m) when a pull
    /// is set up. Back to the middle at each new event.
    void set_start(double y_m);

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
    /// `setup` starting where the player put the sled, within its limits.
    [[nodiscard]] session::Setup started(session::Setup setup) const;
    /// `line_before_m`: the full-pull line in force before the pull.
    [[nodiscard]] godot::Dictionary entry(const field::LogEntry& entry, double line_before_m) const;
    /// The full-pull line each event starts with.
    [[nodiscard]] double first_line_m() const;
    /// The player's tractor as their career has it (garage::equipped),
    /// before the tuning.
    [[nodiscard]] vehicle::Params player_tractor() const;

    /// The season of the player's rung in the loaded career; "" without
    /// one.
    [[nodiscard]] std::string class_season() const;
    /// Whether the season is the career's and the player has not pulled in
    /// it yet, or it is over: a class may change.
    [[nodiscard]] bool between_seasons() const;

    std::optional<data::CareerDefinition> career_definition_;
    career::Progress progress_;
    std::optional<data::SeasonDefinition> definition_;
    /// The season file and seed start_season() was given, for get_save().
    std::string season_path_;
    std::uint64_t seed_ = 0;
    std::vector<std::string> names_;
    vehicle::Tuning tuning_;
    double start_y_m_ = 0.0;
    /// Kept from season to season; set by the first start_season().
    std::optional<garage::Career> career_;
    // Large plain data: on the heap.
    std::unique_ptr<field::LiveSeason> season_;
};

}  // namespace tp::godot_binding
