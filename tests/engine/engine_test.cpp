#include "tp/core/engine/engine.hpp"

#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
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

// Full fuel starts a droop below the governor's target: at rated speed at
// full throttle, rising with the throttle.
TEST(Engine, FullFuelSpeedFollowsTheThrottle) {
    const Params params = reference_params();
    const double droop_rad_s = params.high_idle_omega_rad_s - params.rated_omega_rad_s;
    EXPECT_NEAR(full_fuel_omega_rad_s(params, 1.0), params.rated_omega_rad_s, 1e-9);
    EXPECT_NEAR(full_fuel_omega_rad_s(params, 0.0), params.idle_omega_rad_s - droop_rad_s, 1e-9);
    EXPECT_LT(full_fuel_omega_rad_s(params, 0.3), full_fuel_omega_rad_s(params, 0.6));
    // The throttle is held to its travel.
    EXPECT_EQ(full_fuel_omega_rad_s(params, 1.5), full_fuel_omega_rad_s(params, 1.0));
    EXPECT_EQ(full_fuel_omega_rad_s(params, -0.5), full_fuel_omega_rad_s(params, 0.0));
    // Just below it the governor gives full fuel, just above it less.
    const double at_rad_s = full_fuel_omega_rad_s(params, 0.5);
    State below = initial_state(params);
    below.omega_rad_s = at_rad_s - 1.0;
    EXPECT_EQ(predict(below, params, 0.5, 0.001).output.fuel, 1.0);
    State above = below;
    above.omega_rad_s = at_rad_s + 1.0;
    EXPECT_LT(predict(above, params, 0.5, 0.001).output.fuel, 1.0);
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
    const double load_nm = 0.84 * params.full_load_torque_nm.max_y();
    run(state, params, {.throttle = 1.0, .load_torque_nm = load_nm}, 10.0);
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

// The decaying turbo lag must settle at exactly zero instead of sliding into
// subnormal numbers, which are very slow on many CPUs.
TEST(Engine, BoostDecaysToExactlyZero) {
    const Params params = reference_params();
    State state{.omega_rad_s = 0.0, .boost = 1.0, .running = false};
    for (int i = 0; i < 1'000'000; ++i) {
        step(state, params, {}, kDt_s);
        ASSERT_NE(std::fpclassify(state.boost), FP_SUBNORMAL) << "step " << i;
    }
    EXPECT_EQ(state.boost, 0.0);
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

// Net shaft power at full throttle and full boost, by brute force.
double net_power_w(const Params& params, double omega_rad_s) {
    const double fuel = std::clamp((params.high_idle_omega_rad_s - omega_rad_s) /
                                       (params.high_idle_omega_rad_s - params.rated_omega_rad_s),
                                   0.0, 1.0);
    const double torque_nm = fuel * params.full_load_torque_nm.evaluate(omega_rad_s) -
                             params.friction_torque_nm -
                             params.viscous_friction_nm_s_rad * omega_rad_s;
    return torque_nm * omega_rad_s;
}

TEST(Engine, MaxNetPowerIsThePeakOfTheFullLoadCurve) {
    const Params params = reference_params();
    double brute_force_w = 0.0;
    for (int rpm = 800; rpm <= 2400; ++rpm) {
        brute_force_w = std::max(brute_force_w, net_power_w(params, rpm_to_rad_s(rpm)));
    }
    EXPECT_GE(max_net_power_w(params), brute_force_w);
    EXPECT_NEAR(max_net_power_w(params), brute_force_w, 1e-4 * brute_force_w);
    // Above rated speed the governor takes fuel away: no power at high idle.
    EXPECT_LT(net_power_w(params, params.high_idle_omega_rad_s), 0.0);
}

// Power scales with torque when there is no friction.
TEST(Engine, MaxNetPowerScalesWithTorque) {
    Params params = reference_params();
    params.friction_torque_nm = 0.0;
    params.viscous_friction_nm_s_rad = 0.0;
    const double base_w = max_net_power_w(params);
    std::vector<math::CurvePoint> doubled;
    for (const math::CurvePoint& point : params.full_load_torque_nm.points()) {
        doubled.push_back({.x = point.x, .y = 2.0 * point.y});
    }
    const auto curve = math::Curve::create(doubled);
    ASSERT_TRUE(curve.has_value());
    params.full_load_torque_nm = curve.value_or(params.full_load_torque_nm);
    EXPECT_DOUBLE_EQ(max_net_power_w(params), 2.0 * base_w);
}

// A governor set higher (the garage's): full fuel up to a higher speed and
// the high idle moved with it, the droop and the torque curve kept. Under
// a load the engine holds a higher speed, giving the same torque there.
TEST(Engine, AGovernorSetHigherHoldsAHigherSpeed) {
    const Params stock = reference_params();
    const std::optional<Params> raised = with_rated_speed(stock, rpm_to_rad_s(2500.0));
    ASSERT_TRUE(raised.has_value());
    const Params& p = raised.value_or(stock);
    EXPECT_EQ(p.rated_omega_rad_s, rpm_to_rad_s(2500.0));
    EXPECT_DOUBLE_EQ(p.high_idle_omega_rad_s - p.rated_omega_rad_s,
                     stock.high_idle_omega_rad_s - stock.rated_omega_rad_s);
    EXPECT_EQ(p.full_load_torque_nm.points().size(), stock.full_load_torque_nm.points().size());
    EXPECT_EQ(with_rated_speed(stock, stock.rated_omega_rad_s).value_or(p).high_idle_omega_rad_s,
              stock.high_idle_omega_rad_s);

    constexpr double kLoad_nm = 200.0;
    State held = initial_state(stock);
    State faster = initial_state(p);
    run(held, stock, {.throttle = 1.0}, 3.0);
    run(faster, p, {.throttle = 1.0}, 3.0);
    run(held, stock, {.throttle = 1.0, .load_torque_nm = kLoad_nm}, 10.0);
    run(faster, p, {.throttle = 1.0, .load_torque_nm = kLoad_nm}, 10.0);
    EXPECT_GT(faster.omega_rad_s, held.omega_rad_s + rpm_to_rad_s(250.0));
    // About the same power at the higher speed: within 10 % of the peak.
    EXPECT_GT(max_net_power_w(p), 0.9 * max_net_power_w(stock));
}

// Beyond what the torque curve covers, or not above idle, it is no engine.
TEST(Engine, AGovernorNeedsTheTorqueCurve) {
    const Params stock = reference_params();
    EXPECT_FALSE(with_rated_speed(stock, stock.full_load_torque_nm.max_x()).has_value());
    EXPECT_FALSE(with_rated_speed(stock, stock.idle_omega_rad_s).has_value());
    EXPECT_FALSE(with_rated_speed(stock, std::numeric_limits<double>::quiet_NaN()).has_value());
}

}  // namespace
}  // namespace tp::engine
