#include "tp/core/tire/tire.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <utility>

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

// The reference tire without its response to how fast its tread spins
// (TractionCurve): the slip-ratio curve alone, Brixius' with the fall-off.
Params unspun() {
    Params p = reference_params();
    p.traction.slow_spin_grip = 1.0;
    p.traction.fast_spin_grip = 1.0;
    return p;
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
    EXPECT_EQ(with([](Params& p) { p.traction.load_sensitivity = -0.1; }),
              ParamError::kLoadSensitivityInvalid);
    EXPECT_EQ(with([](Params& p) {
                  p.traction.load_sensitivity = 1.0;
                  p.traction.reference_load_n = 10000.0;
              }),
              ParamError::kLoadSensitivityInvalid);
    EXPECT_EQ(with([](Params& p) {
                  p.traction.load_sensitivity = 0.3;
                  p.traction.reference_load_n = 0.0;
              }),
              ParamError::kLoadSensitivityInvalid);
    EXPECT_EQ(with([](Params& p) { p.wheel_inertia_kg_m2 = 0.0; }),
              ParamError::kInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.min_reference_speed_m_s = 0.0; }),
              ParamError::kMinReferenceSpeedNotPositive);
    EXPECT_EQ(with([](Params& p) { p.traction.slow_spin_grip = 0.0; }),
              ParamError::kSpinGripOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.fast_spin_grip = 1.1; }),
              ParamError::kSpinGripOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.spin_speed_m_s = 0.0; }),
              ParamError::kSpinGripOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.traction.fast_spin_grip = std::nan(""); }),
              ParamError::kNotFinite);
}

// Brixius' mobility number, by hand: CI b d / W (1 + 5 δ/h) / (1 + 3 b/d).
// Above its reference load a tire grips less per unit of load, so pressing
// it down harder still adds grip, but less and less: up to the reference
// load (and with no sensitivity) the coefficient is as the curve says.
TEST(Tire, PressedHarderItGripsLessPerUnitOfLoad) {
    Params params = reference_params();
    const soil::Surface surface = reference_surface();
    const double plain = full_bite(params, 2.0 * kLoad_n, surface);
    params.traction.load_sensitivity = 0.3;
    params.traction.reference_load_n = kLoad_n;
    EXPECT_EQ(load_factor(params.traction, 0.5 * kLoad_n), 1.0);
    EXPECT_EQ(load_factor(params.traction, kLoad_n), 1.0);
    EXPECT_NEAR(load_factor(params.traction, 2.0 * kLoad_n), 1.0 / 1.3, 1e-12);
    EXPECT_NEAR(full_bite(params, 2.0 * kLoad_n, surface), plain / 1.3, 1e-12);
    double last_n = 0.0;
    for (const double share : {1.0, 1.5, 2.0, 3.0, 4.0}) {
        const double grip_n = full_bite(params, share * kLoad_n, surface) * share * kLoad_n;
        EXPECT_GT(grip_n, last_n) << "more load, still more grip";
        last_n = grip_n;
    }
    EXPECT_LT(load_factor(params.traction, 3.0 * kLoad_n),
              load_factor(params.traction, 2.0 * kLoad_n));
}

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
// a quarter slip, then falls: full wheelspin keeps about 80 % of it. (The
// slip-ratio curve: braking at the same ratio sweeps the tread slower than
// driving, so with the spin response neither mirrors exactly.)
TEST(Tire, TractionPeaksThenFalls) {
    const Params params = unspun();
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
    const Params params = unspun();
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
    const Params params = unspun();
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

// ---- How fast the tread spins

// A puller tire: little grip while its tread sweeps slowly, full grip
// spinning fast.
Params puller() {
    Params p = unspun();
    p.traction.slow_spin_grip = 0.6;
    p.traction.spin_speed_m_s = 4.0;
    return p;
}

// A farm tire's lugs: full grip at little slip speed, some lost spun fast.
Params farm() {
    Params p = unspun();
    p.traction.fast_spin_grip = 0.8;
    p.traction.spin_speed_m_s = 4.0;
    return p;
}

// Without the spin response (the default) grip depends on the slip ratio
// alone, as before, bit for bit.
TEST(TireSpin, OffByDefault) {
    constexpr TractionCurve kDefault;
    EXPECT_EQ(kDefault.slow_spin_grip, 1.0);
    EXPECT_EQ(kDefault.fast_spin_grip, 1.0);
    const Params p = unspun();
    const soil::Surface surface = reference_surface();
    Params unchanged_speed = p;
    unchanged_speed.traction.spin_speed_m_s = 7.0;
    for (const double ground_m_s : {0.5, 2.0, 6.0}) {
        const double wheel_m_s = wheel_speed_for(0.3, ground_m_s);
        const Traction a = traction(p, wheel_m_s, ground_m_s, kLoad_n, surface);
        const Traction b = traction(unchanged_speed, wheel_m_s, ground_m_s, kLoad_n, surface);
        EXPECT_EQ(a.force_n, b.force_n);
        EXPECT_EQ(a.force_per_slip_speed, b.force_per_slip_speed);
    }
}

// At the same slip ratio, a puller tire grips harder the faster its tread
// sweeps: at the end of a pull, where the tractor has nearly stopped, the
// wheels' speed at the governor (the gearing) decides its grip. A farm
// tire the other way round. Neither grips more than without the response.
TEST(TireSpin, APullerTireBitesSpinningFastAFarmTireSmears) {
    const soil::Surface surface = reference_surface();
    const auto force = [&](const Params& p, double ground_m_s) {
        return traction(p, wheel_speed_for(0.5, ground_m_s), ground_m_s, kLoad_n, surface).force_n;
    };
    const Params plain = unspun();
    for (const auto& [slow_m_s, fast_m_s] : {std::pair{0.5, 2.0}, {2.0, 8.0}}) {
        EXPECT_GT(force(puller(), fast_m_s), force(puller(), slow_m_s));
        EXPECT_LT(force(farm(), fast_m_s), force(farm(), slow_m_s));
        EXPECT_LE(force(puller(), fast_m_s), force(plain, fast_m_s));
        EXPECT_LE(force(farm(), slow_m_s), force(plain, slow_m_s));
    }
    // Spinning very fast the puller tire has all its grip, the farm tire
    // what it keeps.
    constexpr double kVeryFast_m_s = 40.0;
    EXPECT_NEAR(force(puller(), kVeryFast_m_s), force(plain, kVeryFast_m_s),
                1e-3 * force(plain, kVeryFast_m_s));
    EXPECT_NEAR(force(farm(), kVeryFast_m_s), 0.8 * force(plain, kVeryFast_m_s),
                1e-3 * force(plain, kVeryFast_m_s));
}

// The tread's grip share (k(v)) holds across the tire as along it: the
// same share of its grip is in use as without the response, and its side
// grip is k(v) of the unspun tire's. A farm tire spun fast smears both
// ways (less side grip, never more), and a puller tire turning slowly
// grips less both ways.
TEST(TireSpin, SideGripSharesTheSpinResponse) {
    const soil::Surface surface = reference_surface();
    const auto at = [&](const Params& p, double ground_m_s) {
        return traction(p, wheel_speed_for(0.5, ground_m_s), ground_m_s, kLoad_n, surface);
    };
    const auto side_limit_n = [&](const Params& p, double ground_m_s) {
        return with_traction(side_grip(p.size, p.cornering, kLoad_n, surface), at(p, ground_m_s),
                             kLoad_n)
            .limit_n;
    };
    for (const double ground_m_s : {0.5, 4.0, 20.0}) {
        const double unspun_n = side_limit_n(unspun(), ground_m_s);
        for (const Params& p : {farm(), puller()}) {
            const Traction t = at(p, ground_m_s);
            EXPECT_NEAR(std::abs(t.force_n) / (t.full_bite * kLoad_n),
                        std::abs(at(unspun(), ground_m_s).force_n) /
                            (full_bite(p, kLoad_n, surface) * kLoad_n),
                        1e-12)
                << "the same share of its grip in use as without the response";
            EXPECT_NEAR(side_limit_n(p, ground_m_s), t.grip_share * unspun_n, 1e-9 * unspun_n);
            EXPECT_LE(side_limit_n(p, ground_m_s), unspun_n * (1.0 + 1e-12));
        }
    }
    // Spun fast the farm tire has clearly less side grip; a puller tire has
    // its least turning slowly.
    EXPECT_LT(side_limit_n(farm(), 20.0), 0.9 * side_limit_n(unspun(), 20.0));
    EXPECT_LT(
        side_limit_n(puller(), 0.5),
        side_limit_n(puller(), 20.0) * side_limit_n(unspun(), 0.5) / side_limit_n(unspun(), 20.0));
    // Without the response the share is 1 and the side grip as before.
    EXPECT_EQ(at(unspun(), 4.0).grip_share, 1.0);
    const Traction plain = at(unspun(), 4.0);
    EXPECT_EQ(side_limit_n(unspun(), 4.0),
              with_traction(side_grip(unspun().size, unspun().cornering, kLoad_n, surface),
                            plain.force_n, plain.full_bite * kLoad_n, plain.slip_ratio)
                  .limit_n);
}

// The implicit solve's slope includes the spin response: a small drop in
// ground speed reproduces it on the rising side.
TEST(TireSpin, LinearizationIncludesTheSpinResponse) {
    const soil::Surface surface = reference_surface();
    constexpr double kFast_m_s = 3.0;
    constexpr double kDelta_m_s = 1e-6;
    for (const Params& p : {puller(), farm()}) {
        const double wheel_m_s = wheel_speed_for(0.1, kFast_m_s);
        const Traction t = traction(p, wheel_m_s, kFast_m_s, kLoad_n, surface);
        const Traction nudged = traction(p, wheel_m_s, kFast_m_s - kDelta_m_s, kLoad_n, surface);
        EXPECT_NEAR(t.force_per_slip_speed, (nudged.force_n - t.force_n) / kDelta_m_s,
                    1e-4 * t.force_per_slip_speed);
    }
}

// A puller tire's bars need some spin to bite and lose some of it spun
// very fast (TractionCurve::fall_speed_m_s): at the same slip ratio its
// grip rises from slow_spin_grip with the slip speed, then eases towards
// fast_spin_grip of the unspun tire's; the slope the solve uses follows
// both sides.
TEST(TireSpin, ARisingAndFallingTreadBitesThenEases) {
    const soil::Surface surface = reference_surface();
    Params bars = unspun();
    bars.traction.slow_spin_grip = 0.6;
    bars.traction.spin_speed_m_s = 3.0;
    bars.traction.fast_spin_grip = 0.7;
    bars.traction.fall_speed_m_s = 20.0;
    const auto share = [&](double ground_m_s) {
        return traction(bars, wheel_speed_for(0.5, ground_m_s), ground_m_s, kLoad_n, surface)
            .grip_share;
    };
    // Slip speeds of 0.1, 10 and 1000 m/s at half slip.
    EXPECT_NEAR(share(0.1), 0.6, 0.02) << "turning slowly";
    EXPECT_GT(share(10.0), share(0.1));
    EXPECT_GT(share(10.0), share(1000.0)) << "eased spun very fast";
    EXPECT_NEAR(share(1000.0), 0.7, 1e-6);
    constexpr double kDelta_m_s = 1e-6;
    for (const double ground_m_s : {1.0, 30.0}) {
        const double wheel_m_s = wheel_speed_for(0.1, ground_m_s);
        const Traction t = traction(bars, wheel_m_s, ground_m_s, kLoad_n, surface);
        const Traction nudged =
            traction(bars, wheel_m_s, ground_m_s - kDelta_m_s, kLoad_n, surface);
        const double slope = (nudged.force_n - t.force_n) / kDelta_m_s;
        EXPECT_NEAR(t.force_per_slip_speed, std::max(slope, 0.0), 1e-4 * std::abs(slope) + 1e-6);
    }
    bars.traction.fall_speed_m_s = -1.0;
    EXPECT_EQ(validate(bars), ParamError::kSpinGripOutOfRange);
}

// The soil's reaction as a tire throws it grows with the soil and with the
// slip speed, and follows the slip's sign: a spinning tread is pushed on, a
// skidding one back; with nothing thrown, or no slip, nothing.
TEST(ThrownSoil, ThrowingItCostsTheTire) {
    constexpr double kSoil_m3_s = 0.02;
    constexpr double kSlip_m_s = 10.0;
    const double force_n = throw_force_n(kSoil_m3_s, kSlip_m_s);
    EXPECT_DOUBLE_EQ(force_n, kThrownShare * soil::kLooseDensity_kg_m3 * kSoil_m3_s * kSlip_m_s);
    EXPECT_DOUBLE_EQ(throw_force_n(2.0 * kSoil_m3_s, kSlip_m_s), 2.0 * force_n);
    EXPECT_DOUBLE_EQ(throw_force_n(kSoil_m3_s, -2.0 * kSlip_m_s), -2.0 * force_n);
    EXPECT_EQ(throw_force_n(0.0, kSlip_m_s), 0.0);
    EXPECT_EQ(throw_force_n(kSoil_m3_s, 0.0), 0.0);
}

// ---- Side grip

double strong_ground_grip_n(const Cornering& cornering, double load_n,
                            const soil::Surface& surface) {
    // C / stiffness_per_rad: g m W, the load the tire's side grip works with.
    return side_grip(reference_params().size, cornering, load_n, surface).limit_n / cornering.limit;
}

TEST(SideGrip, GrowsWithLoadAndFirmGround) {
    const Params params = reference_params();
    const soil::Surface firm = reference_surface();
    soil::Surface soft = firm;
    soft.cone_index_pa *= 0.2;
    const SideGrip light = side_grip(params.size, params.cornering, kLoad_n, firm);
    const SideGrip heavy = side_grip(params.size, params.cornering, 2.0 * kLoad_n, firm);
    const SideGrip on_soft = side_grip(params.size, params.cornering, kLoad_n, soft);
    EXPECT_GT(heavy.limit_n, light.limit_n);
    EXPECT_GT(heavy.stiffness_n_per_m, light.stiffness_n_per_m);
    EXPECT_LT(on_soft.limit_n, light.limit_n);
    EXPECT_LT(on_soft.stiffness_n_per_m, light.stiffness_n_per_m);
    // Never more than the limit coefficient times the load.
    EXPECT_LE(light.limit_n, params.cornering.limit * kLoad_n);
    EXPECT_EQ(side_grip(params.size, params.cornering, 0.0, firm).limit_n, 0.0);
}

// Rolling steadily at a small slip angle, the side force settles at C α.
TEST(SideGrip, RollingSteadilyGivesCorneringStiffnessTimesSlipAngle) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    const SideGrip grip = side_grip(params.size, params.cornering, kLoad_n, surface);
    constexpr double kSlipAngle_rad = 0.02;
    // Sliding to the right while rolling forward: the force pushes left.
    const double lateral_m_s = -kGround_m_s * std::tan(kSlipAngle_rad);
    double deflection_m = 0.0;
    // 40 m: fifty relaxation lengths.
    for (int i = 0; i < 20000; ++i) {
        deflection_m = relax_deflection_m(grip, params.cornering, deflection_m, lateral_m_s,
                                          kGround_m_s, params.min_reference_speed_m_s, 0.001);
    }
    const double cornering_stiffness_n_per_rad =
        params.cornering.stiffness_per_rad *
        strong_ground_grip_n(params.cornering, kLoad_n, surface);
    EXPECT_NEAR(side_force_n(grip, deflection_m),
                cornering_stiffness_n_per_rad * std::tan(kSlipAngle_rad), 1e-6);
}

TEST(SideGrip, BuildsOverTheRelaxationLength) {
    const Params params = reference_params();
    const SideGrip grip = side_grip(params.size, params.cornering, kLoad_n, reference_surface());
    const double lateral_m_s = -0.01 * kGround_m_s;
    double deflection_m = 0.0;
    // After rolling one relaxation length the force has built to 1 - 1/e of
    // its steady value.
    const int steps = static_cast<int>(params.cornering.relaxation_length_m / kGround_m_s / 0.001);
    for (int i = 0; i < steps; ++i) {
        deflection_m = relax_deflection_m(grip, params.cornering, deflection_m, lateral_m_s,
                                          kGround_m_s, params.min_reference_speed_m_s, 0.001);
    }
    const double steady_m = lateral_m_s * params.cornering.relaxation_length_m / kGround_m_s;
    EXPECT_NEAR(deflection_m / steady_m, 1.0 - std::exp(-1.0), 0.01);
}

TEST(SideGrip, SlidesAtTheLimit) {
    const Params params = reference_params();
    const SideGrip grip = side_grip(params.size, params.cornering, kLoad_n, reference_surface());
    double deflection_m = 0.0;
    for (int i = 0; i < 5000; ++i) {
        deflection_m = relax_deflection_m(grip, params.cornering, deflection_m, -kGround_m_s,
                                          kGround_m_s, params.min_reference_speed_m_s, 0.001);
        ASSERT_LE(std::abs(side_force_n(grip, deflection_m)), grip.limit_n * (1.0 + 1e-12));
    }
    EXPECT_NEAR(side_force_n(grip, deflection_m), grip.limit_n, 1e-9 * grip.limit_n);
}

// Pushed sideways at rest, the patch holds like a spring.
TEST(SideGrip, HoldsAtRest) {
    const Params params = reference_params();
    const SideGrip grip = side_grip(params.size, params.cornering, kLoad_n, reference_surface());
    const double deflection_m = relax_deflection_m(grip, params.cornering, 0.0, -0.01, 0.0,
                                                   params.min_reference_speed_m_s, 0.001);
    EXPECT_GT(side_force_n(grip, deflection_m), 0.0);
    EXPECT_NEAR(side_force_n(grip, deflection_m), grip.stiffness_n_per_m * 1e-5,
                0.01 * grip.stiffness_n_per_m * 1e-5);
}

TEST(SideGrip, TractionAndWheelspinUseItUp) {
    const Params params = reference_params();
    const soil::Surface surface = reference_surface();
    const SideGrip grip = side_grip(params.size, params.cornering, kLoad_n, surface);
    const double full_bite_n = full_bite(params, kLoad_n, surface) * kLoad_n;
    EXPECT_EQ(with_traction(grip, 0.0, full_bite_n, 0.0).limit_n, grip.limit_n);
    const double pulling = with_traction(grip, 0.5 * full_bite_n, full_bite_n, 0.1).limit_n;
    const double pulling_hard = with_traction(grip, 0.9 * full_bite_n, full_bite_n, 0.3).limit_n;
    EXPECT_LT(pulling, grip.limit_n);
    EXPECT_LT(pulling_hard, pulling);
    EXPECT_EQ(with_traction(grip, 0.8 * full_bite_n, full_bite_n, 1.0).limit_n, 0.0);
    // The patch's stiffness stays; it just slides sooner.
    EXPECT_EQ(with_traction(grip, 0.5 * full_bite_n, full_bite_n, 0.1).stiffness_n_per_m,
              grip.stiffness_n_per_m);
}

TEST(SideGrip, PatchSpringsBackOffTheGround) {
    const Params params = reference_params();
    EXPECT_EQ(relax_deflection_m({}, params.cornering, 0.02, 1.0, 2.0,
                                 params.min_reference_speed_m_s, 0.001),
              0.0);
    EXPECT_EQ(side_force_n({}, 0.02), 0.0);
}

TEST(SideGrip, RejectsNonsense) {
    Params params = reference_params();
    params.cornering.limit = 0.0;
    EXPECT_EQ(validate(params), ParamError::kCorneringLimitOutOfRange);
    params = reference_params();
    params.cornering.relaxation_length_m = 0.0;
    EXPECT_EQ(validate(params), ParamError::kCorneringNotPositive);
    params = reference_params();
    params.cornering.stiffness_per_rad = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(validate(params), ParamError::kNotFinite);
}

// The reference pressure leaves the tire as it is, bit for bit.
TEST(Pressure, TheReferenceIsTheTire) {
    const Params params = reference_params();
    const Params same = at_pressure(params, params.pressure.reference_bar);
    EXPECT_EQ(same.size.deflection_m, params.size.deflection_m);
    EXPECT_EQ(same.traction.limit, params.traction.limit);
    // A tire whose pressure is not set has none to change.
    Params unset = params;
    unset.pressure = {};
    EXPECT_EQ(at_pressure(unset, 0.5).size.deflection_m, params.size.deflection_m);
}

// Lower pressure: more deflection, a bigger mobility number (footprint) and
// a lower traction limit (squirming lugs). Higher pressure: less of both
// deflection and footprint, and lugs that bite no better.
TEST(Pressure, SoftensTheTire) {
    const Params params = reference_params();
    const Params soft = at_pressure(params, 0.5);
    const Params hard = at_pressure(params, 1.6);
    EXPECT_DOUBLE_EQ(soft.size.deflection_m, 2.0 * params.size.deflection_m);
    EXPECT_GT(soft.size.deflection_m, params.size.deflection_m);
    EXPECT_LT(hard.size.deflection_m, params.size.deflection_m);
    const soil::Surface surface = reference_surface();
    EXPECT_GT(mobility_number(soft.size, kLoad_n, surface),
              mobility_number(params.size, kLoad_n, surface));
    EXPECT_LT(mobility_number(hard.size, kLoad_n, surface),
              mobility_number(params.size, kLoad_n, surface));
    EXPECT_LT(soft.traction.limit, params.traction.limit);
    EXPECT_EQ(hard.traction.limit, params.traction.limit);
    // Held to the tire's range; not finite leaves it at the reference.
    EXPECT_EQ(at_pressure(params, 0.1).size.deflection_m, soft.size.deflection_m);
    EXPECT_EQ(at_pressure(params, 9.0).size.deflection_m, hard.size.deflection_m);
    EXPECT_EQ(at_pressure(params, std::numeric_limits<double>::quiet_NaN()).size.deflection_m,
              params.size.deflection_m);
    EXPECT_EQ(at_pressure(params, -std::numeric_limits<double>::infinity()).size.deflection_m,
              params.size.deflection_m);
}

// A rear tire's load in a pull: the tractor's weight shifted back and the
// chain pulling the hitch down [N].
constexpr double kPullingLoad_n = 25000.0;

// What a tire has left to pull with at full bite under that load: its grip
// less its motion resistance, both per unit of load.
double net_traction(const Params& params, const soil::Surface& surface) {
    const double mobility = mobility_number(params.size, kPullingLoad_n, surface);
    return full_bite(params, kPullingLoad_n, surface) -
           driven_rolling_resistance(
               Footing{.mobility_number = mobility, .root_mobility = std::sqrt(mobility)}, 0.0);
}

// The trade-off that makes the setting matter: on soft ground the bigger
// footprint sinks less at low pressure and pulls better for it; on firm
// ground, where it barely sinks anyway, the squirming lugs cost more.
TEST(Pressure, TheBestPressureDependsOnTheGround) {
    const Params params = reference_params();
    const Params soft = at_pressure(params, params.pressure.min_bar);
    const soil::Surface soft_ground{.cone_index_pa = 600'000.0};
    const soil::Surface firm_ground{.cone_index_pa = 6'000'000.0};
    EXPECT_GT(net_traction(soft, soft_ground), net_traction(params, soft_ground));
    EXPECT_LT(net_traction(soft, firm_ground), net_traction(params, firm_ground));
}

TEST(Pressure, RejectsNonsense) {
    const auto with = [](auto change) {
        Params params = reference_params();
        change(params.pressure);
        return validate(params);
    };
    EXPECT_EQ(with([](Pressure&) {}), std::nullopt);
    EXPECT_EQ(with([](Pressure& p) { p = {}; }), std::nullopt) << "pressure not set";
    EXPECT_EQ(with([](Pressure& p) { p.min_bar = 1.2; }), ParamError::kPressureOutOfRange);
    EXPECT_EQ(with([](Pressure& p) { p.max_bar = 0.9; }), ParamError::kPressureOutOfRange);
    EXPECT_EQ(with([](Pressure& p) { p.lug_squirm = 1.0; }), ParamError::kPressureOutOfRange);
    // So soft the tire would deflect past its section height.
    EXPECT_EQ(with([](Pressure& p) { p.min_bar = 0.1; }), ParamError::kPressureOutOfRange);
    EXPECT_EQ(with([](Pressure& p) { p.max_bar = std::numeric_limits<double>::infinity(); }),
              ParamError::kPressureOutOfRange);
}

}  // namespace
}  // namespace tp::tire

namespace tp::tire {
namespace {

// The reference tire's lugs sink fully into its own track; on ground twice
// as strong as they press they sink in half way, and the tire grips the
// unsunk share plus half the rest; ever harder ground costs more, down to
// the unsunk share.
TEST(LugGrip, LugsRideOnGroundTooHardForThem) {
    const TractionCurve curve = reference_params().traction;
    EXPECT_EQ(lug_grip(curve, reference_surface().cone_index_pa), 1.0);
    EXPECT_EQ(lug_grip(curve, curve.lug_bearing_pa), 1.0);
    EXPECT_DOUBLE_EQ(lug_grip(curve, 2.0 * curve.lug_bearing_pa),
                     curve.unsunk_lug_grip + 0.5 * (1.0 - curve.unsunk_lug_grip));
    double last = 1.0;
    constexpr int kSteps = 20;
    for (int i = 0; i < kSteps; ++i) {
        const double now = lug_grip(curve, curve.lug_bearing_pa * std::pow(1.5, i));
        EXPECT_LE(now, last);
        EXPECT_GT(now, curve.unsunk_lug_grip);
        last = now;
    }
    TractionCurve sharp = curve;
    sharp.lug_bearing_pa = 0.0;
    EXPECT_EQ(lug_grip(sharp, 1.0e8), 1.0) << "without a bearing the lugs always sink in";
}

// On ground too hard for the lugs, the tire's grip falls by its lug share,
// at every slip.
TEST(LugGrip, ScalesTheTraction) {
    const Params params = reference_params();
    soil::Surface hard = reference_surface();
    hard.cone_index_pa = 4.0 * params.traction.lug_bearing_pa;
    Params sharp = params;
    sharp.traction.lug_bearing_pa = 0.0;
    const double share = lug_grip(params.traction, hard.cone_index_pa);
    for (const double s : {0.05, 0.2, 0.6}) {
        EXPECT_NEAR(mu_at(params, s, hard), share * mu_at(sharp, s, hard), 1e-12) << s;
    }
}

// A tire spinning on the spot digs itself in, the more the faster it spins;
// one rolling over the ground hardly digs and climbs out of its hole; the
// hole is never deeper than the tire's section; a tire without digging
// stays on top.
TEST(Digging, ATireSpinningOnTheSpotDigsItselfIn) {
    const Params params = reference_params();
    constexpr double kDt_s = 0.001;
    constexpr int kSecond = 1000;
    const auto dug_m = [&](double slip_m_s, double ground_m_s, double from_m, int steps) {
        double depth_m = from_m;
        for (int i = 0; i < steps; ++i) {
            depth_m = dig_depth_m(params, depth_m, slip_m_s, ground_m_s, kDt_s);
        }
        return depth_m;
    };
    const double slow_m = dug_m(2.0, 0.0, 0.0, kSecond);
    EXPECT_NEAR(slow_m, params.digging.rate * 2.0, 1e-9) << "rate times the slip, standing";
    EXPECT_GT(dug_m(4.0, 0.0, 0.0, kSecond), 1.9 * slow_m);
    EXPECT_LT(dug_m(2.0, 3.0, 0.0, kSecond), 0.01 * slow_m) << "moving, it hardly digs";
    EXPECT_LT(dug_m(0.0, 1.0, slow_m, 3 * kSecond), 0.1 * slow_m) << "rolling, it climbs out";
    EXPECT_EQ(dug_m(30.0, 0.0, 0.0, 100 * kSecond), params.size.section_height_m);
    Params none = params;
    none.digging = {};
    EXPECT_EQ(dig_depth_m(none, 0.0, 10.0, 0.0, kDt_s), 0.0);
}

// The deeper the hole the more it holds the tractor back, never more than
// the steepest edge; a deflected tire climbs a shallow hole more gently
// than a rigid wheel would.
TEST(Digging, TheHoleHoldsTheTractorBack) {
    const Size size = reference_params().size;
    EXPECT_EQ(dig_resistance(size, 0.0), 0.0);
    double last = 0.0;
    constexpr double kStep_m = 0.005;
    for (int i = 1; i * kStep_m < size.section_height_m; ++i) {
        const double depth_m = i * kStep_m;
        const double now = dig_resistance(size, depth_m);
        EXPECT_GT(now, last) << depth_m;
        EXPECT_LE(now, kMaxDigSlope);
        EXPECT_LE(now, std::sqrt(2.0 * depth_m / size.radius_m)) << depth_m;
        last = now;
    }
    Size rigid = size;
    rigid.deflection_m = 0.0;
    EXPECT_DOUBLE_EQ(dig_resistance(rigid, 0.02), std::sqrt(2.0 * 0.02 / size.radius_m));
}

TEST(Digging, RejectsNonsense) {
    Params params = reference_params();
    params.digging.rate = -0.1;
    EXPECT_EQ(validate(params), ParamError::kDiggingOutOfRange);
    params = reference_params();
    params.digging.escape_speed_m_s = 0.0;
    EXPECT_EQ(validate(params), ParamError::kDiggingOutOfRange);
    params = reference_params();
    params.traction.unsunk_lug_grip = 0.0;
    EXPECT_EQ(validate(params), ParamError::kLugGripOutOfRange);
    params = reference_params();
    params.traction.lug_bearing_pa = -1.0;
    EXPECT_EQ(validate(params), ParamError::kLugGripOutOfRange);
    params = reference_params();
    params.digging.rate = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(validate(params), ParamError::kNotFinite);
}

}  // namespace
}  // namespace tp::tire
