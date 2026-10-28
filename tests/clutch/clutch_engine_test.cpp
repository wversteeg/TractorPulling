// Engine and clutch coupled, driving a loaded stand-in for the driveline
// (until the real driveline arrives in Milestone 1, step 3).

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include "../engine/reference_engine.hpp"
#include "flywheel.hpp"
#include "reference_clutch.hpp"

namespace tp::clutch {
namespace {

constexpr double kDt_s = 0.001;

// Tractor and sled reflected to the clutch shaft: a large inertia that the
// engine's flywheel cannot bring up to speed on its own (so dumping the clutch
// stalls), with a resistance the engine can carry once it is moving.
constexpr double kDrivelineInertia_kg_m2 = 10.0;
constexpr double kDrivelineResistance_nm = 70.0;

struct Rig {
    engine::Params engine_params = engine::testing::reference_params();
    Params clutch_params = testing::reference_params();
    engine::State engine = engine::initial_state(engine_params);
    State clutch;
    testing::Flywheel driveline{.inertia_kg_m2 = kDrivelineInertia_kg_m2,
                                .external_torque_nm = -kDrivelineResistance_nm,
                                .cannot_reverse = true};

    Output step(double throttle, double pedal) {
        const engine::Prediction prediction =
            engine::predict(engine, engine_params, throttle, kDt_s);
        const Output out =
            clutch::step(clutch, clutch_params, {.pedal = pedal},
                         engine::shaft_prediction(prediction), driveline.predict(kDt_s));
        engine::advance(engine, engine_params, prediction, out.torque_nm);
        driveline.advance(kDt_s, out.torque_nm);
        return out;
    }

    void run(double seconds, double throttle, double pedal) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            step(throttle, pedal);
        }
    }

    // Pedal from fully pressed to released over `seconds`, at full throttle.
    void slip_in(double seconds) {
        const int steps = static_cast<int>(seconds / kDt_s);
        for (int i = 0; i <= steps; ++i) {
            step(1.0, 1.0 - static_cast<double>(i) / steps);
        }
    }
};

TEST(ClutchWithEngine, PressedClutchLetsEngineRevFreely) {
    Rig rig;
    rig.run(3.0, 1.0, 1.0);
    EXPECT_TRUE(rig.engine.running);
    EXPECT_GT(rig.engine.omega_rad_s, rig.engine_params.rated_omega_rad_s);
    EXPECT_EQ(rig.driveline.omega_rad_s, 0.0);
}

TEST(ClutchWithEngine, ReleasingAtIdleStallsUnderHeavyLoad) {
    Rig rig;
    rig.run(1.0, 0.0, 0.0);
    EXPECT_FALSE(rig.engine.running);
}

// Dumping the clutch at full revs: the flywheel's energy is not enough to
// bring the load up to speed and the engine lugs down into a stall.
TEST(ClutchWithEngine, DumpingTheClutchStallsUnderHeavyLoad) {
    Rig rig;
    rig.run(3.0, 1.0, 1.0);
    rig.run(5.0, 1.0, 0.0);
    EXPECT_FALSE(rig.engine.running);
}

// Slipping the clutch in over three seconds lets the load accelerate while
// the engine stays in its torque band; the clutch then locks with both shafts
// turning together.
TEST(ClutchWithEngine, SlippingTheClutchInLaunchesWithoutStalling) {
    Rig rig;
    rig.run(3.0, 1.0, 1.0);
    rig.slip_in(3.0);
    rig.run(8.0, 1.0, 0.0);
    EXPECT_TRUE(rig.engine.running);
    EXPECT_TRUE(rig.clutch.locked);
    EXPECT_GT(rig.driveline.omega_rad_s, rig.engine_params.stall_omega_rad_s);
    EXPECT_NEAR(rig.engine.omega_rad_s, rig.driveline.omega_rad_s, 1e-6);
}

std::uint64_t launch_hash() {
    Rig rig;
    hash::Fnv1a hash;
    const auto record = [&] {
        engine::hash_append(hash, rig.engine);
        clutch::hash_append(hash, rig.clutch);
        hash.add(rig.driveline.omega_rad_s);
    };
    for (int i = 0; i < 3000; ++i) {
        rig.step(1.0, 1.0);
        record();
    }
    for (int i = 0; i <= 3000; ++i) {
        rig.step(1.0, 1.0 - i / 3000.0);
        record();
    }
    for (int i = 0; i < 8000; ++i) {
        rig.step(1.0, 0.0);
        record();
    }
    return hash.value();
}

TEST(ClutchWithEngine, LaunchIsDeterministic) {
    EXPECT_EQ(launch_hash(), launch_hash());
}

// Regression hash; must match on every compiler, build type and platform.
// Update deliberately, with a note in the commit, when the model changes.
TEST(ClutchWithEngine, LaunchMatchesRegressionHash) {
    EXPECT_EQ(launch_hash(), 0x1ddcbfa90e055872ULL);
}

}  // namespace
}  // namespace tp::clutch
