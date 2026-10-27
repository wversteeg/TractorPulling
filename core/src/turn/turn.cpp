#include "tp/core/turn/turn.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/core/math/trig.hpp"

#include <cmath>
#include <numbers>

namespace tp::turn {

std::optional<ParamError> validate(const Parking& parking, const run::Rules& rules,
                                   double half_width_m) noexcept {
    if (!std::isfinite(parking.x_m) || !std::isfinite(parking.y_m)) {
        return ParamError::kNotFinite;
    }
    if (std::abs(parking.y_m) < 0.5 * rules.lane_width_m + half_width_m) {
        return ParamError::kInTheLane;
    }
    return std::nullopt;
}

State at_the_line(const pull::Model& model) noexcept {
    return State{.stage = Stage::kPulling,
                 .run = run::start(model),
                 .from_the_parking = false,
                 .tension_held_steps = 0,
                 .sled_back = false};
}

State from_the_parking(const pull::Model& model, const Parking& parking) noexcept {
    State state{.stage = Stage::kToTheSled,
                .run = run::start(model),
                .from_the_parking = true,
                .tension_held_steps = 0,
                .sled_back = false};
    pull::State& pull = state.run.pull;
    pull.hooked = false;
    pull.sled_mode = pull::SledMode::kHeld;
    pull.tractor.chassis.position_m = model.start_line_m() + parking.x_m;
    pull.tractor.chassis.lateral_position_m = parking.y_m;
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

bool parked(const pull::Model& model, const pull::State& state, const Parking& parking) noexcept {
    const chassis::State& body = state.tractor.chassis;
    const double dx_m = body.position_m - (model.start_line_m() + parking.x_m);
    const double dy_m = body.lateral_position_m - parking.y_m;
    return stands(state) && std::sqrt(dx_m * dx_m + dy_m * dy_m) <= kParkedWithin_m;
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

}  // namespace

bool hooks_on(const pull::Model& model, const pull::State& state) noexcept {
    const HitchToHook gap = hitch_to_hook(model, state);
    const double turned_rad = std::abs(std::remainder(
        state.tractor.chassis.heading_rad - state.sled.heading_rad, 2.0 * std::numbers::pi));
    return stands(state) && gap.along_m > 0.0 && gap.distance_m <= model.sled().chain.length_m &&
           std::abs(gap.across_m) <= kHookOffLine_m && turned_rad <= kHookTurned_rad;
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
    input.pull_on = false;
    restart_if_stalled(pull, model, input);
    const pull::Output out = pull::step(pull, model, input, dt_s);
    state.sled_back = state.sled_back || (pull.sled_mode == pull::SledMode::kDrivingBack &&
                                          pull.sled.position_m == model.start_line_m() &&
                                          pull.sled.velocity_m_s == 0.0);

    switch (state.stage) {
        case Stage::kToTheSled:
            if (hooks_on(model, pull)) {
                pull.hooked = true;
                state.stage = Stage::kTakingUpSlack;
            }
            break;
        case Stage::kTakingUpSlack: {
            state.tension_held_steps =
                out.chain.tension_n >= kTakeUpTension_n ? state.tension_held_steps + 1 : 0;
            if (static_cast<double>(state.tension_held_steps) * dt_s >= kFlagAfter_s) {
                // The green flag: the sled is free. The pull's clock starts
                // now: run::step has not run before the flag.
                pull.sled_mode = pull::SledMode::kOnTheChain;
                state.stage = Stage::kPulling;
            }
            break;
        }
        case Stage::kReleasing:
            if (pull.sled_mode == pull::SledMode::kOnTheChain && pull.sled.velocity_m_s == 0.0) {
                // Stopped after the pull: the official holds it from now on.
                pull.sled_mode = pull::SledMode::kHeld;
            }
            // Off only once the sled stands too: one still rolling can run
            // up on the tractor and slacken the chain by itself.
            if (pull.sled_mode == pull::SledMode::kHeld && stands(pull) &&
                chain_span_m(model, pull) <= model.sled().chain.length_m - kReleaseSlack_m) {
                pull.hooked = false;
                pull.sled_mode = pull::SledMode::kDrivingBack;
                state.stage = Stage::kToTheParking;
            }
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
