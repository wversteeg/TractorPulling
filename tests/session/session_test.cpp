#include "tp/core/session/session.hpp"

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/math/random.hpp"
#include "tp/core/session/inspect.hpp"
#include "tp/core/units/constants.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "../controls/reference_controls.hpp"
#include "../sled/reference_sled.hpp"
#include "../track/reference_track.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::session {
namespace {

constexpr double kDt_s = 0.001;
constexpr std::int64_t kStep_ns = kSimulationStepNs;
constexpr run::Rules kRules{.track_length_m = 100.0,
                            .max_duration_s = 120.0,
                            .lane_width_m = 12.0,
                            .full_pull_line_m = 100.0};
// A run never lasts this long: the session's runs below all end first.
constexpr std::int64_t kLongerThanAnyRun_ns = 200 * kNanosecondsPerSecond;

Setup reference_setup(Layout layout) {
    return {.tractor = vehicle::testing::reference_params(),
            .sled = sled::testing::reference_params(),
            .ground = track::testing::reference_params(),
            .rules = kRules,
            .layout = layout,
            .easy = controls::testing::kReferenceEasy,
            .pro = controls::testing::kReferencePro};
}

Session make(const Setup& setup) {
    std::optional<Session> session = Session::create(setup);
    if (!session) {
        std::abort();
    }
    return *session;
}

// Advances `session` in frames of `frame_ns` with `controls` until its run
// has ended.
void run_to_the_end(Session& session, const Controls& controls, std::int64_t frame_ns) {
    for (std::int64_t t_ns = 0; !session.result() && t_ns < kLongerThanAnyRun_ns;
         t_ns += frame_ns) {
        session.advance(controls, frame_ns);
    }
}

// The pro driver: rev up in neutral, select 6th, slip the clutch in over
// 3 s, pull at full throttle with a steering correction on the way, left
// then right, past the dead zone.
Controls pro_script(double t_s) {
    Controls controls{.throttle = t_s < 0.5 ? 0.0 : 1.0,
                      .clutch_pedal = 1.0,
                      .gear = t_s < 0.5 ? driveline::kNeutral : 6};
    if (t_s >= 3.0) {
        controls.clutch_pedal = t_s < 6.0 ? 1.0 - (t_s - 3.0) / 3.0 : 0.0;
    }
    constexpr double kCorrection = 0.1;
    if (t_s > 7.0 && t_s < 7.5) {
        controls.steering = kCorrection;
    } else if (t_s >= 7.5 && t_s < 8.0) {
        controls.steering = -kCorrection;
    }
    return controls;
}

TEST(Session, ValidatesItsSetup) {
    EXPECT_FALSE(validate(reference_setup(Layout::kPro)));
    EXPECT_FALSE(validate(reference_setup(Layout::kEasy)));

    session::Setup setup = reference_setup(Layout::kPro);
    setup.tractor.chassis.mass_kg = -1.0;
    EXPECT_EQ(validate(setup), SetupError::kTractor);
    EXPECT_FALSE(Session::create(setup));

    setup = reference_setup(Layout::kPro);
    setup.sled.box_mass_kg = -1.0;
    EXPECT_EQ(validate(setup), SetupError::kSled);

    setup = reference_setup(Layout::kPro);
    setup.rules.lane_width_m = 0.0;
    EXPECT_EQ(validate(setup), SetupError::kRules);

    setup = reference_setup(Layout::kPro);
    setup.max_steps_per_frame = 0;
    EXPECT_EQ(validate(setup), SetupError::kMaxStepsPerFrame);
}

// The easy layout's parameters only matter when the easy layout is played.
TEST(Session, ChecksTheEasyLayoutOnlyWhenItIsPlayed) {
    session::Setup setup = reference_setup(Layout::kEasy);
    setup.easy.steer_share = 0.0;
    EXPECT_EQ(validate(setup), SetupError::kEasyControls);
    setup.layout = Layout::kPro;
    EXPECT_FALSE(validate(setup));
}

// And the pro layout's only when it is played.
TEST(Session, ChecksTheProLayoutOnlyWhenItIsPlayed) {
    session::Setup setup = reference_setup(Layout::kPro);
    setup.pro.steer_rate_per_s = 0.0;
    EXPECT_EQ(validate(setup), SetupError::kProControls);
    setup.layout = Layout::kEasy;
    EXPECT_FALSE(validate(setup));
}

// The session is the run, stepped as a front end would by hand: the same
// state after every step, bit for bit, the pro layout's steering included.
TEST(Session, IsTheRunSteppedByHand) {
    const session::Setup setup = reference_setup(Layout::kPro);
    Session session = make(setup);
    run::State by_hand = run::start(Inspector::model(session));
    controls::AssistState steering;
    double most_steering = 0.0;
    const vehicle::Params& tractor = Inspector::model(session).tractor().params();
    for (int i = 0; !by_hand.result; ++i) {
        const Controls controls = pro_script(i * kDt_s);
        const controls::ProInput pro{.throttle = controls.throttle,
                                     .clutch_pedal = controls.clutch_pedal,
                                     .gear = controls.gear,
                                     .steering = controls.steering};
        run::step(by_hand, Inspector::model(session), kRules,
                  controls::pro(pro, setup.pro, steering, tractor, kDt_s), kDt_s);
        session.advance(controls, kStep_ns);
        most_steering = std::max(most_steering, steering.steering);

        hash::Fnv1a expected;
        run::hash_append(expected, by_hand);
        controls::hash_append(expected, steering);
        ASSERT_EQ(session.hash(), expected.value()) << "step " << i;
    }
    EXPECT_GT(most_steering, 0.0);
    const std::optional<run::Result>& result = session.result();
    if (!result || !by_hand.result) {
        FAIL() << "the run did not end";
    }
    EXPECT_EQ(result->distance_m, by_hand.result->distance_m);
    EXPECT_EQ(result->ending, by_hand.result->ending);
}

// The easy layout: the session runs the clutch assist and the steering
// exactly as controls::easy does before each step. A steering correction
// past the dead zone on the way carries the steering's state too, and the
// tractor stays in its lane.
TEST(Session, RunsTheClutchAssistOfTheEasyLayout) {
    const session::Setup setup = reference_setup(Layout::kEasy);
    Session session = make(setup);
    run::State by_hand = run::start(Inspector::model(session));
    controls::AssistState assist;
    const vehicle::Params& tractor = Inspector::model(session).tractor().params();
    // A correction to the left, then back to the right, past the dead zone.
    const auto steering_at = [](std::uint64_t step) {
        constexpr std::uint64_t kLeftFrom = 5000;
        constexpr std::uint64_t kRightFrom = 5500;
        constexpr std::uint64_t kStraightFrom = 6000;
        constexpr double kCorrection = 0.1;
        if (step >= kLeftFrom && step < kRightFrom) {
            return kCorrection;
        }
        return step >= kRightFrom && step < kStraightFrom ? -kCorrection : 0.0;
    };
    double most_steering = 0.0;
    for (std::uint64_t step = 0; !by_hand.result; ++step) {
        const double steering = steering_at(step);
        const vehicle::Input input =
            controls::easy({.throttle = 1.0, .gear = 6, .steering = steering}, setup.easy, assist,
                           tractor, by_hand.pull.tractor, kDt_s);
        run::step(by_hand, Inspector::model(session), kRules, input, kDt_s);
        session.advance({.throttle = 1.0, .gear = 6, .steering = steering}, kStep_ns);
        most_steering = std::max(most_steering, assist.steering);
    }
    EXPECT_GT(most_steering, 0.0);
    hash::Fnv1a expected;
    run::hash_append(expected, by_hand);
    controls::hash_append(expected, assist);
    EXPECT_EQ(session.hash(), expected.value());
    const std::optional<run::Result>& result = session.result();
    if (!result) {
        FAIL() << "the run did not end";
    }
    EXPECT_GT(result->distance_m, 0.0);
}

// The frame time decides how many steps run, never their length: frames
// at 60 fps, 30 fps, faster than the step and exactly one step all give
// the same run.
TEST(Session, GivesTheSameRunHoweverTheTimeIsSplitIntoFrames) {
    // Steering past the dead zone, so the steering's state is carried too.
    const Controls pulling{.throttle = 1.0, .gear = 6, .steering = 0.1};
    constexpr std::array<std::int64_t, 4> kFrames_ns = {16'666'667, 33'333'333, 400'000, kStep_ns};
    std::optional<std::uint64_t> first_hash;
    std::optional<double> first_distance_m;
    for (const std::int64_t frame_ns : kFrames_ns) {
        Session session = make(reference_setup(Layout::kEasy));
        run_to_the_end(session, pulling, frame_ns);
        const std::optional<run::Result>& result = session.result();
        if (!result) {
            FAIL() << "the run did not end with " << frame_ns << " ns frames";
        }
        if (!first_hash || !first_distance_m) {
            first_hash = session.hash();
            first_distance_m = result->distance_m;
            continue;
        }
        EXPECT_EQ(session.hash(), *first_hash) << frame_ns << " ns frames";
        EXPECT_EQ(result->distance_m, *first_distance_m) << frame_ns << " ns frames";
    }
}

// A frame that took too long runs at most max_steps_per_frame steps and
// drops the rest of its time, so a slow frame never snowballs.
TEST(Session, CapsTheStepsOfOneFrame) {
    session::Setup setup = reference_setup(Layout::kPro);
    setup.max_steps_per_frame = 50;
    Session session = make(setup);
    const Frame frame = session.advance({}, kNanosecondsPerSecond);
    EXPECT_EQ(frame.steps, 50U);
    EXPECT_EQ(frame.dropped_ns, kNanosecondsPerSecond - 50 * kStep_ns);
    EXPECT_DOUBLE_EQ(session.current().time_s, 0.05);
}

TEST(Session, StartsAtTheStartLine) {
    const session::Setup setup = reference_setup(Layout::kPro);
    const Session session = make(setup);
    const Snapshot& start = session.current();
    EXPECT_EQ(start.time_s, 0.0);
    EXPECT_EQ(start.sled_x_m, 0.0);
    EXPECT_GT(start.tractor_x_m, setup.sled.chain.length_m);
    EXPECT_EQ(start.tractor_y_m, 0.0);
    EXPECT_EQ(start.gear, driveline::kNeutral);
    EXPECT_TRUE(start.engine_running);
    EXPECT_EQ(start.box_ahead_of_wheels_m, setup.sled.box_start_ahead_of_wheels_m);
    EXPECT_EQ(start.pushdown, 0.0);
    // The front tires carry their share of the weight at rest.
    const double weight_n = setup.tractor.chassis.mass_kg * units::kStandardGravity_m_s2;
    EXPECT_NEAR(start.front_axle_load_n,
                weight_n * (1.0 - chassis::rear_axle_share(setup.tractor.chassis)),
                1e-6 * weight_n);
    EXPECT_EQ(session.previous().time_s, start.time_s);
    EXPECT_FALSE(session.result());
}

// previous() and current() are one step apart, and a frame shorter than a
// step moves neither but alpha().
TEST(Session, KeepsTheLastTwoStepsToDrawBetween) {
    Session session = make(reference_setup(Layout::kEasy));
    const Controls pulling{.throttle = 1.0, .gear = 6};
    session.advance(pulling, 5 * kNanosecondsPerSecond);  // capped at 100 steps
    session.advance(pulling, 16 * kStep_ns);
    EXPECT_DOUBLE_EQ(session.current().time_s - session.previous().time_s, kDt_s);
    EXPECT_EQ(session.alpha(), 0.0);

    const Snapshot before = session.current();
    const Frame frame = session.advance(pulling, kStep_ns / 2);
    EXPECT_EQ(frame.steps, 0U);
    EXPECT_EQ(session.current().time_s, before.time_s);
    EXPECT_DOUBLE_EQ(session.alpha(), 0.5);
}

TEST(Session, ShowsTheTractorPullingTheSled) {
    Session session = make(reference_setup(Layout::kEasy));
    const double start_front_n = session.current().front_axle_load_n;
    const Controls pulling{.throttle = 1.0, .gear = 6};
    for (int frame = 0; frame < 600; ++frame) {
        session.advance(pulling, 16 * kStep_ns);
    }
    ASSERT_FALSE(session.result());
    const Snapshot& now = session.current();
    EXPECT_GT(now.sled_x_m, 1.0);
    EXPECT_GT(now.tractor_x_m, now.sled_x_m);
    EXPECT_GT(now.chain_tension_n, 0.0);
    EXPECT_GT(now.sled_speed_m_s, 0.0);
    EXPECT_EQ(now.gear, 6);
    EXPECT_EQ(now.throttle, 1.0);
    // The assist has the clutch fully in.
    EXPECT_EQ(now.clutch_pedal, 0.0);
    // Going straight, both sides' wheels turn alike, and faster than the
    // ground passes under them: the tires slip.
    EXPECT_EQ(now.rear_left_omega_rad_s, now.rear_right_omega_rad_s);
    const double radius_m = vehicle::testing::reference_params().rear_tire.size.radius_m;
    EXPECT_GT(now.rear_left_omega_rad_s * radius_m, now.tractor_speed_m_s);
    EXPECT_GT(now.slip_ratio, 0.0);
    // The pull takes weight off the front, never all of it.
    EXPECT_GT(now.front_axle_load_n, 0.0);
    EXPECT_LT(now.front_axle_load_n, start_front_n);
}

// The lowest gear the run takes now: any before moving off; under way one
// below the gear it moved off in (the reference powershift's step), then,
// once that step is used, the gear it went to.
TEST(Session, ShowsTheLowestGearTheRunTakes) {
    Session session = make(reference_setup(Layout::kEasy));
    EXPECT_EQ(session.current().lowest_gear, 1);
    const Controls pulling{.throttle = 1.0, .gear = 6};
    for (int frame = 0; frame < 600; ++frame) {
        session.advance(pulling, 16 * kStep_ns);
    }
    ASSERT_FALSE(session.result());
    EXPECT_EQ(session.current().gear, 6);
    EXPECT_EQ(session.current().lowest_gear, 5);

    Controls lower = pulling;
    lower.gear = 5;
    session.advance(lower, 16 * kStep_ns);
    ASSERT_FALSE(session.result());
    EXPECT_EQ(session.current().gear, 5);
    EXPECT_EQ(session.current().lowest_gear, 5);

    lower.gear = 4;
    for (int frame = 0; frame < 60; ++frame) {
        session.advance(lower, 16 * kStep_ns);
    }
    EXPECT_EQ(session.current().gear, 5);
    EXPECT_EQ(session.current().lowest_gear, 5);
}

TEST(Session, StandsStillOnceTheRunHasEnded) {
    Session session = make(reference_setup(Layout::kEasy));
    const Controls pulling{.throttle = 1.0, .gear = 6};
    run_to_the_end(session, pulling, 16 * kStep_ns);
    ASSERT_TRUE(session.result());
    const std::uint64_t hash = session.hash();
    const Snapshot last = session.current();
    // The pull ends past the push-down: set, the sled at its heaviest.
    EXPECT_EQ(last.pushdown, 1.0) << last.sled_x_m << " m";

    const Frame frame = session.advance(pulling, 16 * kStep_ns);
    EXPECT_EQ(frame.steps, 16U);
    EXPECT_EQ(session.hash(), hash);
    EXPECT_EQ(session.current().sled_x_m, last.sled_x_m);
    EXPECT_EQ(session.current().chain_tension_n, last.chain_tension_n);
    EXPECT_EQ(session.previous().sled_x_m, session.current().sled_x_m);
}

// Controls that are not a number (a NaN from GDScript maths) never reach
// the simulation: the run goes on exactly as with the controls at rest.
TEST(Session, ReadsNotANumberAsAtRest) {
    const double nan = std::nan("");
    const Controls bad{.throttle = nan,
                       .clutch_pedal = nan,
                       .gear = driveline::kNeutral,
                       .steering = nan,
                       .brake_left = nan,
                       .brake_right = nan};
    const Controls at_rest{.throttle = 0.0,
                           .clutch_pedal = 1.0,
                           .gear = driveline::kNeutral,
                           .steering = 0.0,
                           .brake_left = 0.0,
                           .brake_right = 0.0};
    for (const Layout layout : {Layout::kPro, Layout::kEasy}) {
        Session with_bad = make(reference_setup(layout));
        Session with_rest = make(reference_setup(layout));
        with_bad.advance(bad, kNanosecondsPerSecond / 10);
        with_rest.advance(at_rest, kNanosecondsPerSecond / 10);
        EXPECT_EQ(with_bad.hash(), with_rest.hash());
        EXPECT_TRUE(std::isfinite(with_bad.current().engine_omega_rad_s));
        EXPECT_EQ(with_bad.current().throttle, 0.0);
    }
}

TEST(Blend, GoesFromOneSnapshotToTheNext) {
    const Snapshot from{.time_s = 1.0, .tractor_x_m = 10.0, .gear = 5, .sled_x_m = 2.0};
    const Snapshot to{.time_s = 1.001, .tractor_x_m = 10.01, .gear = 6, .sled_x_m = 2.004};
    EXPECT_EQ(blend(from, to, 0.0).tractor_x_m, from.tractor_x_m);
    EXPECT_EQ(blend(from, to, 1.0).tractor_x_m, to.tractor_x_m);
    const Snapshot half = blend(from, to, 0.5);
    EXPECT_DOUBLE_EQ(half.tractor_x_m, 10.005);
    EXPECT_DOUBLE_EQ(half.sled_x_m, 2.002);
    EXPECT_DOUBLE_EQ(half.time_s, 1.0005);
    // A gear is in or it is not.
    EXPECT_EQ(half.gear, 6);
}

// At its ends the blend is the snapshot itself, exactly, for any values,
// and in between it never leaves the span between them.
TEST(Blend, LandsExactlyOnBothEnds) {
    constexpr int kPairs = 1000;
    constexpr double kFar_m = 50.0;
    constexpr std::uint64_t kSeed = 19;
    math::SplitMix64 random{kSeed};
    for (int i = 0; i < kPairs; ++i) {
        const Snapshot from{.tractor_x_m = random.uniform(-kFar_m, kFar_m)};
        const Snapshot to{.tractor_x_m = random.uniform(-kFar_m, kFar_m)};
        ASSERT_EQ(blend(from, to, 0.0).tractor_x_m, from.tractor_x_m) << i;
        ASSERT_EQ(blend(from, to, 1.0).tractor_x_m, to.tractor_x_m) << i;
        const double middle_m = blend(from, to, 0.5).tractor_x_m;
        ASSERT_GE(middle_m, std::min(from.tractor_x_m, to.tractor_x_m)) << i;
        ASSERT_LE(middle_m, std::max(from.tractor_x_m, to.tractor_x_m)) << i;
    }
}

}  // namespace
}  // namespace tp::session
