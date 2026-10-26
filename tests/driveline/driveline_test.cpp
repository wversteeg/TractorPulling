#include "tp/core/driveline/driveline.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "reference_driveline.hpp"

namespace tp::driveline {
namespace {

using testing::lossless_params;
using testing::reference_params;

constexpr double kDt_s = 0.001;

void step(State& state, const Params& params, double clutch_torque_nm, double axle_load_nm) {
    const Prediction p = predict(state, params, axle_load_nm, kDt_s);
    advance(state, p, clutch_torque_nm);
}

double kinetic_energy_j(const State& state, const Params& params) {
    return 0.5 * params.input_inertia_kg_m2 * state.input_omega_rad_s * state.input_omega_rad_s +
           0.5 * params.output_inertia_kg_m2 * state.output_omega_rad_s * state.output_omega_rad_s;
}

TEST(DrivelineParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(DrivelineParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.gear_count = 0; }), ParamError::kNoGears);
    EXPECT_EQ(with([](Params& p) { p.gear_count = kMaxGears + 1; }), ParamError::kTooManyGears);
    EXPECT_EQ(with([](Params& p) { p.gear_ratios[2] = std::nan(""); }), ParamError::kNotFinite);
    EXPECT_EQ(with([](Params& p) { p.gear_ratios[7] = 0.0; }), ParamError::kRatioNotPositive);
    EXPECT_EQ(with([](Params& p) { p.gear_ratios[3] = p.gear_ratios[2]; }),
              ParamError::kRatiosNotDecreasing);
    EXPECT_EQ(with([](Params& p) { p.final_drive_ratio = -1.0; }),
              ParamError::kFinalDriveNotPositive);
    EXPECT_EQ(with([](Params& p) { p.efficiency = 0.0; }), ParamError::kEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.efficiency = 1.01; }), ParamError::kEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.output_inertia_kg_m2 = 0.0; }),
              ParamError::kInertiaNotPositive);
    EXPECT_EQ(with([](Params& p) { p.drag_torque_nm = -1.0; }), ParamError::kDragNegative);
    EXPECT_EQ(with([](Params& p) { p.load_downshift_gears = -1; }),
              ParamError::kLoadDownshiftOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.load_downshift_gears = 8; }),
              ParamError::kLoadDownshiftOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.load_downshift_gears = 7; }), std::nullopt);
    EXPECT_EQ(with([](Params& p) { p.load_shifts_per_pull = -1; }),
              ParamError::kLoadShiftsNegative);
}

TEST(Driveline, OverallRatioIsGearTimesFinalDrive) {
    const Params params = reference_params();
    EXPECT_EQ(overall_ratio(params, kNeutral), 0.0);
    EXPECT_DOUBLE_EQ(overall_ratio(params, 1), 15.0 * 22.0);
    EXPECT_DOUBLE_EQ(overall_ratio(params, 8), 22.0);
    EXPECT_EQ(overall_ratio(params, 9), 0.0);
    EXPECT_EQ(overall_ratio(params, -1), 0.0);
}

// In gear the train is rigid: input speed is always axle speed times G.
TEST(Driveline, InGearInputTurnsAtRatioTimesAxle) {
    const Params params = reference_params();
    State state;
    ASSERT_TRUE(shift(state, params, 4, true));
    for (int i = 0; i < 2000; ++i) {
        step(state, params, i < 1000 ? 80.0 : -40.0, 500.0);
        ASSERT_NEAR(state.input_omega_rad_s, state.output_omega_rad_s * overall_ratio(params, 4),
                    1e-9 * (1.0 + state.input_omega_rad_s));
    }
}

// From rest, a constant clutch torque accelerates the train as one body with
// the output inertia reflected through G².
TEST(Driveline, AcceleratesWithReflectedInertia) {
    const Params params = lossless_params();
    for (const int gear : {1, 4, 8}) {
        State state;
        ASSERT_TRUE(shift(state, params, gear, true));
        const double ratio = overall_ratio(params, gear);
        const double inertia_kg_m2 =
            params.input_inertia_kg_m2 + params.output_inertia_kg_m2 / (ratio * ratio);
        constexpr double kTorque_nm = 10.0;
        for (int i = 0; i < 1000; ++i) {
            step(state, params, kTorque_nm, 0.0);
        }
        EXPECT_NEAR(state.input_omega_rad_s, kTorque_nm * 1.0 / inertia_kg_m2,
                    1e-9 * state.input_omega_rad_s)
            << "gear " << gear;
    }
}

// Without losses, the work done by the clutch and against the load accounts
// for the change in kinetic energy.
TEST(Driveline, LosslessTrainConservesEnergy) {
    const Params params = lossless_params();
    State state;
    ASSERT_TRUE(shift(state, params, 3, true));
    constexpr double kClutch_nm = 60.0;
    constexpr double kLoad_nm = 2000.0;
    double work_j = 0.0;
    for (int i = 0; i < 3000; ++i) {
        const State before = state;
        step(state, params, kClutch_nm, kLoad_nm);
        // Trapezoidal work over the step matches the semi-implicit update.
        work_j += kClutch_nm * 0.5 * (before.input_omega_rad_s + state.input_omega_rad_s) * kDt_s;
        work_j -= kLoad_nm * 0.5 * (before.output_omega_rad_s + state.output_omega_rad_s) * kDt_s;
    }
    EXPECT_NEAR(kinetic_energy_j(state, params), work_j, 1e-9 * work_j);
}

// Efficiency applies in the direction power flows. Driving: the axle carries
// a load of efficiency × G × clutch torque at constant speed. Overrun: the
// clutch absorbs only efficiency × the torque the axle feeds back.
TEST(Driveline, LosesPowerInTheDirectionItFlows) {
    Params params = lossless_params();
    params.efficiency = 0.9;
    const double ratio = overall_ratio(params, 5);
    for (const double clutch_nm : {150.0, -150.0}) {
        State state;
        ASSERT_TRUE(shift(state, params, 5, true));
        state.output_omega_rad_s = 10.0;
        state.input_omega_rad_s = 10.0 * ratio;
        const double balancing_load_nm =
            clutch_nm > 0.0 ? clutch_nm * ratio * 0.9 : clutch_nm * ratio / 0.9;
        for (int i = 0; i < 1000; ++i) {
            step(state, params, clutch_nm, balancing_load_nm);
        }
        EXPECT_NEAR(state.output_omega_rad_s, 10.0, 1e-9) << "clutch " << clutch_nm;
    }
}

// The prediction the clutch solves against matches advance() for torque in
// either direction, including the efficiency.
TEST(Driveline, InputShaftPredictionMatchesAdvance) {
    const Params params = reference_params();
    State start;
    ASSERT_TRUE(shift(start, params, 4, true));
    start.output_omega_rad_s = 5.0;
    start.input_omega_rad_s = 5.0 * overall_ratio(params, 4);
    const Prediction p = predict(start, params, 800.0, kDt_s);
    const coupling::ShaftPrediction shaft = input_shaft(p);
    for (const double torque_nm : {-200.0, -1.0, 0.0, 1.0, 200.0}) {
        State state = start;
        advance(state, p, torque_nm);
        const double k =
            torque_nm >= 0.0 ? shaft.omega_per_torque : shaft.omega_per_negative_torque;
        EXPECT_NEAR(state.input_omega_rad_s, shaft.free_omega_next_rad_s + k * torque_nm, 1e-12)
            << "torque " << torque_nm;
    }
    EXPECT_LT(shaft.omega_per_torque, shaft.omega_per_negative_torque);
}

// The two-shaft response a chain solver uses (clutch torque at the input,
// tire torque at the axle) matches advance() in gear and in neutral.
TEST(Driveline, ResponseMatchesAdvanceForClutchAndAxleTorque) {
    const Params params = reference_params();
    for (const int gear : {kNeutral, 3}) {
        State start{.gear = kNeutral, .input_omega_rad_s = 150.0, .output_omega_rad_s = 2.0};
        ASSERT_TRUE(shift(start, params, gear, true));
        const Prediction p = predict(start, params, 300.0, kDt_s);
        const Response r = response(p);
        for (const double clutch_nm : {-120.0, 0.0, 250.0}) {
            // Within the range where neither shaft is driven backwards (no reverse).
            for (const double axle_nm : {-400.0, 0.0, 1500.0}) {
                State state = start;
                advance(state, p, clutch_nm, axle_nm);
                const bool negative = clutch_nm < 0.0;
                const double input_rad_s =
                    r.free_input_rad_s +
                    (negative ? r.input_per_negative_clutch : r.input_per_clutch) * clutch_nm +
                    r.input_per_axle * axle_nm;
                const double axle_rad_s =
                    r.free_axle_rad_s +
                    (negative ? r.axle_per_negative_clutch : r.axle_per_clutch) * clutch_nm +
                    r.axle_per_axle * axle_nm;
                EXPECT_NEAR(state.input_omega_rad_s, input_rad_s, 1e-9)
                    << "gear " << gear << ", clutch " << clutch_nm << ", axle " << axle_nm;
                EXPECT_NEAR(state.output_omega_rad_s, axle_rad_s, 1e-9)
                    << "gear " << gear << ", clutch " << clutch_nm << ", axle " << axle_nm;
            }
        }
    }
}

TEST(Driveline, NeutralDecouplesInputFromAxle) {
    const Params params = lossless_params();
    State state{.gear = kNeutral, .input_omega_rad_s = 0.0, .output_omega_rad_s = 3.0};
    for (int i = 0; i < 1000; ++i) {
        step(state, params, 5.0, 0.0);
    }
    EXPECT_NEAR(state.input_omega_rad_s, 5.0 / params.input_inertia_kg_m2, 1e-9);
    EXPECT_EQ(state.output_omega_rad_s, 3.0);

    // The axle slows under its own load only.
    step(state, params, 0.0, 100.0);
    EXPECT_NEAR(state.output_omega_rad_s, 3.0 - kDt_s * 100.0 / params.output_inertia_kg_m2, 1e-12);
}

TEST(Driveline, DragSpinsTheFreeInputShaftDownToRest) {
    const Params params = reference_params();
    State state{.gear = kNeutral, .input_omega_rad_s = 200.0, .output_omega_rad_s = 0.0};
    double previous_rad_s = state.input_omega_rad_s;
    for (int i = 0; i < 20000; ++i) {
        step(state, params, 0.0, 0.0);
        ASSERT_LE(state.input_omega_rad_s, previous_rad_s);
        ASSERT_GE(state.input_omega_rad_s, 0.0);
        previous_rad_s = state.input_omega_rad_s;
    }
    EXPECT_EQ(state.input_omega_rad_s, 0.0);
}

TEST(Driveline, ResistanceHoldsATrainAtRestWithoutReversingIt) {
    const Params params = reference_params();
    State state;
    ASSERT_TRUE(shift(state, params, 2, true));
    for (int i = 0; i < 1000; ++i) {
        step(state, params, 10.0, 50000.0);
        ASSERT_EQ(state.output_omega_rad_s, 0.0);
    }
}

TEST(Driveline, ShiftsOnlyWithTheClutchDisengaged) {
    Params params = reference_params();
    params.load_downshift_gears = 0;
    State state;
    EXPECT_FALSE(shift(state, params, 3, false));
    EXPECT_EQ(state.gear, kNeutral);
    EXPECT_TRUE(shift(state, params, 3, true));
    EXPECT_EQ(state.gear, 3);
    EXPECT_TRUE(shift(state, params, 3, false)) << "staying in gear is always allowed";
    EXPECT_FALSE(shift(state, params, kNeutral, false));
    EXPECT_FALSE(shift(state, params, 9, true));
    EXPECT_FALSE(shift(state, params, -1, true));
    EXPECT_EQ(state.gear, 3);
}

bool shifts_under_load_from(const Params& params, int from, int to) {
    return shifts_under_load(State{.gear = from}, params, to);
}

// The reference gearbox shifts one gear down under load, the clutch in;
// never up, never two down, never to neutral. The axle keeps its speed; the
// input shaft takes the new gear's, and the momentum that takes is what the
// engine gives.
TEST(Driveline, ShiftsOneGearDownUnderLoad) {
    const Params params = reference_params();
    EXPECT_TRUE(shifts_under_load_from(params, 6, 5));
    EXPECT_FALSE(shifts_under_load_from(params, 6, 4)) << "two down";
    EXPECT_FALSE(shifts_under_load_from(params, 5, 6)) << "up";
    EXPECT_FALSE(shifts_under_load_from(params, 1, kNeutral)) << "to neutral";
    EXPECT_FALSE(shifts_under_load_from(params, 9, 8)) << "from a gear it does not have";
    const double axle_rad_s = 200.0 / 55.0;
    State state{.gear = 6, .input_omega_rad_s = 200.0, .output_omega_rad_s = axle_rad_s};
    EXPECT_FALSE(shift_under_load(state, params, 4));
    EXPECT_FALSE(shift_under_load(state, params, 7));
    EXPECT_FALSE(shift(state, params, 5, false)) << "shift() wants the clutch out";
    EXPECT_EQ(state.gear, 6);
    const std::optional<double> impulse_nm_s = shift_under_load(state, params, 5);
    ASSERT_TRUE(impulse_nm_s.has_value());
    EXPECT_EQ(state.gear, 5);
    EXPECT_EQ(state.output_omega_rad_s, axle_rad_s) << "the axle keeps its speed";
    EXPECT_DOUBLE_EQ(state.input_omega_rad_s, axle_rad_s * overall_ratio(params, 5));
    EXPECT_DOUBLE_EQ(impulse_nm_s.value_or(0.0),
                     params.input_inertia_kg_m2 * (state.input_omega_rad_s - 200.0));
    EXPECT_GT(impulse_nm_s.value_or(0.0), 0.0);
    EXPECT_EQ(state.load_shifts, 1);

    Params two = params;
    two.load_downshift_gears = 2;
    EXPECT_TRUE(shifts_under_load_from(two, 6, 4));
    EXPECT_FALSE(shifts_under_load_from(two, 6, 3));
}

// One shift under load a pull: the next one down is refused, and needs the
// clutch pressed. A gearbox allowing two takes a second.
TEST(Driveline, ShiftsUnderLoadOnceAPull) {
    const Params params = reference_params();
    State state{.gear = 6, .input_omega_rad_s = 200.0, .output_omega_rad_s = 200.0 / 55.0};
    ASSERT_TRUE(shift_under_load(state, params, 5).has_value());
    EXPECT_FALSE(shifts_under_load(state, params, 4));
    EXPECT_FALSE(shift_under_load(state, params, 4).has_value());
    EXPECT_FALSE(shift(state, params, 4, false));
    EXPECT_EQ(state.gear, 5);
    EXPECT_EQ(state.load_shifts, 1);
    EXPECT_TRUE(shift(state, params, 4, true)) << "with the clutch pressed";

    Params twice = params;
    twice.load_shifts_per_pull = 2;
    State again{.gear = 6, .input_omega_rad_s = 200.0, .output_omega_rad_s = 200.0 / 55.0};
    EXPECT_TRUE(shift_under_load(again, twice, 5).has_value());
    EXPECT_TRUE(shift_under_load(again, twice, 4).has_value());
    EXPECT_FALSE(shift_under_load(again, twice, 3).has_value());
    EXPECT_EQ(again.gear, 4);

    Params never = params;
    never.load_shifts_per_pull = 0;
    EXPECT_FALSE(shifts_under_load_from(never, 6, 5));
}

// Engaging a gear synchronizes the two sides: momentum through the gears is
// conserved, energy can only be lost, and the common speed lies between the
// two. With the vehicle on the axle (its mass reflected as m r²), the
// input shaft reflected through G² is still ~7 % of the axle inertia in 6th,
// so the vehicle visibly slows when the input shaft was at rest.
TEST(Driveline, EngagingAGearSynchronizesWithoutCreatingEnergy) {
    Params params = reference_params();
    params.output_inertia_kg_m2 += 3500.0 * 0.8 * 0.8;
    for (const double input_rad_s : {0.0, 50.0, 300.0}) {
        State state{.gear = kNeutral, .input_omega_rad_s = input_rad_s, .output_omega_rad_s = 2.0};
        const double ratio = overall_ratio(params, 6);
        const double momentum = params.output_inertia_kg_m2 * state.output_omega_rad_s +
                                params.input_inertia_kg_m2 * ratio * state.input_omega_rad_s;
        const double energy_before_j = kinetic_energy_j(state, params);
        const double input_as_axle_rad_s = state.input_omega_rad_s / ratio;
        const double low_rad_s = std::min(input_as_axle_rad_s, state.output_omega_rad_s);
        const double high_rad_s = std::max(input_as_axle_rad_s, state.output_omega_rad_s);

        ASSERT_TRUE(shift(state, params, 6, true));
        EXPECT_DOUBLE_EQ(state.input_omega_rad_s, state.output_omega_rad_s * ratio);
        EXPECT_NEAR(params.output_inertia_kg_m2 * state.output_omega_rad_s +
                        params.input_inertia_kg_m2 * ratio * state.input_omega_rad_s,
                    momentum, 1e-9 * momentum);
        EXPECT_LE(kinetic_energy_j(state, params), energy_before_j * (1.0 + 1e-12));
        EXPECT_GE(state.output_omega_rad_s, low_rad_s) << "input " << input_rad_s;
        EXPECT_LE(state.output_omega_rad_s, high_rad_s) << "input " << input_rad_s;
    }
}

}  // namespace
}  // namespace tp::driveline
