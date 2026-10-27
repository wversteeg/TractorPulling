#include "tp/core/vehicle/vehicle.hpp"

#include "tp/core/math/trig.hpp"
#include "tp/core/vehicle/chain_solver.hpp"

#include <algorithm>
#include <cmath>

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
    if (params.rear_tire_count < 2 || params.rear_tire_count > kMaxRearTires ||
        params.rear_tire_count % 2 != 0) {
        return ParamError::kRearTireCount;
    }
    if (tire::validate(params.front_tire)) {
        return ParamError::kFrontTire;
    }
    if (!std::isfinite(params.steering.max_angle_rad) || params.steering.max_angle_rad <= 0.0 ||
        params.steering.max_angle_rad > math::kSinCosMaxAngle_rad ||
        tire::validate(params.steering.front_tires)) {
        return ParamError::kSteering;
    }
    if (!std::isfinite(params.brakes.max_torque_nm) || params.brakes.max_torque_nm < 0.0) {
        return ParamError::kBrakes;
    }
    if (chassis::validate(params.chassis)) {
        return ParamError::kChassis;
    }
    if (params.chassis.rear_axle_height_m != params.rear_tire.size.radius_m) {
        return ParamError::kRearAxleHeight;
    }
    if (!std::isfinite(params.pto_efficiency) || params.pto_efficiency <= 0.0 ||
        params.pto_efficiency > 1.0) {
        return ParamError::kPtoEfficiencyOutOfRange;
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

namespace {

// Front tires come in a pair.
constexpr double kFrontTires = 2.0;
// The rear tires are split between two sides.
constexpr double kSides = 2.0;

// One side's rear tires at the current speeds: traction per tire,
// linearized for the solve, and the motion resistance its slip digs.
struct Side {
    double wheel_m_s = 0.0;
    double ground_m_s = 0.0;
    tire::Traction traction;
    double rolling_resistance = 0.0;
};

Side rear_side(const tire::Params& tire, const tire::Footing& footing, double omega_rad_s,
               double ground_m_s, double load_per_tire_n, const soil::Surface& surface) noexcept {
    Side side;
    side.wheel_m_s = omega_rad_s * tire.size.radius_m;
    side.ground_m_s = ground_m_s;
    side.traction =
        tire::traction(tire, footing, side.wheel_m_s, ground_m_s, load_per_tire_n, surface);
    side.rolling_resistance =
        load_per_tire_n > 0.0 ? tire::driven_rolling_resistance(footing, side.traction.slip_ratio)
                              : 0.0;
    return side;
}

// Everything the rear solve needs that the brakes do not change.
//
// The open differential gives both sides the same torque. Its two motions
// separate: the axle (the sides' mean speed, driven through the gearbox) and
// the difference d (half the right side's speed minus the left's), which
// only the sides' unequal traction and brakes drive:
//
//   2 I_s d' = -r (F_R - F_L) - (B_R - B_L)
//
// with I_s one side's wheel inertia. Each side's traction is a line in its
// own slip speed change, F = f + k (c ∓ r Δd) for the common change c, so
// over a step Δd = a0 + a1 c, solved implicitly; the total traction is then
// again a line in c, which the chain solve takes as one tire.
struct RearProblem {
    coupling::ShaftPrediction engine;
    chassis::Prediction body;
    double rolling_nm = 0.0;
    double clutch_capacity_nm = 0.0;
    double side_inertia_kg_m2 = 0.0;
    // Each side's traction (all its tires) and its slope per slip speed.
    double force_left_n = 0.0;
    double force_right_n = 0.0;
    double slope_left = 0.0;
    double slope_right = 0.0;
    // Mean wheel surface speed minus ground speed [m/s].
    double common_slip_m_s = 0.0;
    double dt_s = 0.0;
};

struct RearSolution {
    driveline::Prediction driveline;
    ChainSolution chain;
    clutch::State clutch;
    double brake_left_nm = 0.0;
    double brake_right_nm = 0.0;
    // Change of the common slip speed over the step [m/s], and of the
    // differential [rad/s].
    double slip_change_m_s = 0.0;
    double differential_change_rad_s = 0.0;
    // End-of-step speeds of the axle and both sides' wheels [rad/s].
    double axle_rad_s = 0.0;
    double left_rad_s = 0.0;
    double right_rad_s = 0.0;
};

RearSolution solve_rear(const State& state, const Model& model, const RearProblem& p,
                        double brake_left_nm, double brake_right_nm) noexcept {
    const Params& params = model.params();
    const double r = params.rear_tire.size.radius_m;
    RearSolution out;
    out.brake_left_nm = brake_left_nm;
    out.brake_right_nm = brake_right_nm;
    out.driveline = driveline::predict(state.driveline, model.driveline(),
                                       p.rolling_nm + brake_left_nm + brake_right_nm, p.dt_s);

    const double slope_difference = p.slope_right - p.slope_left;
    const double denominator =
        2.0 * p.side_inertia_kg_m2 / p.dt_s + r * r * (p.slope_left + p.slope_right);
    const double a0 =
        (-r * (p.force_right_n - p.force_left_n) - (brake_right_nm - brake_left_nm)) / denominator;
    const double a1 = -r * slope_difference / denominator;
    const TireLine line{
        .force_n = p.force_left_n + p.force_right_n + r * slope_difference * a0,
        .force_per_slip_speed = p.slope_left + p.slope_right + r * slope_difference * a1,
        .slip_speed_m_s = p.common_slip_m_s,
        .radius_m = r};

    out.clutch = state.clutch;
    out.chain = solve_chain(p.engine, driveline::response(out.driveline), line, p.body, out.clutch,
                            params.clutch, p.clutch_capacity_nm);

    // End-of-step speeds as advancing would set them (see chassis::advance).
    driveline::State driveline = state.driveline;
    driveline::advance(driveline, out.driveline, out.chain.clutch_torque_nm,
                       -r * out.chain.tire_force_n);
    const double ground_m_s = chassis::end_velocity_m_s(p.body, out.chain.tire_force_n);
    out.axle_rad_s = driveline.output_omega_rad_s;
    out.slip_change_m_s = r * out.axle_rad_s - ground_m_s - p.common_slip_m_s;
    out.differential_change_rad_s = a0 + a1 * out.slip_change_m_s;
    const double differential_rad_s = state.differential_rad_s + out.differential_change_rad_s;
    out.left_rad_s = out.axle_rad_s - differential_rad_s;
    out.right_rad_s = out.axle_rad_s + differential_rad_s;
    return out;
}

// A brake resists its side's rotation, whichever way it turns, with up to
// its capacity. Where that would carry the side through rest, it holds the
// side at rest with just the torque that takes instead: Coulomb friction,
// like the clutch. The sides' end speeds are piecewise affine in the brake
// torques (a clutch changing state, the axle coming to rest), so Newton
// steps on the held sides' end speeds find those torques, exactly once the
// torque is in the right piece; each step's slopes come from one more
// solve per held side. A single held side, once a torque that leaves it
// turning forwards and one that leaves it turning backwards are known, is
// narrowed between them instead (Illinois), which a kink cannot throw off.
constexpr int kMaxHoldSteps = 8;

struct Brake {
    double capacity_nm = 0.0;
    double torque_nm = 0.0;
    bool held = false;
};

// Kinetic friction against the side's rotation; a side at rest is first
// tried turning forwards.
Brake kinetic(double capacity_nm, double omega_rad_s) noexcept {
    return {.capacity_nm = capacity_nm,
            .torque_nm = omega_rad_s < -kBrakeHoldTolerance_rad_s ? -capacity_nm : capacity_nm,
            .held = false};
}

// Two brake torques around the one that holds a side at rest: one that
// leaves it turning forwards, one that leaves it turning backwards.
struct Bracket {
    double forward_nm = 0.0;
    double forward_end_rad_s = 0.0;
    double backward_nm = 0.0;
    double backward_end_rad_s = 0.0;
    bool has_forward = false;
    bool has_backward = false;
    // Which end the last torque replaced: +1 forward, -1 backward.
    int last = 0;
};

// Keeps the torque as one end; where the same end is replaced twice in a
// row, the other end's speed is halved (Illinois), so the guesses close in
// from both sides instead of creeping up on one.
void narrow(Bracket& bracket, double torque_nm, double end_rad_s) noexcept {
    if (end_rad_s > kBrakeHoldTolerance_rad_s) {
        if (bracket.last > 0 && bracket.has_backward) {
            bracket.backward_end_rad_s *= 0.5;
        }
        bracket.forward_nm = torque_nm;
        bracket.forward_end_rad_s = end_rad_s;
        bracket.has_forward = true;
        bracket.last = 1;
    } else if (end_rad_s < -kBrakeHoldTolerance_rad_s) {
        if (bracket.last < 0 && bracket.has_forward) {
            bracket.forward_end_rad_s *= 0.5;
        }
        bracket.backward_nm = torque_nm;
        bracket.backward_end_rad_s = end_rad_s;
        bracket.has_backward = true;
        bracket.last = -1;
    }
}

// The torque where the line through the bracket's ends crosses rest.
double false_position_nm(const Bracket& b) noexcept {
    return (b.forward_nm * b.backward_end_rad_s - b.backward_nm * b.forward_end_rad_s) /
           (b.backward_end_rad_s - b.forward_end_rad_s);
}

// The brake would drive the side through rest: it should hold it instead.
bool carries_through_rest(const Brake& brake, double end_rad_s) noexcept {
    return (brake.torque_nm > 0.0 && end_rad_s < 0.0) || (brake.torque_nm < 0.0 && end_rad_s > 0.0);
}

// Free, held at rest, or slipping at capacity the way the brake resists.
bool settled(const Brake& brake, double end_rad_s) noexcept {
    return !brake.held || std::abs(end_rad_s) <= kBrakeHoldTolerance_rad_s ||
           (std::abs(brake.torque_nm) == brake.capacity_nm &&
            !carries_through_rest(brake, end_rad_s));
}

// How a solve's end speeds change per N·m of one side's brake.
struct Slopes {
    double left = 0.0;
    double right = 0.0;
};

// Share of a brake's capacity a probe moves its torque by: small, so the
// slope it measures is the one where the torque is now.
constexpr double kProbeShare = 0.01;

// A probe eases a brake towards zero torque.
double probe_nm(const Brake& brake) noexcept {
    const double h_nm = kProbeShare * brake.capacity_nm;
    return brake.torque_nm > 0.0 ? -h_nm : h_nm;
}

Slopes slopes(const State& state, const Model& model, const RearProblem& p, const RearSolution& at,
              double left_nm, double right_nm) noexcept {
    const double h_nm = left_nm != 0.0 ? left_nm : right_nm;
    const RearSolution s =
        solve_rear(state, model, p, at.brake_left_nm + left_nm, at.brake_right_nm + right_nm);
    return {.left = (s.left_rad_s - at.left_rad_s) / h_nm,
            .right = (s.right_rad_s - at.right_rad_s) / h_nm};
}

// Newton step: the brake torque changes that bring the held sides to rest.
struct Change {
    double left_nm = 0.0;
    double right_nm = 0.0;
};

Change hold_step(const Brake& left, const Brake& right, const RearSolution& at,
                 const Slopes& per_left, const Slopes& per_right) noexcept {
    if (left.held && right.held) {
        const double determinant =
            per_left.left * per_right.right - per_right.left * per_left.right;
        if (determinant == 0.0) {
            return {};
        }
        return {.left_nm = (-at.left_rad_s * per_right.right + at.right_rad_s * per_right.left) /
                           determinant,
                .right_nm = (-at.right_rad_s * per_left.left + at.left_rad_s * per_left.right) /
                            determinant};
    }
    if (left.held && per_left.left != 0.0) {
        return {.left_nm = -at.left_rad_s / per_left.left};
    }
    if (right.held && per_right.right != 0.0) {
        return {.right_nm = -at.right_rad_s / per_right.right};
    }
    return {};
}

// The next change to the held sides' brake torques: false position for a
// single held side once it is bracketed, a Newton step otherwise.
Change hold_change(const State& state, const Model& model, const RearProblem& p,
                   const RearSolution& solution, const Brake& left, const Brake& right,
                   const Bracket& left_bracket, const Bracket& right_bracket) noexcept {
    if (left.held != right.held) {
        const Bracket& bracket = left.held ? left_bracket : right_bracket;
        if (bracket.has_forward && bracket.has_backward) {
            const double nm = false_position_nm(bracket);
            return left.held ? Change{.left_nm = nm - left.torque_nm}
                             : Change{.right_nm = nm - right.torque_nm};
        }
    }
    const Slopes per_left =
        left.held ? slopes(state, model, p, solution, probe_nm(left), 0.0) : Slopes{};
    const Slopes per_right =
        right.held ? slopes(state, model, p, solution, 0.0, probe_nm(right)) : Slopes{};
    return hold_step(left, right, solution, per_left, per_right);
}

RearSolution solve_rear_with_brakes(const State& state, const Model& model, const RearProblem& p,
                                    double left_capacity_nm, double right_capacity_nm) noexcept {
    const double axle_rad_s = state.driveline.output_omega_rad_s;
    Brake left = kinetic(left_capacity_nm, axle_rad_s - state.differential_rad_s);
    Brake right = kinetic(right_capacity_nm, axle_rad_s + state.differential_rad_s);
    RearSolution solution = solve_rear(state, model, p, left.torque_nm, right.torque_nm);
    Bracket left_bracket;
    Bracket right_bracket;
    for (int step = 0; step < kMaxHoldSteps; ++step) {
        left.held = left.held || carries_through_rest(left, solution.left_rad_s);
        right.held = right.held || carries_through_rest(right, solution.right_rad_s);
        if (settled(left, solution.left_rad_s) && settled(right, solution.right_rad_s)) {
            break;
        }
        narrow(left_bracket, left.torque_nm, solution.left_rad_s);
        narrow(right_bracket, right.torque_nm, solution.right_rad_s);
        const Change change =
            hold_change(state, model, p, solution, left, right, left_bracket, right_bracket);
        const double left_nm = left.torque_nm + change.left_nm;
        const double right_nm = right.torque_nm + change.right_nm;
        left.torque_nm = std::clamp(left_nm, -left.capacity_nm, left.capacity_nm);
        right.torque_nm = std::clamp(right_nm, -right.capacity_nm, right.capacity_nm);
        solution = solve_rear(state, model, p, left.torque_nm, right.torque_nm);
    }
    return solution;
}

// Each patch follows its contact's sideways speed at the step's end speeds:
// the rear tires' at the rear axle, the front tires' at the front axle,
// turned into the steered wheels' frame.
void relax_side_deflections(State& state, const Params& params, const tire::SideGrip& front_grip,
                            const tire::SideGrip& rear_left_grip,
                            const tire::SideGrip& rear_right_grip, const math::SinCos& steer,
                            double dt_s) noexcept {
    const chassis::State& body = state.chassis;
    const double min_rolling_m_s = params.rear_tire.min_reference_speed_m_s;
    const double front_side_m_s =
        body.lateral_velocity_m_s + params.chassis.wheelbase_m * body.yaw_rate_rad_s;
    state.front_side_deflection_m = tire::relax_deflection_m(
        front_grip, params.steering.front_tires, state.front_side_deflection_m,
        front_side_m_s * steer.cos - body.velocity_m_s * steer.sin,
        body.velocity_m_s * steer.cos + front_side_m_s * steer.sin, min_rolling_m_s, dt_s);
    state.rear_left_side_deflection_m = tire::relax_deflection_m(
        rear_left_grip, params.rear_tire.cornering, state.rear_left_side_deflection_m,
        body.lateral_velocity_m_s, body.velocity_m_s, min_rolling_m_s, dt_s);
    state.rear_right_side_deflection_m = tire::relax_deflection_m(
        rear_right_grip, params.rear_tire.cornering, state.rear_right_side_deflection_m,
        body.lateral_velocity_m_s, body.velocity_m_s, min_rolling_m_s, dt_s);
}

}  // namespace

Output step(State& state, const Model& model, const Input& input, const Ground& ground,
            const chassis::Drawbar& drawbar, double dt_s) noexcept {
    const Params& params = model.params();
    const driveline::Params& driveline_params = model.driveline();
    const tire::Params& tire_params = params.rear_tire;
    const double radius_m = tire_params.size.radius_m;
    const double tires_per_side = params.rear_tire_count / kSides;
    Output out;

    const double clutch_capacity_nm = clutch::capacity_nm(params.clutch, input.clutch_pedal);
    // Moving off, the pull is under way: from here the gearbox goes down
    // only by its powershift (driveline::floor_gear).
    if (std::abs(state.chassis.velocity_m_s) > kUnderWay_m_s) {
        driveline::start_pull(state.driveline);
    }
    if (input.gear != state.driveline.gear) {
        const bool clutch_out = clutch_capacity_nm == 0.0;
        if (!clutch_out &&
            driveline::shifts_under_load(state.driveline, driveline_params, input.gear)) {
            // The engine gives the input shaft its new speed's momentum, down
            // to standing still (a stalled engine has none to give).
            const double impulse_nm_s =
                driveline::shift_under_load(state.driveline, driveline_params, input.gear)
                    .value_or(0.0);
            state.engine.omega_rad_s = std::max(
                state.engine.omega_rad_s - impulse_nm_s / params.engine.inertia_kg_m2, 0.0);
        } else {
            out.shift_refused =
                !driveline::shift(state.driveline, driveline_params, input.gear, clutch_out);
        }
    }

    out.axle_loads = chassis::axle_loads(params.chassis, state.chassis, drawbar);
    const double load_per_tire_n = out.axle_loads.rear_n / params.rear_tire_count;

    // Each side's traction at its own speeds: the differential splits the
    // wheels' speeds, and a turning tractor moves its outer side faster
    // over the ground. The slip also sets how deep the tires dig.
    const double axle_rad_s = state.driveline.output_omega_rad_s;
    const double ground_m_s = state.chassis.velocity_m_s;
    const double turning_m_s = state.chassis.yaw_rate_rad_s * params.chassis.rear_track_m / kSides;
    // Both sides stand on the same ground with the same load.
    const tire::Footing footing = tire::footing(tire_params, load_per_tire_n, ground.rear);
    const Side left = rear_side(tire_params, footing, axle_rad_s - state.differential_rad_s,
                                ground_m_s - turning_m_s, load_per_tire_n, ground.rear);
    const Side right = rear_side(tire_params, footing, axle_rad_s + state.differential_rad_s,
                                 ground_m_s + turning_m_s, load_per_tire_n, ground.rear);
    out.slip_ratio = std::max(left.traction.slip_ratio, right.traction.slip_ratio);
    out.rear_left.slip_ratio = left.traction.slip_ratio;
    out.rear_right.slip_ratio = right.traction.slip_ratio;
    out.rear_rolling_resistance = (left.rolling_resistance + right.rolling_resistance) / kSides;
    out.thrown_soil_m3_s =
        tires_per_side * (tire::thrown_soil_m3_s(tire_params.size, footing, load_per_tire_n,
                                                 left.traction.slip_ratio, left.wheel_m_s) +
                          tire::thrown_soil_m3_s(tire_params.size, footing, load_per_tire_n,
                                                 right.traction.slip_ratio, right.wheel_m_s));
    out.front_rolling_resistance =
        out.axle_loads.front_n > 0.0
            ? tire::towed_rolling_resistance(params.front_tire,
                                             out.axle_loads.front_n / kFrontTires, ground.front)
            : 0.0;

    // Side forces from the patch deflections at the start of the step. The
    // front pair steers as one; a rear tire's side grip is what its
    // traction leaves.
    const double steering_rad = std::clamp(input.steering_rad, -params.steering.max_angle_rad,
                                           params.steering.max_angle_rad);
    const math::SinCos steer = math::sin_cos(steering_rad);
    const tire::SideGrip front_grip =
        tire::side_grip(params.front_tire, params.steering.front_tires,
                        out.axle_loads.front_n / kFrontTires, ground.front);
    const tire::SideGrip rear_grip =
        tire::side_grip(tire_params.cornering, footing, load_per_tire_n, ground.rear);
    const tire::SideGrip rear_left_grip =
        tire::with_traction(rear_grip, left.traction.force_n,
                            left.traction.full_bite * load_per_tire_n, left.traction.slip_ratio);
    const tire::SideGrip rear_right_grip =
        tire::with_traction(rear_grip, right.traction.force_n,
                            right.traction.full_bite * load_per_tire_n, right.traction.slip_ratio);
    out.front_side_force_n =
        kFrontTires * tire::side_force_n(front_grip, state.front_side_deflection_m);
    out.rear_side_force_n =
        tires_per_side * (tire::side_force_n(rear_left_grip, state.rear_left_side_deflection_m) +
                          tire::side_force_n(rear_right_grip, state.rear_right_side_deflection_m));

    // Predictions. Rolling resistance acts as a known torque on the axle
    // (rear) and force on the body (front tires, wheelie bar); like the
    // drawbar pull, it holds things at rest but never drives them backwards.
    const engine::Prediction engine_prediction =
        engine::predict(state.engine, params.engine, input.throttle, dt_s);
    const double side_load_n = out.axle_loads.rear_n / kSides;
    const double rear_rolling_nm = left.rolling_resistance * side_load_n * radius_m +
                                   right.rolling_resistance * side_load_n * radius_m;
    // The steered front tires' side force has a component along the body
    // that holds the tractor back.
    const double front_drag_n =
        out.front_rolling_resistance * out.axle_loads.front_n +
        params.chassis.wheelie_bar.rolling_resistance_coefficient * out.axle_loads.wheelie_bar_n +
        out.front_side_force_n * steer.sin;
    const chassis::Prediction body_prediction =
        chassis::predict(state.chassis, params.chassis, drawbar.pull_n + front_drag_n, dt_s);

    const RearProblem problem{
        .engine = engine::shaft_prediction(engine_prediction),
        .body = body_prediction,
        .rolling_nm = rear_rolling_nm,
        .clutch_capacity_nm = clutch_capacity_nm,
        .side_inertia_kg_m2 = tires_per_side * tire_params.wheel_inertia_kg_m2,
        .force_left_n = tires_per_side * left.traction.force_n,
        .force_right_n = tires_per_side * right.traction.force_n,
        .slope_left = tires_per_side * left.traction.force_per_slip_speed,
        .slope_right = tires_per_side * right.traction.force_per_slip_speed,
        .common_slip_m_s = axle_rad_s * radius_m - ground_m_s,
        .dt_s = dt_s};
    const RearSolution solution = solve_rear_with_brakes(
        state, model, problem, std::clamp(input.brake_left, 0.0, 1.0) * params.brakes.max_torque_nm,
        std::clamp(input.brake_right, 0.0, 1.0) * params.brakes.max_torque_nm);
    const ChainSolution& chain = solution.chain;

    // Each side's traction at the solved speeds.
    const double side_change_m_s = radius_m * solution.differential_change_rad_s;
    const double left_force_n =
        problem.force_left_n + problem.slope_left * (solution.slip_change_m_s - side_change_m_s);
    const double right_force_n =
        problem.force_right_n + problem.slope_right * (solution.slip_change_m_s + side_change_m_s);

    state.clutch = solution.clutch;
    engine::advance(state.engine, params.engine, engine_prediction, chain.clutch_torque_nm);
    driveline::advance(state.driveline, solution.driveline, chain.clutch_torque_nm,
                       -radius_m * chain.tire_force_n);
    // The axle never turns backwards (there is no reverse gear), but one
    // side may: an open differential lets a tractor turning on the spot roll
    // its inner wheels back.
    const double next_axle_rad_s = state.driveline.output_omega_rad_s;
    state.differential_rad_s += solution.differential_change_rad_s;
    chassis::advance_lateral(
        state.chassis, params.chassis,
        {.front_n = out.front_side_force_n * steer.cos,
         .rear_n = out.rear_side_force_n,
         .hitch_n = drawbar.side_n,
         .yaw_moment_nm = (right_force_n - left_force_n) * params.chassis.rear_track_m / kSides},
        dt_s);
    chassis::advance(state.chassis, body_prediction, chain.tire_force_n, dt_s);
    relax_side_deflections(state, params, front_grip, rear_left_grip, rear_right_grip, steer, dt_s);

    // The torque the gearbox and the brakes put on the rear wheels, from
    // their own balance: it spins them up and overcomes the tires' traction
    // and rolling resistance. Its reaction on the body lifts the nose. At a
    // standstill the resistances only hold, and the solved tire force is not
    // a physical one: the tires then hold just the drawbar pull. Rolling
    // resistance counts only while the wheels turn, as in the driveline.
    const bool standstill = next_axle_rad_s == 0.0 && state.chassis.velocity_m_s == 0.0;
    const double tire_force_n = standstill ? drawbar.pull_n : chain.tire_force_n;
    const double axle_torque_nm =
        driveline_params.output_inertia_kg_m2 * (next_axle_rad_s - axle_rad_s) / dt_s +
        radius_m * tire_force_n + (next_axle_rad_s > 0.0 ? rear_rolling_nm : 0.0);
    chassis::advance_pitch(state.chassis, params.chassis, axle_torque_nm, drawbar,
                           out.front_rolling_resistance, dt_s);

    out.engine = engine_prediction.output;
    out.clutch_torque_nm = chain.clutch_torque_nm;
    out.clutch_slip_rad_s = state.engine.omega_rad_s - state.driveline.input_omega_rad_s;
    out.clutch_slip_power_w =
        state.clutch.locked ? 0.0 : out.clutch_torque_nm * out.clutch_slip_rad_s;
    out.tire_force_n = tire_force_n;
    out.rear_left.tire_force_n = standstill ? tire_force_n / kSides : left_force_n;
    out.rear_right.tire_force_n = standstill ? tire_force_n / kSides : right_force_n;
    out.rear_left.brake_torque_nm = solution.brake_left_nm;
    out.rear_right.brake_torque_nm = solution.brake_right_nm;
    return out;
}

}  // namespace tp::vehicle
