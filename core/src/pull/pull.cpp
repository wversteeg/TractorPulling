#include "tp/core/pull/pull.hpp"

#include <algorithm>
#include <cmath>

namespace tp::pull {

std::optional<ParamError> validate(const Params& params) noexcept {
    if (vehicle::validate(params.tractor)) {
        return ParamError::kTractor;
    }
    if (sled::validate(params.sled)) {
        return ParamError::kSled;
    }
    if (soil::validate(params.soil)) {
        return ParamError::kSoil;
    }
    return std::nullopt;
}

Model::Model(const vehicle::Model& tractor, const sled::Params& sled,
             const soil::Surface& surface) noexcept
    : tractor_(tractor), sled_(sled), surface_(surface) {}

std::optional<Model> Model::create(const Params& params) noexcept {
    if (validate(params)) {
        return std::nullopt;
    }
    const std::optional<vehicle::Model> tractor = vehicle::Model::create(params.tractor);
    if (!tractor) {
        return std::nullopt;
    }
    return Model{*tractor, params.sled, soil::surface(params.soil)};
}

ChainForce chain_force(const sled::Chain& chain, const ChainEnd& hitch,
                       const ChainEnd& hook) noexcept {
    const double dx_m = hitch.x_m - hook.x_m;
    const double dz_m = hitch.z_m - hook.z_m;
    // sqrt is correctly rounded everywhere, so this stays deterministic.
    const double span_m = std::sqrt(dx_m * dx_m + dz_m * dz_m);
    if (span_m <= 0.0) {
        return {};
    }
    const ChainForce slack{
        .tension_n = 0.0, .direction_x = dx_m / span_m, .direction_z = dz_m / span_m};
    const double stretch_m = span_m - chain.length_m;
    if (stretch_m <= 0.0) {
        return slack;
    }
    const double stretch_rate_m_s =
        slack.direction_x * (hitch.velocity_x_m_s - hook.velocity_x_m_s) +
        slack.direction_z * (hitch.velocity_z_m_s - hook.velocity_z_m_s);
    const double tension_n =
        chain.stiffness_n_per_m * stretch_m + chain.damping_n_s_per_m * stretch_rate_m_s;
    return {.tension_n = std::max(tension_n, 0.0),
            .direction_x = slack.direction_x,
            .direction_z = slack.direction_z};
}

namespace {

// At the start line the chain lies with a little slack: nothing pulls until
// the tractor moves.
constexpr double kStartSlack_m = 0.001;

ChainEnd hitch_end(const Model& model, const State& state) noexcept {
    const chassis::Hitch hitch =
        chassis::hitch(model.tractor().params().chassis, state.tractor.chassis);
    return {.x_m = hitch.x_m,
            .z_m = hitch.z_m,
            .velocity_x_m_s = hitch.velocity_x_m_s,
            .velocity_z_m_s = hitch.velocity_z_m_s};
}

ChainEnd hook_end(const sled::Params& sled, const sled::State& state) noexcept {
    return {.x_m = state.position_m + sled.hook_ahead_of_wheels_m,
            .z_m = sled.hook_height_m,
            .velocity_x_m_s = state.velocity_m_s,
            .velocity_z_m_s = 0.0};
}

}  // namespace

State initial_state(const Model& model) noexcept {
    State state{.tractor = vehicle::initial_state(model.tractor()), .sled = {}};
    const sled::Params& sled = model.sled();
    const ChainEnd hitch = hitch_end(model, state);
    const double rise_m = hitch.z_m - sled.hook_height_m;
    const double laid_out_m = sled.chain.length_m - kStartSlack_m;
    const double reach_m = std::sqrt(std::max(laid_out_m * laid_out_m - rise_m * rise_m, 0.0));
    state.sled.position_m = hitch.x_m - reach_m - sled.hook_ahead_of_wheels_m;
    return state;
}

Output step(State& state, const Model& model, const vehicle::Input& input, double dt_s) noexcept {
    const sled::Params& sled = model.sled();
    Output out;
    out.chain = chain_force(sled.chain, hitch_end(model, state), hook_end(sled, state.sled));

    // Equal and opposite at the two ends, along the chain.
    const double along_x_n = out.chain.tension_n * out.chain.direction_x;
    const double along_z_n = out.chain.tension_n * out.chain.direction_z;
    const chassis::Drawbar drawbar{.pull_n = along_x_n, .lift_n = -along_z_n};
    const sled::HookForce hook{.forward_n = along_x_n, .up_n = along_z_n};

    const vehicle::Ground ground{.rear = model.surface(), .front = model.surface()};
    out.tractor = vehicle::step(state.tractor, model.tractor(), input, ground, drawbar, dt_s);

    out.sled =
        sled::loads(sled, state.sled, hook, {.pan = model.surface(), .wheels = model.surface()});
    const sled::Prediction prediction =
        sled::predict(state.sled, sled, out.sled.resistance_n, dt_s);
    sled::advance(state.sled, prediction, hook.forward_n, dt_s);
    return out;
}

}  // namespace tp::pull
