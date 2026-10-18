#include "tp/core/run/run.hpp"

#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "../sled/reference_sled.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::run {
namespace {

constexpr double kDt_s = 0.001;
constexpr Rules kRules{.track_length_m = 100.0, .max_duration_s = 120.0};

pull::Model model_with(const sled::Params& sled) {
    const auto model = pull::Model::create({.tractor = vehicle::testing::reference_params(),
                                            .sled = sled,
                                            .soil = tire::testing::reference_soil()});
    if (!model) {
        std::abort();
    }
    return *model;
}

// A sled the reference tractor pulls the whole track: the push-down is out
// of reach.
sled::Params light_sled() {
    sled::Params sled = sled::testing::reference_params();
    sled.pushdown_start_m = 1000.0;
    return sled;
}

// The scripted driver: idle in neutral, select the gear, rev up with the
// clutch pressed, slip it in over 3 s (or dump it) and pull at full
// throttle; optionally back off at `lift_at_s`: throttle closed, clutch
// pressed.
struct Driver {
    int gear = 6;
    bool dump = false;
    double lift_at_s = 1e9;

    [[nodiscard]] vehicle::Input input(double t_s) const {
        vehicle::Input in{.throttle = t_s < 0.5 || t_s >= lift_at_s ? 0.0 : 1.0,
                          .clutch_pedal = 1.0,
                          .gear = t_s < 0.5 ? driveline::kNeutral : gear};
        if (t_s >= 3.0) {
            in.clutch_pedal = dump ? 0.0 : std::max(0.0, 1.0 - (t_s - 3.0) / 3.0);
        }
        if (t_s >= lift_at_s) {
            in.clutch_pedal = 1.0;
        }
        return in;
    }
};

struct Outcome {
    State state;
    Result result;
    double distance_when_engine_stopped_m = -1.0;
};

// The loop that drives a run always ends with a result.
Result result_of(const State& state) {
    if (!state.result) {
        std::abort();
    }
    return *state.result;
}

Outcome drive(const pull::Model& model, const Driver& driver, const Rules& rules = kRules) {
    Outcome outcome{.state = start(model), .result = {}};
    State& state = outcome.state;
    for (int i = 0; !state.result; ++i) {
        step(state, model, rules, driver.input(i * kDt_s), kDt_s);
        if (!state.pull.tractor.engine.running && outcome.distance_when_engine_stopped_m < 0.0) {
            outcome.distance_when_engine_stopped_m = state.pull.sled.distance_m;
        }
    }
    outcome.result = result_of(state);
    return outcome;
}

TEST(RunRules, RejectsNonsense) {
    EXPECT_EQ(validate(kRules), std::nullopt);
    EXPECT_EQ(validate({.track_length_m = 0.0, .max_duration_s = 1.0}),
              ParamError::kTrackLengthNotPositive);
    EXPECT_EQ(validate({.track_length_m = 100.0, .max_duration_s = 0.0}),
              ParamError::kMaxDurationNotPositive);
    EXPECT_EQ(validate({.track_length_m = std::nan(""), .max_duration_s = 1.0}),
              ParamError::kNotFinite);
}

// In its pulling gear the engine stalls after the push-down; the run ends
// where the sled comes to rest, a little beyond where the engine died.
TEST(Run, PullingGearEndsWithAStall) {
    const Outcome outcome = drive(model_with(sled::testing::reference_params()), {.gear = 6});
    const Result& result = outcome.result;
    EXPECT_EQ(result.ending, Ending::kStalled);
    EXPECT_GT(result.distance_m, sled::testing::reference_params().pushdown_start_m);
    EXPECT_LT(result.distance_m, kRules.track_length_m);
    EXPECT_GE(result.distance_m, outcome.distance_when_engine_stopped_m);
    EXPECT_EQ(result.distance_m, outcome.state.pull.sled.distance_m);
    EXPECT_EQ(outcome.state.pull.sled.velocity_m_s, 0.0);
}

TEST(Run, LowGearEndsSpinningOut) {
    const Outcome outcome = drive(model_with(sled::testing::reference_params()), {.gear = 4});
    EXPECT_EQ(outcome.result.ending, Ending::kSpunOut);
    EXPECT_TRUE(outcome.state.pull.tractor.engine.running);
}

// Lifting off mid-track: the sled drags to a halt with the engine running
// and the tires gripping, and that first stop ends the run.
TEST(Run, BackingOffEndsTheRunAtTheFirstStop) {
    const Outcome outcome =
        drive(model_with(sled::testing::reference_params()), {.gear = 6, .lift_at_s = 15.0});
    EXPECT_EQ(outcome.result.ending, Ending::kStopped);
    EXPECT_TRUE(outcome.state.pull.tractor.engine.running);
    EXPECT_LT(outcome.result.distance_m, 50.0);
}

// Waiting at the start line is not a stop: the run only ends once the sled
// has moved.
TEST(Run, WaitingAtTheStartIsNotAStop) {
    const pull::Model model = model_with(sled::testing::reference_params());
    State state = start(model);
    for (int i = 0; i < 5000; ++i) {
        step(state, model, kRules, {.throttle = 0.5}, kDt_s);
    }
    EXPECT_FALSE(state.result.has_value());
}

// A full pull ends at the end of the track; the pull-off sled stops the
// same tractor short of it.
TEST(Run, FullPullThenPullOffOnAHeavierSled) {
    sled::Params sled = light_sled();
    const Outcome full = drive(model_with(sled), {.gear = 6});
    EXPECT_EQ(full.result.ending, Ending::kFullPull);
    EXPECT_EQ(full.result.distance_m, kRules.track_length_m);

    sled.pull_off = {
        .extra_box_mass_kg = 1000.0, .pushdown_start_m = 80.0, .extra_pan_friction = 0.05};
    const Outcome pull_off = drive(model_with(sled::heavier(sled)), {.gear = 6});
    EXPECT_NE(pull_off.result.ending, Ending::kFullPull);
    EXPECT_LT(pull_off.result.distance_m, kRules.track_length_m);
}

TEST(Run, TimesOutAsASafetyNet) {
    const Outcome outcome = drive(model_with(sled::testing::reference_params()), {.gear = 4},
                                  {.track_length_m = 100.0, .max_duration_s = 10.0});
    EXPECT_EQ(outcome.result.ending, Ending::kTimedOut);
    EXPECT_EQ(outcome.result.time_s, 10.0);
}

// Once the run has ended nothing moves any more.
TEST(Run, NothingMovesAfterTheEnd) {
    const pull::Model model = model_with(sled::testing::reference_params());
    Outcome outcome = drive(model, {.gear = 6});
    State& state = outcome.state;
    const State frozen = state;
    for (int i = 0; i < 1000; ++i) {
        step(state, model, kRules, {.throttle = 1.0, .clutch_pedal = 0.0, .gear = 6}, kDt_s);
    }
    hash::Fnv1a before;
    hash_append(before, frozen);
    hash::Fnv1a after;
    hash_append(after, state);
    EXPECT_EQ(before.value(), after.value());
}

std::uint64_t run_hash(Result& result) {
    const pull::Model model = model_with(sled::testing::reference_params());
    const Driver driver{.gear = 6};
    State state = start(model);
    hash::Fnv1a hash;
    for (int i = 0; !state.result; ++i) {
        step(state, model, kRules, driver.input(i * kDt_s), kDt_s);
        hash_append(hash, state);
    }
    result = result_of(state);
    return hash.value();
}

// A replay: the same inputs give the same run, bit for bit, and the same
// result to the last digit, on every compiler, build type and platform.
TEST(Run, ReplayMatchesRegression) {
    Result result;
    const std::uint64_t hash = run_hash(result);
    Result again;
    EXPECT_EQ(run_hash(again), hash);
    EXPECT_EQ(again.distance_m, result.distance_m);
    EXPECT_EQ(hash, 0xdc70529f8bf1669cULL);
    // 71.75645655137582 m, every bit pinned.
    EXPECT_EQ(result.distance_m, 0x1.1f069c8bd4043p+6);
    EXPECT_EQ(result.ending, Ending::kStalled);
}

}  // namespace
}  // namespace tp::run
