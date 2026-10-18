#include "tp/core/event/event.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <limits>
#include <vector>

#include "../sled/reference_sled.hpp"

namespace tp::event {
namespace {

// The NTTO's rules for a standard class, and a 5 m rule of thumb.
constexpr Rules kRules{.full_pull_m = 100.0,
                       .extended_full_pull_m = 110.0,
                       .max_distance_m = 130.0,
                       .pull_off_places = 4,
                       .qualifying_distance_m = 90.0,
                       .decide_after_pulls = 2,
                       .repull_after_pulls = 5,
                       .sled_target_m = 90.0,
                       .step_m = 5.0};

std::vector<int> in_order(int count) {
    std::vector<int> order(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        order[static_cast<std::size_t>(i)] = i;
    }
    return order;
}

Event make(int count, const sled::Params& sled = sled::testing::reference_params()) {
    const std::vector<int> order = in_order(count);
    const auto event = Event::create(kRules, sled, order);
    if (!event) {
        std::abort();
    }
    return *event;
}

Pull next_of(const Event& event) {
    const std::optional<Pull> pull = event.next();
    if (!pull) {
        std::abort();
    }
    return *pull;
}

// Runs the event to its end: each pull reaches what `reach` says, and test
// pullers decide with `keep` (the suggested choice by default). Returns the
// pulls made, in order.
std::vector<Pull> run(Event& event, const std::function<double(const Pull&)>& reach,
                      const std::function<bool(const Event&)>& keep = {}) {
    std::vector<Pull> pulls;
    while (!event.finished()) {
        if (event.decision_due()) {
            event.decide(keep ? keep(event) : event.suggested_keep());
            continue;
        }
        const Pull pull = next_of(event);
        pulls.push_back(pull);
        event.record(reach(pull));
    }
    return pulls;
}

int knives(const Event& event) {
    return event.sled().knives.count;
}

TEST(EventRules, RejectsNonsense) {
    EXPECT_EQ(validate(kRules), std::nullopt);
    const auto with = [](auto mutate) {
        Rules r = kRules;
        mutate(r);
        return validate(r);
    };
    EXPECT_EQ(with([](Rules& r) { r.step_m = std::numeric_limits<double>::quiet_NaN(); }),
              ParamError::kNotFinite);
    EXPECT_EQ(with([](Rules& r) { r.extended_full_pull_m = 95.0; }),
              ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.max_distance_m = 105.0; }), ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.qualifying_distance_m = 120.0; }),
              ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.pull_off_places = 0; }), ParamError::kPlacesOutOfRange);
    EXPECT_EQ(with([](Rules& r) { r.repull_after_pulls = 2; }), ParamError::kTurnsOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.step_m = 0.0; }), ParamError::kStepNotPositive);
}

TEST(Event, RejectsABadRunningOrder) {
    const sled::Params sled = sled::testing::reference_params();
    const std::array<int, 3> twice = {0, 1, 1};
    const std::array<int, 2> outside = {0, 2};
    EXPECT_FALSE(Event::create(kRules, sled, twice).has_value());
    EXPECT_FALSE(Event::create(kRules, sled, outside).has_value());
    EXPECT_FALSE(Event::create(kRules, sled, std::span<const int>{}).has_value());
}

// The draw is a permutation of the field, the same for the same seed.
TEST(Event, RunningOrderIsADeterministicDraw) {
    const auto order = running_order(15, 42);
    std::array<int, 15> sorted{};
    std::copy_n(order.begin(), 15, sorted.begin());
    std::ranges::sort(sorted);
    for (int i = 0; i < 15; ++i) {
        EXPECT_EQ(sorted[static_cast<std::size_t>(i)], i);
    }
    EXPECT_EQ(running_order(15, 42), order);
    EXPECT_NE(running_order(15, 43), order);
}

// The first puller tests the sled. A test pull the operator is happy with
// changes nothing; the test puller decides after the third pull, and a kept
// test pull is their result.
TEST(Event, FirstPullIsATestPull) {
    Event event = make(6);
    EXPECT_EQ(next_of(event).kind, PullKind::kTest);
    const Outcome outcome = event.record(95.0);
    EXPECT_EQ(outcome.change, SledChange::kNone);
    EXPECT_EQ(next_of(event).kind, PullKind::kCompetition);
    event.record(80.0);
    EXPECT_FALSE(event.decision_due());
    event.record(85.0);
    ASSERT_EQ(event.decision_due(), 0);
    EXPECT_FALSE(event.next().has_value()) << "waits for the decision";
    EXPECT_TRUE(event.suggested_keep()) << "95 m beats everyone since";
    event.decide(true);
    EXPECT_EQ(event.standings()[0].puller, 0);
    EXPECT_EQ(event.standings()[0].distance_m, 95.0);
}

// Later test pulls count for the decision too: a test pull beaten by the
// next test pullers is better re-pulled.
TEST(Event, TestPullBeatenByLaterTestPullsIsRepulled) {
    Event event = make(6);
    event.record(80.0);  // lighter: the next puller tests
    event.record(88.0);  // lighter again
    event.record(95.0);  // within the target: no change
    ASSERT_EQ(event.decision_due(), 0);
    EXPECT_FALSE(event.suggested_keep());
}

// A test pull between 100 and 110 m moves the line to 110 m; the sled stays.
TEST(Event, TestPullJustPastTheLineExtendsIt) {
    Event event = make(6);
    const sled::Params before = event.sled();
    const Outcome outcome = event.record(108.0);
    EXPECT_EQ(outcome.change, SledChange::kLineExtended);
    EXPECT_EQ(event.full_pull_m(), 110.0);
    EXPECT_EQ(event.sled().knives.count, before.knives.count);
    EXPECT_EQ(next_of(event).kind, PullKind::kCompetition) << "no change, no new test";
}

// An easy test pull makes the sled heavier, enough that a pull like it
// stops at the line. The next puller tests the new sled; the first test
// puller cannot keep their pull and re-pulls after the 6th pull.
TEST(Event, EasyTestPullMakesTheSledHeavier) {
    Event event = make(10);
    const int before = knives(event);
    const Outcome outcome = event.record(118.0);
    EXPECT_EQ(outcome.change, SledChange::kHeavier);
    EXPECT_EQ(outcome.steps, 4);
    EXPECT_EQ(knives(event), before + 4);
    EXPECT_EQ(event.full_pull_m(), 100.0);
    EXPECT_EQ(next_of(event).kind, PullKind::kTest);

    const std::vector<Pull> pulls = run(event, [](const Pull&) { return 92.0; });
    // Pulls 2 to 6 (puller 1 tests, no change), then puller 0 again.
    ASSERT_GE(pulls.size(), 6U);
    EXPECT_EQ(pulls[5].puller, 0);
    EXPECT_EQ(pulls[5].kind, PullKind::kRepull);
}

// A short test pull makes the sled lighter, and the test puller may keep it.
TEST(Event, ShortTestPullMakesTheSledLighter) {
    Event event = make(8);
    const int before = knives(event);
    const Outcome outcome = event.record(80.0);
    EXPECT_EQ(outcome.change, SledChange::kLighter);
    EXPECT_EQ(outcome.steps, 2);
    EXPECT_EQ(knives(event), before - 2);
    EXPECT_EQ(next_of(event).kind, PullKind::kTest);
}

// With no knives left, the operator lightens with a later push-down.
TEST(Event, LighteningPastTheKnivesMovesThePushdown) {
    sled::Params sled = sled::testing::reference_params();
    sled.knives.count = 1;
    Event event = make(4, sled);
    event.record(75.0);  // 3 steps: one knife, then 10 m later push-down
    EXPECT_EQ(knives(event), 0);
    EXPECT_EQ(event.sled().pushdown_start_m, sled.pushdown_start_m + 10.0);
}

// A test puller who re-pulls pulls again after the 6th pull; one who keeps
// does not.
TEST(Event, RepullComesAfterTheSixthPull) {
    for (const bool keep : {true, false}) {
        Event event = make(10);
        const std::vector<Pull> pulls =
            run(event, [](const Pull&) { return 95.0; }, [&](const Event&) { return keep; });
        const auto repulls =
            std::ranges::count_if(pulls, [](const Pull& p) { return p.kind == PullKind::kRepull; });
        EXPECT_EQ(repulls, keep ? 0 : 1);
        if (!keep) {
            EXPECT_EQ(pulls[6].puller, 0);
            EXPECT_EQ(pulls[6].kind, PullKind::kRepull);
        }
    }
}

// In a small field the decision is due at the end of the round at the
// latest.
TEST(Event, DecisionIsDueByTheEndOfTheRound) {
    Event event = make(2);
    event.record(95.0);
    event.record(93.0);
    EXPECT_EQ(event.decision_due(), 0);
}

// Every full pull qualifies; with fewer than four, the best runs from 90 m
// on fill the places. Shorter runs never qualify.
TEST(Event, QualifyingFillsUpWithTheBestFromNinetyMetres) {
    // Puller 0 tests at 95 m and keeps it; then 100, 89, 93, 91, 100, 70.
    constexpr std::array<double, 7> kReach = {95.0, 100.0, 89.0, 93.0, 91.0, 100.0, 70.0};
    Event event = make(7);
    const std::vector<Pull> pulls = run(event, [&](const Pull& p) {
        return p.kind == PullKind::kPullOff ? 80.0 : kReach[static_cast<std::size_t>(p.puller)];
    });
    std::vector<int> pull_off;
    for (const Pull& p : pulls) {
        if (p.kind == PullKind::kPullOff) {
            pull_off.push_back(p.puller);
        }
    }
    EXPECT_EQ(pull_off, (std::vector<int>{0, 1, 3, 5})) << "full pulls 1 and 5, then 95 and 93";
}

// The pull-off sled is heavier by enough steps that the strongest full pull
// so far would stop at the line: a pull good for 120 m takes four.
TEST(Event, PullOffSledMatchesTheStrongestFullPull) {
    Event event = make(5);
    const int before = knives(event);
    for (const double reach : {95.0, 120.0, 104.0}) {
        event.record(reach);
    }
    ASSERT_EQ(event.decision_due(), 0);
    event.decide(true);
    event.record(101.0);
    event.record(97.0);
    ASSERT_EQ(next_of(event).kind, PullKind::kPullOff);
    EXPECT_EQ(knives(event), before + 4);
}

// When several pullers make a full pull again, another, heavier pull-off
// follows among them; it ends when at most one gets to the line.
TEST(Event, PullOffsRepeatUntilOneIsStrongest) {
    Event event = make(5);
    const std::vector<Pull> pulls = run(event, [](const Pull& p) {
        if (p.kind != PullKind::kPullOff) {
            return 95.0 + p.puller;  // nobody full-pulls: 95 to 99 m
        }
        if (p.pull_off_round == 1) {
            return p.puller >= 3 ? 105.0 : 96.0;  // two full pulls again
        }
        return p.puller == 4 ? 99.0 : 97.0;
    });
    EXPECT_EQ(pulls.back().pull_off_round, 2);
    const std::span<const Standing> standings = event.standings();
    EXPECT_EQ(standings[0].puller, 4);
    EXPECT_EQ(standings[0].place, 1);
    EXPECT_EQ(standings[1].puller, 3);
    EXPECT_EQ(standings[1].pull_off_rounds, 2);
    EXPECT_EQ(standings[2].pull_off_rounds, 1);
    EXPECT_EQ(standings.size(), 5U);
}

// Equal results share a place; the next place counts both.
TEST(Event, EqualResultsShareAPlace) {
    Event event = make(4);
    run(event, [](const Pull& p) {
        if (p.kind == PullKind::kPullOff) {
            return 80.0;
        }
        return p.puller == 3 ? 70.0 : 95.0;
    });
    const std::span<const Standing> standings = event.standings();
    ASSERT_EQ(standings.size(), 4U);
    EXPECT_EQ(standings[0].place, 1);
    EXPECT_EQ(standings[1].place, 1);
    EXPECT_EQ(standings[2].place, 1);
    EXPECT_EQ(standings[3].place, 4);
}

}  // namespace
}  // namespace tp::event
