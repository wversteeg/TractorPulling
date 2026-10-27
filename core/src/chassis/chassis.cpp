#include "tp/core/chassis/chassis.hpp"

#include "tp/core/math/trig.hpp"
#include "tp/core/units/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace tp::chassis {

namespace {

using math::SinCos;
using units::kStandardGravity_m_s2;

double weight_n(const Params& params) noexcept {
    return params.mass_kg * kStandardGravity_m_s2;
}

// Centre of gravity above the rear axle with the tractor level [m].
double cg_above_axle_m(const Params& params) noexcept {
    return params.cg_height_m - params.rear_axle_height_m;
}

// Wheelie bar tip relative to the rear axle with the tractor level [m].
double bar_tip_z_m(const Params& params) noexcept {
    return params.wheelie_bar.tip_height_m - params.rear_axle_height_m;
}

// A body point (x, z) relative to the rear axle, after pitching by θ.
struct Point {
    double x_m;
    double z_m;
};

Point rotate(const SinCos& pitch, double x_m, double z_m) noexcept {
    return {.x_m = x_m * pitch.cos - z_m * pitch.sin, .z_m = x_m * pitch.sin + z_m * pitch.cos};
}

// How far a contact point is pressed into the ground, and how that changes
// with pitch: δ and dδ/dθ.
struct Penetration {
    double depth_m;
    double per_rad_m;
};

Penetration front_tires(const Params& params, const SinCos& pitch) noexcept {
    // Preloaded so that the front tires carry their static share at θ = 0.
    const double static_front_n =
        weight_n(params) * params.cg_ahead_of_rear_axle_m / params.wheelbase_m;
    const double preload_m = static_front_n / params.front_tires.stiffness_n_per_m;
    // The front tires act at the front axle's x, at rear axle height.
    return {.depth_m = preload_m - params.wheelbase_m * pitch.sin,
            .per_rad_m = -params.wheelbase_m * pitch.cos};
}

Penetration wheelie_bar(const Params& params, const SinCos& pitch) noexcept {
    const double behind_m = params.wheelie_bar.behind_rear_axle_m;
    const double tip_z_m = bar_tip_z_m(params);
    const Point tip = rotate(pitch, -behind_m, tip_z_m);
    return {.depth_m = -(params.rear_axle_height_m + tip.z_m),
            .per_rad_m = behind_m * pitch.cos + tip_z_m * pitch.sin};
}

// A contact's vertical force, its moment about the rear axle, and that
// moment's derivatives in pitch and pitch rate, for the implicit step.
struct ContactLoad {
    double force_n = 0.0;
    double moment_nm = 0.0;
    double moment_per_rad = 0.0;
    double moment_per_rad_s = 0.0;
};

ContactLoad contact_load(const Contact& contact, const Penetration& p,
                         double pitch_rate_rad_s) noexcept {
    if (p.depth_m <= 0.0) {
        return {};
    }
    const double force_n = contact.stiffness_n_per_m * p.depth_m +
                           contact.damping_n_s_per_m * p.per_rad_m * pitch_rate_rad_s;
    if (force_n <= 0.0) {
        // Separating faster than the damper can follow: no grip on the ground.
        return {};
    }
    // The force lifts the point it acts on; pressing in further nose-up
    // (dδ/dθ > 0) means it acts nose-down, and vice versa.
    return {.force_n = force_n,
            .moment_nm = -p.per_rad_m * force_n,
            .moment_per_rad = -contact.stiffness_n_per_m * p.per_rad_m * p.per_rad_m,
            .moment_per_rad_s = -contact.damping_n_s_per_m * p.per_rad_m * p.per_rad_m};
}

}  // namespace

namespace {

// Where the ballast sits at `position`, ahead of the rear axle [m].
double ballast_x_m(const Ballast& ballast, double position) noexcept {
    return ballast.rear_ahead_of_rear_axle_m +
           position * (ballast.front_ahead_of_rear_axle_m - ballast.rear_ahead_of_rear_axle_m);
}

// Its mass less than the tractor's, its positions rear to front, the
// centre of gravity between the axles wherever the weights go, and the
// inertias positive there too.
bool ballast_valid(const Params& params) noexcept {
    const Ballast& ballast = params.ballast;
    if (ballast.mass_kg == 0.0) {
        return true;
    }
    if (ballast.mass_kg < 0.0 || ballast.mass_kg >= params.mass_kg ||
        ballast.rear_ahead_of_rear_axle_m >= ballast.front_ahead_of_rear_axle_m ||
        ballast.position < 0.0 || ballast.position > 1.0) {
        return false;
    }
    // The centre of gravity is linear in the weights' position: the ends
    // bound it.
    const bool cg_inside = std::ranges::all_of(std::array{0.0, 1.0}, [&params](double end) {
        const double cg_m = with_ballast_at(params, end).cg_ahead_of_rear_axle_m;
        return cg_m > 0.0 && cg_m < params.wheelbase_m;
    });
    // An inertia is I_rest + μ (x - c_rest)² with the weights at x, c_rest
    // the centre of gravity of the rest of the tractor and μ = m_b (M - m_b)
    // / M: lowest with the weights at c_rest, which may lie between the
    // ends. So the rest of the tractor's own inertia must be positive.
    const double mass_kg = params.mass_kg;
    const double built_m = ballast_x_m(ballast, ballast.position);
    const double rest_cg_m =
        (mass_kg * params.cg_ahead_of_rear_axle_m - ballast.mass_kg * built_m) /
        (mass_kg - ballast.mass_kg);
    const double reduced_kg = ballast.mass_kg * (mass_kg - ballast.mass_kg) / mass_kg;
    const double own_kg_m2 = reduced_kg * (built_m - rest_cg_m) * (built_m - rest_cg_m);
    return cg_inside && params.pitch_inertia_kg_m2 > own_kg_m2 &&
           params.yaw_inertia_kg_m2 > own_kg_m2;
}

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    const WheelieBar& bar = params.wheelie_bar;
    const std::array scalars = {params.mass_kg,
                                params.wheelbase_m,
                                params.cg_ahead_of_rear_axle_m,
                                params.cg_height_m,
                                params.pitch_inertia_kg_m2,
                                params.yaw_inertia_kg_m2,
                                params.rear_axle_height_m,
                                params.rear_track_m,
                                params.front_track_m,
                                params.hitch_height_m,
                                params.hitch_behind_rear_axle_m,
                                params.front_tires.stiffness_n_per_m,
                                params.front_tires.damping_n_s_per_m,
                                bar.behind_rear_axle_m,
                                bar.tip_height_m,
                                bar.contact.stiffness_n_per_m,
                                bar.contact.damping_n_s_per_m,
                                bar.rolling_resistance_coefficient,
                                params.ballast.mass_kg,
                                params.ballast.rear_ahead_of_rear_axle_m,
                                params.ballast.front_ahead_of_rear_axle_m,
                                params.ballast.position};
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (params.mass_kg <= 0.0) {
        return ParamError::kMassNotPositive;
    }
    if (params.wheelbase_m <= 0.0) {
        return ParamError::kWheelbaseNotPositive;
    }
    if (params.cg_ahead_of_rear_axle_m <= 0.0 ||
        params.cg_ahead_of_rear_axle_m >= params.wheelbase_m) {
        return ParamError::kCgOutsideWheelbase;
    }
    if (params.cg_height_m <= 0.0) {
        return ParamError::kCgHeightNotPositive;
    }
    if (params.pitch_inertia_kg_m2 <= 0.0) {
        return ParamError::kPitchInertiaNotPositive;
    }
    if (params.yaw_inertia_kg_m2 <= 0.0) {
        return ParamError::kYawInertiaNotPositive;
    }
    if (params.rear_axle_height_m <= 0.0) {
        return ParamError::kRearAxleHeightNotPositive;
    }
    if (params.rear_track_m <= 0.0) {
        return ParamError::kRearTrackNotPositive;
    }
    if (params.front_track_m <= 0.0) {
        return ParamError::kFrontTrackNotPositive;
    }
    if (params.hitch_height_m < 0.0) {
        return ParamError::kHitchHeightNegative;
    }
    if (params.hitch_behind_rear_axle_m < 0.0) {
        return ParamError::kHitchAheadOfRearAxle;
    }
    if (params.front_tires.stiffness_n_per_m <= 0.0) {
        return ParamError::kFrontTireStiffnessNotPositive;
    }
    if (params.front_tires.damping_n_s_per_m < 0.0) {
        return ParamError::kFrontTireDampingNegative;
    }
    if (bar.behind_rear_axle_m <= 0.0) {
        return ParamError::kWheelieBarNotBehindRearAxle;
    }
    if (bar.tip_height_m <= 0.0) {
        return ParamError::kWheelieBarTipNotAboveGround;
    }
    if (wheelie_bar(params, math::sin_cos(kMaxWheelieBarContactPitch_rad)).depth_m <= 0.0) {
        return ParamError::kWheelieBarTooHigh;
    }
    if (bar.contact.stiffness_n_per_m <= 0.0) {
        return ParamError::kWheelieBarStiffnessNotPositive;
    }
    if (bar.contact.damping_n_s_per_m < 0.0) {
        return ParamError::kWheelieBarDampingNegative;
    }
    if (bar.rolling_resistance_coefficient < 0.0 || bar.rolling_resistance_coefficient >= 1.0) {
        return ParamError::kWheelieBarRollingResistanceOutOfRange;
    }
    if (!ballast_valid(params)) {
        return ParamError::kBallastOutOfRange;
    }
    return std::nullopt;
}

Params with_ballast_at(const Params& params, double position) noexcept {
    const Ballast& ballast = params.ballast;
    if (!std::isfinite(position)) {
        return params;
    }
    const double moved_m = ballast_x_m(ballast, std::clamp(position, 0.0, 1.0));
    const double built_m = ballast_x_m(ballast, ballast.position);
    if (ballast.mass_kg <= 0.0 || moved_m == built_m) {
        return params;
    }
    Params out = params;
    const double cg_m = params.cg_ahead_of_rear_axle_m;
    const double new_cg_m = cg_m + ballast.mass_kg * (moved_m - built_m) / params.mass_kg;
    // About the old centre of gravity only the ballast's term changes; then
    // the parallel axis theorem moves the sum to the new one. The weights
    // sit at the centre of gravity's height, so only x enters.
    const double about_old_cg = ballast.mass_kg * ((moved_m - cg_m) * (moved_m - cg_m) -
                                                   (built_m - cg_m) * (built_m - cg_m));
    const double shift = params.mass_kg * (new_cg_m - cg_m) * (new_cg_m - cg_m);
    out.cg_ahead_of_rear_axle_m = new_cg_m;
    out.pitch_inertia_kg_m2 = params.pitch_inertia_kg_m2 + about_old_cg - shift;
    out.yaw_inertia_kg_m2 = params.yaw_inertia_kg_m2 + about_old_cg - shift;
    out.ballast.position = std::clamp(position, 0.0, 1.0);
    return out;
}

Params with_ballast_mass(const Params& params, double mass_kg) noexcept {
    const Ballast& ballast = params.ballast;
    if (!std::isfinite(mass_kg) || mass_kg < 0.0 || mass_kg == ballast.mass_kg) {
        return params;
    }
    const double added_kg = mass_kg - ballast.mass_kg;
    const double at_m = ballast_x_m(ballast, ballast.position);
    const double cg_m = params.cg_ahead_of_rear_axle_m;
    const double total_kg = params.mass_kg + added_kg;
    const double new_cg_m = (params.mass_kg * cg_m + added_kg * at_m) / total_kg;
    // The added weights about the new centre of gravity, and the rest moved
    // to it (parallel axes); taking weights off is the same with a minus.
    const double moved = params.mass_kg * (cg_m - new_cg_m) * (cg_m - new_cg_m) +
                         added_kg * (at_m - new_cg_m) * (at_m - new_cg_m);
    Params out = params;
    out.mass_kg = total_kg;
    out.cg_ahead_of_rear_axle_m = new_cg_m;
    out.pitch_inertia_kg_m2 = params.pitch_inertia_kg_m2 + moved;
    out.yaw_inertia_kg_m2 = params.yaw_inertia_kg_m2 + moved;
    out.ballast.mass_kg = mass_kg;
    return out;
}

AxleLoads axle_loads_at_rest(const Params& params) noexcept {
    return axle_loads(params, State{}, Drawbar{});
}

double rear_axle_share(const Params& params) noexcept {
    return 1.0 - params.cg_ahead_of_rear_axle_m / params.wheelbase_m;
}

namespace {

Point hitch_offset(const Params& params, const SinCos& pitch) noexcept {
    return rotate(pitch, -params.hitch_behind_rear_axle_m,
                  params.hitch_height_m - params.rear_axle_height_m);
}

}  // namespace

Hitch hitch(const Params& params, const State& state) noexcept {
    const Point offset = hitch_offset(params, math::sin_cos(state.pitch_rad));
    const SinCos heading = math::sin_cos_any(state.heading_rad);
    // A point at (x, z) from the axle, turning nose-up at θ', moves at
    // (-z θ', x θ') relative to the axle; turning left at r, it moves
    // sideways at x r. Body-frame speeds, then turned into the world.
    const double forward_m_s = state.velocity_m_s - offset.z_m * state.pitch_rate_rad_s;
    const double side_m_s = state.lateral_velocity_m_s + offset.x_m * state.yaw_rate_rad_s;
    return {.x_m = state.position_m + offset.x_m * heading.cos,
            .y_m = state.lateral_position_m + offset.x_m * heading.sin,
            .z_m = params.rear_axle_height_m + offset.z_m,
            .velocity_x_m_s = forward_m_s * heading.cos - side_m_s * heading.sin,
            .velocity_y_m_s = forward_m_s * heading.sin + side_m_s * heading.cos,
            .velocity_z_m_s = offset.x_m * state.pitch_rate_rad_s};
}

AxleLoads axle_loads(const Params& params, const State& state, const Drawbar& drawbar) noexcept {
    const SinCos pitch = math::sin_cos(state.pitch_rad);
    const Penetration front = front_tires(params, pitch);
    const double front_n = contact_load(params.front_tires, front, state.pitch_rate_rad_s).force_n;
    const double bar_n =
        contact_load(params.wheelie_bar.contact, wheelie_bar(params, pitch), state.pitch_rate_rad_s)
            .force_n;

    // Vertical acceleration of the centre of gravity as it swings about the
    // rear axle: z'' = θ'' x - θ'² z.
    const Point cg = rotate(pitch, params.cg_ahead_of_rear_axle_m, cg_above_axle_m(params));
    const double cg_vertical_m_s2 = state.pitch_acceleration_rad_s2 * cg.x_m -
                                    state.pitch_rate_rad_s * state.pitch_rate_rad_s * cg.z_m;
    const double rear_n =
        weight_n(params) + params.mass_kg * cg_vertical_m_s2 - front_n - bar_n - drawbar.lift_n;

    return {.rear_n = std::max(rear_n, 0.0),
            .front_n = front_n,
            .wheelie_bar_n = bar_n,
            .front_lifted = front.depth_m <= 0.0};
}

Prediction predict(const State& state, const Params& params, double resisting_force_n, double dt_s,
                   double direction) noexcept {
    const double velocity_per_n = dt_s / params.mass_kg;
    const double cg_side_m_s =
        state.lateral_velocity_m_s + params.cg_ahead_of_rear_axle_m * state.yaw_rate_rad_s;
    return {.free_velocity_m_s = state.velocity_m_s - velocity_per_n * resisting_force_n +
                                 dt_s * cg_side_m_s * state.yaw_rate_rad_s,
            .velocity_per_n = velocity_per_n,
            .direction = direction};
}

void advance_lateral(State& state, const Params& params, const SideForces& forces,
                     double dt_s) noexcept {
    const double b_m = params.cg_ahead_of_rear_axle_m;
    // Yaw moments about the centre of gravity, left turn positive. The
    // hitch's pitch is left out: it moves the hitch by centimetres.
    const double moment_nm = forces.front_n * (params.wheelbase_m - b_m) - forces.rear_n * b_m -
                             forces.hitch_n * (b_m + params.hitch_behind_rear_axle_m) +
                             forces.yaw_moment_nm;
    const double side_n = forces.front_n + forces.rear_n + forces.hitch_n;
    const double cg_side_m_s = state.lateral_velocity_m_s + b_m * state.yaw_rate_rad_s;

    const double yaw_rate_rad_s =
        state.yaw_rate_rad_s + dt_s * moment_nm / params.yaw_inertia_kg_m2;
    const double next_cg_side_m_s =
        cg_side_m_s + dt_s * (side_n / params.mass_kg - state.velocity_m_s * state.yaw_rate_rad_s);
    state.yaw_rate_rad_s = yaw_rate_rad_s;
    state.lateral_velocity_m_s = next_cg_side_m_s - b_m * yaw_rate_rad_s;
    // Kept within ±π, where sin_cos_any() works.
    constexpr double kFullTurn_rad = 2.0 * std::numbers::pi;
    double heading_rad = state.heading_rad + yaw_rate_rad_s * dt_s;
    if (heading_rad > std::numbers::pi) {
        heading_rad -= kFullTurn_rad;
    } else if (heading_rad < -std::numbers::pi) {
        heading_rad += kFullTurn_rad;
    }
    state.heading_rad = heading_rad;
}

double end_velocity_m_s(const Prediction& prediction, double drive_force_n) noexcept {
    const double velocity_m_s =
        prediction.free_velocity_m_s + prediction.velocity_per_n * drive_force_n;
    return prediction.direction > 0.0 ? std::max(velocity_m_s, 0.0) : std::min(velocity_m_s, 0.0);
}

void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept {
    const double velocity_m_s = end_velocity_m_s(prediction, drive_force_n);
    state.acceleration_m_s2 = (velocity_m_s - state.velocity_m_s) / dt_s;
    state.velocity_m_s = velocity_m_s;
    // Semi-implicit Euler: position advances with the new velocities.
    const SinCos heading = math::sin_cos_any(state.heading_rad);
    state.position_m +=
        (velocity_m_s * heading.cos - state.lateral_velocity_m_s * heading.sin) * dt_s;
    state.lateral_position_m +=
        (velocity_m_s * heading.sin + state.lateral_velocity_m_s * heading.cos) * dt_s;
}

void advance_pitch(State& state, const Params& params, double axle_torque_nm,
                   const Drawbar& drawbar, double front_rolling_resistance, double dt_s) noexcept {
    const SinCos pitch = math::sin_cos(state.pitch_rad);
    const double b_m = params.cg_ahead_of_rear_axle_m;
    const double h_m = cg_above_axle_m(params);
    const Point cg = rotate(pitch, b_m, h_m);
    const Point hitch = hitch_offset(params, pitch);
    const double inertia_kg_m2 =
        params.pitch_inertia_kg_m2 + params.mass_kg * (b_m * b_m + h_m * h_m);

    const ContactLoad front =
        contact_load(params.front_tires, front_tires(params, pitch), state.pitch_rate_rad_s);
    const ContactLoad bar = contact_load(params.wheelie_bar.contact, wheelie_bar(params, pitch),
                                         state.pitch_rate_rad_s);

    // Moment of a force (F_x, F_z) at (x, z) from the axle: x F_z - z F_x.
    // The drawbar pulls back (F_x = -pull) and lifts (F_z = lift) at the hitch.
    double moment_nm = axle_torque_nm + drawbar.pull_n * hitch.z_m + drawbar.lift_n * hitch.x_m -
                       weight_n(params) * cg.x_m +
                       params.mass_kg * state.acceleration_m_s2 * cg.z_m + front.moment_nm +
                       bar.moment_nm;
    if (state.velocity_m_s != 0.0) {
        // Rolling drag of the front tires and the bar acts against the
        // motion at the ground, below the axle: nose-down going forwards,
        // nose-up backwards. Like all rolling resistance it only acts while
        // moving.
        const double drag_n = front_rolling_resistance * front.force_n +
                              params.wheelie_bar.rolling_resistance_coefficient * bar.force_n;
        moment_nm -= std::copysign(drag_n, state.velocity_m_s) * params.rear_axle_height_m;
    }

    // Implicit midpoint on the linearized contacts, explicit in everything
    // else: with Δθ = dt (ω + ω') / 2,
    //
    //   I (ω' - ω) / dt = M + K Δθ / 2 + C (ω' - ω) / 2
    //
    // where K and C (both ≤ 0) are the contacts' moment derivatives in pitch
    // and pitch rate. Stable for any contact stiffness, and unlike backward
    // Euler it keeps a spring's energy instead of damping it numerically.
    const double stiffness = front.moment_per_rad + bar.moment_per_rad;
    const double damping = front.moment_per_rad_s + bar.moment_per_rad_s;
    const double rate_rad_s = state.pitch_rate_rad_s;
    const double next_rate_rad_s =
        (inertia_kg_m2 / dt_s * rate_rad_s + moment_nm + 0.25 * stiffness * dt_s * rate_rad_s -
         0.5 * damping * rate_rad_s) /
        (inertia_kg_m2 / dt_s - 0.25 * stiffness * dt_s - 0.5 * damping);

    state.pitch_acceleration_rad_s2 = (next_rate_rad_s - rate_rad_s) / dt_s;
    state.pitch_rate_rad_s = next_rate_rad_s;
    // The bar holds pitch far inside sin_cos's range; the clamp only guards
    // it against absurd inputs.
    state.pitch_rad = std::clamp(state.pitch_rad + 0.5 * (rate_rad_s + next_rate_rad_s) * dt_s,
                                 -math::kSinCosMaxAngle_rad, math::kSinCosMaxAngle_rad);
}

}  // namespace tp::chassis
