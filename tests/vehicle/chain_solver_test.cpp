#include "tp/core/vehicle/chain_solver.hpp"

#include <gtest/gtest.h>

#include "../clutch/reference_clutch.hpp"

namespace tp::vehicle {
namespace {

// A representative step in 4th: engine, driveline response through a 114:1
// train with 90 % efficiency, two rear tires on the rising side of the slip
// curve, and the vehicle body.
struct Fixture {
    coupling::ShaftPrediction engine{.free_omega_next_rad_s = 230.0,
                                     .omega_per_torque = 0.001 / 1.2,
                                     .omega_per_negative_torque = 0.001 / 1.2};
    driveline::Response driveline = make_driveline(228.0);
    TireLine tire{.force_n = 9000.0,
                  .force_per_slip_speed = 60000.0,
                  .slip_speed_m_s = 0.08,
                  .radius_m = 0.8};
    chassis::Prediction body{.free_velocity_m_s = 1.55, .velocity_per_n = 0.001 / 3500.0};

    static driveline::Response make_driveline(double free_input_rad_s) {
        constexpr double kRatio = 114.4;
        constexpr double kInput_per_nm = 0.001 / (0.05 + 62.0 / (kRatio * kRatio));
        driveline::Response r;
        r.free_input_rad_s = free_input_rad_s;
        r.input_per_clutch = kInput_per_nm * 0.9;
        r.input_per_negative_clutch = kInput_per_nm / 0.9;
        r.input_per_axle = kInput_per_nm / kRatio;
        r.free_axle_rad_s = free_input_rad_s / kRatio;
        r.axle_per_clutch = r.input_per_clutch / kRatio;
        r.axle_per_negative_clutch = r.input_per_negative_clutch / kRatio;
        r.axle_per_axle = r.input_per_axle / kRatio;
        return r;
    }

    // End-of-step speeds for a given solution, from the same linear models.
    struct Speeds {
        double engine_rad_s;
        double input_rad_s;
        double axle_rad_s;
        double body_m_s;
    };
    [[nodiscard]] Speeds speeds(const ChainSolution& s) const {
        const bool positive = s.clutch_torque_nm >= 0.0;
        const double axle_torque_nm = -tire.radius_m * s.tire_force_n;
        return {
            engine.free_omega_next_rad_s -
                (positive ? engine.omega_per_negative_torque : engine.omega_per_torque) *
                    s.clutch_torque_nm,
            driveline.free_input_rad_s +
                (positive ? driveline.input_per_clutch : driveline.input_per_negative_clutch) *
                    s.clutch_torque_nm +
                driveline.input_per_axle * axle_torque_nm,
            driveline.free_axle_rad_s +
                (positive ? driveline.axle_per_clutch : driveline.axle_per_negative_clutch) *
                    s.clutch_torque_nm +
                driveline.axle_per_axle * axle_torque_nm,
            body.free_velocity_m_s + body.velocity_per_n * s.tire_force_n,
        };
    }

    // How far the tire force misses its linearized relation [N].
    [[nodiscard]] double tire_residual_n(const ChainSolution& s) const {
        const Speeds v = speeds(s);
        const double slip_speed_m_s = tire.radius_m * v.axle_rad_s - v.body_m_s;
        return s.tire_force_n -
               (tire.force_n + tire.force_per_slip_speed * (slip_speed_m_s - tire.slip_speed_m_s));
    }
};

// With enough capacity the clutch locks: engine and input shaft end the step
// at the same speed, and the tire force satisfies its relation, exactly.
TEST(ChainSolver, LockedClutchSatisfiesBothCouplings) {
    for (const double free_input_rad_s : {225.0, 232.0}) {  // engine faster / slower
        Fixture f;
        f.driveline = Fixture::make_driveline(free_input_rad_s);
        clutch::State clutch{.locked = true};
        const ChainSolution s = solve_chain(f.engine, f.driveline, f.tire, f.body, clutch,
                                            clutch::testing::reference_params(), 650.0);
        EXPECT_TRUE(clutch.locked);
        const Fixture::Speeds v = f.speeds(s);
        EXPECT_NEAR(v.engine_rad_s, v.input_rad_s, 1e-9) << "free input " << free_input_rad_s;
        EXPECT_NEAR(f.tire_residual_n(s), 0.0, 1e-6);
    }
}

// Without enough capacity the clutch slips at its kinetic capacity, against
// the slip, and the tire force still satisfies its relation.
TEST(ChainSolver, SlippingClutchCarriesKineticCapacity) {
    Fixture f;
    f.driveline = Fixture::make_driveline(100.0);
    clutch::State clutch;
    const clutch::Params params = clutch::testing::reference_params();
    constexpr double kCapacity_nm = 200.0;
    const ChainSolution s =
        solve_chain(f.engine, f.driveline, f.tire, f.body, clutch, params, kCapacity_nm);
    EXPECT_FALSE(clutch.locked);
    EXPECT_DOUBLE_EQ(s.clutch_torque_nm, params.kinetic_ratio * kCapacity_nm);
    EXPECT_NEAR(f.tire_residual_n(s), 0.0, 1e-6);
    const Fixture::Speeds v = f.speeds(s);
    EXPECT_GT(v.engine_rad_s, v.input_rad_s) << "slip keeps its direction";
}

TEST(ChainSolver, PressedClutchLeavesOnlyTheTire) {
    const Fixture f;
    clutch::State clutch{.locked = true};
    const ChainSolution s = solve_chain(f.engine, f.driveline, f.tire, f.body, clutch,
                                        clutch::testing::reference_params(), 0.0);
    EXPECT_FALSE(clutch.locked);
    EXPECT_EQ(s.clutch_torque_nm, 0.0);
    EXPECT_NEAR(f.tire_residual_n(s), 0.0, 1e-6);
}

// In neutral the cross terms vanish: the clutch solve is the engine/input
// solve alone, and the tire couples only axle and body.
TEST(ChainSolver, NeutralDecouplesClutchFromTire) {
    Fixture f;
    driveline::Response neutral;
    neutral.free_input_rad_s = 228.0;  // close enough to the engine to lock
    neutral.input_per_clutch = 0.001 / 0.05;
    neutral.input_per_negative_clutch = 0.001 / 0.05;
    neutral.free_axle_rad_s = 2.0;
    neutral.axle_per_axle = 0.001 / 62.0;
    f.driveline = neutral;
    clutch::State clutch{.locked = true};
    const ChainSolution s = solve_chain(f.engine, f.driveline, f.tire, f.body, clutch,
                                        clutch::testing::reference_params(), 650.0);
    const double expected_lock_nm = (f.engine.free_omega_next_rad_s - neutral.free_input_rad_s) /
                                    (f.engine.omega_per_negative_torque + neutral.input_per_clutch);
    EXPECT_NEAR(s.clutch_torque_nm, expected_lock_nm, 1e-9);
    EXPECT_NEAR(f.tire_residual_n(s), 0.0, 1e-6);
}

}  // namespace
}  // namespace tp::vehicle
