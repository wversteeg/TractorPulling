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
EasyRun drive(
    const pull::Model& model, const Script& script,
    const std::function<void(double, const vehicle::Input&, const EasyRun&)>& watch = {}) {
    EasyRun r{.state = run::start(model), .result = {}, .assist = {}};
    const vehicle::Params& tractor = model.tractor().params();
    for (int i = 0; !r.state.result; ++i) {
        const double t_s = i * kDt_s;
        const vehicle::Input in =
            easy(script(t_s), kEasy, r.assist, tractor, r.state.pull.tractor, kDt_s);
        const pull::Output out = run::step(r.state, model, kRules, in, kDt_s);
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

// Easy: the first stretch of the steering control turns the wheels; past
// it they stay at full lock and the inner rear brake comes on, to full
// pedal at the end. Left and right mirror each other.
TEST(Easy, SteersThenBrakesTheInnerWheels) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const std::optional<vehicle::Model> model = vehicle::Model::create(tractor);
    if (!model) {
        ADD_FAILURE() << "invalid reference tractor";
        return;
    }
    const vehicle::State state = vehicle::initial_state(*model);
    const double max_rad = tractor.steering.max_angle_rad;
    const auto with = [&](double steering) {
        AssistState assist;
        return easy({.steering = steering}, kEasy, assist, tractor, state, kDt_s);
    };
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
            hash.add(now.assist.engagement);
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
    EXPECT_EQ(hash, 0x31384392584fe305ULL);
    // 88.663808972753955 m, every bit pinned.
    EXPECT_EQ(result.distance_m, 0x1.62a7bd8a13141p+6);
    EXPECT_EQ(result.ending, run::Ending::kSpunOut);
}

}  // namespace
}  // namespace tp::controls
