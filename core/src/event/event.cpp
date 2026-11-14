#include "tp/core/event/event.hpp"

#include "tp/core/math/random.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::event {

namespace {

// How quickly the operator's step estimate follows what they see, and the
// range they keep it in, as shares of the rule of thumb.
constexpr double kLearningRate = 0.5;
constexpr double kLowestStepShare = 0.25;
constexpr double kHighestStepShare = 6.0;
// Test pulls in a row the operator puts down to tractor trouble before
// taking the next as the sled's measure.
constexpr int kMaxTroublesInARow = 1;
// After a change, a short test pull changes the sled again only when it
// misses by more than this share of a step.
constexpr double kRetestMarginSteps = 0.5;

// A lighter step's share of the weight box, once the knives and the
// push-down have nothing more to give.
constexpr double kBoxShareALighterStep = 0.1;

// A lighter step: a pull-off step's knives out, and the push-down a step's
// worth later. Knives alone barely lengthen a pull; with the push-down each
// step is worth about what the operator reckons a step is. Once the knives
// are out and the push-down is past `end_m` (where every pull stops), those
// change nothing: the step takes weight out of the box instead, as an
// operator does for a field the sled is far too heavy for. So does a field
// expected to stop at `field_stops_m`, short of where the push-down starts:
// it never reaches the push-down, so moving it later gains nothing, and on
// a heavy, wet track the start is what stops them. The weight stays out
// for the rest of the event: the pull-offs' heavier steps add knives and
// move the push-down (the sled's pull_off_step), as at any event.
sled::Params lighter(const sled::Params& sled, int steps, double step_m, double end_m,
                     double field_stops_m) noexcept {
    const sled::Adjustment& step = sled.pull_off_step;
    sled::Params out = sled;
    for (int i = 0; i < steps; ++i) {
        const bool knives_left = step.knives > 0 && out.knives.count > 0;
        const bool pushdown_in_play =
            out.pushdown_start_m < end_m && field_stops_m >= out.pushdown_start_m;
        if (knives_left || pushdown_in_play) {
            // The push-down moves only where the field gets to it.
            out = sled::adjusted(
                out, {.knives = step.knives, .pushdown_earlier_m = pushdown_in_play ? step_m : 0.0},
                -1);
        } else {
            out.box_mass_kg *= 1.0 - kBoxShareALighterStep;
        }
    }
    return out;
}

bool running_order_valid(std::span<const int> order) noexcept {
    if (order.empty() || order.size() > static_cast<std::size_t>(kMaxPullers)) {
        return false;
    }
    std::array<bool, kMaxPullers> seen{};
    const auto count = static_cast<int>(order.size());
    for (const int puller : order) {
        if (puller < 0 || puller >= count || seen[static_cast<std::size_t>(puller)]) {
            return false;
        }
        seen[static_cast<std::size_t>(puller)] = true;
    }
    return true;
}

}  // namespace

std::optional<ParamError> validate(const Rules& rules) noexcept {
    const std::array distances = {rules.full_pull_m,
                                  rules.extended_full_pull_m,
                                  rules.max_distance_m,
                                  rules.pull_off_full_pull_m,
                                  rules.qualifying_below_line_m,
                                  rules.sled_target_m,
                                  rules.step_m};
    if (!std::ranges::all_of(distances, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (rules.qualifying_below_line_m <= 0.0 ||
        rules.qualifying_below_line_m >= rules.full_pull_m || rules.sled_target_m <= 0.0 ||
        rules.sled_target_m > rules.full_pull_m || rules.full_pull_m > rules.extended_full_pull_m ||
        rules.extended_full_pull_m > rules.pull_off_full_pull_m ||
        rules.pull_off_full_pull_m > rules.max_distance_m) {
        return ParamError::kDistancesOutOfOrder;
    }
    if (rules.pull_off_places < 1 || rules.pull_off_places > kMaxPullers) {
        return ParamError::kPlacesOutOfRange;
    }
    if (rules.decide_after_pulls < 0 || rules.repull_after_pulls <= rules.decide_after_pulls) {
        return ParamError::kTurnsOutOfOrder;
    }
    if (rules.step_m <= 0.0) {
        return ParamError::kStepNotPositive;
    }
    if (rules.max_steps_per_change < 1) {
        return ParamError::kMaxStepsNotPositive;
    }
    const double strong = rules.strong_full_pull_from.value_or(0.0);
    const double trouble = rules.trouble_share.value_or(0.0);
    if (!std::isfinite(strong) || !std::isfinite(trouble) || strong < 0.0 || strong > 1.0 ||
        trouble < 0.0 || trouble >= 1.0) {
        return ParamError::kOperatorJudgementOutOfRange;
    }
    return std::nullopt;
}

std::optional<Event> Event::create(const Rules& rules, const sled::Params& class_sled,
                                   std::span<const int> running_order) noexcept {
    if (validate(rules) || sled::validate(class_sled) || !running_order_valid(running_order)) {
        return std::nullopt;
    }
    return Event{rules, class_sled, running_order};
}

Event::Event(const Rules& rules, const sled::Params& class_sled,
             std::span<const int> running_order) noexcept
    : rules_(rules),
      sled_(class_sled),
      line_m_(rules.full_pull_m),
      count_(static_cast<int>(running_order.size())) {
    for (std::size_t i = 0; i < running_order.size(); ++i) {
        order_[i] = running_order[i];
        queue_[i] = {.puller = running_order[i],
                     .kind = i == 0 ? PullKind::kTest : PullKind::kCompetition,
                     .pull_off_round = 0};
    }
    queue_size_ = running_order.size();
    step_estimate_m_ = rules.step_m;
}

void Event::expect(int puller, double expected_m) noexcept {
    if (puller >= 0 && puller < count_ && std::isfinite(expected_m) && expected_m > 0.0) {
        expected_m_[static_cast<std::size_t>(puller)] = expected_m;
    }
}

double Event::strength(int puller) const noexcept {
    constexpr double kMiddle = 0.5;
    const double mine = expected_m_[static_cast<std::size_t>(puller)];
    if (mine <= 0.0) {
        return kMiddle;
    }
    int known = 0;
    int below = 0;
    for (int i = 0; i < count_; ++i) {
        const double theirs = expected_m_[static_cast<std::size_t>(i)];
        if (theirs > 0.0) {
            ++known;
            below += theirs < mine ? 1 : 0;
        }
    }
    return known < 2 ? kMiddle : static_cast<double>(below) / static_cast<double>(known - 1);
}

double Event::gap_to_average_m(int puller) const noexcept {
    const double mine = expected_m_[static_cast<std::size_t>(puller)];
    if (mine <= 0.0) {
        return 0.0;
    }
    double sum_m = 0.0;
    int known = 0;
    for (int i = 0; i < count_; ++i) {
        const double theirs = expected_m_[static_cast<std::size_t>(i)];
        if (theirs > 0.0) {
            sum_m += theirs;
            ++known;
        }
    }
    // `mine` is known, so `known` is at least 1.
    return sum_m / static_cast<double>(known) - mine;
}

std::optional<Pull> Event::next() const noexcept {
    if (pending_decision_ || head_ >= queue_size_) {
        return std::nullopt;
    }
    return queue_[head_];
}

double Event::official(double reached_m) const noexcept {
    return std::min(reached_m, line_m_);
}

double Event::official_pull_off(double reached_m) const noexcept {
    return std::min(reached_m, rules_.pull_off_full_pull_m);
}

void Event::insert(std::size_t position, const Pull& pull) noexcept {
    const std::size_t at = std::clamp(position, head_, queue_size_);
    if (queue_size_ >= kMaxQueue) {
        return;
    }
    for (std::size_t i = queue_size_; i > at; --i) {
        queue_[i] = queue_[i - 1];
    }
    queue_[at] = pull;
    ++queue_size_;
}

void Event::schedule_repull(int puller) noexcept {
    // In the main round queue index i is pull number i + 1: the re-pull
    // follows the pull `repull_after_pulls` after the test pull.
    const Entry& entry = entries_[static_cast<std::size_t>(puller)];
    const int pull_number = entry.test_pull_number + rules_.repull_after_pulls;
    insert(static_cast<std::size_t>(pull_number),
           {.puller = puller, .kind = PullKind::kRepull, .pull_off_round = 0});
}

int Event::steps_for(double distance_m) const noexcept {
    const int steps = static_cast<int>(std::ceil(distance_m / step_estimate_m_));
    return std::clamp(steps, 1, rules_.max_steps_per_change);
}

void Event::learn(double reached_m) noexcept {
    if (last_change_steps_ == 0) {
        return;
    }
    // Heavier should shorten the pull, lighter lengthen it; a test puller
    // who goes the other way is simply stronger or weaker, and says little.
    const double gained_m =
        last_change_steps_ > 0 ? last_test_m_ - reached_m : reached_m - last_test_m_;
    const double per_step_m = gained_m / std::abs(last_change_steps_);
    last_change_steps_ = 0;
    if (per_step_m <= 0.0) {
        return;
    }
    const double seen_m =
        std::clamp(per_step_m, kLowestStepShare * rules_.step_m, kHighestStepShare * rules_.step_m);
    // A pull stopped at the maximum distance shows only "at least this".
    const bool capped = std::max(last_test_m_, reached_m) >= rules_.max_distance_m;
    const double blended = step_estimate_m_ + kLearningRate * (seen_m - step_estimate_m_);
    step_estimate_m_ = capped ? std::max(step_estimate_m_, blended) : blended;
}

Outcome Event::judge_test(int puller, double reached_m) noexcept {
    Entry& entry = entries_[static_cast<std::size_t>(puller)];
    entry.may_keep = true;
    Outcome outcome;
    // Far short of what this tractor was expected to pull (on the sled as
    // it is now: each step heavier is worth a step less): trouble with the
    // tractor, not a sled too heavy. The next puller tests the sled. Not
    // twice in a row: two tractors far short in turn say the sled is too
    // heavy for the day (the track wetter than the gear trials told), and
    // the operator judges it by this test pull.
    const double expected_m = expected_m_[static_cast<std::size_t>(puller)];
    if (rules_.trouble_share && expected_m > 0.0 && troubles_in_a_row_ < kMaxTroublesInARow &&
        reached_m < *rules_.trouble_share *
                        (expected_m - static_cast<double>(net_steps_) * step_estimate_m_)) {
        ++troubles_in_a_row_;
        outcome.change = SledChange::kTroubleSuspected;
        retest_next();
        return outcome;
    }
    troubles_in_a_row_ = 0;
    learn(reached_m);
    const bool standard_line = line_m_ == rules_.full_pull_m;
    if (standard_line && reached_m >= rules_.full_pull_m &&
        reached_m < rules_.extended_full_pull_m) {
        line_m_ = rules_.extended_full_pull_m;
        outcome.change = SledChange::kLineExtended;
        return outcome;
    }
    // After a change a short test pull changes the sled again only if it
    // misses by more than half a step, where another step would come closer:
    // the first change is usually enough. A full pull is a full pull.
    const double margin_m = changes_ > 0 ? kRetestMarginSteps * step_estimate_m_ : 0.0;
    // What the field's average tractor would reach on this sled: a weak
    // tractor's short test pull may still leave the sled right for the
    // field. The average, not the strongest: one tractor far stronger than
    // the rest (a player who built theirs up) would otherwise have the sled
    // set for it alone, too heavy for everyone else.
    const double average_m = reached_m + gap_to_average_m(puller);
    if (reached_m >= line_m_) {
        if (rules_.strong_full_pull_from && strength(puller) >= *rules_.strong_full_pull_from) {
            // One of the strongest pulled it the whole way: as meant.
            return outcome;
        }
        // Too easy: heavier, so that the average stops at the target.
        outcome = {.change = SledChange::kHeavier,
                   .steps = steps_for(average_m - rules_.sled_target_m)};
        sled_ = sled::heavier(sled_, outcome.steps);
        entry.may_keep = false;
        schedule_repull(puller);
    } else if (average_m < rules_.sled_target_m - margin_m) {
        outcome = {.change = SledChange::kLighter,
                   .steps = steps_for(rules_.sled_target_m - average_m)};
        sled_ = lighter(sled_, outcome.steps, rules_.step_m, rules_.max_distance_m, average_m);
    } else {
        return outcome;
    }
    last_test_m_ = reached_m;
    last_change_steps_ = outcome.change == SledChange::kHeavier ? outcome.steps : -outcome.steps;
    net_steps_ += last_change_steps_;
    ++changes_;
    retest_next();
    return outcome;
}

bool Event::may_defer() const noexcept {
    const std::optional<Pull> pull = next();
    // Not the last to pull: there is nowhere later to go.
    return pull && pull->kind != PullKind::kPullOff && head_ + 1 < queue_size_ &&
           !entries_[static_cast<std::size_t>(pull->puller)].deferred;
}

int Event::defer(int places) noexcept {
    const std::optional<Pull> pull = next();
    if (!may_defer() || !pull || places <= 0) {
        return 0;
    }
    entries_[static_cast<std::size_t>(pull->puller)].deferred = true;
    // queue_size_ - 1 is safe: next() named a pull, so the queue has one.
    const std::size_t to = std::min(head_ + static_cast<std::size_t>(places), queue_size_ - 1);
    for (std::size_t i = head_; i < to; ++i) {
        queue_[i] = queue_[i + 1];
    }
    queue_[to] = *pull;
    if (pull->kind == PullKind::kTest && to > head_) {
        queue_[to].kind = PullKind::kCompetition;
        retest_next();
    }
    return static_cast<int>(to - head_);
}

void Event::retest_next() noexcept {
    if (head_ < queue_size_ && queue_[head_].kind == PullKind::kCompetition) {
        queue_[head_].kind = PullKind::kTest;
    }
}

Outcome Event::record(double reached_m) noexcept {
    return record_pull(reached_m, false);
}

Outcome Event::record_disqualified() noexcept {
    return record_pull(0.0, true);
}

Outcome Event::record_pull(double reached_m, bool disqualified) noexcept {
    const std::optional<Pull> pull = next();
    if (!pull) {
        return {};
    }
    const double reached = std::clamp(reached_m, 0.0, rules_.max_distance_m);
    Entry& entry = entries_[static_cast<std::size_t>(pull->puller)];
    ++head_;
    ++completed_;
    Outcome outcome;
    switch (pull->kind) {
        case PullKind::kTest:
            if (disqualified) {
                entry.pulled = true;
                entry.reached_m = 0.0;
                entry.disqualified = true;
                retest_next();
                break;
            }
            entry.tested = true;
            entry.test_reached_m = reached;
            entry.test_pull_number = completed_;
            outcome = judge_test(pull->puller, reached);
            break;
        case PullKind::kCompetition:
        case PullKind::kRepull:
            entry.pulled = true;
            entry.reached_m = reached;
            entry.disqualified = disqualified;
            break;
        case PullKind::kPullOff:
            entry.pull_off_rounds = pull->pull_off_round;
            entry.pull_off_reached_m = reached;
            entry.pull_off_history_m[static_cast<std::size_t>(pull->pull_off_round - 1)] = reached;
            break;
    }
    advance();
    return outcome;
}

void Event::advance() noexcept {
    if (pull_off_round_ == 0) {
        // The earliest test puller whose decision is due: after their turns,
        // or at the end of the round at the latest.
        const bool round_over = head_ >= queue_size_;
        int due = -1;
        for (int i = 0; i < count_; ++i) {
            const Entry& e = entries_[static_cast<std::size_t>(i)];
            const bool turn = e.test_pull_number + rules_.decide_after_pulls <= completed_;
            if (e.may_keep && (turn || round_over) &&
                (due < 0 ||
                 e.test_pull_number < entries_[static_cast<std::size_t>(due)].test_pull_number)) {
                due = i;
            }
        }
        if (due >= 0) {
            pending_decision_ = due;
            return;
        }
    }
    if (head_ < queue_size_) {
        return;
    }
    start_pull_off();
}

std::optional<int> Event::decision_due() const noexcept {
    return pending_decision_;
}

bool Event::suggested_keep() const noexcept {
    if (!pending_decision_) {
        return false;
    }
    const Entry& entry = entries_[static_cast<std::size_t>(*pending_decision_)];
    if (entry.test_reached_m >= line_m_) {
        return true;
    }
    // Everyone else's pulls since count, test pulls too: they show what the
    // sled allows.
    double best_m = 0.0;
    for (int i = 0; i < count_; ++i) {
        const Entry& other = entries_[static_cast<std::size_t>(i)];
        if (i == *pending_decision_) {
            continue;
        }
        if (other.pulled) {
            best_m = std::max(best_m, other.reached_m);
        }
        if (other.tested) {
            best_m = std::max(best_m, other.test_reached_m);
        }
    }
    return entry.test_reached_m >= best_m;
}

void Event::decide(bool keep) noexcept {
    if (!pending_decision_) {
        return;
    }
    const int puller = *pending_decision_;
    Entry& entry = entries_[static_cast<std::size_t>(puller)];
    entry.may_keep = false;
    pending_decision_.reset();
    if (keep) {
        entry.pulled = true;
        entry.reached_m = entry.test_reached_m;
        entry.disqualified = false;
    } else {
        schedule_repull(puller);
    }
    advance();
}

std::optional<int> Event::best_short_of_the_line(
    const std::array<bool, kMaxPullers>& in) const noexcept {
    std::optional<int> best;
    for (int i = 0; i < count_; ++i) {
        const Entry& e = entries_[static_cast<std::size_t>(i)];
        const bool qualifies = !in[static_cast<std::size_t>(i)] && e.pulled && !e.disqualified &&
                               e.reached_m >= line_m_ - rules_.qualifying_below_line_m;
        if (qualifies &&
            (!best || e.reached_m > entries_[static_cast<std::size_t>(*best)].reached_m)) {
            best = i;
        }
    }
    return best;
}

void Event::start_pull_off() noexcept {
    // Who pulls in the next pull-off round, in running order: after the
    // main round every full pull, filled up to the places with the best
    // runs from `qualifying_below_line_m` short of the line on; after a
    // pull-off, those who reached the pull-off line.
    std::array<bool, kMaxPullers> in{};
    int qualifiers = 0;
    double best_full_m = -1.0;
    const double line_m = pull_off_round_ == 0 ? line_m_ : rules_.pull_off_full_pull_m;
    for (int i = 0; i < count_; ++i) {
        const Entry& e = entries_[static_cast<std::size_t>(i)];
        const bool candidate =
            pull_off_round_ == 0 ? e.pulled : e.pull_off_rounds == pull_off_round_;
        const double reached = pull_off_round_ == 0 ? e.reached_m : e.pull_off_reached_m;
        if (candidate && reached >= line_m) {
            in[static_cast<std::size_t>(i)] = true;
            ++qualifiers;
            best_full_m = std::max(best_full_m, reached);
        }
    }
    while (pull_off_round_ == 0 && qualifiers < rules_.pull_off_places) {
        const std::optional<int> best = best_short_of_the_line(in);
        if (!best) {
            break;
        }
        in[static_cast<std::size_t>(*best)] = true;
        ++qualifiers;
    }
    if (qualifiers < 2 || pull_off_round_ >= kMaxPullOffRounds) {
        queue_size_ = 0;
        head_ = 0;
        return;
    }
    // The first pull-off: heavy enough that the strongest full pull stops
    // at the main round's line, short of the pull-off line; one step with
    // no full pull. A full pull stopped at the line (as the run stops it),
    // or within a step of it, shows no margin: the operator then changes
    // the sled the most they do at once. A further pull-off: those left all
    // reached the pull-off line, where their distance is cut off too: twice
    // the last round's steps and the most at once at least, so that a third
    // round is rare.
    int steps = 1;
    if (pull_off_round_ > 0) {
        steps = std::max(2 * pull_off_steps_, rules_.max_steps_per_change);
    } else if (best_full_m >= line_m_ + step_estimate_m_) {
        steps = steps_for(best_full_m - line_m_);
    } else if (best_full_m >= line_m_) {
        // At the line or within a step of it: no margin worth reading.
        steps = rules_.max_steps_per_change;
    }
    pull_off_steps_ = steps;
    sled_ = sled::heavier(sled_, steps);
    ++pull_off_round_;
    queue_size_ = 0;
    head_ = 0;
    for (int i = 0; i < count_; ++i) {
        const int puller = order_[static_cast<std::size_t>(i)];
        if (in[static_cast<std::size_t>(puller)]) {
            queue_[queue_size_++] = {
                .puller = puller, .kind = PullKind::kPullOff, .pull_off_round = pull_off_round_};
        }
    }
}

bool Event::finished() const noexcept {
    return !pending_decision_ && head_ >= queue_size_;
}

std::span<const Standing> Event::standings() noexcept {
    std::size_t size = 0;
    std::array<int, kMaxPullers> position{};
    for (int i = 0; i < count_; ++i) {
        position[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])] = i;
    }
    for (int i = 0; i < count_; ++i) {
        const Entry& e = entries_[static_cast<std::size_t>(i)];
        if (!e.pulled) {
            continue;
        }
        Standing& standing = standings_[size++];
        standing = {.puller = i,
                    .place = 0,
                    .distance_m = official(e.reached_m),
                    .full_pull = e.reached_m >= line_m_,
                    .disqualified = e.disqualified,
                    .pull_off_rounds = e.pull_off_rounds,
                    .pull_off_distances_m = {}};
        for (int r = 0; r < e.pull_off_rounds; ++r) {
            const auto round = static_cast<std::size_t>(r);
            standing.pull_off_distances_m[round] = official_pull_off(e.pull_off_history_m[round]);
        }
    }
    const auto ahead = [](const Standing& a, const Standing& b) {
        if (a.pull_off_rounds != b.pull_off_rounds) {
            return a.pull_off_rounds > b.pull_off_rounds;
        }
        if (a.pull_off_distance_m() != b.pull_off_distance_m()) {
            return a.pull_off_distance_m() > b.pull_off_distance_m();
        }
        if (a.disqualified != b.disqualified) {
            return b.disqualified;
        }
        return a.distance_m > b.distance_m;
    };
    const std::span<Standing> list{standings_.data(), size};
    // Equal results keep the running order, so the list is the same on
    // every platform.
    std::ranges::sort(list, [&](const Standing& a, const Standing& b) {
        if (ahead(a, b) || ahead(b, a)) {
            return ahead(a, b);
        }
        return position[static_cast<std::size_t>(a.puller)] <
               position[static_cast<std::size_t>(b.puller)];
    });
    for (std::size_t i = 0; i < size; ++i) {
        const bool tied = i > 0 && !ahead(list[i - 1], list[i]);
        list[i].place = tied ? list[i - 1].place : static_cast<int>(i) + 1;
    }
    return list;
}

std::array<int, kMaxPullers> running_order(int count, std::uint64_t seed) noexcept {
    std::array<int, kMaxPullers> order{};
    const int n = std::clamp(count, 0, kMaxPullers);
    for (int i = 0; i < n; ++i) {
        order[static_cast<std::size_t>(i)] = i;
    }
    // Fisher-Yates.
    math::SplitMix64 random{seed};
    for (int i = n - 1; i > 0; --i) {
        const auto j = static_cast<int>(random.below(static_cast<std::uint64_t>(i) + 1U));
        std::swap(order[static_cast<std::size_t>(i)], order[static_cast<std::size_t>(j)]);
    }
    return order;
}

}  // namespace tp::event
