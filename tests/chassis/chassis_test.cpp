#include "tp/core/chassis/chassis.hpp"

#include "tp/core/units/constants.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "reference_chassis.hpp"

namespace tp::chassis {
namespace {

using testing::reference_params;
using units::kStandardGravity_m_s2;

constexpr double kDt_s = 0.001;

double weight_n(const Params& params) {
    return params.mass_kg * kStandardGravity_m_s2;
}

// Nose-up moment of the static weight about the rear axle [N·m]: what the
// drive torque must exceed to lift the front.
double tipping_moment_nm(const Params& params) {
    return weight_n(params) * params.cg_ahead_of_rear_axle_m;
}

double cg_above_axle_m(const Params& params) {
    return params.cg_height_m - params.rear_axle_height_m;
}

double pitch_inertia_about_axle(const Params& params) {
    const double b_m = params.cg_ahead_of_rear_axle_m;
    const double h_m = cg_above_axle_m(params);
    return params.pitch_inertia_kg_m2 + params.mass_kg * (b_m * b_m + h_m * h_m);
}

// Pitch at which the wheelie bar tip reaches the ground, by bisection.
double bar_contact_pitch_rad(const Params& params) {
    const double tip_z_m = params.wheelie_bar.tip_height_m - params.rear_axle_height_m;
    const auto depth_m = [&](double pitch_rad) {
        return -params.rear_axle_height_m +
               params.wheelie_bar.behind_rear_axle_m * std::sin(pitch_rad) -
               tip_z_m * std::cos(pitch_rad);
    };
    double low = 0.0;
    double high = kMaxWheelieBarContactPitch_rad;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (low + high);
        (depth_m(mid) > 0.0 ? high : low) = mid;
    }
    return low;
}

// The chassis pitching under constant loads, the longitudinal motion held.
struct PitchRig {
    Params params = reference_params();
    State state;
    AxleLoads loads;
    double max_pitch_rad = -1.0;
    double min_pitch_rad = 1.0;
    bool front_lifted = false;

    void run(double axle_torque_nm, double drawbar_n, double acceleration_m_s2, double seconds) {
        run(axle_torque_nm, {.pull_n = drawbar_n}, acceleration_m_s2, seconds);
    }

    void run(double axle_torque_nm, const Drawbar& drawbar, double acceleration_m_s2,
             double seconds) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            state.acceleration_m_s2 = acceleration_m_s2;
            loads = axle_loads(params, state, drawbar);
            front_lifted = front_lifted || loads.front_lifted;
            advance_pitch(state, params, axle_torque_nm, drawbar, kDt_s);
            max_pitch_rad = std::max(max_pitch_rad, state.pitch_rad);
            min_pitch_rad = std::min(min_pitch_rad, state.pitch_rad);
        }
        loads = axle_loads(params, state, drawbar);
    }
};

TEST(ChassisParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(ChassisParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.mass_kg = 0.0; }), ParamError::kMassNotPositive);
    EXPECT_EQ(with([](Params& p) { p.mass_kg = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) {
                  p.wheelie_bar.contact.damping_n_s_per_m = std::numeric_limits<double>::infinity();
              }),
              ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.wheelbase_m = -1.0; }), ParamError::kWheelbaseNotPositive);
    EXPECT_EQ(with([](Params& p) { p.cg_ahead_of_rear_axle_m = p.wheelbase_m; }),
              ParamError::kCgOutsideWheelbase);
    EXPECT_EQ(with([](Params& p) { p.cg_ahead_of_rear_axle_m = 0.0; }),
              ParamError::kCgOutsideWheelbase);
    EXPECT_EQ(with([](Params& p) { p.cg_height_m = 0.0; }), ParamError::kCgHeightNotPositive);
    EXPECT_EQ(with([](Params& p) { p.pitch_inertia_kg_m2 = 0.0; }),
              ParamError::kPitchInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.rear_axle_height_m = 0.0; }),
              ParamError::kRearAxleHeightNotPositive);
    EXPECT_EQ(with([](Params& p) { p.hitch_height_m = -0.1; }), ParamError::kHitchHeightNegative);
    EXPECT_EQ(with([](Params& p) { p.hitch_behind_rear_axle_m = -0.1; }),
              ParamError::kHitchAheadOfRearAxle);
    EXPECT_EQ(with([](Params& p) { p.front_rolling_resistance_coefficient = 1.0; }),
              ParamError::kRollingResistanceOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.front_tires.stiffness_n_per_m = 0.0; }),
              ParamError::kFrontTireStiffnessNotPositive);
    EXPECT_EQ(with([](Params& p) { p.front_tires.damping_n_s_per_m = -1.0; }),
              ParamError::kFrontTireDampingNegative);
    EXPECT_EQ(with([](Params& p) { p.wheelie_bar.behind_rear_axle_m = 0.0; }),
              ParamError::kWheelieBarNotBehindRearAxle);
    EXPECT_EQ(with([](Params& p) { p.wheelie_bar.tip_height_m = 0.0; }),
              ParamError::kWheelieBarTipNotAboveGround);
    // A short bar with its tip high only touches down past 30° nose-up.
    EXPECT_EQ(with([](Params& p) {
                  p.wheelie_bar.behind_rear_axle_m = 0.5;
                  p.wheelie_bar.tip_height_m = 0.6;
              }),
              ParamError::kWheelieBarTooHigh);
    EXPECT_EQ(with([](Params& p) { p.wheelie_bar.contact.stiffness_n_per_m = 0.0; }),
              ParamError::kWheelieBarStiffnessNotPositive);
    EXPECT_EQ(with([](Params& p) { p.wheelie_bar.contact.damping_n_s_per_m = -1.0; }),
              ParamError::kWheelieBarDampingNegative);
    EXPECT_EQ(with([](Params& p) { p.wheelie_bar.rolling_resistance_coefficient = 1.0; }),
              ParamError::kWheelieBarRollingResistanceOutOfRange);
}

TEST(Chassis, StaticLoadsFollowTheCentreOfGravity) {
    const Params params = reference_params();
    const AxleLoads loads = axle_loads(params, State{}, {});
    EXPECT_NEAR(loads.rear_n, 0.6 * weight_n(params), 1e-9);
    EXPECT_NEAR(loads.front_n, 0.4 * weight_n(params), 1e-9);
    EXPECT_EQ(loads.wheelie_bar_n, 0.0);
    EXPECT_FALSE(loads.front_lifted);
}

// Level and at rest is the balance: the tractor does not drift.
TEST(Chassis, RestsLevel) {
    PitchRig rig;
    rig.run(0.0, 0.0, 0.0, 10.0);
    EXPECT_NEAR(rig.state.pitch_rad, 0.0, 1e-12);
    EXPECT_NEAR(rig.state.pitch_rate_rad_s, 0.0, 1e-12);
}

// Without pitch motion the ground carries exactly the weight, however it is
// shared, from nose-down to the bar's touch-down.
TEST(Chassis, StillLoadsAddUpToTheWeight) {
    const Params params = reference_params();
    const int last = static_cast<int>(bar_contact_pitch_rad(params) / 0.001);
    for (int i = -20; i <= last; ++i) {
        const State state{.pitch_rad = i * 0.001};
        const AxleLoads loads = axle_loads(params, state, {});
        EXPECT_NEAR(loads.rear_n + loads.front_n + loads.wheelie_bar_n, weight_n(params), 1e-6);
        EXPECT_GE(loads.rear_n, 0.0);
    }
}

// Pulling on the hitch while the rear tires carry the pull at the ground
// moves load from the front to the rear axle by F h_hitch / L, as for a
// rigid tractor; accelerating moves m a h_cg / L. The settled pitch is a few
// mrad, so the rigid result holds to well under 1 %.
TEST(Chassis, SettlesToTheRigidWeightTransfer) {
    const Params params = reference_params();
    const double static_front_n = 0.4 * weight_n(params);

    constexpr double kPull_n = 10000.0;
    PitchRig pulling;
    pulling.run(kPull_n * params.rear_axle_height_m, kPull_n, 0.0, 5.0);
    const double pull_transfer_n = kPull_n * params.hitch_height_m / params.wheelbase_m;
    EXPECT_NEAR(static_front_n - pulling.loads.front_n, pull_transfer_n, 0.01 * pull_transfer_n);
    EXPECT_NEAR(pulling.loads.rear_n + pulling.loads.front_n, weight_n(params), 1e-3);

    constexpr double kAcceleration_m_s2 = 1.0;
    const double force_n = params.mass_kg * kAcceleration_m_s2;
    PitchRig accelerating;
    accelerating.run(force_n * params.rear_axle_height_m, 0.0, kAcceleration_m_s2, 5.0);
    const double accel_transfer_n = force_n * params.cg_height_m / params.wheelbase_m;
    EXPECT_NEAR(static_front_n - accelerating.loads.front_n, accel_transfer_n,
                0.01 * accel_transfer_n);
}

// Raising the axle torque slowly, the front lifts once its reaction
// outweighs the tractor about the rear axle, and not before.
TEST(Chassis, AxleTorqueBeyondTheWeightLiftsTheFront) {
    const Params params = reference_params();
    const auto ramp_to = [&](double fraction) {
        PitchRig rig;
        for (int i = 1; i <= 2000; ++i) {
            rig.run(fraction * tipping_moment_nm(params) * i / 2000.0, 0.0, 0.0, kDt_s);
        }
        rig.run(fraction * tipping_moment_nm(params), 0.0, 0.0, 1.0);
        return rig;
    };
    // Not 0.98: the nose rises some 0.8° before the front lets go, which
    // shortens the weight's lever by 0.3 %.
    const PitchRig below = ramp_to(0.95);
    EXPECT_FALSE(below.front_lifted);
    EXPECT_GT(below.loads.front_n, 0.0);

    const PitchRig above = ramp_to(1.02);
    EXPECT_TRUE(above.loads.front_lifted);
    EXPECT_EQ(above.loads.front_n, 0.0);
}

// With the centre of gravity above the axle, the weight's lever shrinks as
// the nose rises: resting on the bar, a wheelie holds on less torque than it
// took to lift the front (about 97 % here).
TEST(Chassis, AWheelieHoldsOnLessTorqueThanItTakesToLift) {
    const Params params = reference_params();
    const auto from_the_bar = [&](double fraction) {
        PitchRig rig;
        rig.state.pitch_rad = bar_contact_pitch_rad(params);
        rig.run(fraction * tipping_moment_nm(params), 0.0, 0.0, 3.0);
        return rig;
    };
    const PitchRig held = from_the_bar(0.99);
    EXPECT_GT(held.loads.wheelie_bar_n, 0.0);
    EXPECT_GT(held.min_pitch_rad, 0.9 * bar_contact_pitch_rad(params));

    const PitchRig dropped = from_the_bar(0.8);
    EXPECT_FALSE(dropped.loads.front_lifted);
    EXPECT_EQ(dropped.loads.wheelie_bar_n, 0.0);
}

// In a wheelie the bar catches the tractor: pitch stops just past the bar's
// touch-down angle, and the bar and rear tires carry everything. Hitting the
// bar with 1.5 times the tipping torque brings about 2 kJ; the 2 MN/m bar
// takes it in some 4.5 cm of deflection, about 2° of overshoot.
TEST(Chassis, WheelieBarCatchesTheTractor) {
    const Params params = reference_params();
    const double contact_rad = bar_contact_pitch_rad(params);
    PitchRig rig;
    rig.run(1.5 * tipping_moment_nm(params), 0.0, 0.0, 5.0);
    EXPECT_GT(rig.max_pitch_rad, contact_rad);
    EXPECT_LT(rig.max_pitch_rad, contact_rad + 0.04);
    EXPECT_NEAR(rig.state.pitch_rad, contact_rad, 0.01);
    EXPECT_EQ(rig.loads.front_n, 0.0);
    EXPECT_TRUE(rig.loads.front_lifted);
    EXPECT_GT(rig.loads.wheelie_bar_n, 0.0);
    EXPECT_NEAR(rig.loads.rear_n + rig.loads.wheelie_bar_n, weight_n(params), 1.0);
}

// A strong pull at a low hitch pitches the nose down a little and loads the
// rear: the hitch below the axle pulls the body round nose-down.
TEST(Chassis, LowHitchHoldsTheNoseDown) {
    const Params params = reference_params();
    PitchRig rig;
    rig.run(0.0, 20000.0, 0.0, 3.0);
    EXPECT_LT(rig.state.pitch_rad, 0.0);
    EXPECT_GT(rig.loads.front_n, 0.4 * weight_n(params));
}

// Level, the hitch sits where the data puts it; nose-up, a hitch behind the
// axle swings down and forward.
TEST(Chassis, HitchFollowsThePitch) {
    const Params params = reference_params();
    const Hitch level = hitch(params, State{.position_m = 10.0});
    EXPECT_DOUBLE_EQ(level.x_m, 10.0 - params.hitch_behind_rear_axle_m);
    EXPECT_NEAR(level.z_m, params.hitch_height_m, 1e-15);

    const Hitch raised = hitch(params, State{.position_m = 10.0, .pitch_rad = 0.12});
    EXPECT_LT(raised.z_m, level.z_m - 0.04) << "about 5 cm lower on the wheelie bar";
    EXPECT_GT(raised.x_m, level.x_m);
}

// The hitch's velocity is the time derivative of its position.
TEST(Chassis, HitchVelocityMatchesItsMotion) {
    const Params params = reference_params();
    const State state{
        .position_m = 3.0, .velocity_m_s = 2.0, .pitch_rad = 0.05, .pitch_rate_rad_s = 0.4};
    constexpr double kStep_s = 1e-6;
    State later = state;
    later.position_m += state.velocity_m_s * kStep_s;
    later.pitch_rad += state.pitch_rate_rad_s * kStep_s;
    const Hitch now = hitch(params, state);
    const Hitch next = hitch(params, later);
    EXPECT_NEAR(now.velocity_x_m_s, (next.x_m - now.x_m) / kStep_s, 1e-6);
    EXPECT_NEAR(now.velocity_z_m_s, (next.z_m - now.z_m) / kStep_s, 1e-6);
}

// A drawbar pulling the hitch down loads the rear tires by more than its
// own pull: the hitch is behind the axle, so it also levers weight off the
// front, by F d / L.
TEST(Chassis, DownwardPullAtTheHitchLoadsTheRearTires) {
    const Params params = reference_params();
    constexpr double kDown_n = 5000.0;
    PitchRig level;
    level.run(0.0, 0.0, 0.0, 3.0);
    PitchRig pulled_down;
    pulled_down.run(0.0, Drawbar{.lift_n = -kDown_n}, 0.0, 5.0);
    const double lever_n = kDown_n * params.hitch_behind_rear_axle_m / params.wheelbase_m;
    EXPECT_NEAR(level.loads.front_n - pulled_down.loads.front_n, lever_n, 0.01 * lever_n);
    EXPECT_NEAR(pulled_down.loads.rear_n - level.loads.rear_n, kDown_n + lever_n, 0.01 * kDown_n);
}

double pitch_energy_j(const Params& params, const State& state) {
    const double b_m = params.cg_ahead_of_rear_axle_m;
    const double h_m = cg_above_axle_m(params);
    const double s = std::sin(state.pitch_rad);
    const double c = std::cos(state.pitch_rad);
    const double kinetic_j =
        0.5 * pitch_inertia_about_axle(params) * state.pitch_rate_rad_s * state.pitch_rate_rad_s;
    const double potential_j = weight_n(params) * (b_m * s + h_m * c);
    const double preload_m = 0.4 * weight_n(params) / params.front_tires.stiffness_n_per_m;
    const double front_m = std::max(preload_m - params.wheelbase_m * s, 0.0);
    return kinetic_j + potential_j + 0.5 * params.front_tires.stiffness_n_per_m * front_m * front_m;
}

// Without damping the front tires bounce forever: the integrator neither
// creates nor (noticeably) destroys energy.
TEST(Chassis, UndampedPitchKeepsItsEnergy) {
    PitchRig rig;
    rig.params.front_tires.damping_n_s_per_m = 0.0;
    rig.state.pitch_rad = 0.01;
    const double start_j = pitch_energy_j(rig.params, rig.state);
    const double swing_j = start_j - pitch_energy_j(rig.params, State{});
    for (int i = 0; i < 5; ++i) {
        rig.run(0.0, 0.0, 0.0, 1.0);
        EXPECT_NEAR(pitch_energy_j(rig.params, rig.state), start_j, 0.01 * swing_j);
    }
    EXPECT_NEAR(rig.min_pitch_rad, -0.01, 0.002) << "still swinging through";
}

// With damping the bounce dies away and never gains energy.
TEST(Chassis, DampedPitchSettles) {
    PitchRig rig;
    rig.state.pitch_rad = 0.01;
    double energy_j = pitch_energy_j(rig.params, rig.state);
    for (int i = 0; i < 4000; ++i) {
        rig.run(0.0, 0.0, 0.0, kDt_s);
        const double next_j = pitch_energy_j(rig.params, rig.state);
        ASSERT_LE(next_j, energy_j + 1e-6);
        energy_j = next_j;
    }
    // About 15 % of critical damping: the swing decays as e^(-2.8 t).
    EXPECT_NEAR(rig.state.pitch_rad, 0.0, 1e-6);
}

TEST(Chassis, ConstantForceAcceleratesTheMass) {
    const Params params = reference_params();
    State state;
    constexpr double kForce_n = 7000.0;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 0.0, kDt_s);
        advance(state, p, kForce_n, kDt_s);
    }
    const double a_m_s2 = kForce_n / params.mass_kg;
    EXPECT_NEAR(state.velocity_m_s, a_m_s2 * 1.0, 1e-9);
    EXPECT_NEAR(state.acceleration_m_s2, a_m_s2, 1e-9);
    // Semi-implicit Euler is half a step ahead of the exact a t² / 2.
    EXPECT_NEAR(state.position_m, 0.5 * a_m_s2 * (1.0 + kDt_s), 1e-9);
}

TEST(Chassis, ResistanceHoldsAtRestWithoutPushingBack) {
    const Params params = reference_params();
    State state;
    for (int i = 0; i < 1000; ++i) {
        const Prediction p = predict(state, params, 20000.0, kDt_s);
        advance(state, p, 5000.0, kDt_s);
        ASSERT_EQ(state.velocity_m_s, 0.0);
        ASSERT_EQ(state.position_m, 0.0);
    }
}

}  // namespace
}  // namespace tp::chassis
