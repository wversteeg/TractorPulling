#include "tp/core/chassis/chassis.hpp"

#include "tp/core/math/trig.hpp"
#include "tp/core/units/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>

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

std::optional<ParamError> validate(const Params& params) noexcept {
    const WheelieBar& bar = params.wheelie_bar;
    const std::array scalars = {params.mass_kg,
                                params.wheelbase_m,
                                params.cg_ahead_of_rear_axle_m,
                                params.cg_height_m,
                                params.pitch_inertia_kg_m2,
                                params.rear_axle_height_m,
                                params.hitch_height_m,
                                params.hitch_behind_rear_axle_m,
                                params.front_tires.stiffness_n_per_m,
                                params.front_tires.damping_n_s_per_m,
                                bar.behind_rear_axle_m,
                                bar.tip_height_m,
                                bar.contact.stiffness_n_per_m,
                                bar.contact.damping_n_s_per_m,
                                bar.rolling_resistance_coefficient};
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
    if (params.rear_axle_height_m <= 0.0) {
        return ParamError::kRearAxleHeightNotPositive;
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
    return std::nullopt;
}

namespace {

Point hitch_offset(const Params& params, const SinCos& pitch) noexcept {
    return rotate(pitch, -params.hitch_behind_rear_axle_m,
                  params.hitch_height_m - params.rear_axle_height_m);
}

}  // namespace

Hitch hitch(const Params& params, const State& state) noexcept {
    const Point offset = hitch_offset(params, math::sin_cos(state.pitch_rad));
    // A point at (x, z) from the axle, turning nose-up at θ', moves at
    // (-z θ', x θ') relative to the axle.
    return {.x_m = state.position_m + offset.x_m,
            .z_m = params.rear_axle_height_m + offset.z_m,
            .velocity_x_m_s = state.velocity_m_s - offset.z_m * state.pitch_rate_rad_s,
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

Prediction predict(const State& state, const Params& params, double resisting_force_n,
                   double dt_s) noexcept {
    const double velocity_per_n = dt_s / params.mass_kg;
    return {.free_velocity_m_s = state.velocity_m_s - velocity_per_n * resisting_force_n,
            .velocity_per_n = velocity_per_n};
}

void advance(State& state, const Prediction& prediction, double drive_force_n,
             double dt_s) noexcept {
    const double velocity_m_s =
        std::max(prediction.free_velocity_m_s + prediction.velocity_per_n * drive_force_n, 0.0);
    state.acceleration_m_s2 = (velocity_m_s - state.velocity_m_s) / dt_s;
    state.velocity_m_s = velocity_m_s;
    // Semi-implicit Euler: position advances with the new velocity.
    state.position_m += velocity_m_s * dt_s;
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
    if (state.velocity_m_s > 0.0) {
        // Rolling drag of the front tires and the bar acts backwards at the
        // ground, below the axle: nose-down. Like all rolling resistance it
        // only acts while moving.
        const double drag_n = front_rolling_resistance * front.force_n +
                              params.wheelie_bar.rolling_resistance_coefficient * bar.force_n;
        moment_nm -= drag_n * params.rear_axle_height_m;
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
