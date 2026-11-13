#include "tp/core/pull/pull.hpp"

#include "tp/core/math/trig.hpp"

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
    if (track::validate(params.track)) {
        return ParamError::kTrack;
    }
    if (!std::isfinite(params.start_y_m)) {
        return ParamError::kStartPosition;
    }
    return std::nullopt;
}

Model::Model(const vehicle::Model& tractor, const sled::Params& sled, const track::Model& track,
             double start_y_m) noexcept
    : tractor_(tractor), sled_(sled), track_(track), start_y_m_(start_y_m) {
    sled_start_m_ = initial_state(*this).sled.position_m;
}

std::optional<Model> Model::create(const Params& params) noexcept {
    if (validate(params)) {
        return std::nullopt;
    }
    const std::optional<vehicle::Model> tractor = vehicle::Model::create(params.tractor);
    const std::optional<track::Model> track = track::Model::create(params.track);
    if (!tractor || !track) {
        return std::nullopt;
    }
    return Model{*tractor, params.sled, *track, params.start_y_m};
}

ChainForce chain_force(const sled::Chain& chain, const ChainEnd& hitch,
                       const ChainEnd& hook) noexcept {
    const double dx_m = hitch.x_m - hook.x_m;
    const double dy_m = hitch.y_m - hook.y_m;
    const double dz_m = hitch.z_m - hook.z_m;
    // sqrt is correctly rounded everywhere, so this stays deterministic.
    const double span_m = std::sqrt(dx_m * dx_m + dy_m * dy_m + dz_m * dz_m);
    if (span_m <= 0.0) {
        return {};
    }
    const ChainForce slack{.tension_n = 0.0,
                           .direction_x = dx_m / span_m,
                           .direction_y = dy_m / span_m,
                           .direction_z = dz_m / span_m};
    const double stretch_m = span_m - chain.length_m;
    if (stretch_m <= 0.0) {
        return slack;
    }
    const double stretch_rate_m_s =
        slack.direction_x * (hitch.velocity_x_m_s - hook.velocity_x_m_s) +
        slack.direction_y * (hitch.velocity_y_m_s - hook.velocity_y_m_s) +
        slack.direction_z * (hitch.velocity_z_m_s - hook.velocity_z_m_s);
    const double tension_n =
        chain.stiffness_n_per_m * stretch_m + chain.damping_n_s_per_m * stretch_rate_m_s;
    return {.tension_n = std::max(tension_n, 0.0),
            .direction_x = slack.direction_x,
            .direction_y = slack.direction_y,
            .direction_z = slack.direction_z};
}

namespace {

// At the start line the chain lies with a little slack: nothing pulls until
// the tractor moves.
constexpr double kStartSlack_m = 0.001;
constexpr double kSides = 2.0;

ChainEnd hitch_at(const Model& model, const State& state) noexcept {
    const chassis::Hitch hitch =
        chassis::hitch(model.tractor().params().chassis, state.tractor.chassis);
    return {.x_m = hitch.x_m,
            .y_m = hitch.y_m,
            .z_m = hitch.z_m,
            .velocity_x_m_s = hitch.velocity_x_m_s,
            .velocity_y_m_s = hitch.velocity_y_m_s,
            .velocity_z_m_s = hitch.velocity_z_m_s};
}

ChainEnd hook_at(const sled::Params& sled, const sled::State& state) noexcept {
    const sled::Hook hook = sled::hook(sled, state);
    return {.x_m = hook.x_m,
            .y_m = hook.y_m,
            .z_m = hook.z_m,
            .velocity_x_m_s = hook.velocity_x_m_s,
            .velocity_y_m_s = hook.velocity_y_m_s};
}

}  // namespace

ChainEnd hitch_end(const Model& model, const State& state) noexcept {
    return hitch_at(model, state);
}

ChainEnd hook_end(const Model& model, const State& state) noexcept {
    return hook_at(model.sled(), state.sled);
}

State initial_state(const Model& model) noexcept {
    State state{.tractor = vehicle::initial_state(model.tractor()), .sled = {}};
    state.tractor.chassis.lateral_position_m = model.start_y_m();
    state.sled.lateral_position_m = model.start_y_m();
    const sled::Params& sled = model.sled();
    const ChainEnd hitch = hitch_at(model, state);
    const double rise_m = hitch.z_m - sled.hook_height_m;
    const double laid_out_m = sled.chain.length_m - kStartSlack_m;
    const double reach_m = std::sqrt(std::max(laid_out_m * laid_out_m - rise_m * rise_m, 0.0));
    state.sled.position_m = hitch.x_m - reach_m - sled.hook_ahead_of_wheels_m;
    return state;
}

Output step(State& state, const Model& model, const vehicle::Input& input, double dt_s) noexcept {
    const sled::Params& sled = model.sled();
    Output out;
    out.chain = chain_force(sled.chain, hitch_at(model, state), hook_at(sled, state.sled));
    if (!state.hooked) {
        out.chain.tension_n = 0.0;
    }

    // Equal and opposite at the two ends, along the chain; the tractor and
    // the sled each feel it in their own frame.
    const double along_x_n = out.chain.tension_n * out.chain.direction_x;
    const double along_y_n = out.chain.tension_n * out.chain.direction_y;
    const double along_z_n = out.chain.tension_n * out.chain.direction_z;
    const math::SinCos heading = math::sin_cos_any(state.tractor.chassis.heading_rad);
    const chassis::Drawbar drawbar{.pull_n = along_x_n * heading.cos + along_y_n * heading.sin,
                                   .lift_n = -along_z_n,
                                   .side_n = along_x_n * heading.sin - along_y_n * heading.cos};
    const math::SinCos sled_heading = math::sin_cos_any(state.sled.heading_rad);
    const sled::HookForce hook{
        .forward_n = along_x_n * sled_heading.cos + along_y_n * sled_heading.sin,
        .up_n = along_z_n,
        .side_n = along_y_n * sled_heading.cos - along_x_n * sled_heading.sin};

    // The ground under each contact, by where it is along and across the
    // track. Along it the rear tires are taken at their axle and the front
    // tires a wheelbase ahead, the pan its distance ahead of the sled's
    // wheels: heading and pitch move them by centimetres, far less than the
    // soil changes over. Across it each rear side is half the rear track to
    // its side of the axle, the front pair and the pan ahead along their
    // heading: a spot under one side's wheels only is felt there.
    const track::Model& track = model.track();
    const chassis::State& body = state.tractor.chassis;
    const chassis::Params& frame = model.tractor().params().chassis;
    const double rear_axle_m = body.position_m - model.start_line_m();
    const double sled_wheels_m = state.sled.position_m - model.start_line_m();
    const double half_track_y_m = frame.rear_track_m / kSides * heading.cos;
    const vehicle::Ground tractor_ground{
        .rear_left = track.surface_at(rear_axle_m, body.lateral_position_m + half_track_y_m),
        .rear_right = track.surface_at(rear_axle_m, body.lateral_position_m - half_track_y_m),
        .front = track.surface_at(rear_axle_m + frame.wheelbase_m,
                                  body.lateral_position_m + frame.wheelbase_m * heading.sin)};
    const sled::Ground sled_ground{
        .pan = track.surface_across(
            sled_wheels_m + sled.pan_ahead_of_wheels_m,
            state.sled.lateral_position_m + sled.pan_ahead_of_wheels_m * sled_heading.sin,
            sled::kPanHalfWidth_m),
        .wheels = track.surface_at(sled_wheels_m, state.sled.lateral_position_m)};

    out.tractor =
        vehicle::step(state.tractor, model.tractor(), input, tractor_ground, drawbar, dt_s);

    out.sled = sled::loads(sled, state.sled, hook, sled_ground);
    sled::tilt(state.sled, out.sled, dt_s);
    switch (state.sled_mode) {
        case SledMode::kOnTheChain: {
            const sled::Prediction prediction =
                sled::predict(state.sled, sled, out.sled.resistance_n, dt_s);
            sled::advance_lateral(state.sled, sled, out.sled, hook.side_n, sled_ground.wheels,
                                  dt_s);
            sled::advance(state.sled, prediction, hook.forward_n, dt_s);
            sled::collect(state.sled, sled, out.tractor.thrown_soil_m3_s, dt_s);
            break;
        }
        case SledMode::kHeld:
            // Simplified: however hard the chain pulls, it stays put, its
            // hook too (the chain's damping reads its speed).
            state.sled.velocity_m_s = 0.0;
            state.sled.lateral_velocity_m_s = 0.0;
            state.sled.yaw_rate_rad_s = 0.0;
            break;
        case SledMode::kDrivingBack:
            sled::drive_back(state.sled, sled, model.sled_start_m(), model.start_y_m(), dt_s);
            break;
    }
    return out;
}

}  // namespace tp::pull
