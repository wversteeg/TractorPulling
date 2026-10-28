#include "tp/core/vehicle/vehicle.hpp"

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/units/constants.hpp"
#include "tp/core/vehicle/tuning.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "reference_vehicle.hpp"

namespace tp::vehicle {
namespace {

using testing::reference_ground;
using testing::reference_model;
using testing::reference_params;

constexpr double kDt_s = 0.001;
const Ground kGround = reference_ground();

// Coulomb friction: a brake holds its side at rest with up to its capacity,
// or slips at exactly its capacity against the side's rotation. It never
// drives the side, and a side still moving with less than full torque
// would be a hold that did not converge.
bool brakes_right(double brake_torque_nm, double end_rad_s, double capacity_nm) {
    if (std::abs(brake_torque_nm) > capacity_nm) {
        return false;
    }
    if (std::abs(end_rad_s) <= kBrakeHoldTolerance_rad_s) {
        return true;
    }
    return std::abs(brake_torque_nm) == capacity_nm && brake_torque_nm * end_rad_s >= 0.0;
}

// A driver: idle in neutral, select a gear, rev up with the clutch pressed,
// then slip the clutch in over 3 s or dump it, and pull at full throttle.
struct Pull {
    Model model = reference_model();
    State state = initial_state(model);
    Ground ground = kGround;
    Output last;
    double max_slip = 0.0;
    double min_front_n = 1e12;
    double max_pitch_rad = 0.0;
    double max_bar_n = 0.0;
    double steering_rad = 0.0;
    double brake_left = 0.0;
    double brake_right = 0.0;
    bool steered_in_the_air = false;
    // A brake that broke Coulomb friction (see brakes_right).
    bool brake_misbehaved = false;
    // A brake held its side with torque against backward rotation.
    bool held_against_backward = false;

    [[nodiscard]] double left_rad_s() const {
        return state.driveline.output_omega_rad_s - state.differential_rad_s;
    }
    [[nodiscard]] double right_rad_s() const {
        return state.driveline.output_omega_rad_s + state.differential_rad_s;
    }

    void run(int gear, double drawbar_n, bool dump, double seconds) {
        for (int i = 0; i < static_cast<int>(seconds / kDt_s); ++i) {
            const double t_s = i * kDt_s;
            Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                        .clutch_pedal = 1.0,
                        .gear = t_s < 0.5 ? driveline::kNeutral : gear,
                        .steering_rad = steering_rad,
                        .brake_left = t_s >= 6.0 ? brake_left : 0.0,
                        .brake_right = t_s >= 6.0 ? brake_right : 0.0};
            if (t_s >= 3.0) {
                input.clutch_pedal = dump ? 0.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0);
            }
            last = step(state, model, input, ground, {.pull_n = drawbar_n}, kDt_s);
            if (last.axle_loads.front_lifted && last.front_side_force_n != 0.0) {
                steered_in_the_air = true;
            }
            const double max_nm = model.params().brakes.max_torque_nm;
            brake_misbehaved = brake_misbehaved ||
                               !brakes_right(last.rear_left.brake_torque_nm, left_rad_s(),
                                             input.brake_left * max_nm) ||
                               !brakes_right(last.rear_right.brake_torque_nm, right_rad_s(),
                                             input.brake_right * max_nm);
            held_against_backward =
                held_against_backward || (last.rear_left.brake_torque_nm < 0.0 &&
                                          std::abs(left_rad_s()) <= kBrakeHoldTolerance_rad_s);
            EXPECT_FALSE(last.shift_refused);
            if (t_s >= 3.0) {
                max_slip = std::max(max_slip, last.slip_ratio);
            }
            min_front_n = std::min(min_front_n, last.axle_loads.front_n);
            max_pitch_rad = std::max(max_pitch_rad, state.chassis.pitch_rad);
            max_bar_n = std::max(max_bar_n, last.axle_loads.wheelie_bar_n);
            if (!state.engine.running) {
                return;
            }
        }
    }
};

TEST(VehicleParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
}

TEST(VehicleParams, NamesTheInvalidPart) {
    const auto with = [](auto mutate) {
        Params p = reference_params();
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](Params& p) { p.engine.inertia_kg_m2 = 0.0; }), ParamError::kEngine);
    EXPECT_EQ(with([](Params& p) { p.clutch.kinetic_ratio = 0.0; }), ParamError::kClutch);
    EXPECT_EQ(with([](Params& p) { p.driveline.gear_count = 0; }), ParamError::kDriveline);
    EXPECT_EQ(with([](Params& p) { p.rear_tire.size.radius_m = 0.0; }), ParamError::kRearTire);
    EXPECT_EQ(with([](Params& p) { p.rear_tire_count = 0; }), ParamError::kRearTireCount);
    EXPECT_EQ(with([](Params& p) { p.rear_tire_count = 3; }), ParamError::kRearTireCount);
    EXPECT_EQ(with([](Params& p) { p.brakes.max_torque_nm = -1.0; }), ParamError::kBrakes);
    EXPECT_EQ(with([](Params& p) { p.front_tire.section_width_m = 0.0; }), ParamError::kFrontTire);
    EXPECT_EQ(with([](Params& p) { p.chassis.mass_kg = 0.0; }), ParamError::kChassis);
    EXPECT_EQ(with([](Params& p) { p.chassis.rear_axle_height_m = 0.7; }),
              ParamError::kRearAxleHeight);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = 0.0; }),
              ParamError::kPtoEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = 1.1; }),
              ParamError::kPtoEfficiencyOutOfRange);
    EXPECT_EQ(with([](Params& p) { p.pto_efficiency = std::nan(""); }),
              ParamError::kPtoEfficiencyOutOfRange);
    EXPECT_FALSE(Model::create([] {
                     Params p = reference_params();
                     p.chassis.mass_kg = 0.0;
                     return p;
                 }())
                     .has_value());
}

TEST(Vehicle, ModelPutsTheRearWheelsOnTheAxle) {
    const Model model = reference_model();
    const Params& p = model.params();
    EXPECT_DOUBLE_EQ(model.driveline().output_inertia_kg_m2,
                     p.driveline.output_inertia_kg_m2 + 2 * p.rear_tire.wheel_inertia_kg_m2);
}

TEST(Vehicle, IdlesAtRestInNeutral) {
    Pull pull;
    for (int i = 0; i < 3000; ++i) {
        pull.last = step(pull.state, pull.model, {}, kGround, {}, kDt_s);
    }
    EXPECT_TRUE(pull.state.engine.running);
    EXPECT_EQ(pull.state.chassis.position_m, 0.0);
    EXPECT_EQ(pull.state.driveline.output_omega_rad_s, 0.0);
    EXPECT_NEAR(pull.state.chassis.pitch_rad, 0.0, 1e-12);
}

TEST(Vehicle, RefusesToShiftWithTheClutchEngaged) {
    Pull pull;
    const Output out = step(pull.state, pull.model,
                            {.throttle = 0.0, .clutch_pedal = 0.0, .gear = 3}, kGround, {}, kDt_s);
    EXPECT_TRUE(out.shift_refused);
    EXPECT_EQ(pull.state.driveline.gear, driveline::kNeutral);
}

// Standing (creeping below kUnderWay_m_s), any gear may be chosen with the
// clutch pressed, down too; past it the pull is under way, and down with the
// clutch is refused at once.
TEST(Vehicle, ShiftsDownWithTheClutchOnlyUntilUnderWay) {
    constexpr int kMovedOffIn = 4;
    constexpr int kLower = 2;
    const Input pressed{.throttle = 0.0, .clutch_pedal = 1.0, .gear = kMovedOffIn};
    const auto creeping_at = [&](double speed_m_s) {
        Pull pull;
        step(pull.state, pull.model, pressed, kGround, {}, kDt_s);
        EXPECT_EQ(pull.state.driveline.gear, kMovedOffIn);
        pull.state.chassis.velocity_m_s = speed_m_s;
        Input down = pressed;
        down.gear = kLower;
        pull.last = step(pull.state, pull.model, down, kGround, {}, kDt_s);
        return pull;
    };

    const Pull standing = creeping_at(0.9 * kUnderWay_m_s);
    EXPECT_FALSE(standing.last.shift_refused);
    EXPECT_EQ(standing.state.driveline.gear, kLower);

    const Pull under_way = creeping_at(1.1 * kUnderWay_m_s);
    EXPECT_TRUE(under_way.last.shift_refused);
    EXPECT_EQ(under_way.state.driveline.gear, kMovedOffIn);
    EXPECT_EQ(under_way.state.driveline.floor_gear, kMovedOffIn);

    // Backwards counts the same: the speed, not its sign.
    const Pull rolling_back = creeping_at(-1.1 * kUnderWay_m_s);
    EXPECT_TRUE(rolling_back.last.shift_refused);
}

// The reference gearbox shifts one gear down under load, the clutch in (a
// powershift step), which is what it is for: the load grows past what 6th
// can pull, and the engine is pulled down towards the stall. Left in 6th it
// stalls; shifted to 5th as it lugs, it pulls on. The shift goes in at once:
// the wheels keep their speed (no jolt in the step), the input shaft then
// turns faster than the engine, so the clutch slips, at most its capacity,
// and locks again within half a second. Two gears down with the clutch in
// are refused.
TEST(Vehicle, ShiftsOneGearDownUnderLoad) {
    constexpr double kSteady_n = 16000.0;
    constexpr double kHeavier_n = 22000.0;
    constexpr double kFor_s = 10.0;
    Pull held;
    held.run(6, kSteady_n, false, 10.0);
    ASSERT_TRUE(held.state.engine.running);
    Pull shifted = held;
    const Params& p = held.model.params();
    // The driver shifts down once the engine is pulled below its peak
    // torque, about 80 % of the rated speed.
    const double lugging_rad_s = 0.8 * p.engine.rated_omega_rad_s;

    Input input{.throttle = 1.0, .clutch_pedal = 0.0, .gear = 6};
    for (int i = 0; i < static_cast<int>(kFor_s / kDt_s) && held.state.engine.running; ++i) {
        step(held.state, held.model, input, kGround, {.pull_n = kHeavier_n}, kDt_s);
    }
    EXPECT_FALSE(held.state.engine.running)
        << "6th stalls: " << held.state.engine.omega_rad_s << " rad/s";

    Input two_down = input;
    two_down.gear = 4;
    double shifted_at_rad_s = 0.0;
    double axle_before_rad_s = 0.0;
    double axle_after_rad_s = 0.0;
    double least_rear_n = 1e12;
    int shifted_at = -1;
    int locked_at = -1;
    bool slipped = false;
    bool refused_two = false;
    for (int i = 0; i < static_cast<int>(kFor_s / kDt_s); ++i) {
        const double engine_rad_s = shifted.state.engine.omega_rad_s;
        const bool shift_now = input.gear == 6 && engine_rad_s < lugging_rad_s;
        if (shift_now) {
            refused_two =
                step(shifted.state, shifted.model, two_down, kGround, {.pull_n = kHeavier_n}, kDt_s)
                    .shift_refused;
            shifted_at_rad_s = engine_rad_s;
            axle_before_rad_s = shifted.state.driveline.output_omega_rad_s;
            shifted_at = i;
            input.gear = 5;
        }
        const Output out =
            step(shifted.state, shifted.model, input, kGround, {.pull_n = kHeavier_n}, kDt_s);
        EXPECT_FALSE(out.shift_refused);
        slipped = slipped || (shift_now && !shifted.state.clutch.locked);
        if (shift_now) {
            axle_after_rad_s = shifted.state.driveline.output_omega_rad_s;
        }
        if (shifted_at >= 0) {
            least_rear_n = std::min(least_rear_n, out.axle_loads.rear_n);
            if (locked_at < 0 && shifted.state.clutch.locked) {
                locked_at = i;
            }
        }
    }
    EXPECT_TRUE(refused_two) << "two gears down";
    EXPECT_GT(shifted_at_rad_s, 0.0) << "the engine lugged";
    EXPECT_EQ(shifted.state.driveline.gear, 5);
    EXPECT_TRUE(slipped) << "the clutch slips as the gear goes in";
    // The clutch brakes the wheels at its capacity, not in one jolt: the
    // shift's own step slows them by less than a tenth (the input shaft's
    // jump alone took a quarter), and the rear tires keep a good share of
    // their load.
    EXPECT_GT(axle_after_rad_s, 0.9 * axle_before_rad_s);
    EXPECT_GT(least_rear_n, 0.2 * kHeavier_n) << least_rear_n << " N";
    EXPECT_GT(locked_at, shifted_at);
    EXPECT_LT((locked_at - shifted_at) * kDt_s, 0.5) << "locked again within half a second";
    ASSERT_TRUE(shifted.state.engine.running) << "5th pulls on";
    EXPECT_TRUE(shifted.state.clutch.locked);
    EXPECT_GT(shifted.state.engine.omega_rad_s, shifted_at_rad_s);
    EXPECT_GT(shifted.state.chassis.velocity_m_s, 0.5);

    // Once a pull: 4th is refused with the clutch in, and with it pressed
    // too, for under way the gearbox goes down only by its powershift.
    Input fourth = input;
    fourth.gear = 4;
    EXPECT_TRUE(step(shifted.state, shifted.model, fourth, kGround, {.pull_n = kHeavier_n}, kDt_s)
                    .shift_refused);
    EXPECT_EQ(shifted.state.driveline.gear, 5);
    fourth.clutch_pedal = 1.0;
    EXPECT_TRUE(step(shifted.state, shifted.model, fourth, kGround, {.pull_n = kHeavier_n}, kDt_s)
                    .shift_refused);
    EXPECT_EQ(shifted.state.driveline.gear, 5) << "not with the clutch pressed either";
    Input neutral = fourth;
    neutral.gear = driveline::kNeutral;
    EXPECT_FALSE(step(shifted.state, shifted.model, neutral, kGround, {.pull_n = kHeavier_n}, kDt_s)
                     .shift_refused)
        << "neutral is always there";
    EXPECT_TRUE(step(shifted.state, shifted.model, fourth, kGround, {.pull_n = kHeavier_n}, kDt_s)
                    .shift_refused)
        << "nor down through neutral";
}

// A shift under load with the engine stalled and the tractor still rolling:
// the engine has no momentum to give the input shaft and stays at rest,
// never below it.
TEST(Vehicle, ShiftsDownUnderLoadWithTheEngineStalled) {
    Pull pull;
    pull.run(6, 5000.0, false, 10.0);
    ASSERT_TRUE(pull.state.engine.running);
    pull.state.engine.running = false;
    pull.state.engine.omega_rad_s = 0.0;
    const Output out =
        step(pull.state, pull.model, {.throttle = 0.0, .clutch_pedal = 0.0, .gear = 5}, kGround,
             {.pull_n = 5000.0}, kDt_s);
    EXPECT_FALSE(out.shift_refused);
    EXPECT_EQ(pull.state.driveline.gear, 5);
    EXPECT_GE(pull.state.engine.omega_rad_s, 0.0);
}

// Pulling steadily, the tires find the slip at which traction balances the
// drawbar pull and the front tires' rolling resistance; the clutch is locked
// and road speed is wheel speed less the slip.
TEST(Vehicle, SteadyPullBalancesTractionAndResistance) {
    Pull pull;
    constexpr double kDrawbar_n = 5000.0;
    pull.run(4, kDrawbar_n, false, 15.0);
    ASSERT_TRUE(pull.state.engine.running);
    EXPECT_TRUE(pull.state.clutch.locked);
    const Params& p = pull.model.params();
    const double front_rolling_n =
        pull.last.front_rolling_resistance * pull.last.axle_loads.front_n;
    EXPECT_NEAR(pull.last.tire_force_n, kDrawbar_n + front_rolling_n, 1.0);
    EXPECT_GT(pull.last.slip_ratio, 0.0);
    EXPECT_LT(pull.last.slip_ratio, 0.1) << "well below the 28 % peak";
    const double wheel_m_s = pull.state.driveline.output_omega_rad_s * p.rear_tire.size.radius_m;
    EXPECT_NEAR(pull.state.chassis.velocity_m_s, wheel_m_s * (1.0 - pull.last.slip_ratio), 1e-3);
    EXPECT_NEAR(pull.state.engine.omega_rad_s, pull.state.driveline.input_omega_rad_s, 1e-9);
}

// Dumping the clutch at full revs spins the wheels far past the traction
// peak; they hook up again once the tractor is moving.
TEST(Vehicle, DumpingTheClutchSpinsTheWheels) {
    Pull dumped;
    dumped.run(3, 5000.0, true, 15.0);
    ASSERT_TRUE(dumped.state.engine.running);
    EXPECT_GT(dumped.max_slip, 0.5);
    EXPECT_LT(dumped.last.slip_ratio, 0.1);

    Pull slipped;
    slipped.run(3, 5000.0, false, 15.0);
    EXPECT_LT(slipped.max_slip, dumped.max_slip);
}

// A load the tires cannot hold (beyond μ 0.91 of the loaded rear axle): in a
// low gear the engine spins the wheels and the tractor stays where it is.
TEST(Vehicle, TooHeavyALoadSpinsTheWheelsInALowGear) {
    Pull pull;
    pull.run(1, 26000.0, false, 15.0);
    EXPECT_TRUE(pull.state.engine.running);
    EXPECT_LT(pull.state.chassis.position_m, 0.1);
    EXPECT_DOUBLE_EQ(pull.last.slip_ratio, 1.0);
}

// In top gear the tires grip better than the engine can turn them: the
// load stalls the engine.
TEST(Vehicle, TooHeavyALoadStallsTheEngineInTopGear) {
    Pull pull;
    pull.run(8, 10000.0, false, 15.0);
    EXPECT_FALSE(pull.state.engine.running);
}

// Pulling steadily, the front axle carries its static share less the
// moments about the rear contact patch: the drawbar pull at hitch height and
// the rear tires' rolling resistance (a forward shift of their load).
TEST(Vehicle, SteadyPullUnloadsTheFrontByItsMoments) {
    Pull pull;
    constexpr double kDrawbar_n = 5000.0;
    pull.run(4, kDrawbar_n, false, 15.0);
    ASSERT_TRUE(pull.state.engine.running);
    const Params& p = pull.model.params();
    const chassis::AxleLoads& loads = pull.last.axle_loads;
    const double weight_n = p.chassis.mass_kg * units::kStandardGravity_m_s2;
    const double rolling_nm =
        pull.last.rear_rolling_resistance * loads.rear_n * p.rear_tire.size.radius_m;
    const double expected_front_n =
        weight_n * p.chassis.cg_ahead_of_rear_axle_m / p.chassis.wheelbase_m -
        (kDrawbar_n * p.chassis.hitch_height_m + rolling_nm) / p.chassis.wheelbase_m;
    EXPECT_NEAR(loads.front_n, expected_front_n, 20.0);
    EXPECT_GT(pull.state.chassis.pitch_rad, 0.0) << "squats onto the rear";
    EXPECT_NEAR(loads.rear_n + loads.front_n, weight_n, 1.0);
}

// A pull on the reference tractor with its ballast at `position`.
Pull with_ballast(double position) {
    Tuning tuning = as_built(testing::reference_params());
    tuning.ballast_position = position;
    std::optional<Model> model = Model::create(tuned(testing::reference_params(), tuning));
    if (!model.has_value()) {
        throw std::runtime_error("the tuned tractor was refused");
    }
    Pull pull;
    pull.model = *model;
    pull.state = initial_state(pull.model);
    return pull;
}

// The weights reach the pull: at the rear they load the rear axle more and
// the tractor pulls harder for it, but the nose goes light (nothing left to
// steer with); at the front the nose stays down.
TEST(VehicleTuning, RearBallastLoadsTheRearAndLightensTheNose) {
    constexpr double kDrawbar_n = 8000.0;
    Pull rear = with_ballast(0.0);
    rear.run(2, kDrawbar_n, true, 15.0);
    Pull front = with_ballast(1.0);
    front.run(2, kDrawbar_n, true, 15.0);
    ASSERT_TRUE(rear.state.engine.running);
    ASSERT_TRUE(front.state.engine.running);
    EXPECT_GT(rear.last.axle_loads.rear_n, front.last.axle_loads.rear_n);
    EXPECT_EQ(rear.min_front_n, 0.0) << "the front tires lift";
    EXPECT_GT(front.min_front_n, 0.0) << "the front tires stay down";
    EXPECT_GT(rear.max_pitch_rad, front.max_pitch_rad);
}

// Dumping the clutch in a low gear throws the front up: the wheels spin up
// against the tires' grip and the axle torque's reaction briefly outweighs
// the tractor. Slipping the clutch in keeps the front on the ground. (Holding
// a wheelie takes more: a traction torque above the weight's moment about
// the rear axle, μ r > b, so μ > 1.2 here.)
TEST(Vehicle, DumpingTheClutchInALowGearLiftsTheFront) {
    Pull dumped;
    dumped.run(2, 8000.0, true, 15.0);
    ASSERT_TRUE(dumped.state.engine.running);
    EXPECT_EQ(dumped.min_front_n, 0.0);

    Pull slipped;
    slipped.run(2, 8000.0, false, 15.0);
    EXPECT_GT(slipped.min_front_n, 1000.0);
    EXPECT_GT(dumped.max_pitch_rad, slipped.max_pitch_rad);
}

// The class's hitch keeps the nose down: the drawbar's weight transfer,
// pull × hitch height / wheelbase, stays below what the front axle carries
// even with the weights fully back. The same tractor with its drawbar at
// 1.0 m (outside the class) pulls a wheelie onto its bar: wheelies emerge
// from the moments, they are not scripted.
TEST(Vehicle, AHighHitchPullsAWheelieOntoTheBar) {
    constexpr double kHeavy_n = 24000.0;
    const auto with_hitch = [](double hitch_height_m) {
        Params params =
            tuned(reference_params(), {.ballast_position = 0.0, .rear_tire_pressure_bar = 1.0});
        params.chassis.hitch_height_m = hitch_height_m;
        return Model::create(params);
    };
    const std::optional<Model> legal_model = with_hitch(0.5);
    const std::optional<Model> high_model = with_hitch(1.0);
    ASSERT_TRUE(legal_model.has_value());
    ASSERT_TRUE(high_model.has_value());
    const auto pulled = [](const Model& model) {
        Pull pull;
        pull.model = model;
        pull.state = initial_state(model);
        pull.run(6, kHeavy_n, false, 12.0);
        return pull;
    };
    const Pull legal = pulled(legal_model.value_or(reference_model()));
    const Pull high = pulled(high_model.value_or(reference_model()));
    EXPECT_GT(legal.min_front_n, 0.0) << "the front stays down at 0.5 m";
    EXPECT_EQ(high.min_front_n, 0.0);
    EXPECT_GT(high.max_bar_n, 0.0) << "on the bar";
    EXPECT_EQ(legal.max_bar_n, 0.0);
    // The bar's tip touches down at about 7° nose-up.
    EXPECT_GT(high.max_pitch_rad, 0.1) << high.max_pitch_rad << " rad";
    EXPECT_LT(legal.max_pitch_rad, 0.02) << legal.max_pitch_rad << " rad";
}

// How a pull against a slowly growing load ends: like a sled whose weight
// box moves forward, the drawbar pull rises steadily after the launch.
struct GrowingLoad {
    bool stalled = false;
    bool spun_out = false;
    double max_slip = 0.0;
    double load_n = 0.0;
};

GrowingLoad pull_growing_load(int gear) {
    constexpr double kStartLoad_n = 3000.0;
    constexpr double kLoadRate_n_per_s = 500.0;
    constexpr double kRiseFrom_s = 6.0;
    constexpr double kSpunOutSlip = 0.9;
    const Model model = reference_model();
    State state = initial_state(model);
    GrowingLoad result;
    for (int i = 0; i < 120000; ++i) {
        const double t_s = i * kDt_s;
        const Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                          .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
                          .gear = t_s < 0.5 ? driveline::kNeutral : gear};
        result.load_n = kStartLoad_n + std::max(0.0, t_s - kRiseFrom_s) * kLoadRate_n_per_s;
        const Output out = step(state, model, input, kGround, {.pull_n = result.load_n}, kDt_s);
        if (t_s > kRiseFrom_s + 2.0) {
            result.max_slip = std::max(result.max_slip, out.slip_ratio);
        }
        if (!state.engine.running) {
            result.stalled = true;
            break;
        }
        if (out.slip_ratio > kSpunOutSlip && t_s > kRiseFrom_s) {
            result.spun_out = true;
            break;
        }
    }
    return result;
}

// The light class pulls on engine torque: in its pulling gears the engine
// lugs down and stalls while the tires still grip (slip below the 28 % peak).
// In a low gear the engine multiplies enough torque to break the tires loose
// first, and the pull ends in wheelspin. Choosing the gear matters.
TEST(Vehicle, LightClassPullsOnTorqueInItsPullingGears) {
    // Where traction peaks on the reference track (see the tire tests).
    constexpr double kPeakSlip = 0.28;
    for (const int gear : {6, 7}) {
        const GrowingLoad pull = pull_growing_load(gear);
        EXPECT_TRUE(pull.stalled) << "gear " << gear;
        EXPECT_LT(pull.max_slip, kPeakSlip) << "gear " << gear;
    }
    const GrowingLoad low = pull_growing_load(4);
    EXPECT_TRUE(low.spun_out);
    EXPECT_FALSE(low.stalled);
}

// On softer ground both axles sink in further: the same drawbar pull costs
// more rolling resistance, so the rear tires work harder for it.
TEST(Vehicle, SofterGroundRollsHarder) {
    Pull firm;
    firm.run(4, 5000.0, false, 15.0);
    Pull soft;
    soft.ground.rear.cone_index_pa *= 0.3;
    soft.ground.front.cone_index_pa *= 0.3;
    soft.run(4, 5000.0, false, 15.0);
    ASSERT_TRUE(firm.state.engine.running);
    ASSERT_TRUE(soft.state.engine.running);
    EXPECT_GT(soft.last.front_rolling_resistance, firm.last.front_rolling_resistance);
    EXPECT_GT(soft.last.rear_rolling_resistance, firm.last.rear_rolling_resistance);
    EXPECT_GT(soft.last.tire_force_n, firm.last.tire_force_n);
    EXPECT_GT(soft.last.slip_ratio, firm.last.slip_ratio);
}

// Pulling with grip throws back a little soil for every metre gained;
// spinning in place throws it back without gaining any ground.
TEST(Vehicle, SpinningThrowsSoilBack) {
    Pull gripping;
    gripping.run(4, 5000.0, false, 15.0);
    const double per_metre_m3 =
        gripping.last.thrown_soil_m3_s / gripping.state.chassis.velocity_m_s;
    EXPECT_GT(per_metre_m3, 0.0);

    Pull spinning;
    spinning.run(1, 26000.0, false, 15.0);
    ASSERT_DOUBLE_EQ(spinning.last.slip_ratio, 1.0);
    EXPECT_LT(spinning.state.chassis.position_m, 0.1);
    EXPECT_GT(spinning.last.thrown_soil_m3_s, 0.5 * gripping.last.thrown_soil_m3_s);
}

TEST(Vehicle, PullingLoadsTheRearAxle) {
    Pull light;
    light.run(4, 2000.0, false, 15.0);
    Pull heavy;
    heavy.run(4, 10000.0, false, 15.0);
    ASSERT_TRUE(heavy.state.engine.running);
    EXPECT_GT(heavy.last.axle_loads.rear_n, light.last.axle_loads.rear_n);
}

// ---- Reversing

// A driver backing up: reverse in at rest, the clutch let in over a second
// at a little throttle, the wheels at `steering_rad`; after `seconds`,
// the clutch pressed again. Free of the sled: no drawbar.
struct Reversing {
    Model model = reference_model();
    State state = initial_state(model);
    Output last;
    double max_speed_m_s = 0.0;
    bool went_forwards = false;

    void run(double seconds, double steering_rad, double coast_s = 0.0) {
        const int driving = static_cast<int>(seconds / kDt_s);
        const int coasting = static_cast<int>(coast_s / kDt_s);
        for (int i = 0; i < driving + coasting; ++i) {
            const double t_s = i * kDt_s;
            const Input input{.throttle = 0.3,
                              .clutch_pedal = i >= driving ? 1.0 : std::clamp(1.5 - t_s, 0.0, 1.0),
                              .gear = driveline::kReverse,
                              .steering_rad = steering_rad,
                              .pull_on = false};
            last = step(state, model, input, kGround, {}, kDt_s);
            EXPECT_FALSE(last.shift_refused);
            went_forwards = went_forwards || state.chassis.velocity_m_s > 0.0;
            max_speed_m_s = std::max(max_speed_m_s, -state.chassis.velocity_m_s);
        }
    }
};

TEST(VehicleReversing, BacksUpInReverse) {
    Reversing back;
    back.run(8.0, 0.0);
    EXPECT_EQ(back.state.driveline.gear, driveline::kReverse);
    EXPECT_TRUE(back.state.engine.running);
    EXPECT_LT(back.state.chassis.velocity_m_s, -0.3);
    EXPECT_LT(back.state.chassis.position_m, -2.0);
    EXPECT_FALSE(back.went_forwards);
    // The wheels turn backwards and pull the tractor back: traction and
    // speed point the same way, the engine still turns forwards.
    EXPECT_LT(back.state.driveline.output_omega_rad_s, 0.0);
    EXPECT_LT(back.last.tire_force_n, 0.0);
    EXPECT_GT(back.state.driveline.input_omega_rad_s, 0.0);
    EXPECT_EQ(back.state.chassis.lateral_position_m, 0.0);
    // Reverse is a crawling gear: at most a few km/h.
    EXPECT_LT(back.max_speed_m_s, 1.5);
}

// With the clutch pressed the resistances bring it to rest, and it stays
// there: they never turn it round.
TEST(VehicleReversing, ComesToRestWithoutRollingForwards) {
    Reversing back;
    back.run(6.0, 0.0, 6.0);
    EXPECT_FALSE(back.went_forwards);
    EXPECT_EQ(back.state.chassis.velocity_m_s, 0.0);
    EXPECT_EQ(back.state.driveline.output_omega_rad_s, 0.0);
}

// Backing up with the wheels to the left swings the nose left too, so the
// tractor turns right: the other way from going forwards.
TEST(VehicleReversing, SteersTheOtherWayRound) {
    Reversing left;
    left.run(8.0, 0.2);
    Reversing right;
    right.run(8.0, -0.2);
    EXPECT_LT(left.state.chassis.heading_rad, -0.02);
    EXPECT_EQ(right.state.chassis.heading_rad, -left.state.chassis.heading_rad);
    EXPECT_EQ(right.state.chassis.position_m, left.state.chassis.position_m);
}

// No change of direction while moving: reverse waits for the tractor to
// stand, as does a forward gear while it backs up.
TEST(VehicleReversing, ChangesDirectionOnlyStanding) {
    Pull forwards;
    forwards.run(4, 2000.0, false, 8.0);
    ASSERT_GT(forwards.state.chassis.velocity_m_s, 0.5);
    const Output into_reverse = step(
        forwards.state, forwards.model,
        {.clutch_pedal = 1.0, .gear = driveline::kReverse, .pull_on = false}, kGround, {}, kDt_s);
    EXPECT_TRUE(into_reverse.shift_refused);
    EXPECT_EQ(forwards.state.driveline.gear, 4);

    Reversing back;
    back.run(6.0, 0.0);
    ASSERT_LT(back.state.chassis.velocity_m_s, -0.3);
    const Output into_first =
        step(back.state, back.model, {.clutch_pedal = 1.0, .gear = 1, .pull_on = false}, kGround,
             {}, kDt_s);
    EXPECT_TRUE(into_first.shift_refused);
    EXPECT_EQ(back.state.driveline.gear, driveline::kReverse);
}

// Off the pull no gear is barred: the floor an earlier pull set is gone.
TEST(VehicleReversing, OffThePullAnyGear) {
    Pull pull;
    pull.run(6, 2000.0, false, 8.0);
    ASSERT_EQ(pull.state.driveline.floor_gear, 6);
    for (int i = 0; i < 3000; ++i) {
        pull.last = step(pull.state, pull.model, {.clutch_pedal = 1.0, .gear = 6, .pull_on = false},
                         kGround, {}, kDt_s);
    }
    EXPECT_EQ(pull.state.driveline.floor_gear, driveline::kNeutral);
    EXPECT_FALSE(step(pull.state, pull.model, {.clutch_pedal = 1.0, .gear = 2, .pull_on = false},
                      kGround, {}, kDt_s)
                     .shift_refused);
    EXPECT_EQ(pull.state.driveline.gear, 2);
}

// ---- Steering

TEST(VehicleSteering, StraightWheelsKeepItOnTheLine) {
    Pull pull;
    pull.run(6, 10000.0, false, 15.0);
    ASSERT_GT(pull.state.chassis.position_m, 10.0);
    EXPECT_EQ(pull.state.chassis.lateral_position_m, 0.0);
    EXPECT_EQ(pull.state.chassis.heading_rad, 0.0);
    EXPECT_EQ(pull.state.chassis.yaw_rate_rad_s, 0.0);
    EXPECT_EQ(pull.last.front_side_force_n, 0.0);
    EXPECT_EQ(pull.last.rear_side_force_n, 0.0);
}

TEST(VehicleSteering, TurnsTheWayTheWheelsPoint) {
    Pull left;
    left.steering_rad = 0.1;
    left.run(6, 10000.0, false, 12.0);
    Pull right;
    right.steering_rad = -0.1;
    right.run(6, 10000.0, false, 12.0);
    EXPECT_GT(left.state.chassis.heading_rad, 0.05);
    EXPECT_GT(left.state.chassis.lateral_position_m, 0.1);
    // Left and right mirror each other exactly.
    EXPECT_EQ(right.state.chassis.heading_rad, -left.state.chassis.heading_rad);
    EXPECT_EQ(right.state.chassis.lateral_position_m, -left.state.chassis.lateral_position_m);
    EXPECT_EQ(right.state.chassis.position_m, left.state.chassis.position_m);
}

// Slowly, on firm ground, the tires barely slip sideways: the tractor turns
// about where the wheels point, at r = u tan(δ) / L.
TEST(VehicleSteering, TurnsAtTheGeometricRateWhenSlow) {
    Pull pull;
    pull.steering_rad = 0.15;
    pull.run(4, 5000.0, false, 10.0);
    const chassis::State& body = pull.state.chassis;
    ASSERT_GT(body.velocity_m_s, 0.5);
    const double geometric_rad_s =
        body.velocity_m_s * std::tan(pull.steering_rad) / pull.model.params().chassis.wheelbase_m;
    EXPECT_NEAR(body.yaw_rate_rad_s / geometric_rad_s, 1.0, 0.1);
    EXPECT_LT(body.yaw_rate_rad_s, geometric_rad_s);
}

TEST(VehicleSteering, HeldToTheSteeringRange) {
    Pull far;
    far.steering_rad = 2.0;
    far.run(4, 5000.0, false, 8.0);
    Pull full;
    full.steering_rad = full.model.params().steering.max_angle_rad;
    full.run(4, 5000.0, false, 8.0);
    EXPECT_EQ(far.state.chassis.heading_rad, full.state.chassis.heading_rad);
}

// A wheelie lifts the front tires off the ground: steering does nothing.
TEST(VehicleSteering, CannotSteerWithTheFrontInTheAir) {
    Pull pull;
    pull.steering_rad = 0.3;
    pull.run(1, 15000.0, true, 6.0);
    EXPECT_FALSE(pull.steered_in_the_air);
}

TEST(VehicleSteering, SteeringHoldsTheTractorBack) {
    Pull straight;
    straight.run(6, 10000.0, false, 12.0);
    Pull steered;
    steered.steering_rad = 0.3;
    steered.run(6, 10000.0, false, 12.0);
    EXPECT_LT(steered.state.chassis.velocity_m_s, straight.state.chassis.velocity_m_s);
}

TEST(VehicleParams, RejectsAnImpossibleSteeringRange) {
    Params params = reference_params();
    params.steering.max_angle_rad = 1.0;
    EXPECT_EQ(validate(params), ParamError::kSteering);
    params = reference_params();
    params.steering.front_tires.limit = 0.0;
    EXPECT_EQ(validate(params), ParamError::kSteering);
}

// ---- Brakes and the differential

// Turning, the outer wheels cover more ground: the open differential lets
// them turn faster.
TEST(VehicleDifferential, OuterWheelsTurnFasterInATurn) {
    Pull pull;
    pull.steering_rad = 0.2;
    pull.run(4, 5000.0, false, 10.0);
    ASSERT_GT(pull.state.chassis.yaw_rate_rad_s, 0.0);
    EXPECT_GT(pull.state.differential_rad_s, 0.0);
    // Both sides grip alike, so their speeds follow the ground they cover.
    const double track_m = pull.model.params().chassis.rear_track_m;
    EXPECT_NEAR(
        pull.right_rad_s() / pull.left_rad_s(),
        (pull.state.chassis.velocity_m_s + 0.5 * track_m * pull.state.chassis.yaw_rate_rad_s) /
            (pull.state.chassis.velocity_m_s - 0.5 * track_m * pull.state.chassis.yaw_rate_rad_s),
        0.01);
}

TEST(VehicleBrakes, OneBrakeTurnsTheTractorToItsSide) {
    Pull right;
    right.brake_right = 0.3;
    right.run(4, 5000.0, false, 12.0);
    EXPECT_LT(right.state.chassis.heading_rad, -0.02);
    EXPECT_LT(right.state.chassis.lateral_position_m, 0.0);
    // The braked side turns slower; the differential gives the other side
    // the difference.
    EXPECT_LT(right.right_rad_s(), right.left_rad_s());
    Pull left;
    left.brake_left = 0.3;
    left.run(4, 5000.0, false, 12.0);
    EXPECT_EQ(left.state.chassis.heading_rad, -right.state.chassis.heading_rad);
    EXPECT_EQ(left.state.chassis.lateral_position_m, -right.state.chassis.lateral_position_m);
}

TEST(VehicleBrakes, BothBrakesSlowItDownWithoutTurning) {
    Pull free;
    free.run(4, 5000.0, false, 12.0);
    Pull braked;
    braked.brake_left = 0.2;
    braked.brake_right = 0.2;
    braked.run(4, 5000.0, false, 12.0);
    EXPECT_LT(braked.state.chassis.position_m, free.state.chassis.position_m);
    EXPECT_EQ(braked.state.chassis.heading_rad, 0.0);
    EXPECT_EQ(braked.state.differential_rad_s, 0.0);
}

// Rolling in neutral, full pedal on one side stops its wheels and holds
// them with no more torque than the ground's drag on them; the open
// differential lets the other side roll on.
TEST(VehicleBrakes, FullPedalHoldsOneSideStill) {
    const Model model = reference_model();
    State state = initial_state(model);
    constexpr double kSpeed_m_s = 2.0;
    const double radius_m = model.params().rear_tire.size.radius_m;
    state.chassis.velocity_m_s = kSpeed_m_s;
    state.driveline.output_omega_rad_s = kSpeed_m_s / radius_m;
    const Input input{.clutch_pedal = 1.0, .gear = driveline::kNeutral, .brake_left = 1.0};
    Output out;
    int held_steps = 0;
    for (int i = 0; i < 1500; ++i) {
        out = step(state, model, input, kGround, {}, kDt_s);
        const double left_rad_s = state.driveline.output_omega_rad_s - state.differential_rad_s;
        ASSERT_TRUE(brakes_right(out.rear_left.brake_torque_nm, left_rad_s,
                                 model.params().brakes.max_torque_nm))
            << i;
        held_steps += std::abs(left_rad_s) <= kBrakeHoldTolerance_rad_s ? 1 : 0;
    }
    EXPECT_GT(held_steps, 1000);
    EXPECT_GT(out.rear_left.brake_torque_nm, 0.0);
    EXPECT_LT(out.rear_left.brake_torque_nm, model.params().brakes.max_torque_nm);
    const double right_rad_s = state.driveline.output_omega_rad_s + state.differential_rad_s;
    EXPECT_NEAR(right_rad_s * radius_m, state.chassis.velocity_m_s, 0.1);
    // The held side drags: the tractor slows and turns towards it.
    EXPECT_LT(state.chassis.velocity_m_s, kSpeed_m_s);
    EXPECT_GT(state.chassis.heading_rad, 0.0)
        << state.chassis.velocity_m_s << ' ' << state.chassis.yaw_rate_rad_s;
}

// Whatever the gear, pedals, load and steering, a brake follows Coulomb
// friction (see brakes_right): it holds its side at rest or slips at full
// torque against it, and never drives it.
TEST(VehicleBrakes, NeverDriveTheWheels) {
    for (const int gear : {1, 2, 4, 6}) {
        for (const double pedal : {0.05, 0.5, 1.0}) {
            for (const double drawbar_n : {5000.0, 20000.0}) {
                for (const double steering_rad : {0.0, 0.3}) {
                    Pull pull;
                    pull.brake_left = pedal;
                    pull.brake_right = 0.6 * pedal;
                    pull.steering_rad = steering_rad;
                    pull.run(gear, drawbar_n, false, 12.0);
                    EXPECT_FALSE(pull.brake_misbehaved)
                        << "gear " << gear << ", pedal " << pedal << ", drawbar " << drawbar_n
                        << " N, steering " << steering_rad << " rad";
                }
            }
        }
    }
}

// Pulling hard in first gear with the left brake full on, the tractor
// comes to rest; the held side's tire then pushes it backwards and the
// brake holds it with negative torque: friction works both ways.
TEST(VehicleBrakes, HoldAgainstTurningBackwards) {
    Pull pull;
    pull.brake_left = 1.0;
    pull.run(1, 20000.0, false, 14.0);
    EXPECT_FALSE(pull.brake_misbehaved);
    EXPECT_TRUE(pull.held_against_backward);
}

// Standing in neutral, the rear sides turning against each other (as a
// tractor stopped in a turn leaves them), the steering held: both brakes
// on hold both sides at rest. With the axle held, only the sides'
// difference is free and both brakes act on it; they must not overshoot
// through rest every step, which kept the wheels flicking either way and
// reported a large slip at a standstill. Mirrored, and with one brake much
// weaker than the other, the hold is the same. Faster sides take a few
// steps to stop, slowing all the way, through rest at most once.
struct TurningAgainst {
    double differential_rad_s = 0.0;
    double steering_rad = 0.0;
    double brake_left = 0.0;
    double brake_right = 0.0;
    // Steps before both sides are at rest.
    int steps_to_hold = 0;
};

class BothBrakesHold : public ::testing::TestWithParam<TurningAgainst> {};

TEST_P(BothBrakesHold, SidesTurningAgainstEachOther) {
    const TurningAgainst c = GetParam();
    Pull pull;
    pull.state.differential_rad_s = c.differential_rad_s;
    const Input input{.clutch_pedal = 1.0,
                      .gear = driveline::kNeutral,
                      .steering_rad = c.steering_rad,
                      .brake_left = c.brake_left,
                      .brake_right = c.brake_right};
    // The tires' contact patches settle after the stop (relaxation).
    constexpr int kTiresSettledSteps = 100;
    constexpr int kSteps = 2000;
    double worst_rad_s = 0.0;
    double worst_slip = 0.0;
    int through_rest = 0;
    double last_left_rad_s = pull.left_rad_s();
    for (int i = 0; i < kSteps; ++i) {
        const Output out = step(pull.state, pull.model, input, kGround, {}, kDt_s);
        const double left_rad_s = pull.left_rad_s();
        if (std::abs(left_rad_s) > kBrakeHoldTolerance_rad_s) {
            through_rest += left_rad_s * last_left_rad_s < 0.0 ? 1 : 0;
            last_left_rad_s = left_rad_s;
        }
        if (i > c.steps_to_hold) {
            worst_rad_s =
                std::max({worst_rad_s, std::abs(left_rad_s), std::abs(pull.right_rad_s())});
        }
        if (i > kTiresSettledSteps) {
            worst_slip = std::max(worst_slip, std::abs(out.slip_ratio));
        }
    }
    EXPECT_LE(worst_rad_s, kBrakeHoldTolerance_rad_s);
    EXPECT_LE(through_rest, 1);
    EXPECT_LT(worst_slip, 0.01);
    EXPECT_EQ(pull.state.chassis.velocity_m_s, 0.0);
}

INSTANTIATE_TEST_SUITE_P(VehicleBrakes, BothBrakesHold,
                         ::testing::Values(TurningAgainst{0.3, 0.2, 1.0, 1.0, 0},
                                           TurningAgainst{-0.3, -0.2, 1.0, 1.0, 0},
                                           TurningAgainst{0.3, 0.2, 0.1, 1.0, 0},
                                           TurningAgainst{0.3, 0.2, 1.0, 0.1, 0},
                                           TurningAgainst{-0.3, -0.2, 0.1, 1.0, 0},
                                           TurningAgainst{-0.3, -0.2, 1.0, 0.1, 0},
                                           TurningAgainst{1.5, 0.2, 0.1, 1.0, 5},
                                           TurningAgainst{-1.5, -0.2, 1.0, 0.1, 5}));

// The reported slip is the more positive (driving) slip of the two sides:
// one side spinning is how an open differential spins out.
TEST(VehicleBrakes, SlipIsTheSideThatSlipsMore) {
    Pull pull;
    pull.brake_right = 0.5;
    pull.run(4, 8000.0, false, 12.0);
    EXPECT_GT(pull.last.rear_left.slip_ratio, pull.last.rear_right.slip_ratio);
    EXPECT_EQ(pull.last.slip_ratio, pull.last.rear_left.slip_ratio);
}

// In a high gear, one brake can take more torque than the engine gives.
TEST(VehicleBrakes, FullPedalInATopGearStallsTheEngine) {
    Pull pull;
    pull.brake_left = 1.0;
    pull.run(8, 1000.0, false, 15.0);
    EXPECT_FALSE(pull.state.engine.running);
}

std::uint64_t pull_hash() {
    Pull pull;
    hash::Fnv1a hash;
    for (int i = 0; i < 15000; ++i) {
        const double t_s = i * kDt_s;
        // Steering left after 8 s and braking the right side after 11 s
        // exercise the sideways motion, the differential and the brakes.
        const Input input{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                          .clutch_pedal = t_s < 3.0 ? 1.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0),
                          .gear = t_s < 0.5 ? driveline::kNeutral : 3,
                          .steering_rad = t_s < 8.0 ? 0.0 : 0.1,
                          .brake_right = t_s < 11.0 ? 0.0 : 0.3};
        step(pull.state, pull.model, input, kGround, {.pull_n = 8000.0}, kDt_s);
        hash_append(hash, pull.state);
    }
    return hash.value();
}

TEST(Vehicle, PullIsDeterministic) {
    EXPECT_EQ(pull_hash(), pull_hash());
}

// Regression hash; must match on every compiler, build type and platform.
// Update deliberately, with a note in the commit, when the model changes.
TEST(Vehicle, PullMatchesRegressionHash) {
    EXPECT_EQ(pull_hash(), 0x4cbe5b1e61c8c56ULL);
}

}  // namespace
}  // namespace tp::vehicle
