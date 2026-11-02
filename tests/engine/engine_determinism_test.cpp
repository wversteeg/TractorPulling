// Determinism of the engine step: identical inputs must give bit-identical
// state on every compiler, build type and platform.

#include "tp/core/engine/engine.hpp"
#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <cstdint>

#include "reference_engine.hpp"

namespace tp::engine {
namespace {

constexpr double kDt_s = 0.001;
constexpr int kSteps = 20'000;

// A 20 s scripted run: rev up, spool the turbo, lug under a load ramp, back
// off, and finally overload into a stall. Inputs are piecewise linear so the
// script itself uses no math library functions.
Input scripted_input(int step_index) {
    const double t_s = step_index * kDt_s;
    if (t_s < 1.0) {
        return {.throttle = 0.0};
    }
    if (t_s < 4.0) {
        return {.throttle = (t_s - 1.0) / 3.0};
    }
    if (t_s < 12.0) {
        return {.throttle = 1.0, .load_torque_nm = 45.0 * (t_s - 4.0)};
    }
    if (t_s < 15.0) {
        return {.throttle = 0.6, .load_torque_nm = 100.0};
    }
    return {.throttle = 1.0, .load_torque_nm = 600.0};
}

std::uint64_t run_script(const Params& params) {
    State state = initial_state(params);
    hash::Fnv1a hash;
    for (int i = 0; i < kSteps; ++i) {
        step(state, params, scripted_input(i), kDt_s);
        hash_append(hash, state);
    }
    return hash.value();
}

TEST(EngineDeterminism, IdenticalInputsGiveIdenticalHash) {
    const Params params = testing::reference_params();
    EXPECT_EQ(run_script(params), run_script(params));
}

TEST(EngineDeterminism, DifferentInputsGiveDifferentHash) {
    const Params params = testing::reference_params();
    Params heavier = params;
    heavier.inertia_kg_m2 *= 1.0 + 1e-12;
    EXPECT_NE(run_script(params), run_script(heavier));
}

// Regression hash. It must be the same for gcc and clang, debug and release,
// x86-64 and ARM. If an intended model change alters it, update the value in
// the same commit and say so in the commit message.
TEST(EngineDeterminism, MatchesRegressionHash) {
    EXPECT_EQ(run_script(testing::reference_params()), 0xffd881e048122c98ULL);
}

}  // namespace
}  // namespace tp::engine
