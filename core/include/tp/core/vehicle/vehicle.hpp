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

/// A whole tractor: the components and how many rear tires share the drive.
struct Params {
    engine::Params engine;
    clutch::Params clutch;
    /// Without the wheels; Model adds the rear wheels' inertia to the axle.
    driveline::Params driveline;
    tire::Params rear_tire;
    int rear_tire_count = 2;
    /// One of the two (unpowered) front tires; its size sets their rolling
    /// resistance on soil.
    tire::Size front_tire;
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
};

struct Input {
    /// Hand throttle, 0 to 1.
    double throttle = 0.0;
    /// 0 released (engaged) to 1 pressed.
    double clutch_pedal = 1.0;
    /// Requested gear; 0 is neutral. Changes only with the clutch pressed.
    int gear = driveline::kNeutral;
};

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
    double slip_ratio = 0.0;
    /// Motion resistance of all rear tires, per unit of their load: sinkage
    /// and the rut a slipping tire digs.
    double rear_rolling_resistance = 0.0;
    /// Motion resistance of the front tires per unit of their load.
    double front_rolling_resistance = 0.0;
    /// Soil all rear tires throw back behind the tractor [m³/s].
    double thrown_soil_m3_s = 0.0;
    chassis::AxleLoads axle_loads;
    /// The requested gear could not be engaged this step.
    bool shift_refused = false;
};

/// Engine idling, clutch pressed, neutral, at rest.
[[nodiscard]] State initial_state(const Model& model) noexcept;

/// Advances the tractor by `dt_s` on `ground` while the drawbar pulls on its
/// hitch (see tp::pull for the chain to a sled; tools and tests may pass a
/// constant pull).
///
/// Order: gear change, axle loads from the last step's state, the rear
/// tires' traction and both axles' rolling resistance at the current speeds,
/// every body's prediction, the exact chain solve for clutch torque and tire
/// force, every body advances, then the chassis pitches under the axle
/// torque's reaction.
Output step(State& state, const Model& model, const Input& input, const Ground& ground,
            const chassis::Drawbar& drawbar, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    engine::hash_append(hash, state.engine);
    clutch::hash_append(hash, state.clutch);
    driveline::hash_append(hash, state.driveline);
    chassis::hash_append(hash, state.chassis);
}

}  // namespace tp::vehicle
