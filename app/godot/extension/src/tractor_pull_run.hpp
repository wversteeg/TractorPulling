#pragma once

#include "tp/core/session/session.hpp"
#include "tp/data/files.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include "tractor_pull_event.hpp"
#include <godot_cpp/classes/ref.hpp>
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

    /// Starts the player's next pull at `event`'s event (TractorPullEvent):
    /// the reference tractor on the event's sled as the operator has set
    /// it, the run going on past the full-pull line until the sled stops,
    /// in the layout `controls_path` names, as setup(). Returns "" or what is
    /// wrong (no pull of the player's is due). Hand the finished run to
    /// event.record_pull().
    godot::String setup_event_pull(const godot::Ref<TractorPullEvent>& event,
                                   const godot::String& controls_path);
    /// A practice pull at `event`'s event, as setup_event_pull() but at any
    /// time and not for the event: the sled as the event has it now.
    godot::String setup_practice(const godot::Ref<TractorPullEvent>& event,
                                 const godot::String& controls_path);

    /// From the next setup on, each run is the player's whole turn from a
    /// parking beside the track (turn::Parking): x_m down the track from
    /// the start line (negative behind it), right_of_lane_m to the right of
    /// the lane's right-hand chalk line, whatever the track's lane width.
    /// The turn drives to the sled, pulls, and comes back. clear_parking()
    /// goes back to the pull alone, hooked at the line (the default). A
    /// parking in the lane (right_of_lane_m below half the tractor's width)
    /// makes the setup fail.
    void set_parking(double x_m, double right_of_lane_m);
    void clear_parking();
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

    /// The run's fixed sizes, to build the scene from (SI). The tractor's:
    /// wheelbase_m, rear_axle_height_m, rear_track_m, front_track_m,
    /// rear_tire_radius_m, rear_tire_width_m, front_tire_radius_m,
    /// front_tire_width_m, hitch_behind_rear_axle_m, hitch_height_m,
    /// wheelie_bar_behind_rear_axle_m, wheelie_bar_tip_height_m, the
    /// weights as set (ballast_mass_kg, and ballast_position from 0
    /// rearmost to 1 foremost), gear_count, its forward gears, and
    /// has_reverse, whether it has a reverse gear (gear -1). The
    /// sled's: sled_wheel_radius_m, sled_wheel_width_m,
    /// hook_ahead_of_wheels_m, hook_height_m, pan_ahead_of_wheels_m,
    /// box_start_ahead_of_wheels_m, box_end_ahead_of_wheels_m and
    /// chain_length_m. The track's: track_length_m, lane_width_m and
    /// full_pull_line_m. The engine's speeds for a rev counter:
    /// stall_omega_rad_s, rated_omega_rad_s and high_idle_omega_rad_s. And
    /// where the clutch slips along the pedal's travel:
    /// clutch_full_grip_pedal and clutch_bite_pedal
    /// (clutch::engagement_range). {} before setup().
    [[nodiscard]] godot::Dictionary get_dimensions() const;

    /// "easy" or "pro", the layout the run is played in; "" before setup().
    [[nodiscard]] godot::String get_layout() const;

    /// The pull has ended: its result is in (get_result()).
    [[nodiscard]] bool is_finished() const;
    /// The turn is over: the pull at the line has ended, or after a turn
    /// from the parking the tractor stands there again.
    [[nodiscard]] bool is_over() const;
    /// {ending, distance_m, time_s, top_speed_m_s} once finished, else {}.
    [[nodiscard]] godot::Dictionary get_result() const;
    /// The replay hash (session::Session::hash) as 16 hex digits, the way
    /// `tp_pull run --hash` prints it.
    [[nodiscard]] godot::String get_state_hash() const;

    /// For TractorPullEvent: the session, if any, and for a run
    /// setup_event_pull() started, the pull it was set up for (else none),
    /// so the event knows the result is of its pending pull.
    [[nodiscard]] const session::Session* session() const {
        return session_ ? &*session_ : nullptr;
    }
    [[nodiscard]] const std::optional<PullKey>& event_pull() const { return event_pull_; }

protected:
    static void _bind_methods();

private:
    /// Applies the controls file and starts the session; "" or the error.
    godot::String start(session::Setup setup, const godot::String& controls_path,
                        const data::ReadFile& read);
    void reset();

    std::optional<session::Session> session_;
    std::optional<PullKey> event_pull_;
    /// The setup the session was created from, for get_dimensions().
    std::optional<session::Setup> setup_;
    session::Controls controls_;
    /// The parking as set_parking() was given it, to place beside each
    /// setup's lane.
    struct ParkingBesideLane {
        double x_m = 0.0;
        double right_of_lane_m = 0.0;
    };
    std::optional<ParkingBesideLane> parking_;
    std::int64_t dropped_ns_ = 0;
};

}  // namespace tp::godot_binding
