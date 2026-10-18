#include "tp/core/run/run.hpp"

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
    if (!std::isfinite(rules.track_length_m) || !std::isfinite(rules.max_duration_s)) {
        return ParamError::kNotFinite;
    }
    if (rules.track_length_m <= 0.0) {
        return ParamError::kTrackLengthNotPositive;
    }
    if (rules.max_duration_s <= 0.0) {
        return ParamError::kMaxDurationNotPositive;
    }
    return std::nullopt;
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

    const auto finish = [&](Ending ending, double distance_m) {
        state.result = Result{.ending = ending,
                              .distance_m = distance_m,
                              .time_s = time_s,
                              .top_speed_m_s = state.top_speed_m_s};
    };
    if (sled.distance_m >= rules.track_length_m) {
        finish(Ending::kFullPull, rules.track_length_m);
    } else if (sled_was_moving && sled.velocity_m_s == 0.0) {
        // Any stop ends the run, measured where the sled stands.
        finish(why_it_stopped(state.pull, out), sled.distance_m);
    } else if (time_s >= rules.max_duration_s) {
        finish(Ending::kTimedOut, sled.distance_m);
    }
    return out;
}

}  // namespace tp::run
