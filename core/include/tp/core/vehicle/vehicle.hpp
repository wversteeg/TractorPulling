#pragma once

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/clutch/clutch.hpp"
#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/soil/soil.hpp"
#include "tp/core/tire/tire.hpp"

#include <cstdint>
#include <optional>

namespace tp::vehicle {

inline constexpr int kMaxRearTires = 8;
/// A side its brake holds ends the step within this of rest [rad/s].
inline constexpr double kBrakeHoldTolerance_rad_s = 1e-9;
/// Faster than this the tractor is moving off: the pull is under way, and
/// from then on the gearbox goes down only by its powershift [m/s].
inline constexpr double kUnderWay_m_s = 0.3;

/// The front wheels' steering.
struct Steering {
    /// Largest front wheel angle either way [rad]; at most 45°.
    double max_angle_rad = 0.0;
    /// Side grip of one front tire.
    tire::Cornering front_tires;
};

/// The rear wheels' brakes, one per side, each worked by its own pedal.
struct Brakes {
    /// Braking torque on one side's wheels at full pedal [N·m].
    double max_torque_nm = 0.0;
};

/// A whole tractor: the components and how many rear tires share the drive.
/// The rear tires are split evenly between the left and right side, which an
/// open differential drives: it gives both sides the same torque and lets
/// them turn at different speeds.
struct Params {
    engine::Params engine;
    clutch::Params clutch;
    /// Without the wheels; Model adds the rear wheels' inertia to the axle.
    driveline::Params driveline;
    tire::Params rear_tire;
    /// All rear tires, an even number: half on each side.
    int rear_tire_count = 2;
    /// One of the two (unpowered) front tires; its size sets their rolling
    /// resistance on soil.
    tire::Size front_tire;
    Steering steering;
    Brakes brakes;
    chassis::Params chassis;
    /// Crankshaft to PTO shaft, in (0, 1]. The PTO is not simulated; class
    /// rules limit power measured there.
    double pto_efficiency = 1.0;
};

enum class ParamError : std::uint8_t {
    kEngine,
    kClutch,
    kDriveline,
    kRearTire,
    kRearTireCount,
    kFrontTire,
    kSteering,
    kBrakes,
    kChassis,
    /// The chassis' rear axle height is not the rear tire's radius.
    kRearAxleHeight,
    kPtoEfficiencyOutOfRange,
};

/// Which part of `params` is invalid, if any; each component's own validate()
/// says what is wrong with it.
[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// Validated parameters plus what is derived from them once, at setup.
class Model {
public:
    [[nodiscard]] static std::optional<Model> create(const Params& params) noexcept;

    [[nodiscard]] const Params& params() const noexcept { return params_; }
    /// The driveline with the rear wheels' inertia on the axle.
    [[nodiscard]] const driveline::Params& driveline() const noexcept { return driveline_; }

private:
    explicit Model(const Params& params) noexcept;

    Params params_;
    driveline::Params driveline_;
};

struct State {
    engine::State engine;
    clutch::State clutch;
    driveline::State driveline;
    chassis::State chassis;
    /// The open differential: half the right rear wheels' speed minus the
    /// left's. The left wheels turn at the axle speed (the driveline's
    /// output) minus this, the right at the axle speed plus it [rad/s].
    double differential_rad_s = 0.0;
    /// Sideways deflection of the tires' contact patches, to the tire's
    /// left (see tire::Cornering): each front tire, and each rear tire on
    /// the left and the right [m].
    double front_side_deflection_m = 0.0;
    double rear_left_side_deflection_m = 0.0;
    double rear_right_side_deflection_m = 0.0;
};

/// One side's rear wheels this step.
struct RearWheels {
    double slip_ratio = 0.0;
    /// Traction of that side's tires [N].
    double tire_force_n = 0.0;
    /// Brake torque on that side, positive against forward rotation
    /// (negative while it holds the side against turning backwards) [N·m].
    double brake_torque_nm = 0.0;
};

struct Input {
    /// Hand throttle, 0 to 1.
    double throttle = 0.0;
    /// 0 released (engaged) to 1 pressed.
    double clutch_pedal = 1.0;
    /// Requested gear; 0 is neutral, driveline::kReverse reverse. Changes
    /// only with the clutch pressed.
    int gear = driveline::kNeutral;
    /// Front wheel angle, positive to the left; held to the steering's
    /// range [rad].
    double steering_rad = 0.0;
    /// Brake pedals for the left and right rear wheels, 0 to 1.
    double brake_left = 0.0;
    double brake_right = 0.0;
    /// The pull is on: moving off past kUnderWay_m_s arms the gearbox's
    /// floor (driveline::start_pull). Off (driving to the sled or away from
    /// it) any gear may be chosen, and the floor is cleared.
    bool pull_on = true;
};

/// The way the tractor travels this step: -1 in reverse, or rolling
/// backwards in neutral; 1 otherwise. Resistances (rolling, the brakes at
/// rest) act against it, and hold the tractor at rest without turning it
/// round.
[[nodiscard]] double travel_direction(const State& state) noexcept;

/// The ground under the tractor's tires this step. The track decides it from
/// where each axle is; tests and tools may pass one surface for both.
struct Ground {
    soil::Surface rear;
    soil::Surface front;
};

struct Output {
    engine::Output engine;
    double clutch_torque_nm = 0.0;
    /// Engine speed minus gearbox input speed at the end of the step [rad/s].
    double clutch_slip_rad_s = 0.0;
    double clutch_slip_power_w = 0.0;
    /// Traction of all rear tires on the vehicle [N]. At a standstill, the
    /// drawbar pull they hold.
    double tire_force_n = 0.0;
    /// The more positive (driving) slip of the two sides: one side spinning
    /// is how an open differential spins out. A side skidding under its
    /// brake (negative slip) does not count.
    double slip_ratio = 0.0;
    RearWheels rear_left;
    RearWheels rear_right;
    /// Motion resistance of the rear tires per unit of their load, the two
    /// sides' mean: sinkage and the rut a slipping tire digs.
    double rear_rolling_resistance = 0.0;
    /// Motion resistance of the front tires per unit of their load.
    double front_rolling_resistance = 0.0;
    /// Soil all rear tires throw back behind the tractor [m³/s].
    double thrown_soil_m3_s = 0.0;
    chassis::AxleLoads axle_loads;
    /// Side force of both front tires, across the wheels, and of all rear
    /// tires; positive to the left [N].
    double front_side_force_n = 0.0;
    double rear_side_force_n = 0.0;
    /// The requested gear could not be engaged this step.
    bool shift_refused = false;
};

/// Engine idling, clutch pressed, neutral, at rest.
[[nodiscard]] State initial_state(const Model& model) noexcept;

/// Advances the tractor by `dt_s` on `ground` while the drawbar pulls on its
/// hitch (see tp::pull for the chain to a sled; tools and tests may pass a
/// constant pull).
///
/// Order: gear change, axle loads from the last step's state, each side's
/// rear traction and both axles' rolling resistance at the current speeds,
/// both axles' side forces from their tires' patch deflections, every
/// body's prediction, the exact chain solve for clutch torque and tire
/// force with the differential and brakes folded in (a brake that would
/// carry its wheels through rest holds them there instead), the chassis' sideways
/// motion and yaw, every body advances, the patch deflections follow the
/// new speeds, then the chassis pitches under the axle torque's reaction.
Output step(State& state, const Model& model, const Input& input, const Ground& ground,
            const chassis::Drawbar& drawbar, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    engine::hash_append(hash, state.engine);
    clutch::hash_append(hash, state.clutch);
    driveline::hash_append(hash, state.driveline);
    chassis::hash_append(hash, state.chassis);
    hash.add(state.differential_rad_s);
    hash.add(state.front_side_deflection_m);
    hash.add(state.rear_left_side_deflection_m);
    hash.add(state.rear_right_side_deflection_m);
}

}  // namespace tp::vehicle
