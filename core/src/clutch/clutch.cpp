#include "tp/core/clutch/clutch.hpp"

#include <algorithm>
#include <cmath>

namespace tp::clutch {

namespace {

constexpr double kPedalReleased = 0.0;
constexpr double kPedalPressed = 1.0;

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    if (!std::isfinite(params.static_torque_capacity_nm) || !std::isfinite(params.kinetic_ratio)) {
        return ParamError::kNotFinite;
    }
    if (params.static_torque_capacity_nm <= 0.0) {
        return ParamError::kCapacityNotPositive;
    }
    if (params.kinetic_ratio <= 0.0 || params.kinetic_ratio > 1.0) {
        return ParamError::kKineticRatioOutOfRange;
    }
    const math::Curve& engagement = params.engagement;
    if (engagement.min_x() != kPedalReleased || engagement.max_x() != kPedalPressed) {
        return ParamError::kEngagementDoesNotSpanPedalTravel;
    }
    const auto points = engagement.points();
    if (std::ranges::any_of(points,
                            [](const math::CurvePoint& p) { return p.y < 0.0 || p.y > 1.0; })) {
        return ParamError::kEngagementOutOfRange;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (points[i].y > points[i - 1].y) {
            return ParamError::kEngagementIncreasesWithPedal;
        }
    }
    if (engagement.evaluate(kPedalReleased) != 1.0) {
        return ParamError::kEngagementNotFullWhenReleased;
    }
    if (engagement.evaluate(kPedalPressed) != 0.0) {
        return ParamError::kEngagementNotZeroWhenPressed;
    }
    return std::nullopt;
}

double capacity_nm(const Params& params, double pedal) noexcept {
    pedal = std::clamp(pedal, kPedalReleased, kPedalPressed);
    return params.engagement.evaluate(pedal) * params.static_torque_capacity_nm;
}

double resolve(State& state, const Params& params, double capacity_nm,
               double lock_torque_nm) noexcept {
    if (capacity_nm <= 0.0) {
        state.locked = false;
        return 0.0;
    }
    const double kinetic_capacity_nm = params.kinetic_ratio * capacity_nm;
    const double holding_capacity_nm = state.locked ? capacity_nm : kinetic_capacity_nm;
    if (std::abs(lock_torque_nm) <= holding_capacity_nm) {
        state.locked = true;
        return lock_torque_nm;
    }
    // Slipping: sliding friction opposes the slip that remains at the end of
    // the step, which has the sign of lock_torque_nm. The torque is smaller
    // in magnitude than lock_torque_nm, so slip cannot overshoot through zero.
    state.locked = false;
    return std::copysign(kinetic_capacity_nm, lock_torque_nm);
}

Output step(State& state, const Params& params, const Input& input,
            const coupling::ShaftPrediction& engine_side,
            const coupling::ShaftPrediction& driveline_side) noexcept {
    Output out;
    out.capacity_nm = capacity_nm(params, input.pedal);
    const double free_slip_rad_s =
        engine_side.free_omega_next_rad_s - driveline_side.free_omega_next_rad_s;

    if (out.capacity_nm <= 0.0) {
        state.locked = false;
        out.slip_rad_s = free_slip_rad_s;
        return out;
    }

    // End-of-step slip is linear in the clutch torque T (the engine gets -T,
    // the driveline +T): slip = free_slip - (k_engine + k_driveline) * T.
    // Solve for the torque that makes it zero. T has the sign of free_slip,
    // which picks each side's response to the torque it receives.
    const bool engine_drives = free_slip_rad_s >= 0.0;
    const double compliance =
        engine_drives ? engine_side.omega_per_negative_torque + driveline_side.omega_per_torque
                      : engine_side.omega_per_torque + driveline_side.omega_per_negative_torque;
    const double lock_torque_nm = free_slip_rad_s / compliance;
    out.torque_nm = resolve(state, params, out.capacity_nm, lock_torque_nm);
    if (state.locked) {
        return out;
    }
    out.slip_rad_s = free_slip_rad_s - compliance * out.torque_nm;
    out.slip_power_w = out.torque_nm * out.slip_rad_s;
    return out;
}

}  // namespace tp::clutch
