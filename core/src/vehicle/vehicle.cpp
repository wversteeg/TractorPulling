#include "tp/core/vehicle/vehicle.hpp"

#include "tp/core/vehicle/chain_solver.hpp"

namespace tp::vehicle {

std::optional<ParamError> validate(const Params& params) noexcept {
    if (engine::validate(params.engine)) {
        return ParamError::kEngine;
    }
    if (clutch::validate(params.clutch)) {
        return ParamError::kClutch;
    }
    if (driveline::validate(params.driveline)) {
        return ParamError::kDriveline;
    }
    if (tire::validate(params.rear_tire)) {
        return ParamError::kRearTire;
    }
    if (params.rear_tire_count < 1 || params.rear_tire_count > kMaxRearTires) {
        return ParamError::kRearTireCount;
    }
    if (chassis::validate(params.chassis)) {
        return ParamError::kChassis;
    }
    return std::nullopt;
}

Model::Model(const Params& params) noexcept : params_(params), driveline_(params.driveline) {
    driveline_.output_inertia_kg_m2 +=
        params.rear_tire_count * params.rear_tire.wheel_inertia_kg_m2;
}

std::optional<Model> Model::create(const Params& params) noexcept {
    if (validate(params)) {
        return std::nullopt;
    }
    return Model{params};
}

State initial_state(const Model& model) noexcept {
    return State{.engine = engine::initial_state(model.params().engine),
                 .clutch = {},
                 .driveline = {},
                 .chassis = {}};
}

Output step(State& state, const Model& model, const Input& input, double drawbar_force_n,
            double dt_s) noexcept {
    const Params& params = model.params();
    const driveline::Params& driveline_params = model.driveline();
    const tire::Params& tire_params = params.rear_tire;
    const double tires = params.rear_tire_count;
    Output out;

    const double clutch_capacity_nm = clutch::capacity_nm(params.clutch, input.clutch_pedal);
    if (input.gear != state.driveline.gear) {
        out.shift_refused = !driveline::shift(state.driveline, driveline_params, input.gear,
                                              clutch_capacity_nm == 0.0);
    }

    out.axle_loads =
        chassis::axle_loads(params.chassis, state.chassis.acceleration_m_s2, drawbar_force_n);
    const double load_per_tire_n = out.axle_loads.rear_n / tires;

    // Predictions. Rolling resistance acts as a known torque on the axle
    // (rear) and force on the body (front); like the drawbar pull, it holds
    // things at rest but never drives them backwards.
    const engine::Prediction engine_prediction =
        engine::predict(state.engine, params.engine, input.throttle, dt_s);
    const double rear_rolling_nm =
        tires * tire::rolling_resistance_n(tire_params, load_per_tire_n) * tire_params.radius_m;
    const driveline::Prediction driveline_prediction =
        driveline::predict(state.driveline, driveline_params, rear_rolling_nm, dt_s);
    const double front_rolling_n =
        params.chassis.front_rolling_resistance_coefficient * out.axle_loads.front_n;
    const chassis::Prediction body_prediction =
        chassis::predict(state.chassis, params.chassis, drawbar_force_n + front_rolling_n, dt_s);

    const double wheel_m_s = state.driveline.output_omega_rad_s * tire_params.radius_m;
    const double ground_m_s = state.chassis.velocity_m_s;
    const tire::Traction traction =
        tire::traction(tire_params, wheel_m_s, ground_m_s, load_per_tire_n);
    out.slip_ratio = traction.slip_ratio;
    const TireLine tire_line{.force_n = tires * traction.force_n,
                             .force_per_slip_speed = tires * traction.force_per_slip_speed,
                             .slip_speed_m_s = wheel_m_s - ground_m_s,
                             .radius_m = tire_params.radius_m};

    const ChainSolution solution = solve_chain(
        engine::shaft_prediction(engine_prediction), driveline::response(driveline_prediction),
        tire_line, body_prediction, state.clutch, params.clutch, clutch_capacity_nm);

    engine::advance(state.engine, params.engine, engine_prediction, solution.clutch_torque_nm);
    driveline::advance(state.driveline, driveline_prediction, solution.clutch_torque_nm,
                       -tire_params.radius_m * solution.tire_force_n);
    chassis::advance(state.chassis, body_prediction, solution.tire_force_n, dt_s);

    out.engine = engine_prediction.output;
    out.clutch_torque_nm = solution.clutch_torque_nm;
    out.clutch_slip_rad_s = state.engine.omega_rad_s - state.driveline.input_omega_rad_s;
    out.clutch_slip_power_w =
        state.clutch.locked ? 0.0 : out.clutch_torque_nm * out.clutch_slip_rad_s;
    out.tire_force_n = solution.tire_force_n;
    return out;
}

}  // namespace tp::vehicle
