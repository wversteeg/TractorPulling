// Engine, clutch and driveline together, pulling a stand-in for the vehicle
// (its mass reflected to the axle as m r², plus a resistance force at the
// tire radius) until the tires arrive in Milestone 1, step 4.

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "../clutch/reference_clutch.hpp"
#include "../engine/reference_engine.hpp"
#include "reference_driveline.hpp"

namespace tp::driveline {
namespace {

constexpr double kDt_s = 0.001;
constexpr double kVehicleMass_kg = 3500.0;
constexpr double kTireRadius_m = 0.8;

struct Rig {
    engine::Params engine_params = engine::testing::reference_params();
    clutch::Params clutch_params = clutch::testing::reference_params();
    Params driveline_params = with_vehicle(testing::reference_params());
    engine::State engine = engine::initial_state(engine_params);
    clutch::State clutch;
    State driveline;
    double resistance_n = 0.0;
    clutch::Output last_clutch;

    static Params with_vehicle(Params params) {
        params.output_inertia_kg_m2 += kVehicleMass_kg * kTireRadius_m * kTireRadius_m;
        return params;
    }

    explicit Rig(double resistance_force_n) : resistance_n(resistance_force_n) {}

    void step(double throttle, double pedal) {
        const engine::Prediction e = engine::predict(engine, engine_params, throttle, kDt_s);
        const Prediction d =
            predict(driveline, driveline_params, resistance_n * kTireRadius_m, kDt_s);
        last_clutch = clutch::step(clutch, clutch_params, {.pedal = pedal},
                                   engine::shaft_prediction(e), input_shaft(d));
        engine::advance(engine, engine_params, e, last_clutch.torque_nm);
        advance(driveline, d, last_clutch.torque_nm);
    }

    void run(double seconds, double throttle, double pedal) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            step(throttle, pedal);
        }
    }

    // Select `gear` with the clutch pressed, rev up, slip the clutch in over
    // 3 s and keep pulling at full throttle.
    void launch(int gear, double pull_s) {
        EXPECT_TRUE(shift(driveline, driveline_params, gear,
                          clutch::capacity_nm(clutch_params, 1.0) == 0.0));
        run(2.5, 1.0, 1.0);
        constexpr int kSlipSteps = 3000;
        for (int i = 0; i <= kSlipSteps; ++i) {
            step(1.0, 1.0 - static_cast<double>(i) / kSlipSteps);
        }
        run(pull_s, 1.0, 0.0);
    }

    [[nodiscard]] double road_speed_m_s() const {
        return driveline.output_omega_rad_s * kTireRadius_m;
    }
};

// Pulling steadily, road speed is engine speed through the gears, and the
// governor holds the engine between rated speed and high idle.
TEST(DrivelineChain, RoadSpeedFollowsEngineThroughTheGears) {
    Rig rig{5000.0};
    rig.launch(4, 10.0);
    ASSERT_TRUE(rig.engine.running);
    EXPECT_TRUE(rig.clutch.locked);
    const double ratio = overall_ratio(rig.driveline_params, 4);
    EXPECT_NEAR(rig.road_speed_m_s(), rig.engine.omega_rad_s / ratio * kTireRadius_m, 1e-9);
    EXPECT_GT(rig.engine.omega_rad_s, rig.engine_params.rated_omega_rad_s);
    EXPECT_LT(rig.engine.omega_rad_s, rig.engine_params.high_idle_omega_rad_s);
}

TEST(DrivelineChain, HigherGearsPullFaster) {
    double previous_m_s = 0.0;
    for (int gear = 1; gear <= 7; ++gear) {
        Rig rig{5000.0};
        rig.launch(gear, 10.0);
        ASSERT_TRUE(rig.engine.running) << "gear " << gear;
        EXPECT_GT(rig.road_speed_m_s(), previous_m_s) << "gear " << gear;
        previous_m_s = rig.road_speed_m_s();
    }
}

// Choosing the gear matters: a heavy load that a low gear pulls away stalls
// the engine in a high gear, where it cannot multiply enough torque.
TEST(DrivelineChain, LowGearPullsWhatHighGearCannot) {
    Rig low{20000.0};
    low.launch(3, 10.0);
    EXPECT_TRUE(low.engine.running);
    EXPECT_GT(low.road_speed_m_s(), 0.0);

    Rig high{20000.0};
    high.launch(7, 10.0);
    EXPECT_FALSE(high.engine.running);
}

// At a steady pull the axle delivers the efficiency times the power the
// clutch puts in, less the input shaft's drag; the rest heats the gearbox.
TEST(DrivelineChain, SteadyPullDeliversEfficiencyTimesClutchPower) {
    Rig rig{25000.0};
    rig.launch(4, 15.0);
    ASSERT_TRUE(rig.engine.running);
    ASSERT_TRUE(rig.clutch.locked);
    const Params& params = rig.driveline_params;
    const double input_rad_s = rig.driveline.input_omega_rad_s;
    const double clutch_w = rig.last_clutch.torque_nm * input_rad_s;
    const double drag_w =
        (params.drag_torque_nm + params.viscous_drag_nm_s_rad * input_rad_s) * input_rad_s;
    const double axle_w = rig.resistance_n * rig.road_speed_m_s();
    EXPECT_NEAR(axle_w, params.efficiency * clutch_w - drag_w, 1e-3 * axle_w);
}

std::uint64_t launch_hash() {
    Rig rig{15000.0};
    hash::Fnv1a hash;
    EXPECT_TRUE(shift(rig.driveline, rig.driveline_params, 4, true));
    for (int i = 0; i < 2500; ++i) {
        rig.step(1.0, 1.0);
    }
    for (int i = 0; i <= 3000; ++i) {
        rig.step(1.0, 1.0 - i / 3000.0);
        engine::hash_append(hash, rig.engine);
        clutch::hash_append(hash, rig.clutch);
        hash_append(hash, rig.driveline);
    }
    for (int i = 0; i < 8000; ++i) {
        rig.step(1.0, 0.0);
        engine::hash_append(hash, rig.engine);
        clutch::hash_append(hash, rig.clutch);
        hash_append(hash, rig.driveline);
    }
    return hash.value();
}

TEST(DrivelineChain, LaunchIsDeterministic) {
    EXPECT_EQ(launch_hash(), launch_hash());
}

// Regression hash; must match on every compiler, build type and platform.
// Update deliberately, with a note in the commit, when the model changes.
TEST(DrivelineChain, LaunchMatchesRegressionHash) {
    EXPECT_EQ(launch_hash(), 0x448f342c290a0a49ULL);
}

}  // namespace
}  // namespace tp::driveline
