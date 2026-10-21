#include "tp/core/controls/controls.hpp"

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/run/run.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>

#include "../sled/reference_sled.hpp"
#include "../track/reference_track.hpp"
#include "../vehicle/reference_vehicle.hpp"
#include "reference_controls.hpp"

namespace tp::controls {
namespace {

constexpr double kDt_s = 0.001;

constexpr EasyParams kEasy = testing::kReferenceEasy;

constexpr run::Rules kRules{.track_length_m = 100.0,
                            .max_duration_s = 120.0,
                            .lane_width_m = 12.0,
                            .full_pull_line_m = 100.0};

pull::Model model_with(const sled::Params& sled) {
    const auto model = pull::Model::create({.tractor = vehicle::testing::reference_params(),
                                            .sled = sled,
                                            .track = track::testing::reference_params()});
    if (!model) {
        std::abort();
    }
    return *model;
}

// A sled the reference tractor pulls the whole track.
sled::Params light_sled() {
    sled::Params sled = sled::testing::reference_params();
    sled.pushdown_start_m = 1000.0;
    return sled;
}

// The easy driver: idle for half a second, then the gear and full throttle.
EasyInput launch(double t_s, int gear = 6) {
    return t_s < 0.5 ? EasyInput{} : EasyInput{.throttle = 1.0, .gear = gear};
}

using Script = std::function<EasyInput(double t_s)>;

struct EasyRun {
    run::State state;
    run::Result result;
    AssistState assist;
    double lowest_running_omega_rad_s = 1e9;
    bool shift_refused = false;
};

// Runs with the easy layout; `watch` sees each step's input after it.
EasyRun drive(const pull::Model& model, const Script& script,
              const std::function<void(double, const vehicle::Input&, const EasyRun&)>& watch = {},
              const run::Rules& rules = kRules) {
    EasyRun r{.state = run::start(model), .result = {}, .assist = {}};
    const vehicle::Params& tractor = model.tractor().params();
    for (int i = 0; !r.state.result; ++i) {
        const double t_s = i * kDt_s;
        const vehicle::Input in =
            easy(script(t_s), kEasy, r.assist, tractor, r.state.pull.tractor, kDt_s);
        const pull::Output out = run::step(r.state, model, rules, in, kDt_s);
        r.shift_refused = r.shift_refused || out.tractor.shift_refused;
        if (t_s > 2.0) {
            r.lowest_running_omega_rad_s =
                std::min(r.lowest_running_omega_rad_s, r.state.pull.tractor.engine.omega_rad_s);
        }
        if (watch) {
            watch(t_s, in, r);
        }
    }
    if (!r.state.result) {
        std::abort();
    }
    r.result = *r.state.result;
    return r;
}

TEST(ControlsParams, ReferenceIsValid) {
    EXPECT_EQ(validate(kEasy), std::nullopt);
}

TEST(ControlsParams, RejectsNonsense) {
    const auto with = [](auto mutate) {
        EasyParams p = kEasy;
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](EasyParams& p) { p.steer_share = 0.0; }), ParamError::kSteerShareOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.steer_share = 1.1; }), ParamError::kSteerShareOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.steer_dead_zone = -0.01; }),
              ParamError::kSteerDeadZoneOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.steer_dead_zone = p.steer_share; }),
              ParamError::kSteerDeadZoneOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.steer_rate_per_s = 0.0; }),
              ParamError::kSteerRateNotPositive);
    EXPECT_EQ(with([](EasyParams& p) { p.steer_rate_per_s = HUGE_VAL; }), ParamError::kNotFinite);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.engage_time_s = 0.0; }),
              ParamError::kAssistTimeNotPositive);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.release_time_s = -1.0; }),
              ParamError::kAssistTimeNotPositive);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.guard_speed_share = 1.2; }),
              ParamError::kGuardOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.guard_band_share = 0.6; }),
              ParamError::kGuardOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.min_throttle = 1.0; }),
              ParamError::kMinThrottleOutOfRange);
    EXPECT_EQ(with([](EasyParams& p) { p.clutch_assist.engage_time_s = std::nan(""); }),
              ParamError::kNotFinite);
}

TEST(ControlsParams, ProReferenceAndDefaultAreValid) {
    EXPECT_EQ(validate(testing::kReferencePro), std::nullopt);
    EXPECT_EQ(validate(ProParams{}), std::nullopt);
}

TEST(ControlsParams, RejectsProNonsense) {
    const auto with = [](auto mutate) {
        ProParams p = testing::kReferencePro;
        mutate(p);
        return validate(p);
    };
    EXPECT_EQ(with([](ProParams& p) { p.steer_dead_zone = -0.01; }),
              ParamError::kSteerDeadZoneOutOfRange);
    EXPECT_EQ(with([](ProParams& p) { p.steer_dead_zone = 1.0; }),
              ParamError::kSteerDeadZoneOutOfRange);
    EXPECT_EQ(with([](ProParams& p) { p.steer_rate_per_s = 0.0; }),
              ParamError::kSteerRateNotPositive);
    EXPECT_EQ(with([](ProParams& p) { p.steer_rate_per_s = HUGE_VAL; }), ParamError::kNotFinite);
    EXPECT_EQ(with([](ProParams& p) { p.steer_dead_zone = std::nan(""); }), ParamError::kNotFinite);
}

// Pro with its default parameters steers as the control is: the same input
// as the plain mapping, and the steering's state follows the control.
TEST(Pro, DefaultParamsSteerAsTheControlIs) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    for (const double steering : {0.0, 0.02, -0.3, 1.0, 5.0}) {
        AssistState state;
        const ProInput in{.throttle = 0.5, .clutch_pedal = 0.25, .gear = 3, .steering = steering};
        const vehicle::Input shaped = pro(in, ProParams{}, state, tractor, kDt_s);
        const vehicle::Input plain = pro(in, tractor);
        EXPECT_EQ(shaped.steering_rad, plain.steering_rad) << steering;
        EXPECT_EQ(shaped.throttle, plain.throttle);
        EXPECT_EQ(shaped.clutch_pedal, plain.clutch_pedal);
        EXPECT_EQ(state.steering, std::clamp(steering, -1.0, 1.0));
    }
}

// Pro with a dead zone and a rate: a thumb resting in the middle steers
// straight, and the wheels follow a jump of the control at the rate; the
// pedals pass at once.
TEST(Pro, ShapesTheSteeringLikeTheEasyLayout) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const ProParams params = testing::kReferencePro;
    const double max_rad = tractor.steering.max_angle_rad;
    const auto hold = [&](double steering, double held_s) {
        AssistState state;
        vehicle::Input in;
        for (int i = 0; i < static_cast<int>(std::lround(held_s / kDt_s)); ++i) {
            in = pro({.throttle = 1.0, .clutch_pedal = 0.0, .steering = steering}, params, state,
                     tractor, kDt_s);
        }
        return in;
    };
    EXPECT_EQ(hold(params.steer_dead_zone, 2.0).steering_rad, 0.0);
    const double per_step = params.steer_rate_per_s.value_or(0.0) * kDt_s;
    ASSERT_GT(per_step, 0.0);
    const vehicle::Input first = hold(1.0, kDt_s);
    EXPECT_DOUBLE_EQ(first.steering_rad, per_step * max_rad);
    EXPECT_EQ(first.throttle, 1.0);
    EXPECT_EQ(first.clutch_pedal, 0.0);
    EXPECT_EQ(hold(-1.0, 2.0).steering_rad, -max_rad);
    const double half = params.steer_dead_zone + 0.5 * (1.0 - params.steer_dead_zone);
    EXPECT_DOUBLE_EQ(hold(half, 2.0).steering_rad, 0.5 * max_rad);
}

// Pro: the steering control spans the steering's range; the pedals pass
// through, held to their travel.
TEST(Pro, MapsEachControlOnItsOwn) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const double max_rad = tractor.steering.max_angle_rad;
    const vehicle::Input in = pro({.throttle = 0.7,
                                   .clutch_pedal = 0.4,
                                   .gear = 3,
                                   .steering = -0.5,
                                   .brake_left = 0.2,
                                   .brake_right = 1.5},
                                  tractor);
    EXPECT_EQ(in.throttle, 0.7);
    EXPECT_EQ(in.clutch_pedal, 0.4);
    EXPECT_EQ(in.gear, 3);
    EXPECT_DOUBLE_EQ(in.steering_rad, -0.5 * max_rad);
    EXPECT_EQ(in.brake_left, 0.2);
    EXPECT_EQ(in.brake_right, 1.0);
    EXPECT_DOUBLE_EQ(pro({.steering = 3.0}, tractor).steering_rad, max_rad);
}

// A control that is not a number (a NaN from a front end's maths) reads as
// the control at rest, so it never reaches the simulation; an infinity is
// held to the control's range, as any value beyond it.
TEST(Pro, ReadsNotANumberAsAtRest) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const double nan = std::nan("");
    const vehicle::Input in = pro({.throttle = nan,
                                   .clutch_pedal = nan,
                                   .gear = 3,
                                   .steering = nan,
                                   .brake_left = nan,
                                   .brake_right = nan},
                                  tractor);
    EXPECT_EQ(in.throttle, 0.0);
    EXPECT_EQ(in.clutch_pedal, 1.0);
    EXPECT_EQ(in.steering_rad, 0.0);
    EXPECT_EQ(in.brake_left, 0.0);
    EXPECT_EQ(in.brake_right, 0.0);
}

TEST(Pro, HoldsAnInfinityToTheRange) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const vehicle::Input in = pro({.throttle = HUGE_VAL,
                                   .clutch_pedal = -HUGE_VAL,
                                   .gear = 3,
                                   .steering = HUGE_VAL,
                                   .brake_left = HUGE_VAL,
                                   .brake_right = -HUGE_VAL},
                                  tractor);
    EXPECT_EQ(in.throttle, 1.0);
    EXPECT_EQ(in.clutch_pedal, 0.0);
    EXPECT_EQ(in.steering_rad, tractor.steering.max_angle_rad);
    EXPECT_EQ(in.brake_left, 1.0);
    EXPECT_EQ(in.brake_right, 0.0);
}

TEST(Easy, ReadsNotANumberAsAtRest) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const std::optional<vehicle::Model> model = vehicle::Model::create(tractor);
    if (!model) {
        ADD_FAILURE() << "invalid reference tractor";
        return;
    }
    const vehicle::State state = vehicle::initial_state(*model);
    const double nan = std::nan("");
    AssistState assist;
    const vehicle::Input in = easy({.throttle = nan, .gear = driveline::kNeutral, .steering = nan},
                                   kEasy, assist, tractor, state, kDt_s);
    EXPECT_EQ(in.throttle, 0.0);
    EXPECT_EQ(in.steering_rad, 0.0);
    EXPECT_EQ(in.brake_left, 0.0);
    EXPECT_EQ(in.brake_right, 0.0);
    EXPECT_TRUE(std::isfinite(assist.engagement));
}

// The easy layout's steering after holding `control` for `held_s` from
// straight ahead, on the reference tractor standing still.
struct Steered {
    vehicle::Input input;
    AssistState assist;
};

Steered hold_steering(double control, double held_s, const EasyParams& params = kEasy,
                      AssistState assist = {}) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const std::optional<vehicle::Model> model = vehicle::Model::create(tractor);
    if (!model) {
        std::abort();
    }
    const vehicle::State state = vehicle::initial_state(*model);
    Steered out{.input = {}, .assist = assist};
    const auto steps = static_cast<int>(std::lround(held_s / kDt_s));
    for (int i = 0; i < steps; ++i) {
        out.input = easy({.steering = control}, params, out.assist, tractor, state, kDt_s);
    }
    return out;
}

// Easy: the first stretch of the steering control turns the wheels; past
// it they stay at full lock and the inner rear brake comes on, to full
// pedal at the end. Left and right mirror each other. Without a dead zone,
// and with the steering settled.
TEST(Easy, SteersThenBrakesTheInnerWheels) {
    EasyParams params = kEasy;
    params.steer_dead_zone = 0.0;
    const double max_rad = vehicle::testing::reference_params().steering.max_angle_rad;
    const auto with = [&](double steering) { return hold_steering(steering, 2.0, params).input; };
    const vehicle::Input half = with(0.3);
    EXPECT_DOUBLE_EQ(half.steering_rad, 0.5 * max_rad);
    EXPECT_EQ(half.brake_left, 0.0);
    EXPECT_EQ(half.brake_right, 0.0);
    const vehicle::Input past = with(0.8);
    EXPECT_DOUBLE_EQ(past.steering_rad, max_rad);
    EXPECT_DOUBLE_EQ(past.brake_left, 0.5);
    EXPECT_EQ(past.brake_right, 0.0);
    const vehicle::Input right = with(-1.0);
    EXPECT_DOUBLE_EQ(right.steering_rad, -max_rad);
    EXPECT_EQ(right.brake_left, 0.0);
    EXPECT_EQ(right.brake_right, 1.0);
    EXPECT_EQ(with(0.0).steering_rad, 0.0);
}

// A control within the dead zone steers straight; past it, the rest of the
// travel is stretched to the whole range, so the end still reaches full
// brake.
TEST(Easy, DeadZoneReadsAsStraight) {
    const double dead = kEasy.steer_dead_zone;
    EXPECT_EQ(hold_steering(dead, 2.0).assist.steering, 0.0);
    EXPECT_EQ(hold_steering(-0.5 * dead, 2.0).assist.steering, 0.0);
    EXPECT_DOUBLE_EQ(hold_steering(dead + 0.5 * (1.0 - dead), 2.0).assist.steering, 0.5);
    EXPECT_DOUBLE_EQ(hold_steering(-dead - 0.5 * (1.0 - dead), 2.0).assist.steering, -0.5);
    const Steered full = hold_steering(1.0, 2.0);
    EXPECT_EQ(full.assist.steering, 1.0);
    EXPECT_EQ(full.input.brake_left, 1.0);
}

// The steering follows the control at the steering rate, without passing
// it, whichever way and however far the control jumps; a control that is
// not a number brings it back to straight ahead at the same rate.
TEST(Easy, SteeringFollowsAtTheRate) {
    const double per_step = kEasy.steer_rate_per_s * kDt_s;
    EXPECT_DOUBLE_EQ(hold_steering(1.0, kDt_s).assist.steering, per_step);
    EXPECT_NEAR(hold_steering(1.0, 0.2).assist.steering, 0.2 * kEasy.steer_rate_per_s, 1e-9);
    const double full_s = 1.0 / kEasy.steer_rate_per_s;
    EXPECT_LT(hold_steering(1.0, full_s - 2.0 * kDt_s).assist.steering, 1.0);
    EXPECT_EQ(hold_steering(1.0, full_s + 2.0 * kDt_s).assist.steering, 1.0);

    // Lock to lock takes twice as long as centre to lock.
    const AssistState left{.engagement = 0.0, .steering = 1.0};
    EXPECT_GT(hold_steering(-1.0, 2.0 * full_s - 2.0 * kDt_s, kEasy, left).assist.steering, -1.0);
    EXPECT_EQ(hold_steering(-1.0, 2.0 * full_s + 2.0 * kDt_s, kEasy, left).assist.steering, -1.0);

    // Never past the control: a small move is reached and held.
    const double target = 0.5 * (1.0 - kEasy.steer_dead_zone) + kEasy.steer_dead_zone;
    EXPECT_DOUBLE_EQ(hold_steering(target, full_s).assist.steering, 0.5);

    const Steered back = hold_steering(std::nan(""), kDt_s, kEasy, left);
    EXPECT_DOUBLE_EQ(back.assist.steering, 1.0 - per_step);
}

// A sled the engine can pull: the assist lets the clutch in fully, keeps
// it in, and the tractor makes a full pull.
TEST(ClutchAssist, LetsTheClutchInFully) {
    const pull::Model model = model_with(light_sled());
    double fully_in_at_s = -1.0;
    const EasyRun r = drive(
        model, [](double t_s) { return launch(t_s); },
        [&](double t_s, const vehicle::Input& in, const EasyRun&) {
            if (fully_in_at_s < 0.0 && in.clutch_pedal == 0.0) {
                fully_in_at_s = t_s;
            }
        });
    EXPECT_EQ(r.result.ending, run::Ending::kFullPull);
    EXPECT_GT(fully_in_at_s, 0.5);
    EXPECT_LT(fully_in_at_s, 10.0);
    EXPECT_EQ(r.assist.engagement, 1.0);
    EXPECT_FALSE(r.shift_refused);
}

// Full throttle against the push-down, which stalls the engine with the
// clutch let in by a driver: the assist slips the clutch instead. The engine
// never drops far below the guard speed, and the run ends with the sled
// stopped, not the engine.
TEST(ClutchAssist, KeepsTheEngineFromStalling) {
    const pull::Model model = model_with(sled::testing::reference_params());
    const EasyRun r = drive(model, [](double t_s) { return launch(t_s); });
    const vehicle::Params& tractor = model.tractor().params();
    EXPECT_NE(r.result.ending, run::Ending::kStalled);
    EXPECT_TRUE(r.state.pull.tractor.engine.running);
    const double guard_rad_s =
        kEasy.clutch_assist.guard_speed_share * tractor.engine.rated_omega_rad_s;
    const double band_rad_s =
        kEasy.clutch_assist.guard_band_share * tractor.engine.rated_omega_rad_s;
    EXPECT_GT(r.lowest_running_omega_rad_s, guard_rad_s - 2.0 * band_rad_s);
    EXPECT_GT(r.result.distance_m, sled::testing::reference_params().pushdown_start_m);
}

// Lifting off presses the clutch within the release time: the sled stops
// with the engine running.
TEST(ClutchAssist, LiftingOffPressesTheClutch) {
    const pull::Model model = model_with(light_sled());
    constexpr double kLiftAt_s = 15.0;
    double out_at_s = -1.0;
    const EasyRun r = drive(
        model,
        [](double t_s) {
            EasyInput in = launch(t_s);
            in.throttle = t_s < kLiftAt_s ? in.throttle : 0.0;
            return in;
        },
        [&](double t_s, const vehicle::Input& in, const EasyRun&) {
            if (out_at_s < 0.0 && t_s >= kLiftAt_s && in.clutch_pedal == 1.0) {
                out_at_s = t_s;
            }
        });
    EXPECT_GE(out_at_s, kLiftAt_s);
    EXPECT_LE(out_at_s, kLiftAt_s + kEasy.clutch_assist.release_time_s + 2.0 * kDt_s);
    EXPECT_EQ(r.result.ending, run::Ending::kStopped);
    EXPECT_TRUE(r.state.pull.tractor.engine.running);
}

// A gear change on the move: the gear in stays until the assist has the
// clutch out, then the new one goes in and the clutch comes back in.
TEST(ClutchAssist, PressesTheClutchToChangeGear) {
    const pull::Model model = model_with(light_sled());
    constexpr double kShiftAt_s = 10.0;
    double shifted_at_s = -1.0;
    bool wrong_gear_with_clutch_in = false;
    const EasyRun r = drive(
        model, [](double t_s) { return launch(t_s, t_s < kShiftAt_s ? 6 : 5); },
        [&](double t_s, const vehicle::Input& in, const EasyRun& now) {
            if (t_s >= kShiftAt_s && in.gear == 5 && shifted_at_s < 0.0) {
                shifted_at_s = t_s;
                wrong_gear_with_clutch_in = in.clutch_pedal < 1.0;
            }
            (void)now;
        });
    EXPECT_GT(shifted_at_s, kShiftAt_s);
    EXPECT_LE(shifted_at_s, kShiftAt_s + kEasy.clutch_assist.release_time_s + 2.0 * kDt_s);
    EXPECT_FALSE(wrong_gear_with_clutch_in);
    EXPECT_FALSE(r.shift_refused);
    EXPECT_EQ(r.state.pull.tractor.driveline.gear, 5);
    EXPECT_GT(r.assist.engagement, 0.0);
}

// A touch of throttle creeps the tractor away too: at part throttle the
// guard sits where the governor reaches full fuel for that throttle, so the
// assist lets the clutch in although the engine runs far below its full
// throttle guard.
TEST(ClutchAssist, EngagesAtPartThrottle) {
    const pull::Model model = model_with(light_sled());
    run::Rules short_run = kRules;
    short_run.max_duration_s = 30.0;
    for (const double throttle : {0.1, 0.2, 0.35, 0.6}) {
        double most_engaged = 0.0;
        const EasyRun r = drive(
            model,
            [throttle](double t_s) {
                EasyInput in = launch(t_s, 2);
                in.throttle *= throttle;
                return in;
            },
            [&](double, const vehicle::Input&, const EasyRun& now) {
                most_engaged = std::max(most_engaged, now.assist.engagement);
            },
            short_run);
        EXPECT_EQ(most_engaged, 1.0) << "throttle " << throttle;
        EXPECT_GT(r.result.distance_m, 5.0) << "throttle " << throttle;
        EXPECT_TRUE(r.state.pull.tractor.engine.running) << "throttle " << throttle;
    }
}

// A gear the gearbox does not have is held to its range: the top gear goes
// in and the clutch comes back in.
TEST(ClutchAssist, HoldsTheGearToTheGearbox) {
    const pull::Model model = model_with(light_sled());
    const auto top = static_cast<int>(model.tractor().params().driveline.gear_count);
    run::Rules short_run = kRules;
    short_run.max_duration_s = 10.0;
    const EasyRun r =
        drive(model, [top](double t_s) { return launch(t_s, top + 3); }, {}, short_run);
    EXPECT_FALSE(r.shift_refused);
    EXPECT_EQ(r.state.pull.tractor.driveline.gear, top);
    EXPECT_GT(r.assist.engagement, 0.0);
}

std::uint64_t easy_hash(run::Result& result) {
    const pull::Model model = model_with(sled::testing::reference_params());
    hash::Fnv1a hash;
    const EasyRun r = drive(
        model,
        [](double t_s) {
            EasyInput in = launch(t_s);
            // A small correction to the left, then back to the right.
            if (t_s > 5.0 && t_s < 7.0) {
                in.steering = 0.05;
            } else if (t_s >= 7.0 && t_s < 9.0) {
                in.steering = -0.05;
            }
            return in;
        },
        [&](double, const vehicle::Input&, const EasyRun& now) {
            run::hash_append(hash, now.state);
            hash_append(hash, now.assist);
        });
    result = r.result;
    return hash.value();
}

// A replay with the easy layout: the same inputs give the same run, bit for
// bit, on every compiler, build type and platform.
TEST(Easy, ReplayMatchesRegression) {
    run::Result result;
    const std::uint64_t hash = easy_hash(result);
    run::Result again;
    EXPECT_EQ(easy_hash(again), hash);
    EXPECT_EQ(again.distance_m, result.distance_m);
    EXPECT_EQ(hash, 0x97dad199ae63ab93ULL);
    // 88.693174910881481 m, every bit pinned.
    EXPECT_EQ(result.distance_m, 0x1.62c5cfa4d2932p+6);
    EXPECT_EQ(result.ending, run::Ending::kSpunOut);
}

}  // namespace
}  // namespace tp::controls
