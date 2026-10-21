#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/session/session.hpp"
#include "tp/data/files.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace tp::godot_binding {

/// One run, for GDScript: tp::session behind Godot types. The scene sets
/// the player's controls and advances it by each frame's time, then draws
/// the blended snapshot. Values are SI, on the track (x down the track from
/// the start line, y to the left; docs/conventions.md); turning them into
/// Godot's axes is the scene's business.
class TractorPullRun : public godot::RefCounted {
    GDCLASS(TractorPullRun, godot::RefCounted)

public:
    /// Loads the tractor, the class's sled and the track (res:// paths
    /// work) and starts a run on them (data::run_setup), in the layout
    /// `controls_path` names (easy.toml, pro.toml: data::load_control_layout),
    /// or with "" the pro layout steering as the control is. Returns "" or
    /// what is wrong, naming the file and field. A setup ends the run before
    /// it, even one that fails: there is then no run until a setup succeeds.
    godot::String setup(const godot::String& tractor_path, const godot::String& sled_path,
                        const godot::String& track_path, const godot::String& controls_path);

    /// Starts a pull at an event of a season (res:// paths work): the
    /// season's class tractor and sled on the track of event `event_index`
    /// (0 to get_event().count - 1), in pull-off round `round` (0 the
    /// qualifying pull; data::run_setup makes the sled one step heavier
    /// per round), in the layout `controls_path` names, as setup(). Returns
    /// "" or what is wrong.
    godot::String setup_event(const godot::String& season_path, std::int64_t event_index,
                              const godot::String& controls_path, std::int64_t round);
    /// {name, index, count, round, pull_offs} of the event setup_event()
    /// started (`pull_offs` the most a puller alone may pull), else {}.
    [[nodiscard]] godot::Dictionary get_event() const;
    /// Once an event's pull is finished, the round the puller pulls next
    /// (event::solo_next_round: a pull-off after a full pull), or -1 when
    /// they are done, the run is not finished, or it is no event's pull.
    [[nodiscard]] std::int64_t get_next_round() const;

    /// The controls held from now on (see session::Controls: held to their
    /// range, and a value that is not a number reads as the control at rest).
    void set_controls(double throttle, double clutch_pedal, std::int64_t gear, double steering,
                      double brake_left, double brake_right);

    /// Runs the whole 1 ms steps `elapsed_ns` of real time makes up; returns
    /// how many. Nothing before setup().
    std::int64_t advance(std::int64_t elapsed_ns);
    /// The real time the last advance() dropped beyond the steps one frame
    /// may run (a hitch): the run fell that far behind the clock [ns].
    [[nodiscard]] std::int64_t get_dropped_ns() const;

    /// The state to draw now: the last two steps blended (session::blend).
    [[nodiscard]] godot::Dictionary get_snapshot() const;

    /// The run's fixed sizes, to build the scene from (SI): the tractor's
    /// (wheelbase_m, rear_axle_height_m, rear_track_m, front_track_m,
    /// rear_tire_radius_m, rear_tire_width_m, front_tire_radius_m,
    /// front_tire_width_m, hitch_behind_rear_axle_m, hitch_height_m, and
    /// gear_count, its forward gears), the sled's (sled_wheel_radius_m,
    /// hook_ahead_of_wheels_m, hook_height_m, pan_ahead_of_wheels_m,
    /// box_start_ahead_of_wheels_m, box_end_ahead_of_wheels_m,
    /// chain_length_m), the track's (track_length_m, lane_width_m,
    /// full_pull_line_m) and the engine's speeds for a rev counter
    /// (stall_omega_rad_s, rated_omega_rad_s, high_idle_omega_rad_s). {}
    /// before setup().
    [[nodiscard]] godot::Dictionary get_dimensions() const;

    /// "easy" or "pro", the layout the run is played in; "" before setup().
    [[nodiscard]] godot::String get_layout() const;

    [[nodiscard]] bool is_finished() const;
    /// {ending, distance_m, time_s, top_speed_m_s} once finished, else {}.
    [[nodiscard]] godot::Dictionary get_result() const;
    /// The replay hash (session::Session::hash) as 16 hex digits, the way
    /// `tp_pull run --hash` prints it.
    [[nodiscard]] godot::String get_state_hash() const;

protected:
    static void _bind_methods();

private:
    /// An event's pull, as setup_event() started it.
    struct EventPull {
        std::string name;
        std::int64_t index = 0;
        std::int64_t count = 0;
        int round = 0;
        event::Rules rules;
    };

    /// Applies the controls file and starts the session; "" or the error.
    godot::String start(session::Setup setup, const godot::String& controls_path,
                        const data::ReadFile& read);
    void reset();

    std::optional<session::Session> session_;
    std::optional<EventPull> event_;
    /// The setup the session was created from, for get_dimensions().
    std::optional<session::Setup> setup_;
    session::Controls controls_;
    std::int64_t dropped_ns_ = 0;
};

}  // namespace tp::godot_binding
