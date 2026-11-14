#include "tp/core/event/event.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <span>
#include <vector>

#include "../sled/reference_sled.hpp"

namespace tp::event {
namespace {

// The NTTO's rules for a standard class, and a 5 m rule of thumb.
constexpr Rules kRules{.full_pull_m = 100.0,
                       .extended_full_pull_m = 110.0,
                       .max_distance_m = 130.0,
                       .pull_off_full_pull_m = 120.0,
                       .pull_off_places = 4,
                       .qualifying_below_line_m = 10.0,
                       .decide_after_pulls = 2,
                       .repull_after_pulls = 5,
                       .sled_target_m = 90.0,
                       .step_m = 5.0,
                       .max_steps_per_change = 4,
                       .strong_full_pull_from = std::nullopt,
                       .trouble_share = std::nullopt};

std::vector<int> in_order(int count) {
    std::vector<int> order(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        order[static_cast<std::size_t>(i)] = i;
    }
    return order;
}

Event make(int count, const sled::Params& sled = sled::testing::reference_params(),
           const Rules& rules = kRules) {
    const std::vector<int> order = in_order(count);
    const auto event = Event::create(rules, sled, order);
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
    EXPECT_EQ(with([](Rules& r) { r.qualifying_below_line_m = 120.0; }),
              ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.pull_off_full_pull_m = 105.0; }),
              ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.pull_off_full_pull_m = 140.0; }),
              ParamError::kDistancesOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.pull_off_places = 0; }), ParamError::kPlacesOutOfRange);
    EXPECT_EQ(with([](Rules& r) { r.repull_after_pulls = 2; }), ParamError::kTurnsOutOfOrder);
    EXPECT_EQ(with([](Rules& r) { r.step_m = 0.0; }), ParamError::kStepNotPositive);
    EXPECT_EQ(with([](Rules& r) { r.max_steps_per_change = 0; }), ParamError::kMaxStepsNotPositive);
    EXPECT_EQ(with([](Rules& r) { r.strong_full_pull_from = 1.2; }),
              ParamError::kOperatorJudgementOutOfRange);
    EXPECT_EQ(with([](Rules& r) { r.trouble_share = 1.0; }),
              ParamError::kOperatorJudgementOutOfRange);
    EXPECT_EQ(with([](Rules& r) { r.trouble_share = std::nan(""); }),
              ParamError::kOperatorJudgementOutOfRange);
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

// A puller who stopped short of the restart line pulls again a few places
// later; as the test puller, they hand the test to the next puller.
TEST(Event, ADeferredPullerPullsLater) {
    Event event = make(10);
    EXPECT_EQ(event.defer(kDeferPlaces), kDeferPlaces);
    EXPECT_EQ(next_of(event).puller, 1);
    EXPECT_EQ(next_of(event).kind, PullKind::kTest) << "the next puller tests the sled";
    for (int puller = 1; puller <= kDeferPlaces; ++puller) {
        ASSERT_EQ(next_of(event).puller, puller);
        event.record(95.0);
        if (const std::optional<int> due = event.decision_due()) {
            event.decide(true);
        }
    }
    EXPECT_EQ(next_of(event).puller, 0);
    EXPECT_EQ(next_of(event).kind, PullKind::kCompetition);
    // At the end of the order there is nowhere further back to go.
    Event last = make(2);
    last.record(80.0);
    EXPECT_EQ(last.defer(kDeferPlaces), 0);
    EXPECT_EQ(next_of(last).puller, 1);
}

// A disqualified test pull says nothing about the sled: it stays, the next
// puller tests it, and the test puller keeps 0 m without a decision.
TEST(Event, DisqualifiedTestPullLeavesTheSled) {
    Event event = make(6);
    const int before = knives(event);
    ASSERT_EQ(next_of(event).kind, PullKind::kTest);
    const Outcome outcome = event.record_disqualified();
    EXPECT_EQ(outcome.change, SledChange::kNone);
    EXPECT_EQ(knives(event), before);
    const Pull second = next_of(event);
    EXPECT_EQ(second.puller, 1);
    EXPECT_EQ(second.kind, PullKind::kTest);
    // 70 m makes the sled lighter, unaffected by the disqualified pull.
    EXPECT_EQ(event.record(70.0).change, SledChange::kLighter);
    run(event, [](const Pull&) { return 95.0; });
    const std::span<const Standing> standings = event.standings();
    const Standing& last = standings.back();
    EXPECT_EQ(last.puller, 0);
    EXPECT_TRUE(last.disqualified);
    EXPECT_EQ(last.distance_m, 0.0);
}

// A disqualified pull counts 0 m and ranks behind every pull that counts,
// even one that went nowhere.
TEST(Event, DisqualifiedPullRanksLast) {
    Event event = make(4);
    std::vector<Pull> pulls;
    while (!event.finished()) {
        if (event.decision_due()) {
            event.decide(event.suggested_keep());
            continue;
        }
        const Pull pull = next_of(event);
        pulls.push_back(pull);
        if (pull.puller == 2 && pull.kind != PullKind::kPullOff) {
            event.record_disqualified();
        } else {
            event.record(pull.puller == 1 ? 0.0 : 95.0);
        }
    }
    const std::span<const Standing> standings = event.standings();
    ASSERT_EQ(standings.size(), 4U);
    EXPECT_EQ(standings[2].puller, 1);
    EXPECT_FALSE(standings[2].disqualified);
    EXPECT_EQ(standings[3].puller, 2);
    EXPECT_TRUE(standings[3].disqualified);
    EXPECT_EQ(standings[3].place, 4);
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

// The operator learns what a step is worth from what each change does:
// two steps lighter that bring the next test pull 20 m further are worth
// about 10 m each, so the next change is smaller.
TEST(Event, OperatorLearnsWhatAStepIsWorth) {
    Event event = make(8);
    EXPECT_EQ(event.step_estimate_m(), kRules.step_m);
    event.record(84.0);   // 2 steps lighter
    event.record(104.0);  // 20 m further: 10 m a step (and the line moves to 110 m)
    EXPECT_GT(event.step_estimate_m(), kRules.step_m);
    EXPECT_NEAR(event.step_estimate_m(), 0.5 * (kRules.step_m + 10.0), 1e-12);
}

// A change is never more than the rules allow, however far off the test
// pull, and the pull-off sled neither.
TEST(Event, ChangesAreCapped) {
    Event event = make(6);
    const Outcome outcome = event.record(130.0);
    EXPECT_EQ(outcome.change, SledChange::kHeavier);
    EXPECT_EQ(outcome.steps, kRules.max_steps_per_change);
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

// A lighter step takes a knife out and moves the push-down a step later;
// with no knives left, only the push-down moves. The push-down moves only
// for a field that gets to it: one that stops short of it does not.
TEST(Event, ALighterStepTakesAKnifeAndMovesThePushdown) {
    sled::Params sled = sled::testing::reference_params();
    sled.knives.count = 1;
    sled.pushdown_start_m = 60.0;
    Event event = make(4, sled);
    const Outcome outcome = event.record(75.0);  // 3 steps: 15 m short of 90 m
    ASSERT_EQ(outcome.change, SledChange::kLighter);
    EXPECT_EQ(outcome.steps, 3);
    EXPECT_EQ(knives(event), 0);
    EXPECT_EQ(event.sled().pushdown_start_m, sled.pushdown_start_m + outcome.steps * kRules.step_m);

    // Stopping at 55 m, short of the push-down: the knife comes out, the
    // push-down stays, and the other steps take box weight.
    Event short_of_it = make(4, sled);
    ASSERT_EQ(short_of_it.record(55.0).change, SledChange::kLighter);
    EXPECT_EQ(knives(short_of_it), 0);
    EXPECT_EQ(short_of_it.sled().pushdown_start_m, sled.pushdown_start_m);
    EXPECT_LT(short_of_it.sled().box_mass_kg, sled.box_mass_kg);
}

// With the knives out and the push-down where the field does not get to
// (here past the line), a lighter step takes a tenth of the box out
// instead: moving the push-down would change nothing. A heavy, wet track's
// field that stops at the start is lightened so.
TEST(Event, WithNothingElseLeftALighterStepTakesBoxWeightOut) {
    sled::Params sled = sled::testing::reference_params();
    sled.knives.count = 0;
    sled.pushdown_start_m = kRules.max_distance_m - kRules.step_m;
    Event event = make(4, sled);
    const Outcome outcome = event.record(75.0);  // 3 steps: 15 m short of 90 m
    ASSERT_EQ(outcome.change, SledChange::kLighter);
    ASSERT_EQ(outcome.steps, 3);
    EXPECT_EQ(event.sled().pushdown_start_m, sled.pushdown_start_m) << "out of the field's reach";
    EXPECT_EQ(knives(event), 0);
    EXPECT_DOUBLE_EQ(event.sled().box_mass_kg, sled.box_mass_kg * 0.9 * 0.9 * 0.9);

    // While knives are left, the box stays as it is.
    sled::Params with_knives = sled::testing::reference_params();
    with_knives.pushdown_start_m = kRules.max_distance_m;
    Event knives_first = make(4, with_knives);
    ASSERT_EQ(knives_first.record(80.0).change, SledChange::kLighter);
    EXPECT_EQ(knives_first.sled().box_mass_kg, with_knives.box_mass_kg);
    EXPECT_LT(knives(knives_first), with_knives.knives.count);
}

// One test pull far short of its tractor's trials is put down to tractor
// trouble; a second in a row says the sled is too heavy for the day, and
// the operator judges it.
TEST(Event, TwoTroublesInARowAreTheSled) {
    Rules rules = kRules;
    rules.trouble_share = 0.6;
    const std::vector<int> order = in_order(4);
    std::optional<Event> made = Event::create(rules, sled::testing::reference_params(), order);
    if (!made) {
        FAIL() << "the event was refused";
    }
    Event event = made.value();
    for (int i = 0; i < 4; ++i) {
        event.expect(i, 80.0);
    }
    EXPECT_EQ(event.record(5.0).change, SledChange::kTroubleSuspected);
    EXPECT_EQ(next_of(event).kind, PullKind::kTest);
    EXPECT_EQ(event.record(6.0).change, SledChange::kLighter) << "the sled, not the tractors";
}

// The operator weighs a test pull by who pulled it: a weak tractor's short
// pull leaves the sled when the field's average would still reach the target; a
// full pull by one of the strongest leaves the sled; far short of what a
// tractor should do is trouble with the tractor, and the next one tests.
TEST(Event, TheOperatorWeighsATestPullByItsTractor) {
    Rules rules = kRules;
    rules.strong_full_pull_from = 0.8;
    rules.trouble_share = 0.6;
    const std::vector<int> order = in_order(6);
    // Puller 0 the weakest, 5 the strongest: 20 m apart.
    const auto made = [&](std::span<const int> running_order) {
        std::optional<Event> event =
            Event::create(rules, sled::testing::reference_params(), running_order);
        if (!event) {
            std::abort();
        }
        Event out = *event;
        for (int i = 0; i < 6; ++i) {
            out.expect(i, 80.0 + 4.0 * i);
        }
        return out;
    };
    Event weak = made(order);
    EXPECT_EQ(weak.record(82.0).change, SledChange::kNone)
        << "82 m by the weakest: the average would get 92 m";
    Event weak_short = made(order);
    EXPECT_EQ(weak_short.record(66.0).change, SledChange::kLighter)
        << "the average would get only 76 m";

    std::vector<int> strongest_first = order;
    std::swap(strongest_first[0], strongest_first[5]);
    Event strong = made(strongest_first);
    EXPECT_EQ(strong.record(112.0).change, SledChange::kNone)
        << "the strongest's full pull: the sled is right";

    Event trouble = made(order);
    EXPECT_EQ(trouble.record(40.0).change, SledChange::kTroubleSuspected)
        << "40 m where 80 m was expected";
    EXPECT_EQ(next_of(trouble).kind, PullKind::kTest) << "the next puller tests";
}

// The sled is set for the field as a whole: one tractor far stronger than
// the rest (a player who built theirs up) moves the average a little, not
// the sled to its own measure. Judged by the strongest, a rival's short
// test pull on a sled far too heavy for the field would leave it, since the
// strong one would still get there; judged by the average, it is lightened.
TEST(Event, OneFarStrongerTractorDoesNotSetTheSled) {
    const std::vector<int> order = in_order(10);
    const auto made = [&](double strongest_m) {
        std::optional<Event> event =
            Event::create(kRules, sled::testing::reference_params(), order);
        if (!event) {
            std::abort();
        }
        Event out = *event;
        // Puller 0, the first to pull, expected at 60 m; the others up to
        // 76 m; the tenth, last in the order, as given.
        for (int i = 0; i < 9; ++i) {
            out.expect(i, 60.0 + 2.0 * i);
        }
        out.expect(9, strongest_m);
        return out;
    };
    // A tenth like the others: the average 69 m, 9 m more than puller 0.
    // An 84 m test pull says the average would get 93 m: the sled is right.
    Event even = made(78.0);
    EXPECT_EQ(even.record(84.0).change, SledChange::kNone);
    // A tenth expected at 160 m moves the average up 8.2 m, to 77.2 m. A
    // 40 m test pull: the average would get 57.2 m, 32.8 m short of the
    // target, so lighter by the most steps at once. (The strongest would
    // still get 140 m: by its measure the sled would stay far too heavy.)
    Event outlier = made(160.0);
    const Outcome lighter = outlier.record(40.0);
    EXPECT_EQ(lighter.change, SledChange::kLighter) << "the sled is set for the field";
    EXPECT_EQ(lighter.steps, kRules.max_steps_per_change);
}

// A full pull makes the sled heavier until the field's average would stop
// at the target: by the steps the average is over it, and one at least,
// as a full pull's distance only says the sled went that far.
TEST(Event, AFullPullMakesItHeavierForTheAverage) {
    const std::vector<int> order = in_order(10);
    const auto made = [&](int first) {
        std::vector<int> running = order;
        std::swap(running[0], running[static_cast<std::size_t>(first)]);
        std::optional<Event> event =
            Event::create(kRules, sled::testing::reference_params(), running);
        if (!event) {
            std::abort();
        }
        Event out = *event;
        // Expected at 60 to 76 m, and the last at 90 m: the average 70.2 m.
        for (int i = 0; i < 9; ++i) {
            out.expect(i, 60.0 + 2.0 * i);
        }
        out.expect(9, 90.0);
        return out;
    };
    // The weakest (60 m) pulls 112 m: the average would get 122.2 m, 32.2 m
    // over the 90 m target: more steps than one change takes.
    Event weak = made(0);
    const Outcome far = weak.record(112.0);
    EXPECT_EQ(far.change, SledChange::kHeavier);
    EXPECT_EQ(far.steps, kRules.max_steps_per_change);
    // The strongest (90 m) pulls 110 m, past the extended line: the
    // average would get 90.2 m, barely over: still one step.
    Event strong = made(9);
    const Outcome just = strong.record(110.0);
    EXPECT_EQ(just.change, SledChange::kHeavier);
    EXPECT_EQ(just.steps, 1);
}

// After one change, the next test changes the sled again only if it
// misses by more than half a step.
TEST(Event, AfterAChangeOnlyAClearMissChangesItAgain) {
    Event event = make(6);
    ASSERT_EQ(event.record(80.0).change, SledChange::kLighter);
    EXPECT_EQ(event.record(88.0).change, SledChange::kNone) << "2 m short of 90 m";
    Event again = make(6);
    ASSERT_EQ(again.record(80.0).change, SledChange::kLighter);
    EXPECT_EQ(again.record(70.0).change, SledChange::kLighter) << "20 m short";
    // A full pull is a full pull, however soon after a change.
    Event full = make(6);
    ASSERT_EQ(full.record(80.0).change, SledChange::kLighter);
    EXPECT_EQ(full.record(110.5).change, SledChange::kHeavier);
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

// When several pullers reach the pull-off line (120 m) again, another,
// heavier pull-off follows among them; it ends when at most one gets to
// the line. Each round's distance stays in the standings.
TEST(Event, PullOffsRepeatUntilOneIsStrongest) {
    Event event = make(5);
    const std::vector<Pull> pulls = run(event, [](const Pull& p) {
        if (p.kind != PullKind::kPullOff) {
            return 95.0 + p.puller;  // nobody full-pulls: 95 to 99 m
        }
        if (p.pull_off_round == 1) {
            return p.puller >= 3 ? 125.0 : 96.0;  // two full pulls again
        }
        return p.puller == 4 ? 99.0 : 97.0;
    });
    EXPECT_EQ(pulls.back().pull_off_round, 2);
    const std::span<const Standing> standings = event.standings();
    EXPECT_EQ(standings[0].puller, 4);
    EXPECT_EQ(standings[0].place, 1);
    EXPECT_EQ(standings[0].pull_off_distances_m[0], 120.0) << "capped at the pull-off line";
    EXPECT_EQ(standings[0].pull_off_distances_m[1], 99.0);
    EXPECT_EQ(standings[1].puller, 3);
    EXPECT_EQ(standings[1].pull_off_rounds, 2);
    EXPECT_EQ(standings[1].pull_off_distances_m[1], 97.0);
    EXPECT_EQ(standings[2].pull_off_rounds, 1);
    EXPECT_EQ(standings[2].pull_off_distances_m[0], 96.0);
    EXPECT_EQ(standings.size(), 5U);
}

// A second pull-off follows pulls of at least 120 m, so its sled is made
// heavier by twice the first's steps, and at least as many as the
// operator changes at once.
TEST(Event, ASecondPullOffTakesTheMostStepsAtOnce) {
    Event event = make(5);
    std::array<int, 3> knives_in_round{};
    run(event, [&](const Pull& p) {
        if (p.kind != PullKind::kPullOff) {
            return 95.0 + p.puller;
        }
        knives_in_round[static_cast<std::size_t>(p.pull_off_round)] = knives(event);
        if (p.pull_off_round == 1) {
            return p.puller >= 3 ? 125.0 : 96.0;
        }
        return p.puller == 4 ? 99.0 : 97.0;
    });
    EXPECT_EQ(knives_in_round[2] - knives_in_round[1], kRules.max_steps_per_change);
}

// Full pulls stopped at the line (as a run stops them), or within a step of
// it, show no margin: the
// first pull-off is as many steps heavier as the operator changes at once,
// a second twice that, and a third heavier still, as far as the sled goes.
TEST(Event, FullPullsStoppedAtTheLineMakeThePullOffsFarHeavier) {
    Event event = make(5);
    std::array<int, 4> knives_in_round{};
    knives_in_round[0] = knives(event);
    const std::vector<Pull> pulls = run(event, [&](const Pull& p) {
        if (p.kind != PullKind::kPullOff) {
            // One at the line, one a little past it: no margin either.
            constexpr std::array<double, 5> kMain = {95.0, 95.0, 95.0, 100.0, 101.0};
            return kMain.at(static_cast<std::size_t>(p.puller));
        }
        knives_in_round[static_cast<std::size_t>(p.pull_off_round)] = knives(event);
        if (p.pull_off_round < 3) {
            return p.puller >= 3 ? 120.0 : 96.0;
        }
        return p.puller == 4 ? 99.0 : 97.0;
    });
    EXPECT_EQ(pulls.back().pull_off_round, 3);
    const int steps = kRules.max_steps_per_change;
    EXPECT_EQ(knives_in_round[1] - knives_in_round[0], steps) << "the most at once";
    EXPECT_EQ(knives_in_round[2] - knives_in_round[1], 2 * steps) << "twice that";
    EXPECT_GT(knives_in_round[3], knives_in_round[2]) << "heavier still, up to the sled's most";
}

// In a pull-off only the pull-off line (120 m) is a full pull: two pullers
// past the main round's line but short of 120 m decide it in one round.
TEST(Event, PullOffFullPullIsAtThePullOffLine) {
    Event event = make(5);
    const std::vector<Pull> pulls = run(event, [](const Pull& p) {
        if (p.kind != PullKind::kPullOff) {
            return 95.0 + p.puller;
        }
        return p.puller == 4 ? 115.0 : 110.0;
    });
    EXPECT_EQ(pulls.back().pull_off_round, 1);
    const std::span<const Standing> standings = event.standings();
    EXPECT_EQ(standings[0].puller, 4);
    EXPECT_EQ(standings[0].pull_off_distance_m(), 115.0);
    EXPECT_EQ(standings[1].pull_off_distance_m(), 110.0);
}

// The pull-off's places fill up with the best runs from 20 m short of the
// line in force: 80 m on the 100 m line, so a full pull, 95, 82 and 81 m
// make the pull-off and 79 m does not.
TEST(Event, QualifyingFromTwentyMetresShortOfTheLine) {
    Rules rules = kRules;
    rules.qualifying_below_line_m = 20.0;
    // Puller 0 tests at 95 m (the sled stays) and pulls 95 m again.
    constexpr std::array<double, 6> kReach = {95.0, 100.0, 79.0, 82.0, 70.0, 81.0};
    Event event = make(6, sled::testing::reference_params(), rules);
    const std::vector<Pull> pulls = run(event, [&](const Pull& p) {
        return p.kind == PullKind::kPullOff ? 80.0 : kReach[static_cast<std::size_t>(p.puller)];
    });
    std::vector<int> pull_off;
    for (const Pull& p : pulls) {
        if (p.kind == PullKind::kPullOff) {
            pull_off.push_back(p.puller);
        }
    }
    EXPECT_EQ(pull_off, (std::vector<int>{0, 1, 3, 5}));
}

// With the line moved to 110 m by a test pull, qualifying starts at 90 m.
TEST(Event, QualifyingFollowsTheExtendedLine) {
    Rules rules = kRules;
    rules.qualifying_below_line_m = 20.0;
    // Puller 0 tests at 104 m: the line moves to 110 m; they keep it.
    constexpr std::array<double, 5> kReach = {104.0, 95.0, 89.0, 91.0, 60.0};
    Event event = make(5, sled::testing::reference_params(), rules);
    const std::vector<Pull> pulls = run(event, [&](const Pull& p) {
        return p.kind == PullKind::kPullOff ? 80.0 : kReach[static_cast<std::size_t>(p.puller)];
    });
    ASSERT_EQ(event.main_line_m(), 110.0);
    std::vector<int> pull_off;
    for (const Pull& p : pulls) {
        if (p.kind == PullKind::kPullOff) {
            pull_off.push_back(p.puller);
        }
    }
    EXPECT_EQ(pull_off, (std::vector<int>{0, 1, 3})) << "104, 95 and 91 m; not 89 m";
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
