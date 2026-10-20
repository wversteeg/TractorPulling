// The simulation step must never touch the heap (CLAUDE.md): this test
// program replaces the global operator new with one that counts, and runs
// whole pulls through the step functions a front end calls every
// millisecond. Setup (models, loading) may allocate; the steps may not.

#include "tp/core/controls/controls.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <optional>

#include "../controls/reference_controls.hpp"
#include "../sled/reference_sled.hpp"
#include "../track/reference_track.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace {

// Only allocations made while counting is on, on this thread, count.
thread_local bool counting = false;
thread_local std::size_t allocations = 0;

// A replacement operator new has to get its memory from malloc.
// NOLINTBEGIN(cppcoreguidelines-no-malloc)
void* allocate(std::size_t size) noexcept {
    if (counting) {
        ++allocations;
    }
    return std::malloc(size == 0 ? 1 : size);
}

void* allocate_or_throw(std::size_t size) {
    void* memory = allocate(size);
    if (memory == nullptr) {
        throw std::bad_alloc{};
    }
    return memory;
}

void release(void* memory) noexcept {
    std::free(memory);
}
// NOLINTEND(cppcoreguidelines-no-malloc)

}  // namespace

// Every unaligned new and delete, the nothrow forms too, so that all of them
// go through the same allocator (the sanitizers check the pairs match).
void* operator new(std::size_t size) {
    return allocate_or_throw(size);
}
void* operator new[](std::size_t size) {
    return allocate_or_throw(size);
}
void* operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
    return allocate(size);
}
void* operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
    return allocate(size);
}
void operator delete(void* memory) noexcept {
    release(memory);
}
void operator delete[](void* memory) noexcept {
    release(memory);
}
void operator delete(void* memory, std::size_t /*size*/) noexcept {
    release(memory);
}
void operator delete[](void* memory, std::size_t /*size*/) noexcept {
    release(memory);
}
void operator delete(void* memory, const std::nothrow_t& /*tag*/) noexcept {
    release(memory);
}
void operator delete[](void* memory, const std::nothrow_t& /*tag*/) noexcept {
    release(memory);
}

namespace tp {
namespace {

constexpr double kDt_s = 0.001;
constexpr run::Rules kRules{.track_length_m = 100.0,
                            .max_duration_s = 120.0,
                            .lane_width_m = 12.0,
                            .full_pull_line_m = 100.0};

// Counts the heap allocations `steps` makes.
template <typename Steps>
std::size_t allocations_in(const Steps& steps) {
    allocations = 0;
    counting = true;
    steps();
    counting = false;
    return allocations;
}

std::optional<pull::Model> reference_model() {
    return pull::Model::create({.tractor = vehicle::testing::reference_params(),
                                .sled = sled::testing::reference_params(),
                                .track = track::testing::reference_params()});
}

// The counter sees allocations: otherwise a pass below would prove nothing.
TEST(NoAllocation, TheCounterCounts) {
    // A direct call to operator new, which the compiler may not elide as it
    // may a new-expression.
    constexpr std::size_t kSome_bytes = 16;
    const std::size_t counted =
        allocations_in([] { ::operator delete(::operator new(kSome_bytes)); });
    EXPECT_EQ(counted, 1U);
}

// A whole run, the easy layout's clutch assist included, from the launch
// to the sled's stop after the push-down.
TEST(NoAllocation, AWholeEasyRun) {
    const std::optional<pull::Model> model = reference_model();
    if (!model) {
        ADD_FAILURE() << "invalid reference pull";
        return;
    }
    const vehicle::Params& tractor = model->tractor().params();
    run::State state = run::start(*model);
    controls::AssistState assist;
    const std::size_t counted = allocations_in([&] {
        for (int i = 0; !state.result; ++i) {
            const controls::EasyInput in{.throttle = i < 500 ? 0.0 : 1.0, .gear = 6};
            const vehicle::Input input = controls::easy(in, controls::testing::kReferenceEasy,
                                                        assist, tractor, state.pull.tractor, kDt_s);
            run::step(state, *model, kRules, input, kDt_s);
        }
    });
    EXPECT_EQ(counted, 0U);
    EXPECT_TRUE(state.result.has_value());
}

// The pro layout, steering and braking on the way: the differential and
// brake solves and the sled sideways run too.
TEST(NoAllocation, AProRunThatSteersAndBrakes) {
    const std::optional<pull::Model> model = reference_model();
    if (!model) {
        ADD_FAILURE() << "invalid reference pull";
        return;
    }
    const vehicle::Params& tractor = model->tractor().params();
    run::State state = run::start(*model);
    const std::size_t counted = allocations_in([&] {
        for (int i = 0; !state.result; ++i) {
            const double t_s = i * kDt_s;
            const controls::ProInput in{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                                        .clutch_pedal = t_s < 3.0 ? 1.0 : 0.0,
                                        .gear = t_s < 0.5 ? driveline::kNeutral : 6,
                                        .steering = t_s > 5.0 && t_s < 7.0 ? 0.01 : 0.0,
                                        .brake_left = t_s > 8.0 && t_s < 9.0 ? 0.2 : 0.0,
                                        .brake_right = 0.0};
            run::step(state, *model, kRules, controls::pro(in, tractor), kDt_s);
        }
    });
    EXPECT_EQ(counted, 0U);
    EXPECT_TRUE(state.result.has_value());
}

}  // namespace
}  // namespace tp
