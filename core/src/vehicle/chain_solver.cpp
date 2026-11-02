#include "tp/core/vehicle/chain_solver.hpp"

namespace tp::vehicle {

namespace {

// Responses to the clutch torque for one sign of it: power flowing from the
// engine (positive) or back into it (negative).
struct Branch {
    double engine;  // engine speed change per N·m of clutch torque (it receives -Tc)
    double input;   // driveline input speed change per N·m of clutch torque
    double axle;    // axle speed change per N·m of clutch torque
};

Branch branch(const coupling::ShaftPrediction& engine, const driveline::Response& d,
              bool positive) noexcept {
    if (positive) {
        return {engine.omega_per_negative_torque, d.input_per_clutch, d.axle_per_clutch};
    }
    return {engine.omega_per_torque, d.input_per_negative_clutch, d.axle_per_negative_clutch};
}

// The tire relation with the end-of-step speeds substituted:
//   F · a = b0 + b1 · Tc
struct TireEquation {
    double a;
    double b0;
    double b1;
};

TireEquation tire_equation(const driveline::Response& d, const TireLine& t,
                           const chassis::Prediction& body, const Branch& b) noexcept {
    const double c = t.force_per_slip_speed;
    const double r = t.radius_m;
    // The tire puts torque -r·F on the axle and force F on the body.
    return {1.0 + c * (r * r * d.axle_per_axle + body.velocity_per_n),
            t.force_n + c * (r * d.free_axle_rad_s - body.free_velocity_m_s - t.slip_speed_m_s),
            c * r * b.axle};
}

// Clutch torque that makes engine and input shaft speeds equal at the end of
// the step, with the tire force eliminated.
double lock_torque(const coupling::ShaftPrediction& engine, const driveline::Response& d,
                   const TireLine& t, const chassis::Prediction& body, bool positive) noexcept {
    const Branch b = branch(engine, d, positive);
    const TireEquation tire = tire_equation(d, t, body, b);
    const double r = t.radius_m;
    // engine' = free_e - k_e Tc;  input' = free_in + k_in Tc - k_in,axle r F.
    const double numerator =
        engine.free_omega_next_rad_s - d.free_input_rad_s + d.input_per_axle * r * tire.b0 / tire.a;
    const double denominator = b.engine + b.input - d.input_per_axle * r * tire.b1 / tire.a;
    return numerator / denominator;
}

}  // namespace

ChainSolution solve_chain(const coupling::ShaftPrediction& engine,
                          const driveline::Response& driveline, const TireLine& tire,
                          const chassis::Prediction& body, clutch::State& clutch_state,
                          const clutch::Params& clutch_params, double clutch_capacity_nm) noexcept {
    double lock_nm = lock_torque(engine, driveline, tire, body, true);
    if (lock_nm < 0.0) {
        lock_nm = lock_torque(engine, driveline, tire, body, false);
    }

    ChainSolution solution;
    solution.clutch_torque_nm =
        clutch::resolve(clutch_state, clutch_params, clutch_capacity_nm, lock_nm);

    const Branch b = branch(engine, driveline, solution.clutch_torque_nm >= 0.0);
    const TireEquation t = tire_equation(driveline, tire, body, b);
    solution.tire_force_n = (t.b0 + t.b1 * solution.clutch_torque_nm) / t.a;
    return solution;
}

}  // namespace tp::vehicle
