#include "tp/core/tire/tire.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <span>

#include "reference_tire.hpp"

namespace tp::tire {
namespace {

using testing::reference_params;

constexpr double kLoad_n = 10000.0;

// Speeds from -5 to +5 m/s in 0.25 m/s steps, for sweeping both directions.
constexpr int kGridSteps = 40;
double grid_speed(int i) {
    return -5.0 + 0.25 * i;
}

// Wheel surface speed that gives slip ratio `s` at ground speed `v` > 0 with
// the wheel faster (traction).
double wheel_speed_for(double s, double v_m_s) {
    return v_m_s / (1.0 - s);
}

TEST(TireParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(TireParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.radius_m = 0.0; }), ParamError::kRadiusNotPositive);
    EXPECT_EQ(with([](Params& p) { p.radius_m = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.nominal_load_n = 0.0; }), ParamError::kNominalLoadNotPositive);
    EXPECT_EQ(with([](Params& p) { p.load_sensitivity = 1.0; }),
              ParamError::kLoadSensitivityOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.rolling_resistance_coefficient = -0.1; }),
              ParamError::kRollingResistanceOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.wheel_inertia_kg_m2 = 0.0; }),
              ParamError::kInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.min_reference_speed_m_s = 0.0; }),
              ParamError::kMinReferenceSpeedNotPositive);
}

TEST(TireParams, RejectsBadTractionCurves) {
    const auto with_curve = [](std::span<const math::CurvePoint> points) {
        const auto curve = math::Curve::create(points);
        if (!curve) {
            ADD_FAILURE() << "invalid test curve";
            return std::optional<ParamError>{};
        }
        Params p = reference_params();
        p.traction_coefficient = *curve;
        return validate(p);
    };
    const std::array<math::CurvePoint, 2> short_range = {{{0.0, 0.0}, {0.8, 0.5}}};
    const std::array<math::CurvePoint, 2> grip_at_rest = {{{0.0, 0.1}, {1.0, 0.5}}};
    const std::array<math::CurvePoint, 3> negative = {{{0.0, 0.0}, {0.5, -0.1}, {1.0, 0.5}}};
    EXPECT_EQ(with_curve(short_range), ParamError::kCurveDoesNotSpanSlip);
    EXPECT_EQ(with_curve(grip_at_rest), ParamError::kCurveNotZeroAtZeroSlip);
    EXPECT_EQ(with_curve(negative), ParamError::kCurveNegative);
}

TEST(Tire, SlipRatioIsBoundedAndDefinedAtRest) {
    const Params params = reference_params();
    EXPECT_DOUBLE_EQ(slip_ratio(params, 0.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(slip_ratio(params, 3.0, 0.0), 1.0) << "full wheelspin from standstill";
    EXPECT_DOUBLE_EQ(slip_ratio(params, 0.0, 3.0), -1.0) << "locked wheel while moving";
    EXPECT_DOUBLE_EQ(slip_ratio(params, 2.0, 2.0), 0.0) << "free rolling";
    for (int i = 0; i <= kGridSteps; ++i) {
        for (int j = 0; j <= kGridSteps; ++j) {
            const double s = slip_ratio(params, grid_speed(i), grid_speed(j));
            ASSERT_GE(s, -1.0);
            ASSERT_LE(s, 1.0);
        }
    }
}

TEST(Tire, FreeRollingTransmitsNoTraction) {
    const Traction t = traction(reference_params(), 2.0, 2.0, kLoad_n);
    EXPECT_EQ(t.force_n, 0.0);
    EXPECT_EQ(t.slip_ratio, 0.0);
}

// Traction rises with slip up to the peak and falls beyond it.
TEST(Tire, TractionPeaksThenFalls) {
    const Params params = reference_params();
    const auto force_at = [&](int percent) {
        return traction(params, wheel_speed_for(percent / 100.0, 2.0), 2.0, kLoad_n).force_n;
    };
    double previous_n = 0.0;
    for (int percent = 1; percent <= 25; ++percent) {
        ASSERT_GT(force_at(percent), previous_n) << "slip " << percent << " %";
        previous_n = force_at(percent);
    }
    EXPECT_NEAR(previous_n, 0.75 * kLoad_n, 1e-6);
    for (int percent = 26; percent < 100; ++percent) {
        ASSERT_LE(force_at(percent), previous_n) << "slip " << percent << " %";
        previous_n = force_at(percent);
    }
}

TEST(Tire, BrakingMirrorsTraction) {
    const Params params = reference_params();
    for (const double s : {0.05, 0.2, 0.6}) {
        const double drive_n = traction(params, wheel_speed_for(s, 2.0), 2.0, kLoad_n).force_n;
        // Braking: the wheel turns slower than the ground by the same ratio.
        const double brake_n = traction(params, 2.0 * (1.0 - s), 2.0, kLoad_n).force_n;
        EXPECT_NEAR(brake_n, -drive_n, 1e-9) << "slip " << s;
    }
}

// The tire can only take energy out of the relative motion: force and slip
// speed never have opposite signs.
TEST(Tire, NeverAddsEnergy) {
    const Params params = reference_params();
    for (int i = 0; i <= kGridSteps; ++i) {
        for (int j = 0; j <= kGridSteps; ++j) {
            const double wheel = grid_speed(i);
            const double ground = grid_speed(j);
            const double force_n = traction(params, wheel, ground, kLoad_n).force_n;
            ASSERT_GE(force_n * (wheel - ground), 0.0) << wheel << ' ' << ground;
        }
    }
}

// More load gives more traction, but μ falls: less than proportional.
TEST(Tire, LoadSensitivity) {
    const Params params = reference_params();
    const double wheel_m_s = wheel_speed_for(0.25, 2.0);
    double previous_force_n = 0.0;
    double previous_mu = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 12; ++i) {
        const double load_n = 2000.0 * std::pow(1.5, i);
        const double force_n = traction(params, wheel_m_s, 2.0, load_n).force_n;
        const double mu = force_n / load_n;
        ASSERT_GT(force_n, previous_force_n) << "load " << load_n;
        ASSERT_LE(mu, previous_mu * (1.0 + 1e-12)) << "load " << load_n;
        previous_force_n = force_n;
        previous_mu = mu;
    }
    EXPECT_NEAR(traction(params, wheel_m_s, 2.0, params.nominal_load_n).force_n,
                0.75 * params.nominal_load_n, 1e-6);
}

TEST(Tire, NoLoadNoTraction) {
    const Params params = reference_params();
    EXPECT_EQ(traction(params, 5.0, 0.0, 0.0).force_n, 0.0);
    EXPECT_EQ(traction(params, 5.0, 0.0, -100.0).force_n, 0.0);
}

// The slope used for the implicit solve is the change in force per change in
// slip speed with the reference speed held; on the rising side of the curve
// a small drop in ground speed (wheel faster) reproduces it. Beyond the peak
// it is zero.
TEST(Tire, LinearizationMatchesTheCurve) {
    const Params params = reference_params();
    constexpr double kGround_m_s = 3.0;
    constexpr double kDelta_m_s = 1e-6;
    const double rising_m_s = wheel_speed_for(0.1, kGround_m_s);
    const Traction t = traction(params, rising_m_s, kGround_m_s, kLoad_n);
    const Traction nudged = traction(params, rising_m_s, kGround_m_s - kDelta_m_s, kLoad_n);
    EXPECT_NEAR(t.force_per_slip_speed, (nudged.force_n - t.force_n) / kDelta_m_s,
                1e-4 * t.force_per_slip_speed);
    EXPECT_GT(t.force_per_slip_speed, 0.0);

    const double falling_m_s = wheel_speed_for(0.6, kGround_m_s);
    EXPECT_EQ(traction(params, falling_m_s, kGround_m_s, kLoad_n).force_per_slip_speed, 0.0);
}

TEST(Tire, RollingResistanceIsProportionalToLoad) {
    const Params params = reference_params();
    EXPECT_DOUBLE_EQ(rolling_resistance_n(params, kLoad_n), 0.06 * kLoad_n);
    EXPECT_EQ(rolling_resistance_n(params, -5.0), 0.0);
}

}  // namespace
}  // namespace tp::tire
