#include "tp/core/session/session.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <cmath>

namespace tp::session {

namespace {

std::optional<FixedStepClock> make_clock(const Setup& setup) noexcept {
    return FixedStepClock::create(
        {.step_ns = kSimulationStepNs, .max_steps_per_advance = setup.max_steps_per_frame});
}

// std::lerp: exactly `to` at alpha 1 and monotonic in alpha, where
// from + alpha * (to - from) can miss `to` by a few ulp.
double lerp(double from, double to, double alpha) noexcept {
    return std::lerp(from, to, alpha);
}

}  // namespace

std::optional<SetupError> validate(const Setup& setup) noexcept {
    if (vehicle::validate(setup.tractor)) {
        return SetupError::kTractor;
    }
    if (sled::validate(setup.sled)) {
        return SetupError::kSled;
    }
    if (track::validate(setup.ground)) {
        return SetupError::kTrack;
    }
    if (run::validate(setup.rules)) {
        return SetupError::kRules;
    }
    if (setup.layout == Layout::kEasy && controls::validate(setup.easy)) {
        return SetupError::kEasyControls;
    }
    if (setup.layout == Layout::kPro && controls::validate(setup.pro)) {
        return SetupError::kProControls;
    }
    if (!make_clock(setup)) {
        return SetupError::kMaxStepsPerFrame;
    }
    return std::nullopt;
}

// Headings blend as plain numbers: they stay within ±π, and a tractor in
// its lane never turns near the ±π seam.
Snapshot blend(const Snapshot& from, const Snapshot& to, double alpha) noexcept {
    Snapshot out = to;
    out.time_s = lerp(from.time_s, to.time_s, alpha);
    out.tractor_x_m = lerp(from.tractor_x_m, to.tractor_x_m, alpha);
    out.tractor_y_m = lerp(from.tractor_y_m, to.tractor_y_m, alpha);
    out.tractor_heading_rad = lerp(from.tractor_heading_rad, to.tractor_heading_rad, alpha);
    out.tractor_pitch_rad = lerp(from.tractor_pitch_rad, to.tractor_pitch_rad, alpha);
    out.tractor_speed_m_s = lerp(from.tractor_speed_m_s, to.tractor_speed_m_s, alpha);
    out.steering_rad = lerp(from.steering_rad, to.steering_rad, alpha);
    out.rear_left_omega_rad_s = lerp(from.rear_left_omega_rad_s, to.rear_left_omega_rad_s, alpha);
    out.rear_right_omega_rad_s =
        lerp(from.rear_right_omega_rad_s, to.rear_right_omega_rad_s, alpha);
    out.slip_ratio = lerp(from.slip_ratio, to.slip_ratio, alpha);
    out.engine_omega_rad_s = lerp(from.engine_omega_rad_s, to.engine_omega_rad_s, alpha);
    out.throttle = lerp(from.throttle, to.throttle, alpha);
    out.clutch_pedal = lerp(from.clutch_pedal, to.clutch_pedal, alpha);
    out.sled_x_m = lerp(from.sled_x_m, to.sled_x_m, alpha);
    out.sled_y_m = lerp(from.sled_y_m, to.sled_y_m, alpha);
    out.sled_heading_rad = lerp(from.sled_heading_rad, to.sled_heading_rad, alpha);
    out.sled_speed_m_s = lerp(from.sled_speed_m_s, to.sled_speed_m_s, alpha);
    out.box_ahead_of_wheels_m = lerp(from.box_ahead_of_wheels_m, to.box_ahead_of_wheels_m, alpha);
    out.pushdown = lerp(from.pushdown, to.pushdown, alpha);
    out.chain_tension_n = lerp(from.chain_tension_n, to.chain_tension_n, alpha);
    return out;
}

std::optional<Session> Session::create(const Setup& setup) noexcept {
    if (validate(setup)) {
        return std::nullopt;
    }
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    const std::optional<FixedStepClock> clock = make_clock(setup);
    if (!model || !clock) {
        return std::nullopt;
    }
    return Session{*model, setup, *clock};
}

Session::Session(const pull::Model& model, const Setup& setup, const FixedStepClock& clock) noexcept
    : model_(model),
      rules_(setup.rules),
      layout_(setup.layout),
      easy_(setup.easy),
      pro_(setup.pro),
      clock_(clock),
      run_(run::start(model)),
      assist_(),
      input_(),
      output_() {
    // Before the first step the weight box stands where it starts.
    output_.sled.box_ahead_of_wheels_m = model_.sled().box_start_ahead_of_wheels_m;
    current_ = snapshot();
    previous_ = current_;
}

Frame Session::advance(const Controls& controls, std::int64_t elapsed_ns) noexcept {
    const FixedStepClock::Advance time = clock_.advance(elapsed_ns);
    if (time.steps > 0) {
        // Only the last two steps are drawn: their snapshots are all that
        // is taken.
        for (std::uint32_t i = 1; i < time.steps; ++i) {
            step(controls);
        }
        previous_ = snapshot();
        step(controls);
        current_ = snapshot();
    }
    return {.steps = time.steps, .dropped_ns = time.dropped_ns};
}

void Session::step(const Controls& controls) noexcept {
    if (run_.result) {
        // Over: nothing moves, and the last step's output stays.
        return;
    }
    const vehicle::Params& tractor = model_.tractor().params();
    const double dt_s = clock_.step_s();
    if (layout_ == Layout::kEasy) {
        const controls::EasyInput easy{
            .throttle = controls.throttle, .gear = controls.gear, .steering = controls.steering};
        input_ = controls::easy(easy, easy_, assist_, tractor, run_.pull.tractor, dt_s);
    } else {
        const controls::ProInput pro{.throttle = controls.throttle,
                                     .clutch_pedal = controls.clutch_pedal,
                                     .gear = controls.gear,
                                     .steering = controls.steering,
                                     .brake_left = controls.brake_left,
                                     .brake_right = controls.brake_right};
        input_ = controls::pro(pro, pro_, assist_, tractor, dt_s);
    }
    output_ = run::step(run_, model_, rules_, input_, dt_s);
}

Snapshot Session::snapshot() const noexcept {
    const pull::State& pull = run_.pull;
    const vehicle::State& tractor = pull.tractor;
    const chassis::State& body = tractor.chassis;
    const double start_m = model_.start_line_m();
    const double axle_rad_s = tractor.driveline.output_omega_rad_s;
    return {.time_s = static_cast<double>(run_.steps) * clock_.step_s(),
            .tractor_x_m = body.position_m - start_m,
            .tractor_y_m = body.lateral_position_m,
            .tractor_heading_rad = body.heading_rad,
            .tractor_pitch_rad = body.pitch_rad,
            .tractor_speed_m_s = body.velocity_m_s,
            .steering_rad = input_.steering_rad,
            .rear_left_omega_rad_s = axle_rad_s - tractor.differential_rad_s,
            .rear_right_omega_rad_s = axle_rad_s + tractor.differential_rad_s,
            .slip_ratio = output_.tractor.slip_ratio,
            .engine_omega_rad_s = tractor.engine.omega_rad_s,
            .engine_running = tractor.engine.running,
            .gear = tractor.driveline.gear,
            .throttle = input_.throttle,
            .clutch_pedal = input_.clutch_pedal,
            .sled_x_m = pull.sled.position_m - start_m,
            .sled_y_m = pull.sled.lateral_position_m,
            .sled_heading_rad = pull.sled.heading_rad,
            .sled_speed_m_s = pull.sled.velocity_m_s,
            .box_ahead_of_wheels_m = output_.sled.box_ahead_of_wheels_m,
            .pushdown = output_.sled.pushdown,
            .chain_tension_n = output_.chain.tension_n};
}

std::uint64_t Session::hash() const noexcept {
    hash::Fnv1a hash;
    run::hash_append(hash, run_);
    controls::hash_append(hash, assist_);
    return hash.value();
}

}  // namespace tp::session
