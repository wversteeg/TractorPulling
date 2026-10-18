#include "tp/core/tire/tire.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "reference_tire.hpp"

namespace tp::tire {
namespace {

using testing::front_size;
using testing::reference_params;
using testing::reference_soil;
using testing::reference_surface;

constexpr double kLoad_n = 10000.0;
constexpr double kGround_m_s = 2.0;

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

double mu_at(const Params& params, double s, const soil::Surface& surface,
             double load_n = kLoad_n) {
    return traction(params, wheel_speed_for(s, kGround_m_s), kGround_m_s, load_n, surface).force_n /
           load_n;
}

// The traction curve sampled in 1 % slip steps: where it peaks and how high.
struct Peak {
    int percent = 0;
    double mu = 0.0;
};

Peak peak_of(const Params& params, const soil::Surface& surface) {
    Peak peak;
    for (int percent = 1; percent < 100; ++percent) {
        const double mu = mu_at(params, percent / 100.0, surface);
        if (mu > peak.mu) {
            peak = {.percent = percent, .mu = mu};
        }
    }
    return peak;
}

soil::Surface softer(soil::Surface surface) {
    surface.cone_index_pa *= 0.25;
    return surface;
}

TEST(TireParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
    EXPECT_EQ(validate(front_size()), std::nullopt);
}

TEST(TireParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.size.radius_m = 0.0; }), ParamError::kRadiusNotPositive);
    EXPECT_EQ(with([](Params& p) { p.size.radius_m = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.size.section_width_m = 0.0; }),
              ParamError::kSectionNotPositive);
    EXPECT_EQ(with([](Params& p) { p.size.section_height_m = -0.1; }),
              ParamError::kSectionNotPositive);
    EXPECT_EQ(with([](Params& p) { p.size.deflection_m = -0.01; }),
              ParamError::kDeflectionOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.size.deflection_m = p.size.section_height_m; }),
              ParamError::kDeflectionOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.limit = 0.0; }),
              ParamError::kTractionLimitOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.limit = 2.5; }),
              ParamError::kTractionLimitOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.mobility_rate = 0.0; }),
              ParamError::kTractionRateNotPositive);
    EXPECT_EQ(with([](Params& p) { p.traction.slip_rate = -1.0; }),
              ParamError::kTractionRateNotPositive);
    EXPECT_EQ(
        with([](Params& p) { p.traction.slip_rate = std::numeric_limits<double>::infinity(); }),
        ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.wheel_inertia_kg_m2 = 0.0; }),
              ParamError::kInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.min_reference_speed_m_s = 0.0; }),
              ParamError::kMinReferenceSpeedNotPositive);
}

// Brixius' mobility number, by hand: CI b d / W (1 + 5 δ/h) / (1 + 3 b/d).
TEST(Tire, MobilityNumberFollowsBrixius) {
    const Size size = reference_params().size;
    const soil::Surface surface = reference_surface();
    const double d_m = 2.0 * size.radius_m;
    const double expected = surface.cone_index_pa * size.section_width_m * d_m / kLoad_n *
                            (1.0 + 5.0 * size.deflection_m / size.section_height_m) /
                            (1.0 + 3.0 * size.section_width_m / d_m);
    EXPECT_NEAR(mobility_number(size, kLoad_n, surface), expected, 1e-12 * expected);
    // A well-prepared track is strong ground for a big tire: Bn well above
    // the 20 to 40 of a field.
    EXPECT_GT(expected, 50.0);
    EXPECT_GT(mobility_number(size, 0.5 * kLoad_n, surface), expected) << "lighter: more mobile";
    EXPECT_LT(mobility_number(size, kLoad_n, softer(surface)), expected);
    EXPECT_TRUE(std::isinf(mobility_number(size, 0.0, surface)));
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
    const Traction t = traction(reference_params(), 2.0, 2.0, kLoad_n, reference_surface());
    EXPECT_EQ(t.force_n, 0.0);
    EXPECT_EQ(t.slip_ratio, 0.0);
}

// On the reference track traction rises with slip to a peak near μ 0.91 at
// a quarter slip, then falls: full wheelspin keeps about 80 % of it.
TEST(Tire, TractionPeaksThenFalls) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    const Peak peak = peak_of(params, surface);
    EXPECT_NEAR(peak.mu, 0.91, 0.02);
    EXPECT_GE(peak.percent, 15);
    EXPECT_LE(peak.percent, 35);

    double previous = 0.0;
    for (int percent = 1; percent <= peak.percent; ++percent) {
        const double mu = mu_at(params, percent / 100.0, surface);
        ASSERT_GT(mu, previous) << "slip " << percent << " %";
        previous = mu;
    }
    for (int percent = peak.percent + 1; percent < 100; ++percent) {
        const double mu = mu_at(params, percent / 100.0, surface);
        ASSERT_LT(mu, previous) << "slip " << percent << " %";
        previous = mu;
    }
    const double full_spin = traction(params, 5.0, 0.0, kLoad_n, surface).force_n / kLoad_n;
    EXPECT_NEAR(full_spin / peak.mu, 0.8, 0.05);
}

// Without fall-off the curve is Brixius': grip only builds with slip.
TEST(Tire, WithoutFallOffGripSaturates) {
    const Params params = reference_params();
    soil::Surface surface = reference_surface();
    surface.spin_fall_off = 0.0;
    double previous = 0.0;
    for (int percent = 1; percent < 100; ++percent) {
        const double mu = mu_at(params, percent / 100.0, surface);
        ASSERT_GE(mu, previous) << "slip " << percent << " %";
        previous = mu;
    }
    EXPECT_LT(previous, params.traction.limit);
}

TEST(Tire, BrakingMirrorsTraction) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    for (const double s : {0.05, 0.2, 0.6}) {
        const double drive_n =
            traction(params, wheel_speed_for(s, 2.0), 2.0, kLoad_n, surface).force_n;
        // Braking: the wheel turns slower than the ground by the same ratio.
        const double brake_n = traction(params, 2.0 * (1.0 - s), 2.0, kLoad_n, surface).force_n;
        EXPECT_NEAR(brake_n, -drive_n, 1e-9) << "slip " << s;
    }
}

// The tire can only take energy out of the relative motion: force and slip
// speed never have opposite signs, on any soil.
TEST(Tire, NeverAddsEnergy) {
    const Params params = reference_params();
    for (const soil::Composition& composition :
         {reference_soil(), soil::Composition{.clay_fraction = 0.9, .moisture = 0.35},
          soil::Composition{.clay_fraction = 0.0, .moisture = 0.0}}) {
        const soil::Surface surface = soil::surface(composition);
        for (int i = 0; i <= kGridSteps; ++i) {
            for (int j = 0; j <= kGridSteps; ++j) {
                const double wheel = grid_speed(i);
                const double ground = grid_speed(j);
                const double force_n = traction(params, wheel, ground, kLoad_n, surface).force_n;
                ASSERT_GE(force_n * (wheel - ground), 0.0) << wheel << ' ' << ground;
            }
        }
    }
}

// More load gives more traction, but μ falls as the tire works the soil
// harder (lower mobility number): less than proportional.
TEST(Tire, LoadSensitivity) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    double previous_force_n = 0.0;
    double previous_mu = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 12; ++i) {
        const double load_n = 2000.0 * std::pow(1.5, i);
        const double mu = mu_at(params, 0.2, surface, load_n);
        ASSERT_GT(mu * load_n, previous_force_n) << "load " << load_n;
        ASSERT_LT(mu, previous_mu) << "load " << load_n;
        previous_force_n = mu * load_n;
        previous_mu = mu;
    }
}

TEST(Tire, NoLoadNoTraction) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    EXPECT_EQ(traction(params, 5.0, 0.0, 0.0, surface).force_n, 0.0);
    EXPECT_EQ(traction(params, 5.0, 0.0, -100.0, surface).force_n, 0.0);
    EXPECT_EQ(driven_rolling_resistance(params.size, 0.0, 0.5, surface), 0.0);
}

// Softer ground: the tire sinks in, grips less and rolls harder.
TEST(Tire, SofterSoilGripsLessAndRollsHarder) {
    const Params params = reference_params();
    const soil::Surface firm = reference_surface();
    const soil::Surface soft = softer(firm);
    EXPECT_LT(peak_of(params, soft).mu, peak_of(params, firm).mu);
    EXPECT_GT(driven_rolling_resistance(params.size, kLoad_n, 0.1, soft),
              driven_rolling_resistance(params.size, kLoad_n, 0.1, firm));
    EXPECT_GT(towed_rolling_resistance(front_size(), kLoad_n / 2.0, soft),
              towed_rolling_resistance(front_size(), kLoad_n / 2.0, firm));
}

// Grip scales with the surface's grip factor (dust, a slick film).
TEST(Tire, GripFactorScalesTraction) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    soil::Surface slick = surface;
    slick.grip_factor = 0.7 * surface.grip_factor;
    EXPECT_NEAR(mu_at(params, 0.2, slick), 0.7 * mu_at(params, 0.2, surface), 1e-12);
}

// A slipping tire digs a deeper rut: motion resistance grows with slip.
// A towed tire adds its own losses on top of the sinkage.
TEST(Tire, RollingResistance) {
    const Size rear = reference_params().size;
    const soil::Surface surface = reference_surface();
    const double sinkage = 1.0 / mobility_number(rear, kLoad_n, surface);
    EXPECT_NEAR(driven_rolling_resistance(rear, kLoad_n, 0.0, surface), sinkage, 1e-15);
    double previous = 0.0;
    for (int percent = 0; percent <= 100; percent += 10) {
        const double resistance =
            driven_rolling_resistance(rear, kLoad_n, percent / 100.0, surface);
        ASSERT_GT(resistance, previous) << "slip " << percent << " %";
        EXPECT_EQ(driven_rolling_resistance(rear, kLoad_n, -percent / 100.0, surface), resistance);
        previous = resistance;
    }
    EXPECT_NEAR(towed_rolling_resistance(rear, kLoad_n, surface), sinkage + 0.04, 1e-15);
    // The reference front tires roll at about the 0.06 of a firm track.
    EXPECT_NEAR(towed_rolling_resistance(front_size(), 7000.0, surface), 0.06, 0.01);
}

// A tire throws back the soil of its rut along the distance its surface
// travels: more the faster it turns, more when it slips (it digs deeper),
// more on soft ground, and even when it spins in place.
TEST(Tire, ThrownSoilGrowsWithSlipSpeedAndSoftness) {
    const Size size = reference_params().size;
    const soil::Surface firm = reference_surface();
    const double rolling = thrown_soil_m3_s(size, kLoad_n, 0.05, 2.0, firm);
    EXPECT_GT(rolling, 0.0);
    EXPECT_NEAR(thrown_soil_m3_s(size, kLoad_n, 0.05, 4.0, firm), 2.0 * rolling, 1e-15);
    EXPECT_GT(thrown_soil_m3_s(size, kLoad_n, 0.5, 2.0, firm), rolling);
    EXPECT_GT(thrown_soil_m3_s(size, kLoad_n, 0.05, 2.0, softer(firm)), rolling);
    EXPECT_GT(thrown_soil_m3_s(size, kLoad_n, 1.0, 3.0, firm), 0.0) << "spinning in place";
    EXPECT_EQ(thrown_soil_m3_s(size, kLoad_n, 0.0, 0.0, firm), 0.0);
    EXPECT_EQ(thrown_soil_m3_s(size, 0.0, 0.5, 2.0, firm), 0.0);
}

// The slope used for the implicit solve is the change in force per change in
// slip speed with the reference speed held; on the rising side of the curve
// a small drop in ground speed (wheel faster) reproduces it. Beyond the peak
// it is zero.
TEST(Tire, LinearizationMatchesTheCurve) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    constexpr double kFast_m_s = 3.0;
    constexpr double kDelta_m_s = 1e-6;
    const double rising_m_s = wheel_speed_for(0.1, kFast_m_s);
    const Traction t = traction(params, rising_m_s, kFast_m_s, kLoad_n, surface);
    const Traction nudged = traction(params, rising_m_s, kFast_m_s - kDelta_m_s, kLoad_n, surface);
    EXPECT_NEAR(t.force_per_slip_speed, (nudged.force_n - t.force_n) / kDelta_m_s,
                1e-4 * t.force_per_slip_speed);
    EXPECT_GT(t.force_per_slip_speed, 0.0);

    const double falling_m_s = wheel_speed_for(0.6, kFast_m_s);
    EXPECT_EQ(traction(params, falling_m_s, kFast_m_s, kLoad_n, surface).force_per_slip_speed, 0.0);
}

}  // namespace
}  // namespace tp::tire
