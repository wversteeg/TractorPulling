#pragma once

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/clutch/clutch.hpp"
#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/driveline/driveline.hpp"

namespace tp::vehicle {

/// The rear tires' contact, linearized over the step: the force they put on
/// the vehicle is
///
///   F = force_n + force_per_slip_speed · (radius · axle' - ground' - slip_speed_m_s)
///
/// where axle' and ground' are the end-of-step axle and vehicle speeds and
/// slip_speed_m_s the current wheel surface speed minus ground speed.
struct TireLine {
    double force_n = 0.0;
    double force_per_slip_speed = 0.0;
    double slip_speed_m_s = 0.0;
    double radius_m = 0.0;
};

struct ChainSolution {
    double clutch_torque_nm = 0.0;
    double tire_force_n = 0.0;
};

/// Solves the chain engine ⟷ clutch ⟷ driveline ⟷ tires ⟷ vehicle body for
/// one step, exactly. Every body's end-of-step speed is linear in the two
/// coupling loads (clutch torque, tire force), so with the clutch locked the
/// chain is a 2×2 linear system, reduced here by eliminating the tire force.
/// The clutch's friction limit is then applied with clutch::resolve; if it
/// slips, its torque is fixed and the tire force follows directly. The
/// gearbox's direction-dependent efficiency picks the branch for the sign of
/// the clutch torque; the locking torque is monotonic in it, so at most two
/// branches are tried.
[[nodiscard]] ChainSolution solve_chain(const coupling::ShaftPrediction& engine,
                                        const driveline::Response& driveline, const TireLine& tire,
                                        const chassis::Prediction& body,
                                        clutch::State& clutch_state,
                                        const clutch::Params& clutch_params,
                                        double clutch_capacity_nm) noexcept;

}  // namespace tp::vehicle
