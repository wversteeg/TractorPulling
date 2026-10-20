#pragma once

#include "tp/core/session/session.hpp"

#include <cstdint>
#include <optional>

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
    /// work) and starts a run on the sled with the track's conditions, in
    /// the easy layout if `easy_controls_path` names its file, else the pro
    /// layout. Returns "" or what is wrong, naming the file and field.
    godot::String setup(const godot::String& tractor_path, const godot::String& sled_path,
                        const godot::String& track_path, const godot::String& easy_controls_path);

    /// The controls held from now on (see session::Controls).
    void set_controls(double throttle, double clutch_pedal, int64_t gear, double steering,
                      double brake_left, double brake_right);

    /// Runs the whole 1 ms steps `elapsed_ns` of real time makes up; returns
    /// how many. Nothing before setup().
    int64_t advance(int64_t elapsed_ns);

    /// The state to draw now: the last two steps blended (session::blend).
    [[nodiscard]] godot::Dictionary get_snapshot() const;

    [[nodiscard]] bool is_finished() const;
    /// {ending, distance_m, time_s, top_speed_m_s} once finished, else {}.
    [[nodiscard]] godot::Dictionary get_result() const;
    /// The replay hash (session::Session::hash) as 16 hex digits, the way
    /// `tp_pull run --hash` prints it.
    [[nodiscard]] godot::String get_state_hash() const;

protected:
    static void _bind_methods();

private:
    std::optional<session::Session> session_;
    session::Controls controls_;
};

}  // namespace tp::godot_binding
