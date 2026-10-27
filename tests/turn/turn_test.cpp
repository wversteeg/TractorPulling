#include "tp/core/turn/turn.hpp"

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/session/inspect.hpp"
#include "tp/core/session/session.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "../controls/reference_controls.hpp"
#include "../sled/reference_sled.hpp"
#include "../track/reference_track.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::turn {
namespace {

constexpr double kDt_s = 0.001;
constexpr run::Rules kRules{.track_length_m = 100.0,
                            .max_duration_s = 120.0,
                            .lane_width_m = 12.0,
                            .full_pull_line_m = 100.0};
// Behind the start line, beside the lane on its right.
constexpr Parking kParking{.x_m = -15.0, .y_m = -10.0};

pull::Model reference_model() {
    const auto model = pull::Model::create({.tractor = vehicle::testing::reference_params(),
                                            .sled = sled::testing::reference_params(),
                                            .track = track::testing::reference_params()});
    if (!model) {
        std::abort();
    }
    return *model;
}

// Moves the tractor, as a crane would, so its hitch stands `along_m` ahead
// of the sled's hook and `across_m` to its left (the sled is straight).
void place_hitch(const pull::Model& model, pull::State& state, double along_m, double across_m) {
    const HitchToHook now = hitch_to_hook(model, state);
    state.tractor.chassis.position_m += along_m - now.along_m;
    state.tractor.chassis.lateral_position_m += across_m - now.across_m;
}

// The whole tractor beside the lane, out to its tires' outer edges.
TEST(Parking, MustBeBesideTheTrack) {
    constexpr double kHalfWidth_m = 1.2;
    EXPECT_FALSE(validate(kParking, kRules, kHalfWidth_m));
    EXPECT_FALSE(validate({.x_m = 0.0, .y_m = 7.2}, kRules, kHalfWidth_m));
    EXPECT_EQ(validate({.x_m = 0.0, .y_m = 7.1}, kRules, kHalfWidth_m), ParamError::kInTheLane);
    EXPECT_EQ(validate({.x_m = 0.0, .y_m = -5.9}, kRules, 0.0), ParamError::kInTheLane);
    EXPECT_EQ(validate({.x_m = std::nan(""), .y_m = -10.0}, kRules, kHalfWidth_m),
              ParamError::kNotFinite);
}

TEST(Turn, StartsInTheParking) {
    const pull::Model model = reference_model();
    const State state = from_the_parking(model, kParking);
    EXPECT_EQ(state.stage, Stage::kToTheSled);
    EXPECT_TRUE(state.from_the_parking);
    EXPECT_FALSE(state.run.pull.hooked);
    EXPECT_EQ(state.run.pull.sled_mode, pull::SledMode::kHeld);
    const chassis::State& body = state.run.pull.tractor.chassis;
    EXPECT_EQ(body.position_m - model.start_line_m(), kParking.x_m);
    EXPECT_EQ(body.lateral_position_m, kParking.y_m);
    EXPECT_EQ(state.run.pull.tractor.driveline.gear, driveline::kNeutral);
    EXPECT_TRUE(state.run.pull.tractor.engine.running);
    // The sled stands at the start line, as for a pull from the line.
    EXPECT_EQ(state.run.pull.sled.position_m, model.start_line_m());
}

// The chain hooks on with the tractor standing, its hitch ahead of the
// hook within the chain's length, near the sled's line and straight with
// it; anywhere else it does not.
TEST(Turn, HooksOnInFrontOfTheSled) {
    const pull::Model model = reference_model();
    const double chain_m = model.sled().chain.length_m;
    const auto hooks_at = [&](double along_m, double across_m, double turned_rad,
                              double speed_m_s) {
        State state = from_the_parking(model, kParking);
        state.run.pull.tractor.chassis.heading_rad = turned_rad;
        place_hitch(model, state.run.pull, along_m, across_m);
        state.run.pull.tractor.chassis.velocity_m_s = speed_m_s;
        return hooks_on(model, state.run.pull);
    };
    EXPECT_TRUE(hooks_at(0.8 * chain_m, 0.0, 0.0, 0.0));
    EXPECT_TRUE(hooks_at(0.2, 0.9 * kHookOffLine_m, 0.9 * kHookTurned_rad, -0.9 * kAtRest_m_s));
    EXPECT_FALSE(hooks_at(1.1 * chain_m, 0.0, 0.0, 0.0)) << "out of the chain's reach";
    EXPECT_FALSE(hooks_at(-0.2, 0.0, 0.0, 0.0)) << "behind the hook";
    EXPECT_FALSE(hooks_at(0.8 * chain_m, 1.1 * kHookOffLine_m, 0.0, 0.0)) << "off the line";
    EXPECT_FALSE(hooks_at(0.8 * chain_m, 0.0, 1.1 * kHookTurned_rad, 0.0)) << "turned";
    EXPECT_FALSE(hooks_at(0.8 * chain_m, 0.0, 0.0, -2.0 * kAtRest_m_s)) << "still moving";

    // Stepping hooks it on and moves on to taking up the slack.
    State state = from_the_parking(model, kParking);
    place_hitch(model, state.run.pull, 0.8 * chain_m, 0.0);
    step(state, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(state.stage, Stage::kTakingUpSlack);
    EXPECT_TRUE(state.run.pull.hooked);
}

// Hooked, the tractor creeps forward on the held sled; the official waves
// the green flag once the chain has held its tension for a second. The
// sled has not moved, and the pull's clock starts at the flag.
TEST(Turn, TheFlagGoesGreenOnceTheChainHolds) {
    const pull::Model model = reference_model();
    State state = from_the_parking(model, kParking);
    place_hitch(model, state.run.pull, 0.8 * model.sled().chain.length_m, 0.0);
    step(state, model, kRules, kParking, {}, kDt_s);
    ASSERT_EQ(state.stage, Stage::kTakingUpSlack);

    std::uint64_t taken_up_steps = 0;
    std::uint64_t steps = 0;
    for (; state.stage == Stage::kTakingUpSlack && steps < 20000; ++steps) {
        const double t_s = static_cast<double>(steps) * kDt_s;
        const vehicle::Input creep{.throttle = 0.3,
                                   .clutch_pedal = std::clamp(1.0 - 0.3 * t_s, 0.5, 1.0),
                                   .gear = t_s < 0.1 ? driveline::kNeutral : 4};
        const pull::Output out = step(state, model, kRules, kParking, creep, kDt_s);
        taken_up_steps = out.chain.tension_n >= kTakeUpTension_n ? taken_up_steps + 1 : 0;
        ASSERT_EQ(state.run.pull.sled.position_m, model.start_line_m());
        ASSERT_EQ(state.run.steps, 0U) << "no pull clock before the flag";
    }
    ASSERT_EQ(state.stage, Stage::kPulling);
    EXPECT_EQ(state.run.pull.sled_mode, pull::SledMode::kOnTheChain);
    EXPECT_EQ(static_cast<double>(taken_up_steps), std::round(kFlagAfter_s / kDt_s));
    EXPECT_FALSE(state.run.result);
}

// A pull that ends with the sled still rolling (here a full pull on a
// short track) leaves it on the chain until it has stopped; then it is
// held, standing still for good, its speeds all 0.
TEST(Turn, ASledStillRollingAfterThePullStopsBeforeItIsHeld) {
    const pull::Model model = reference_model();
    run::Rules short_track = kRules;
    short_track.track_length_m = 5.0;
    short_track.full_pull_line_m = 5.0;
    State state = from_the_parking(model, kParking);
    place_hitch(model, state.run.pull, 0.8 * model.sled().chain.length_m, 0.0);
    step(state, model, short_track, kParking, {}, kDt_s);
    ASSERT_EQ(state.stage, Stage::kTakingUpSlack);
    // The slack taken up in 4th, then full throttle until the full pull.
    for (int i = 0; state.stage != Stage::kReleasing && i < 60000; ++i) {
        const double t_s = i * kDt_s;
        const bool pulling = state.stage == Stage::kPulling;
        const vehicle::Input in{
            .throttle = pulling ? 1.0 : 0.3,
            .clutch_pedal = pulling ? 0.0 : std::clamp(1.0 - 0.3 * t_s, 0.5, 1.0),
            .gear = t_s < 0.1 ? driveline::kNeutral : 4};
        step(state, model, short_track, kParking, in, kDt_s);
    }
    ASSERT_EQ(state.stage, Stage::kReleasing);
    ASSERT_TRUE(state.run.result);
    EXPECT_EQ(state.run.result.value_or(run::Result{}).ending, run::Ending::kFullPull);
    EXPECT_GT(state.run.pull.sled.velocity_m_s, 0.0) << "still rolling at the result";
    EXPECT_EQ(state.run.pull.sled_mode, pull::SledMode::kOnTheChain);

    // Throttle closed, the clutch pressed: it rolls to a stop on the chain,
    // and only then is held (here and unhooked at once: the tractor stands
    // with the chain slack).
    const vehicle::Input stopping{
        .clutch_pedal = 1.0, .gear = 4, .brake_left = 1.0, .brake_right = 1.0};
    int steps = 0;
    while (state.run.pull.sled_mode == pull::SledMode::kOnTheChain && steps < 30000) {
        step(state, model, short_track, kParking, stopping, kDt_s);
        ++steps;
    }
    EXPECT_NE(state.run.pull.sled_mode, pull::SledMode::kOnTheChain);
    EXPECT_EQ(state.run.pull.sled.velocity_m_s, 0.0) << "it stood when it was held";
}

// The tension must hold without a break: a slack moment starts the count
// again.
TEST(Turn, ASlackChainStartsTheFlagCountAgain) {
    const pull::Model model = reference_model();
    State state = from_the_parking(model, kParking);
    state.stage = Stage::kTakingUpSlack;
    state.run.pull.hooked = true;
    state.tension_held_steps = 900;
    place_hitch(model, state.run.pull, 0.5 * model.sled().chain.length_m, 0.0);
    step(state, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(state.tension_held_steps, 0U);
    EXPECT_EQ(state.stage, Stage::kTakingUpSlack);
}

// After the pull the chain comes off once it is slack and the tractor
// stands: the sled then drives itself back.
TEST(Turn, TheChainComesOffOnceSlack) {
    const pull::Model model = reference_model();
    State state = from_the_parking(model, kParking);
    state.stage = Stage::kReleasing;
    state.run.pull.hooked = true;
    ASSERT_EQ(state.run.pull.sled_mode, pull::SledMode::kHeld);
    state.run.result = run::Result{.ending = run::Ending::kStopped, .distance_m = 40.0};
    const double chain_m = model.sled().chain.length_m;
    place_hitch(model, state.run.pull, chain_m - 0.5 * kReleaseSlack_m, 0.0);
    step(state, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(state.stage, Stage::kReleasing) << "not slack enough";
    place_hitch(model, state.run.pull, chain_m - 2.0 * kReleaseSlack_m, 0.0);
    step(state, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(state.stage, Stage::kToTheParking);
    EXPECT_FALSE(state.run.pull.hooked);
    EXPECT_EQ(state.run.pull.sled_mode, pull::SledMode::kDrivingBack);
}

// Back in the parking, standing: the turn is over.
TEST(Turn, OverOnceParked) {
    const pull::Model model = reference_model();
    State state = from_the_parking(model, kParking);
    state.stage = Stage::kToTheParking;
    state.run.pull.tractor.chassis.lateral_position_m += 0.5 * kParkedWithin_m;
    step(state, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(state.stage, Stage::kParked);

    State away = from_the_parking(model, kParking);
    away.stage = Stage::kToTheParking;
    away.run.pull.tractor.chassis.lateral_position_m += 1.5 * kParkedWithin_m;
    step(away, model, kRules, kParking, {}, kDt_s);
    EXPECT_EQ(away.stage, Stage::kToTheParking);
}

// Off the pull a stalled engine starts again once the clutch is pressed.
TEST(Turn, AStalledEngineStartsAgainOffThePull) {
    const pull::Model model = reference_model();
    State state = from_the_parking(model, kParking);
    state.run.pull.tractor.engine.running = false;
    state.run.pull.tractor.engine.omega_rad_s = 0.0;
    state.run.pull.tractor.driveline.gear = 2;
    step(state, model, kRules, kParking, {.clutch_pedal = 0.0, .gear = 2}, kDt_s);
    EXPECT_FALSE(state.run.pull.tractor.engine.running) << "the clutch is in";
    step(state, model, kRules, kParking, {.clutch_pedal = 1.0, .gear = 2}, kDt_s);
    EXPECT_TRUE(state.run.pull.tractor.engine.running);
}

// ---- A whole turn, played through the session

struct Point {
    double x_m;
    double y_m;
};

// A scripted driver in the pro layout: follows a path of points by pure
// pursuit, forwards or in reverse, at a crawl; stops with the clutch
// pressed and both brakes on.
class Driver {
public:
    explicit Driver(const session::Snapshot& now) : now_(&now) {}

    // Steering towards the path, and whether its end is reached.
    session::Controls follow(std::span<const Point> path, int gear, bool reverse) {
        const double heading_rad = now_->tractor_heading_rad + (reverse ? std::numbers::pi : 0.0);
        if (path.empty()) {
            return stop(gear);
        }
        // A shorter path than the last one: its last point at most.
        const std::size_t last = path.size() - 1;
        std::size_t next = std::min(next_, last);
        while (next < last && distance_m(path[next]) < kLookAhead_m) {
            ++next;
        }
        next_ = next;
        const Point target = path[next];
        const double bearing_rad =
            std::atan2(target.y_m - now_->tractor_y_m, target.x_m - now_->tractor_x_m);
        const double alpha_rad = std::remainder(bearing_rad - heading_rad, 2.0 * std::numbers::pi);
        const double wheels_rad =
            std::atan(2.0 * kWheelbase_m * std::sin(alpha_rad) / kLookAhead_m);
        const double steering =
            std::clamp((reverse ? -wheels_rad : wheels_rad) / kMaxSteer_rad, -1.0, 1.0);
        ramp_ = std::min(ramp_ + kDt_s, 1.0);
        return {.throttle = 0.35, .clutch_pedal = 1.0 - ramp_, .gear = gear, .steering = steering};
    }

    [[nodiscard]] bool at_end(std::span<const Point> path) const {
        return next_ + 1 == path.size() && distance_m(path.back()) < kArrived_m;
    }

    // Clutch pressed, brakes on; ready for the next path.
    session::Controls stop(int gear) {
        next_ = 0;
        ramp_ = 0.0;
        return {.clutch_pedal = 1.0, .gear = gear, .brake_left = 1.0, .brake_right = 1.0};
    }

private:
    static constexpr double kLookAhead_m = 4.0;
    static constexpr double kArrived_m = 1.0;
    static constexpr double kWheelbase_m = 2.4;
    static constexpr double kMaxSteer_rad = 40.0 * std::numbers::pi / 180.0;

    [[nodiscard]] double distance_m(const Point& p) const {
        return std::hypot(p.x_m - now_->tractor_x_m, p.y_m - now_->tractor_y_m);
    }

    const session::Snapshot* now_;
    std::size_t next_ = 0;
    double ramp_ = 0.0;
};

// What a whole turn did.
struct Played {
    std::vector<Stage> stages;
    std::optional<run::Result> result;
    session::Snapshot end;
    bool hooked_while_pulling_off = false;
    std::uint64_t hash = 0;
};

Played play_a_whole_turn() {
    const session::Setup setup{.tractor = vehicle::testing::reference_params(),
                               .sled = sled::testing::reference_params(),
                               .ground = track::testing::reference_params(),
                               .rules = kRules,
                               .layout = session::Layout::kPro,
                               .easy = controls::testing::kReferenceEasy,
                               .pro = controls::testing::kReferencePro,
                               .parking = kParking};
    std::optional<session::Session> made = session::Session::create(setup);
    if (!made) {
        std::abort();
    }
    session::Session& session = *made;
    const session::Snapshot& now = session.current();
    Driver driver{now};
    Played played;
    const double chain_m = setup.sled.chain.length_m;

    // Out of the parking, along the lane's right and into it, past the
    // sled; then straight back to it.
    const std::array to_the_lane{Point{0.0, -10.0}, Point{10.0, -8.0}, Point{18.0, -3.0},
                                 Point{24.0, 0.0}, Point{32.0, 0.0}};
    const std::array back_to_the_sled{Point{20.0, 0.0}, Point{-20.0, 0.0}};
    bool backing = false;
    // The pull: the slack taken up in 6th, then full throttle.
    double pull_clutch = 1.0;
    // Home: round to the right and back along the lane's right.
    std::vector<Point> home;
    bool released = false;

    for (int step = 0; step < 600000 && !(session.over() && now.sled_back); ++step) {
        if (played.stages.empty() || played.stages.back() != now.stage) {
            played.stages.push_back(now.stage);
        }
        session::Controls controls;
        switch (now.stage) {
            case Stage::kToTheSled:
                if (!backing) {
                    controls = driver.follow(to_the_lane, 2, false);
                    if (driver.at_end(to_the_lane) || now.tractor_x_m > 32.0) {
                        backing = true;
                        controls = driver.stop(2);
                    }
                } else if (std::abs(now.tractor_speed_m_s) > 0.01 && now.gear != -1) {
                    controls = driver.stop(driveline::kNeutral);
                } else if (now.hitch_along_m < 0.75 * chain_m) {
                    controls = driver.stop(driveline::kReverse);
                } else {
                    controls = driver.follow(back_to_the_sled, driveline::kReverse, true);
                }
                break;
            case Stage::kTakingUpSlack:
                pull_clutch = std::max(pull_clutch - 0.4 * kDt_s, 0.55);
                controls = {.throttle = 0.4, .clutch_pedal = pull_clutch, .gear = 6};
                break;
            case Stage::kPulling:
                pull_clutch = std::max(pull_clutch - 0.5 * kDt_s, 0.0);
                controls = {.throttle = 1.0, .clutch_pedal = pull_clutch, .gear = 6};
                break;
            case Stage::kReleasing:
                // Stand, then reverse a little; it comes off by itself.
                if (now.gear != driveline::kReverse) {
                    controls =
                        driver.stop(std::abs(now.tractor_speed_m_s) > 0.01 ? driveline::kNeutral
                                                                           : driveline::kReverse);
                } else {
                    const std::array straight_back{Point{now.tractor_x_m - 10.0, 0.0}};
                    controls = driver.follow(straight_back, driveline::kReverse, true);
                    if (now.hitch_to_hook_m < chain_m - 2.0 * kReleaseSlack_m) {
                        controls = driver.stop(driveline::kReverse);
                    }
                }
                break;
            case Stage::kToTheParking: {
                if (!released) {
                    released = true;
                    const double x0 = now.tractor_x_m;
                    for (int i = 0; i <= 8; ++i) {
                        const double a = std::numbers::pi * i / 8.0;
                        home.push_back({x0 + 3.0 + 5.0 * std::sin(a), -5.0 + 5.0 * std::cos(a)});
                    }
                    home.push_back({0.0, -10.0});
                    home.push_back({kParking.x_m, kParking.y_m});
                }
                const bool rolling_back = now.gear != 2 && std::abs(now.tractor_speed_m_s) > 0.01;
                const bool home_now =
                    std::hypot(now.tractor_x_m - kParking.x_m, now.tractor_y_m - kParking.y_m) <
                    0.5 * kParkedWithin_m;
                if (rolling_back || home_now) {
                    controls = driver.stop(driveline::kNeutral);
                } else {
                    controls = driver.follow(home, 2, false);
                }
                break;
            }
            case Stage::kParked:
                controls = driver.stop(driveline::kNeutral);
                break;
        }
        session.advance(controls, kSimulationStepNs);
        played.hooked_while_pulling_off = played.hooked_while_pulling_off ||
                                          (now.stage == Stage::kToTheParking && now.chain_hooked);
    }
    if (played.stages.back() != now.stage) {
        played.stages.push_back(now.stage);
    }
    played.result = session.result();
    played.end = now;
    played.hash = session.hash();
    return played;
}

// The whole turn as the game plays it, every stage in order: out of the
// parking, backing up to the sled, the slack taken up, the flag, the pull,
// the chain off, and back to the parking as the sled drives itself back.
TEST(Turn, AWholeTurnFromTheParkingAndBack) {
    const Played played = play_a_whole_turn();
    const std::vector<Stage> in_order{Stage::kToTheSled, Stage::kTakingUpSlack, Stage::kPulling,
                                      Stage::kReleasing, Stage::kToTheParking,  Stage::kParked};
    EXPECT_EQ(played.stages, in_order);
    if (!played.result) {
        FAIL() << "the pull did not end";
    }
    const run::Result& result = *played.result;
    // Driving to the sled and away crosses the chalk lines freely: only the
    // pull counts them.
    EXPECT_NE(result.ending, run::Ending::kOutOfBounds);
    EXPECT_GT(result.distance_m, 20.0);
    EXPECT_EQ(played.end.sled_x_m, 0.0);
    EXPECT_EQ(played.end.sled_y_m, 0.0);
    EXPECT_TRUE(played.end.sled_back);
    EXPECT_FALSE(played.end.chain_hooked);
    EXPECT_FALSE(played.hooked_while_pulling_off);
    EXPECT_LT(
        std::hypot(played.end.tractor_x_m - kParking.x_m, played.end.tractor_y_m - kParking.y_m),
        kParkedWithin_m);
    // The pull's clock ran from the flag to the sled's stop only.
    EXPECT_EQ(played.end.time_s, result.time_s);
}

TEST(Turn, AWholeTurnIsDeterministic) {
    EXPECT_EQ(play_a_whole_turn().hash, play_a_whole_turn().hash);
}

}  // namespace
}  // namespace tp::turn
