#include "tp/core/driveline/driveline.hpp"

#include <algorithm>
#include <cmath>
#include <span>

namespace tp::driveline {

namespace {

std::span<const double> ratios(const Params& params) noexcept {
    return {params.gear_ratios.data(), std::min(params.gear_count, kMaxGears)};
}

// Speed of a shaft at the end of a step under constant `torque_nm`, with the
// viscous drag integrated implicitly, as a line free + k * extra torque.
struct Line {
    double free_rad_s;
    double per_nm;
};

Line integrate(double omega_rad_s, double inertia_kg_m2, double torque_nm, double viscous_nm_s_rad,
               double dt_s) noexcept {
    const double dt_over_inertia = dt_s / inertia_kg_m2;
    const double divisor = 1.0 + dt_over_inertia * viscous_nm_s_rad;
    return {(omega_rad_s + dt_over_inertia * torque_nm) / divisor, dt_over_inertia / divisor};
}

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    const std::array scalars = {params.final_drive_ratio,   params.efficiency,
                                params.input_inertia_kg_m2, params.output_inertia_kg_m2,
                                params.drag_torque_nm,      params.viscous_drag_nm_s_rad};
    if (params.gear_count == 0) {
        return ParamError::kNoGears;
    }
    if (params.gear_count > kMaxGears) {
        return ParamError::kTooManyGears;
    }
    const auto gears = ratios(params);
    if (!std::ranges::all_of(scalars, [](double v) { return std::isfinite(v); }) ||
        !std::ranges::all_of(gears, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (std::ranges::any_of(gears, [](double r) { return r <= 0.0; })) {
        return ParamError::kRatioNotPositive;
    }
    for (std::size_t i = 1; i < gears.size(); ++i) {
        if (!(gears[i] < gears[i - 1])) {
            return ParamError::kRatiosNotDecreasing;
        }
    }
    if (params.final_drive_ratio <= 0.0) {
        return ParamError::kFinalDriveNotPositive;
    }
    if (params.efficiency <= 0.0 || params.efficiency > 1.0) {
        return ParamError::kEfficiencyOutOfRange;
    }
    if (params.input_inertia_kg_m2 <= 0.0 || params.output_inertia_kg_m2 <= 0.0) {
        return ParamError::kInertiaNotPositive;
    }
    if (params.drag_torque_nm < 0.0 || params.viscous_drag_nm_s_rad < 0.0) {
        return ParamError::kDragNegative;
    }
    if (params.load_downshift_gears < 0 ||
        static_cast<std::size_t>(params.load_downshift_gears) >= params.gear_count) {
        return ParamError::kLoadDownshiftOutOfRange;
    }
    return std::nullopt;
}

double overall_ratio(const Params& params, int gear) noexcept {
    const auto gears = ratios(params);
    if (gear <= kNeutral || static_cast<std::size_t>(gear) > gears.size()) {
        return 0.0;
    }
    return gears[static_cast<std::size_t>(gear) - 1] * params.final_drive_ratio;
}

bool shifts_under_load(const Params& params, int from, int to) noexcept {
    return to > kNeutral && to < from && from - to <= params.load_downshift_gears &&
           static_cast<std::size_t>(from) <= ratios(params).size();
}

bool shift(State& state, const Params& params, int gear, bool clutch_disengaged) noexcept {
    if (gear == state.gear) {
        return true;
    }
    if (gear < kNeutral || static_cast<std::size_t>(gear) > ratios(params).size() ||
        (!clutch_disengaged && !shifts_under_load(params, state.gear, gear))) {
        return false;
    }
    state.gear = gear;
    if (gear != kNeutral) {
        // Perfectly inelastic coupling of the two sides through ratio G, in
        // axle terms: (I_out + I_in G²) ω = I_out ω_out + I_in G ω_in.
        const double ratio = overall_ratio(params, gear);
        const double momentum = params.output_inertia_kg_m2 * state.output_omega_rad_s +
                                params.input_inertia_kg_m2 * ratio * state.input_omega_rad_s;
        state.output_omega_rad_s =
            momentum / (params.output_inertia_kg_m2 + params.input_inertia_kg_m2 * ratio * ratio);
        state.input_omega_rad_s = state.output_omega_rad_s * ratio;
    }
    return true;
}

Prediction predict(const State& state, const Params& params, double axle_load_torque_nm,
                   double dt_s) noexcept {
    Prediction p;
    p.ratio = overall_ratio(params, state.gear);
    p.efficiency = params.efficiency;

    // Constant drag opposes rotation; a shaft at rest is held, not reversed
    // (see the clamp in advance()).
    const double drag_nm = state.input_omega_rad_s > 0.0 ? params.drag_torque_nm : 0.0;

    if (p.ratio == 0.0) {
        const Line input = integrate(state.input_omega_rad_s, params.input_inertia_kg_m2, -drag_nm,
                                     params.viscous_drag_nm_s_rad, dt_s);
        p.free_input_rad_s = input.free_rad_s;
        p.input_per_nm = input.per_nm;
        p.output_per_nm = dt_s / params.output_inertia_kg_m2;
        p.free_output_rad_s = state.output_omega_rad_s - p.output_per_nm * axle_load_torque_nm;
        return p;
    }

    // In gear, the whole train as seen from the input shaft. Only the clutch
    // torque passes the meshes with losses; the axle load acts on the train
    // directly.
    const double inertia_kg_m2 =
        params.input_inertia_kg_m2 + params.output_inertia_kg_m2 / (p.ratio * p.ratio);
    const Line input =
        integrate(state.input_omega_rad_s, inertia_kg_m2, -axle_load_torque_nm / p.ratio - drag_nm,
                  params.viscous_drag_nm_s_rad, dt_s);
    p.free_input_rad_s = input.free_rad_s;
    p.input_per_nm = input.per_nm;
    return p;
}

coupling::ShaftPrediction input_shaft(const Prediction& p) noexcept {
    if (p.ratio == 0.0) {
        return {.free_omega_next_rad_s = p.free_input_rad_s,
                .omega_per_torque = p.input_per_nm,
                .omega_per_negative_torque = p.input_per_nm};
    }
    return {.free_omega_next_rad_s = p.free_input_rad_s,
            .omega_per_torque = p.input_per_nm * p.efficiency,
            .omega_per_negative_torque = p.input_per_nm / p.efficiency};
}

Response response(const Prediction& p) noexcept {
    const coupling::ShaftPrediction input = input_shaft(p);
    Response r;
    r.free_input_rad_s = input.free_omega_next_rad_s;
    r.input_per_clutch = input.omega_per_torque;
    r.input_per_negative_clutch = input.omega_per_negative_torque;
    if (p.ratio == 0.0) {
        r.free_axle_rad_s = p.free_output_rad_s;
        r.axle_per_axle = p.output_per_nm;
        return r;
    }
    // Rigid train: the axle turns at input / G, and torque at the axle acts
    // on the input shaft as τ / G (no mesh losses on that path, as for the
    // axle load in predict()).
    r.input_per_axle = p.input_per_nm / p.ratio;
    r.free_axle_rad_s = r.free_input_rad_s / p.ratio;
    r.axle_per_clutch = r.input_per_clutch / p.ratio;
    r.axle_per_negative_clutch = r.input_per_negative_clutch / p.ratio;
    r.axle_per_axle = r.input_per_axle / p.ratio;
    return r;
}

void advance(State& state, const Prediction& p, double clutch_torque_nm,
             double axle_torque_nm) noexcept {
    if (p.ratio == 0.0) {
        state.input_omega_rad_s =
            std::max(p.free_input_rad_s + p.input_per_nm * clutch_torque_nm, 0.0);
        state.output_omega_rad_s =
            std::max(p.free_output_rad_s + p.output_per_nm * axle_torque_nm, 0.0);
        return;
    }
    const double effective_nm =
        clutch_torque_nm >= 0.0 ? clutch_torque_nm * p.efficiency : clutch_torque_nm / p.efficiency;
    // Resistance holds the train at rest but never drives it backwards; there
    // is no reverse gear.
    state.input_omega_rad_s = std::max(p.free_input_rad_s + p.input_per_nm * effective_nm +
                                           p.input_per_nm / p.ratio * axle_torque_nm,
                                       0.0);
    state.output_omega_rad_s = state.input_omega_rad_s / p.ratio;
}

}  // namespace tp::driveline
