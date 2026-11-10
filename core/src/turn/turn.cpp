#include "tp/core/turn/turn.hpp"

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/math/trig.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tp::turn {

std::optional<ParamError> validate(const Parking& parking, const run::Rules& rules,
                                   const Footprint& footprint) noexcept {
    if (!std::isfinite(parking.x_m) || !std::isfinite(parking.y_m) ||
        !std::isfinite(parking.heading_rad)) {
        return ParamError::kNotFinite;
    }
    // The footprint's four corners, across the track from the centre line,
    // all past the chalk line on the parking's side.
    const math::SinCos heading = math::sin_cos_any(parking.heading_rad);
    const double side = parking.y_m < 0.0 ? -1.0 : 1.0;
    for (const double ahead_m : {-footprint.behind_m, footprint.ahead_m}) {
        for (const double left_m : {-footprint.half_width_m, footprint.half_width_m}) {
            const double y_m = parking.y_m + ahead_m * heading.sin + left_m * heading.cos;
            if (side * y_m < 0.5 * rules.lane_width_m) {
                return ParamError::kInTheLane;
            }
        }
    }
    return std::nullopt;
}

State at_the_line(const pull::Model& model) noexcept {
    return State{.stage = Stage::kPulling,
                 .run = run::start(model),
                 .from_the_parking = false,
                 .ready_steps = 0,
                 .sled_back = false};
}

State from_the_parking(const pull::Model& model, const Parking& parking) noexcept {
    State state{.stage = Stage::kToTheSled,
                .run = run::start(model),
                .from_the_parking = true,
                .ready_steps = 0,
                .sled_back = false};
    pull::State& pull = state.run.pull;
    pull.hooked = false;
    pull.sled_mode = pull::SledMode::kHeld;
    pull.tractor.chassis.position_m = model.start_line_m() + parking.x_m;
    pull.tractor.chassis.lateral_position_m = parking.y_m;
    pull.tractor.chassis.heading_rad = parking.heading_rad;
    return state;
}

HitchToHook hitch_to_hook(const pull::Model& model, const pull::State& state) noexcept {
    const pull::ChainEnd hitch = pull::hitch_end(model, state);
    const pull::ChainEnd hook = pull::hook_end(model, state);
    const double dx_m = hitch.x_m - hook.x_m;
    const double dy_m = hitch.y_m - hook.y_m;
    const math::SinCos sled = math::sin_cos_any(state.sled.heading_rad);
    return {.distance_m = std::sqrt(dx_m * dx_m + dy_m * dy_m),
            .along_m = dx_m * sled.cos + dy_m * sled.sin,
            .across_m = dy_m * sled.cos - dx_m * sled.sin};
}

namespace {

bool stands(const pull::State& state) noexcept {
    return std::abs(state.tractor.chassis.velocity_m_s) <= kAtRest_m_s;
}

// The chain's span from the hitch to the hook, the height between them
// included [m].
double chain_span_m(const pull::Model& model, const pull::State& state) noexcept {
    const pull::ChainEnd hitch = pull::hitch_end(model, state);
    const pull::ChainEnd hook = pull::hook_end(model, state);
    const double dx_m = hitch.x_m - hook.x_m;
    const double dy_m = hitch.y_m - hook.y_m;
    const double dz_m = hitch.z_m - hook.z_m;
    return std::sqrt(dx_m * dx_m + dy_m * dy_m + dz_m * dz_m);
}

// Moves the standing tractor `by_m` back along its heading.
void ease_back(chassis::State& body, double by_m) noexcept {
    const math::SinCos heading = math::sin_cos_any(body.heading_rad);
    body.position_m -= by_m * heading.cos;
    body.lateral_position_m -= by_m * heading.sin;
}

bool parked(const pull::Model& model, const pull::State& state, const Parking& parking) noexcept {
    const chassis::State& body = state.tractor.chassis;
    const double dx_m = body.position_m - (model.start_line_m() + parking.x_m);
    const double dy_m = body.lateral_position_m - parking.y_m;
    return stands(state) && std::sqrt(dx_m * dx_m + dy_m * dy_m) <= kParkedWithin_m;
}

// Backing up to the sled, before the pull or after it, the rear tires stop
// against its fenders: the tractor stands there, as against a wall. Only its
// speed along is stopped: the wheels may spin on, and a turn in progress is not.
void stop_at_the_fenders(pull::State& state, const pull::Model& model) noexcept {
    const sled::Fenders& fenders = model.sled().fenders;
    const HitchToHook gap = hitch_to_hook(model, state);
    const double stop_m = fender_stop_m(model);
    chassis::State& body = state.tractor.chassis;
    if (fenders.half_span_m <= 0.0 || body.velocity_m_s >= 0.0 ||
        std::abs(gap.across_m) > fenders.half_span_m || gap.along_m >= stop_m ||
        gap.along_m < stop_m - fenders.depth_m) {
        return;
    }
    const math::SinCos sled = math::sin_cos_any(state.sled.heading_rad);
    const double back_out_m = stop_m - gap.along_m;
    body.position_m += back_out_m * sled.cos;
    body.lateral_position_m += back_out_m * sled.sin;
    body.velocity_m_s = 0.0;
}

// Outside the pull the driver starts a stalled engine again, the clutch
// pressed or the gear out, as a real one would before driving on.
void restart_if_stalled(pull::State& state, const pull::Model& model,
                        const vehicle::Input& input) noexcept {
    engine::State& engine = state.tractor.engine;
    const bool may_start =
        input.clutch_pedal >= kRestartPedal || state.tractor.driveline.gear == driveline::kNeutral;
    if (!engine.running && may_start) {
        engine = engine::initial_state(model.tractor().params().engine);
    }
}

// The slack is taken up once the chain is tight to within the ready band
// (kReadyTautWithin_m), or pulls on the hook.
bool taken_up(const pull::Model& model, const pull::State& state,
              const pull::ChainForce& chain) noexcept {
    return chain.tension_n >= kTakeUpTension_n ||
           chain_span_m(model, state) >= model.sled().chain.length_m - kReadyTautWithin_m;
}

// kWaitingForTheFlag, after the step: the count while the tractor stands
// ready, the green flag after kFlagAfter_s, or back to taking up the slack.
void wait_for_the_flag(State& state, const pull::Model& model, const vehicle::Input& input,
                       double dt_s) noexcept {
    pull::State& pull = state.run.pull;
    const double span_m = chain_span_m(model, pull);
    const double chain_m = model.sled().chain.length_m;
    if (span_m < chain_m - kReleaseSlack_m) {
        // Backed off: the slack is to be taken up again.
        state.ready_steps = 0;
        state.stage = Stage::kTakingUpSlack;
        return;
    }
    const bool drive_out =
        pull.tractor.driveline.gear == driveline::kNeutral ||
        clutch::capacity_nm(model.tractor().params().clutch, input.clutch_pedal) == 0.0;
    if (drive_out && stands(pull) && span_m > chain_m) {
        ease_back(pull.tractor.chassis, std::min(span_m - chain_m, kChainEasesBack_m_s * dt_s));
    }
    // Measured after easing back, so the step that eases the last of the
    // jolt out may count.
    const double eased_m = chain_span_m(model, pull);
    const bool ready = stands(pull) && eased_m >= chain_m - kReadyTautWithin_m &&
                       eased_m <= chain_m + kReadyStretched_m && drive_out;
    state.ready_steps = ready ? state.ready_steps + 1 : 0;
    if (static_cast<double>(state.ready_steps) * dt_s >= kFlagAfter_s) {
        // The green flag: the sled is free. The pull's clock starts now
        // (run::step has not run before the flag), from a standstill, in
        // the gear the driver has picked.
        pull.sled_mode = pull::SledMode::kOnTheChain;
        state.stage = Stage::kPulling;
    }
}

// kReleasing, after the step: the sled held once it stops, the chain off
// once it is slack.
void release(State& state, const pull::Model& model) noexcept {
    pull::State& pull = state.run.pull;
    if (pull.sled_mode == pull::SledMode::kOnTheChain && pull.sled.velocity_m_s == 0.0) {
        // Stopped after the pull: the official holds it from now on.
        pull.sled_mode = pull::SledMode::kHeld;
    }
    // Off only once the sled stands too: one still rolling can run up on
    // the tractor and slacken the chain by itself.
    if (pull.sled_mode == pull::SledMode::kHeld && stands(pull) &&
        chain_span_m(model, pull) <= model.sled().chain.length_m - kReleaseSlack_m) {
        pull.hooked = false;
        pull.sled_mode = pull::SledMode::kDrivingBack;
        state.stage = Stage::kToTheParking;
    }
}

}  // namespace

double fender_stop_m(const vehicle::Params& tractor, const sled::Fenders& fenders) noexcept {
    // The rear tires' backs are a tire's radius behind the axle, the hitch
    // its own distance behind it.
    return tractor.rear_tire.size.radius_m - tractor.chassis.hitch_behind_rear_axle_m -
           fenders.behind_hook_m;
}

double fender_stop_m(const pull::Model& model) noexcept {
    return fender_stop_m(model.tractor().params(), model.sled().fenders);
}

double chain_slack_m(const pull::Model& model, const pull::State& state) noexcept {
    return model.sled().chain.length_m - chain_span_m(model, state);
}

bool hooks_on(const pull::Model& model, const pull::State& state) noexcept {
    const HitchToHook gap = hitch_to_hook(model, state);
    const double turned_rad = std::abs(std::remainder(
        state.tractor.chassis.heading_rad - state.sled.heading_rad, 2.0 * std::numbers::pi));
    return stands(state) && gap.along_m > 0.0 && gap.distance_m <= model.sled().chain.length_m &&
           std::abs(gap.across_m) <= kHookOffLine_m && turned_rad <= kHookTurned_rad;
}

bool pull_rules_apply(const State& state) noexcept {
    return state.stage == Stage::kPulling ||
           (state.stage == Stage::kReleasing &&
            state.run.pull.sled_mode == pull::SledMode::kOnTheChain);
}

bool may_restart(const State& state) noexcept {
    const std::optional<run::Result>& result = state.run.result;
    const bool just_stopped =
        state.from_the_parking ? state.stage == Stage::kReleasing : state.stage == Stage::kParked;
    if (!result || !just_stopped || state.restarts >= kMaxRestarts) {
        return false;
    }
    // A sled nudged less than the stop counts from stands until the time is
    // up (run::kStopCountsFrom_m): a stop too.
    const bool nudged =
        result->ending == run::Ending::kTimedOut && result->distance_m < run::kStopCountsFrom_m;
    const bool stopped = result->ending == run::Ending::kStalled ||
                         result->ending == run::Ending::kSpunOut ||
                         result->ending == run::Ending::kStopped || nudged;
    // Out of bounds before the restart line is a short pull too: pulled
    // again from the start, it no longer counts as disqualified.
    if (result->ending == run::Ending::kOutOfBounds) {
        return result->out_of_bounds_at_m < kRestartBefore_m;
    }
    return stopped && result->distance_m < kRestartBefore_m;
}

void restart(State& state, const pull::Model& model) noexcept {
    if (!may_restart(state)) {
        return;
    }
    // Pulled back by the officials: the tractor at the line on the chain as
    // run::start has it, in neutral, its engine idling.
    state.run = run::start(model);
    state.ready_steps = 0;
    state.sled_back = false;
    ++state.restarts;
    if (state.from_the_parking) {
        state.run.pull.sled_mode = pull::SledMode::kHeld;
        state.stage = Stage::kTakingUpSlack;
    } else {
        state.stage = Stage::kPulling;
    }
}

pull::Output step(State& state, const pull::Model& model, const run::Rules& rules,
                  const Parking& parking, vehicle::Input input, double dt_s) noexcept {
    pull::State& pull = state.run.pull;
    if (state.stage == Stage::kPulling) {
        input.pull_on = true;
        const pull::Output out = run::step(state.run, model, rules, input, dt_s);
        if (state.run.result && !state.from_the_parking) {
            // At the line the pull is the whole turn.
            state.stage = Stage::kParked;
        } else if (state.run.result) {
            // The chain stays on until the tractor backs off; a sled still
            // rolling (a full pull, a disqualification) rolls on until it
            // stops, and is then held (kReleasing).
            state.stage = Stage::kReleasing;
        }
        return out;
    }
    if (state.stage == Stage::kParked) {
        if (!state.from_the_parking) {
            return {};
        }
        // The turn is over: the tractor idles where it stands while the
        // sled finishes driving back.
        input = vehicle::Input{.pull_on = false};
    }
    // Past a full pull the sled rolls on, and the pull's rules with it,
    // until it stops.
    input.pull_on = pull_rules_apply(state);
    restart_if_stalled(pull, model, input);
    const pull::Output out = pull::step(pull, model, input, dt_s);
    state.sled_back = state.sled_back || (pull.sled_mode == pull::SledMode::kDrivingBack &&
                                          pull.sled.position_m == model.sled_start_m() &&
                                          pull.sled.velocity_m_s == 0.0);

    if (state.stage != Stage::kToTheParking && state.stage != Stage::kParked) {
        stop_at_the_fenders(pull, model);
    }
    switch (state.stage) {
        case Stage::kToTheSled:
            if (hooks_on(model, pull)) {
                pull.hooked = true;
                state.stage = Stage::kTakingUpSlack;
            }
            break;
        case Stage::kTakingUpSlack:
            if (taken_up(model, pull, out.chain)) {
                state.ready_steps = 0;
                state.stage = Stage::kWaitingForTheFlag;
            }
            break;
        case Stage::kWaitingForTheFlag:
            wait_for_the_flag(state, model, input, dt_s);
            break;
        case Stage::kReleasing:
            release(state, model);
            break;
        case Stage::kToTheParking:
            if (parked(model, pull, parking)) {
                state.stage = Stage::kParked;
            }
            break;
        case Stage::kPulling:
        case Stage::kParked:
            break;
    }
    return out;
}

}  // namespace tp::turn
