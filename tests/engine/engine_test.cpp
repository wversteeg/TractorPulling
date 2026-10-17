#include "tp/core/engine/engine.hpp"

#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "reference_engine.hpp"

namespace tp::engine {
namespace {

using testing::reference_params;
using units::rpm_to_rad_s;

constexpr double kDt_s = 0.001;

struct RunStats {
    double min_omega_rad_s = std::numeric_limits<double>::infinity();
    double max_omega_rad_s = 0.0;
};

// Runs `seconds` of constant input and returns the speed range seen.
RunStats run(State& state, const Params& params, const Input& input, double seconds) {
    RunStats stats;
    const auto steps = static_cast<int>(std::lround(seconds / kDt_s));
    for (int i = 0; i < steps; ++i) {
        step(state, params, input, kDt_s);
        stats.min_omega_rad_s = std::min(stats.min_omega_rad_s, state.omega_rad_s);
        stats.max_omega_rad_s = std::max(stats.max_omega_rad_s, state.omega_rad_s);
    }
    return stats;
}

TEST(EngineParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(EngineParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.inertia_kg_m2 = 0.0; }), ParamError::kInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.inertia_kg_m2 = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.stall_omega_rad_s = 0.0; }), ParamError::kStallNotPositive);
    EXPECT_EQ(with([](Params& p) { p.stall_omega_rad_s = p.idle_omega_rad_s; }),
              ParamError::kStallNotBelowIdle);
    EXPECT_EQ(with([](Params& p) { p.idle_omega_rad_s = p.rated_omega_rad_s; }),
              ParamError::kIdleNotBelowRated);
    EXPECT_EQ(with([](Params& p) { p.rated_omega_rad_s = p.high_idle_omega_rad_s; }),
              ParamError::kRatedNotBelowHighIdle);
    EXPECT_EQ(with([](Params& p) { p.viscous_friction_nm_s_rad = -0.1; }),
              ParamError::kFrictionNegative);
    EXPECT_EQ(with([](Params& p) { p.na_torque_fraction = 0.0; }),
              ParamError::kNaTorqueFractionOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.na_torque_fraction = 1.1; }),
              ParamError::kNaTorqueFractionOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.turbo_time_constant_s = 0.0; }),
              ParamError::kTurboTimeConstantNotPositive);
    EXPECT_EQ(with([](Params& p) { p.boost_onset_omega_rad_s = p.full_boost_omega_rad_s; }),
              ParamError::kBoostRangeInvalid);
    EXPECT_EQ(with([](Params& p) { p.high_idle_omega_rad_s = rpm_to_rad_s(3000.0); }),
              ParamError::kTorqueCurveDoesNotCoverRange);
}

TEST(EngineParams, RejectsNegativeTorque) {
    const std::array<math::CurvePoint, 2> negative = {{{0.0, -1.0}, {500.0, 100.0}}};
    const auto curve = math::Curve::create(negative);
    if (!curve) {
        FAIL() << "invalid test curve";
    }
    Params p = reference_params();
    p.full_load_torque_nm = *curve;
    EXPECT_EQ(validate(p), ParamError::kTorqueCurveNegative);
}

TEST(Engine, StartsRunningAtIdle) {
    const Params params = reference_params();
    const State state = initial_state(params);
    EXPECT_TRUE(state.running);
    EXPECT_DOUBLE_EQ(state.omega_rad_s, params.idle_omega_rad_s);
    EXPECT_DOUBLE_EQ(state.boost, 0.0);
}

TEST(Engine, HoldsIdleWithoutThrottle) {
    const Params params = reference_params();
    State state = initial_state(params);
    run(state, params, {.throttle = 0.0}, 5.0);
    const RunStats settled = run(state, params, {.throttle = 0.0}, 1.0);
    EXPECT_TRUE(state.running);
    EXPECT_LE(settled.max_omega_rad_s, params.idle_omega_rad_s);
    EXPECT_GT(settled.min_omega_rad_s,
              params.idle_omega_rad_s - (params.high_idle_omega_rad_s - params.rated_omega_rad_s));
}

// Full throttle with no load: the governor settles just below high idle,
// which it never exceeds, and does not oscillate.
TEST(Engine, GovernorHoldsHighIdleWithoutOvershoot) {
    const Params params = reference_params();
    State state = initial_state(params);
    const RunStats rev_up = run(state, params, {.throttle = 1.0}, 5.0);
    const RunStats settled = run(state, params, {.throttle = 1.0}, 1.0);
    EXPECT_LE(rev_up.max_omega_rad_s, params.high_idle_omega_rad_s);
    EXPECT_GT(settled.min_omega_rad_s, params.rated_omega_rad_s);
    EXPECT_LT(settled.max_omega_rad_s - settled.min_omega_rad_s, rpm_to_rad_s(1.0));
}

TEST(Engine, ThrottleIsClamped) {
    const Params params = reference_params();
    State full = initial_state(params);
    State over = initial_state(params);
    run(full, params, {.throttle = 1.0}, 2.0);
    run(over, params, {.throttle = 5.0}, 2.0);
    EXPECT_EQ(full.omega_rad_s, over.omega_rad_s);
}

// Governor droop: at full throttle, steady speed falls as the load rises,
// and the engine still holds above rated speed up to its rated torque. The
// load is applied after revving up: at idle, without boost, it would stall.
TEST(Engine, SteadySpeedDroopsWithLoad) {
    const Params params = reference_params();
    double previous_omega_rad_s = std::numeric_limits<double>::infinity();
    for (const double load_nm : {0.0, 50.0, 100.0, 200.0, 280.0}) {
        State state = initial_state(params);
        run(state, params, {.throttle = 1.0}, 3.0);
        run(state, params, {.throttle = 1.0, .load_torque_nm = load_nm}, 10.0);
        EXPECT_TRUE(state.running) << "load " << load_nm;
        EXPECT_LT(state.omega_rad_s, previous_omega_rad_s) << "load " << load_nm;
        EXPECT_GT(state.omega_rad_s, params.rated_omega_rad_s) << "load " << load_nm;
        previous_omega_rad_s = state.omega_rad_s;
    }
}

// Above rated torque the engine lugs down the torque curve, and holds where
// the falling curve meets the load (torque backup).
TEST(Engine, LugsDownBelowRatedSpeedUnderHeavyLoad) {
    const Params params = reference_params();
    State state = initial_state(params);
    run(state, params, {.throttle = 1.0}, 3.0);
    run(state, params, {.throttle = 1.0, .load_torque_nm = 360.0}, 10.0);
    EXPECT_TRUE(state.running);
    EXPECT_LT(state.omega_rad_s, params.rated_omega_rad_s);
    EXPECT_GT(state.omega_rad_s, rpm_to_rad_s(1400.0));
}

TEST(Engine, StallsWhenLoadedAtIdle) {
    const Params params = reference_params();
    State state = initial_state(params);
    run(state, params, {.throttle = 1.0, .load_torque_nm = 250.0}, 2.0);
    EXPECT_FALSE(state.running);
}

TEST(Engine, StallsUnderOverloadAndStaysStalled) {
    const Params params = reference_params();
    State state = initial_state(params);
    run(state, params, {.throttle = 1.0}, 3.0);
    const double overload_nm = 1.2 * params.full_load_torque_nm.max_y();
    run(state, params, {.throttle = 1.0, .load_torque_nm = overload_nm}, 5.0);
    EXPECT_FALSE(state.running);

    const Output out = step(state, params, {.throttle = 1.0}, kDt_s);
    EXPECT_DOUBLE_EQ(out.fuel, 0.0);
    EXPECT_DOUBLE_EQ(out.combustion_torque_nm, 0.0);
}

// A stopped engine only loses speed, comes to rest, and never turns backwards.
TEST(Engine, CoastsDownToRestWhenStopped) {
    const Params params = reference_params();
    State state{.omega_rad_s = rpm_to_rad_s(2000.0), .boost = 0.0, .running = false};
    double previous_omega_rad_s = state.omega_rad_s;
    for (int i = 0; i < 60'000; ++i) {
        step(state, params, {}, kDt_s);
        ASSERT_LE(state.omega_rad_s, previous_omega_rad_s);
        ASSERT_GE(state.omega_rad_s, 0.0);
        previous_omega_rad_s = state.omega_rad_s;
    }
    EXPECT_DOUBLE_EQ(state.omega_rad_s, 0.0);
}

// Turbo lag in isolation: hold the speed with a huge inertia and check the
// first-order response, boost reaching ~63 % after one time constant.
TEST(Engine, TurboSpoolsUpWithFirstOrderLag) {
    Params params = reference_params();
    params.inertia_kg_m2 = 1e12;
    State state{.omega_rad_s = rpm_to_rad_s(1800.0), .boost = 0.0, .running = true};

    double previous_torque_nm = 0.0;
    const auto steps = static_cast<int>(std::lround(params.turbo_time_constant_s / kDt_s));
    for (int i = 0; i < steps; ++i) {
        const Output out = step(state, params, {.throttle = 1.0}, kDt_s);
        ASSERT_GE(out.combustion_torque_nm, previous_torque_nm);
        previous_torque_nm = out.combustion_torque_nm;
    }
    EXPECT_NEAR(state.boost, 1.0 - std::exp(-1.0), 0.005);
}

TEST(Engine, NoBoostBelowOnsetSpeed) {
    Params params = reference_params();
    params.inertia_kg_m2 = 1e12;
    State state{.omega_rad_s = rpm_to_rad_s(900.0), .boost = 0.0, .running = true};
    run(state, params, {.throttle = 1.0}, 5.0);
    EXPECT_DOUBLE_EQ(state.boost, 0.0);
}

// Without boost the engine makes exactly the naturally aspirated fraction of
// its full-load torque.
TEST(Engine, TorqueWithoutBoostIsNaturallyAspiratedFraction) {
    const Params params = reference_params();
    State state{.omega_rad_s = rpm_to_rad_s(900.0), .boost = 0.0, .running = true};
    const Output out = step(state, params, {.throttle = 1.0}, kDt_s);
    EXPECT_DOUBLE_EQ(out.fuel, 1.0);
    EXPECT_DOUBLE_EQ(
        out.combustion_torque_nm,
        params.na_torque_fraction * params.full_load_torque_nm.evaluate(rpm_to_rad_s(900.0)));
}

// When the driveline drives the engine above its governed speed, the governor
// cuts fuel completely.
TEST(Engine, CutsFuelWhenOverrun) {
    const Params params = reference_params();
    State state{.omega_rad_s = params.high_idle_omega_rad_s + 1.0, .boost = 1.0, .running = true};
    const Output out = step(state, params, {.throttle = 1.0, .load_torque_nm = -500.0}, kDt_s);
    EXPECT_DOUBLE_EQ(out.fuel, 0.0);
    EXPECT_DOUBLE_EQ(out.combustion_torque_nm, 0.0);
}

TEST(Engine, PredictDoesNotChangeState) {
    const Params params = reference_params();
    const State before{.omega_rad_s = rpm_to_rad_s(1500.0), .boost = 0.3, .running = true};
    const State state = before;
    static_cast<void>(predict(state, params, 1.0, kDt_s));
    EXPECT_EQ(state.omega_rad_s, before.omega_rad_s);
    EXPECT_EQ(state.boost, before.boost);
}

// The linear speed prediction a coupling solves against must match what
// advance() then does for any load torque.
TEST(Engine, ShaftPredictionMatchesAdvance) {
    const Params params = reference_params();
    const State start{.omega_rad_s = rpm_to_rad_s(1500.0), .boost = 0.3, .running = true};
    const Prediction p = predict(start, params, 0.7, kDt_s);
    const coupling::ShaftPrediction shaft = shaft_prediction(p);
    for (const double load_nm : {-300.0, 0.0, 150.0, 600.0}) {
        State state = start;
        advance(state, params, p, load_nm);
        const double predicted_rad_s =
            shaft.free_omega_next_rad_s + shaft.omega_per_torque * -load_nm;
        EXPECT_NEAR(state.omega_rad_s, predicted_rad_s, 1e-12) << "load " << load_nm;
    }
    EXPECT_GT(shaft.omega_per_torque, 0.0);
}

TEST(Engine, StepEqualsPredictThenAdvance) {
    const Params params = reference_params();
    State stepped{.omega_rad_s = rpm_to_rad_s(1500.0), .boost = 0.3, .running = true};
    State split = stepped;
    const Output out = step(stepped, params, {.throttle = 0.7, .load_torque_nm = 120.0}, kDt_s);
    const Prediction p = predict(split, params, 0.7, kDt_s);
    advance(split, params, p, 120.0);
    EXPECT_EQ(stepped.omega_rad_s, split.omega_rad_s);
    EXPECT_EQ(stepped.boost, split.boost);
    EXPECT_EQ(out.combustion_torque_nm, p.output.combustion_torque_nm);
}

}  // namespace
}  // namespace tp::engine
