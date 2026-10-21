// The simulation step must never touch the heap (CLAUDE.md): this test
// program replaces the global operator new with one that counts, and runs
// whole pulls through the step functions a front end calls every
// millisecond. Setup (models, loading) may allocate; the steps may not.

#include "tp/core/controls/controls.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/session/session.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>

#include "../controls/reference_controls.hpp"
#include "../rules/reference_class.hpp"
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
// go through the same allocator (the sanitizers check the pairs match). The
// aligned forms (std::align_val_t) are left alone, so an over-aligned
// allocation would not be counted; core/ has no over-aligned types.
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

// The game's own loop: a whole run through the session, frame by frame at
// 60 fps, snapshots and their blending included.
TEST(NoAllocation, AWholeSessionInFrames) {
    std::optional<session::Session> game =
        session::Session::create({.tractor = vehicle::testing::reference_params(),
                                  .sled = sled::testing::reference_params(),
                                  .ground = track::testing::reference_params(),
                                  .rules = kRules,
                                  .layout = session::Layout::kEasy,
                                  .easy = controls::testing::kReferenceEasy,
                                  .pro = controls::testing::kReferencePro});
    if (!game) {
        ADD_FAILURE() << "invalid reference session";
        return;
    }
    constexpr std::int64_t kFrame_ns = 16'666'667;
    constexpr int kMaxFrames = 20'000;
    double drawn_x_m = 0.0;
    const std::size_t counted = allocations_in([&] {
        for (int frame = 0; !game->result() && frame < kMaxFrames; ++frame) {
            game->advance({.throttle = 1.0, .gear = 6}, kFrame_ns);
            drawn_x_m = session::blend(game->previous(), game->current(), game->alpha()).sled_x_m;
        }
    });
    EXPECT_EQ(counted, 0U);
    EXPECT_TRUE(game->result().has_value());
    EXPECT_GT(drawn_x_m, 0.0);
}

// A game's event, frame by frame: the rivals choosing gears and pulling in
// slices, the operator, the decisions, and the player's pulls and choices
// handed in. Creating it may allocate; playing it may not.
TEST(NoAllocation, AnEventWithAPlayer) {
    constexpr int kPullers = 4;
    const field::Puller reference{
        .tractor = vehicle::testing::reference_params(),
        .driver = {.gear = 6, .select_gear_s = 0.5, .launch_s = 3.0, .clutch_slip_in_s = 3.0}};
    const field::Variation variation{.torque_scale = {.low = 0.95, .high = 1.03},
                                     .mass_kg = {.low = 3300.0, .high = 3550.0},
                                     .traction_limit = {.low = 0.90, .high = 0.95},
                                     .lowest_gear = 5,
                                     .highest_gear = 6,
                                     .clutch_slip_in_s = {.low = 2.0, .high = 4.0},
                                     .track_reading = {.low = 0.5, .high = 0.95}};
    const std::optional<field::Field> field =
        field::generate(reference, variation, rules::testing::ntto_standard_3500(), kPullers, 1);
    const event::Rules rules{.full_pull_m = 100.0,
                             .extended_full_pull_m = 110.0,
                             .max_distance_m = 130.0,
                             .pull_off_places = 2,
                             .qualifying_distance_m = 90.0,
                             .decide_after_pulls = 1,
                             .repull_after_pulls = 2,
                             .sled_target_m = 94.0,
                             .step_m = 5.0,
                             .max_steps_per_change = 4};
    const field::Venue venue{.track = track::testing::reference_params(),
                             .sled = sled::testing::reference_params(),
                             .max_duration_s = 120.0,
                             .lane_width_m = 12.0};
    const std::array<int, kPullers> order{2, 0, 3, 1};
    if (!field) {
        ADD_FAILURE() << "invalid reference field";
        return;
    }
    std::unique_ptr<field::LiveEvent> live;
    if (auto created = field::LiveEvent::create(rules, venue, *field, variation, {}, order, 2, 1)) {
        live = std::make_unique<field::LiveEvent>(*created);
    } else {
        ADD_FAILURE() << "invalid reference event";
        return;
    }
    constexpr std::int64_t kSlice = 5000;
    constexpr int kMaxTurns = 100'000;
    int player_pulls = 0;
    const std::size_t counted = allocations_in([&] {
        for (int i = 0; i < kMaxTurns; ++i) {
            switch (live->turn()) {
                case field::LiveEvent::Turn::kRivals:
                    live->advance(kSlice);
                    break;
                case field::LiveEvent::Turn::kPlayerPull:
                    ++player_pulls;
                    live->record_player({.ending = run::Ending::kStalled, .distance_m = 92.0}, 6);
                    break;
                case field::LiveEvent::Turn::kPlayerDecision:
                    live->decide_player(live->suggested_keep());
                    break;
                case field::LiveEvent::Turn::kFinished:
                case field::LiveEvent::Turn::kFailed:
                    return;
            }
        }
    });
    EXPECT_EQ(counted, 0U);
    EXPECT_EQ(live->turn(), field::LiveEvent::Turn::kFinished);
    EXPECT_GT(player_pulls, 0);
}

}  // namespace
}  // namespace tp
