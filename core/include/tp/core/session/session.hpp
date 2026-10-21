#pragma once

#include "tp/core/controls/controls.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/time/fixed_step_clock.hpp"
#include "tp/core/track/track.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::session {

/// How the player drives: every control on its own, or the easy layout
/// with the clutch assist (see tp::controls).
enum class Layout : std::uint8_t {
    kPro,
    kEasy,
};

/// Everything one run needs, given once at the start.
struct Setup {
    vehicle::Params tractor;
    /// The sled as it is pulled today: the class's sled with the track's
    /// conditions (sled::with_conditions), or a heavier one for a pull-off
    /// (sled::heavier).
    sled::Params sled;
    track::Params ground;
    run::Rules rules;
    Layout layout = Layout::kPro;
    /// Used with the easy layout only.
    controls::EasyParams easy;
    /// Used with the pro layout only; the default steers as the control is.
    controls::ProParams pro;
    /// The most steps one frame may run; a longer frame drops the rest of
    /// its time instead of catching up (see FixedStepClock).
    std::uint32_t max_steps_per_frame = FixedStepClock::Config{}.max_steps_per_advance;
};

enum class SetupError : std::uint8_t {
    kTractor,
    kSled,
    kTrack,
    kRules,
    kEasyControls,
    kProControls,
    kMaxStepsPerFrame,
};

/// What went wrong with `setup`, if anything.
[[nodiscard]] std::optional<SetupError> validate(const Setup& setup) noexcept;

/// The player's controls, held for a whole frame. Both layouts read the
/// throttle, the gear and the steering; the pro layout also reads the
/// clutch and the brakes, which the easy layout works itself. Values
/// outside their range, infinities too, are held to it; a value that is not
/// a number (a NaN from a front end's maths) reads as the control at rest:
/// throttle closed, clutch pressed, steering straight, brakes off.
struct Controls {
    /// 0 to 1.
    double throttle = 0.0;
    /// 0 released (engaged) to 1 pressed.
    double clutch_pedal = 1.0;
    /// 0 is neutral.
    int gear = driveline::kNeutral;
    /// -1 (full right) to 1 (full left): the share of the steering's
    /// range; in the easy layout its last stretch brakes the inner rear
    /// wheels.
    double steering = 0.0;
    /// 0 to 1.
    double brake_left = 0.0;
    double brake_right = 0.0;
};

/// What a front end draws and shows after a step: plain data in SI units.
/// Positions are on the track: x down the track from the start line, y to
/// the left of the lane's centre line, the ground at z = 0 (see
/// docs/conventions.md).
struct Snapshot {
    /// Seconds since the start: steps × dt.
    double time_s = 0.0;

    /// The tractor's rear axle, at the middle of the axle.
    double tractor_x_m = 0.0;
    double tractor_y_m = 0.0;
    /// Positive turning left.
    double tractor_heading_rad = 0.0;
    /// Positive nose up.
    double tractor_pitch_rad = 0.0;
    double tractor_speed_m_s = 0.0;
    /// The front wheels' angle, positive to the left.
    double steering_rad = 0.0;
    /// Each side's rear wheels; the front end turns them by these.
    double rear_left_omega_rad_s = 0.0;
    double rear_right_omega_rad_s = 0.0;
    double slip_ratio = 0.0;

    double engine_omega_rad_s = 0.0;
    bool engine_running = false;
    int gear = driveline::kNeutral;
    double throttle = 0.0;
    /// The clutch pedal as the car has it: the player's, or the assist's
    /// in the easy layout. 0 engaged, 1 pressed.
    double clutch_pedal = 1.0;

    /// The sled's rear wheels, at the middle of their axle; sled_x_m is how
    /// far it has pulled.
    double sled_x_m = 0.0;
    double sled_y_m = 0.0;
    double sled_heading_rad = 0.0;
    double sled_speed_m_s = 0.0;
    /// The weight box, forward of the sled's rear wheels.
    double box_ahead_of_wheels_m = 0.0;
    double chain_tension_n = 0.0;
};

/// The snapshot `alpha` of the way from `from` to `to` (0 to 1), for drawing
/// between two steps. What changes in jumps (the gear, the engine running)
/// is taken from `to`.
[[nodiscard]] Snapshot blend(const Snapshot& from, const Snapshot& to, double alpha) noexcept;

/// What one frame did.
struct Frame {
    /// Fixed steps run this frame.
    std::uint32_t steps = 0;
    /// Frame time dropped beyond max_steps_per_frame [ns].
    std::int64_t dropped_ns = 0;
};

/// One run as the game plays it: the player's controls in, once per frame,
/// with the frame's real time; the run stepped at the fixed 1 kHz rate; a
/// snapshot to draw out. The frame time decides only how many steps run,
/// never their length, so the same controls on the same steps give the
/// same run however the time is split into frames.
///
/// No heap: create() copies the setup, and nothing after it allocates.
/// After a full pull the game starts a new session on the heavier sled.
class Session {
public:
    /// Nothing if `setup` is invalid (see validate()).
    [[nodiscard]] static std::optional<Session> create(const Setup& setup) noexcept;

    /// Adds `elapsed_ns` of real time and runs the whole steps it makes up,
    /// all with `controls`. Once the run has ended the steps still count,
    /// but nothing moves.
    Frame advance(const Controls& controls, std::int64_t elapsed_ns) noexcept;

    /// The state before the last step and after it; draw blend(previous(),
    /// current(), alpha()).
    [[nodiscard]] const Snapshot& previous() const noexcept { return previous_; }
    [[nodiscard]] const Snapshot& current() const noexcept { return current_; }
    /// How far the time not yet stepped is into the next step, 0 to 1.
    [[nodiscard]] double alpha() const noexcept { return clock_.alpha(); }

    /// The run's result, once it has ended.
    [[nodiscard]] const std::optional<run::Result>& result() const noexcept { return run_.result; }
    /// The replay hash: the run's state and the layout's (the clutch
    /// assist's and the steering's, controls::AssistState). It leaves
    /// out the frame time not yet stepped, so two sessions with the same
    /// hash may run a different number of steps on their next frame: compare
    /// it at the same step (a replay's end), not to detect a desync mid-run
    /// or to save and restore a session.
    [[nodiscard]] std::uint64_t hash() const noexcept;

private:
    // Tools and tests look at the whole simulation through it (inspect.hpp);
    // the binding reads the snapshots only.
    friend class Inspector;

    Session(const pull::Model& model, const Setup& setup, const FixedStepClock& clock) noexcept;

    void step(const Controls& controls) noexcept;
    [[nodiscard]] Snapshot snapshot() const noexcept;

    pull::Model model_;
    run::Rules rules_;
    Layout layout_;
    controls::EasyParams easy_;
    controls::ProParams pro_;
    FixedStepClock clock_;
    run::State run_;
    controls::AssistState assist_;
    vehicle::Input input_;
    pull::Output output_;
    Snapshot previous_;
    Snapshot current_;
};

}  // namespace tp::session
