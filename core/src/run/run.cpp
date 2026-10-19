#include "tp/core/run/run.hpp"

#include "tp/core/math/trig.hpp"

#include <algorithm>
#include <cmath>

namespace tp::run {

namespace {

// With the sled stopped, wheels slipping more than this are spinning out
// rather than gripping.
constexpr double kSpinningSlip = 0.5;

Ending why_it_stopped(const pull::State& pull, const pull::Output& out) noexcept {
    if (!pull.tractor.engine.running) {
        return Ending::kStalled;
    }
    return out.tractor.slip_ratio > kSpinningSlip ? Ending::kSpunOut : Ending::kStopped;
}

}  // namespace

std::optional<ParamError> validate(const Rules& rules) noexcept {
    if (!std::isfinite(rules.track_length_m) || !std::isfinite(rules.max_duration_s) ||
        !std::isfinite(rules.lane_width_m) || !std::isfinite(rules.full_pull_line_m)) {
        return ParamError::kNotFinite;
    }
    if (rules.track_length_m <= 0.0) {
        return ParamError::kTrackLengthNotPositive;
    }
    if (rules.max_duration_s <= 0.0) {
        return ParamError::kMaxDurationNotPositive;
    }
    if (!(rules.lane_width_m > 0.0)) {
        return ParamError::kLaneWidthNotPositive;
    }
    if (!(rules.full_pull_line_m > 0.0)) {
        return ParamError::kFullPullLineNotPositive;
    }
    if (rules.full_pull_line_m > rules.track_length_m) {
        return ParamError::kFullPullLinePastTrackEnd;
    }
    return std::nullopt;
}

namespace {

// One tire's outer edge: whether it touches a chalk line, at its place
// along the track. `ahead_m` and `side_m` place the edge on the body
// (forward of the rear axle, to the left).
bool edge_touches(const chassis::State& body, const math::SinCos& heading, double start_line_m,
                  double ahead_m, double side_m, const Rules& rules) noexcept {
    const double along_m =
        body.position_m + ahead_m * heading.cos - side_m * heading.sin - start_line_m;
    const double across_m = body.lateral_position_m + ahead_m * heading.sin + side_m * heading.cos;
    return along_m < rules.full_pull_line_m && std::abs(across_m) >= 0.5 * rules.lane_width_m;
}

}  // namespace

bool touches_line(const pull::Model& model, const pull::State& state, const Rules& rules) noexcept {
    const vehicle::Params& tractor = model.tractor().params();
    const chassis::State& body = state.tractor.chassis;
    const math::SinCos heading = math::sin_cos_any(body.heading_rad);
    const double rear_m =
        0.5 * (tractor.chassis.rear_track_m + tractor.rear_tire.size.section_width_m);
    const double front_m =
        0.5 * (tractor.chassis.front_track_m + tractor.front_tire.section_width_m);
    const double wheelbase_m = tractor.chassis.wheelbase_m;
    const double start_m = model.start_line_m();
    return edge_touches(body, heading, start_m, 0.0, rear_m, rules) ||
           edge_touches(body, heading, start_m, 0.0, -rear_m, rules) ||
           edge_touches(body, heading, start_m, wheelbase_m, front_m, rules) ||
           edge_touches(body, heading, start_m, wheelbase_m, -front_m, rules);
}

State start(const pull::Model& model) noexcept {
    return State{
        .pull = pull::initial_state(model), .steps = 0, .top_speed_m_s = 0.0, .result = {}};
}

pull::Output step(State& state, const pull::Model& model, const Rules& rules,
                  const vehicle::Input& input, double dt_s) noexcept {
    if (state.result) {
        return {};
    }
    const bool sled_was_moving = state.pull.sled.distance_m > 0.0;
    const pull::Output out = pull::step(state.pull, model, input, dt_s);
    ++state.steps;
    const double time_s = static_cast<double>(state.steps) * dt_s;
    const sled::State& sled = state.pull.sled;
    state.top_speed_m_s = std::max(state.top_speed_m_s, sled.velocity_m_s);
    // Measured down the track, like the chalk lines: a sled that weaves
    // travels further than it pulls (sled.distance_m is that travel).
    const double pulled_m = sled.position_m - model.start_line_m();

    const auto finish = [&](Ending ending, double distance_m) {
        state.result = Result{.ending = ending,
                              .distance_m = distance_m,
                              .time_s = time_s,
                              .top_speed_m_s = state.top_speed_m_s};
    };
    if (touches_line(model, state.pull, rules)) {
        finish(Ending::kOutOfBounds, 0.0);
    } else if (pulled_m >= rules.track_length_m) {
        finish(Ending::kFullPull, rules.track_length_m);
    } else if (sled_was_moving && sled.velocity_m_s == 0.0) {
        // Any stop ends the run, measured where the sled stands.
        finish(why_it_stopped(state.pull, out), pulled_m);
    } else if (time_s >= rules.max_duration_s) {
        finish(Ending::kTimedOut, pulled_m);
    }
    return out;
}

}  // namespace tp::run
