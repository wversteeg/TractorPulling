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

// Lighter by `steps`: the operator takes knives out first (quick to do);
// once there are none left, the push-down moves later instead, a step's
// worth each.
sled::Params lighter(const sled::Params& sled, int steps, double step_m) noexcept {
    const sled::Adjustment& step = sled.pull_off_step;
    if (step.knives <= 0) {
        return sled::adjusted(sled, step, -steps);
    }
    const int knife_steps = std::min(steps, sled.knives.count / step.knives);
    const sled::Params fewer_knives = sled::adjusted(sled, step, -knife_steps);
    return sled::adjusted(fewer_knives, {.pushdown_earlier_m = -step_m}, steps - knife_steps);
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
    const std::array distances = {rules.full_pull_m,    rules.extended_full_pull_m,
                                  rules.max_distance_m, rules.qualifying_distance_m,
                                  rules.sled_target_m,  rules.step_m};
    if (!std::ranges::all_of(distances, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (rules.qualifying_distance_m <= 0.0 || rules.qualifying_distance_m > rules.full_pull_m ||
        rules.sled_target_m <= 0.0 || rules.sled_target_m > rules.full_pull_m ||
        rules.full_pull_m > rules.extended_full_pull_m ||
        rules.extended_full_pull_m > rules.max_distance_m) {
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

std::optional<Pull> Event::next() const noexcept {
    if (pending_decision_ || head_ >= queue_size_) {
        return std::nullopt;
    }
    return queue_[head_];
}

double Event::official(double reached_m) const noexcept {
    return std::min(reached_m, line_m_);
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
    learn(reached_m);
    Outcome outcome;
    const bool standard_line = line_m_ == rules_.full_pull_m;
    if (standard_line && reached_m >= rules_.full_pull_m &&
        reached_m < rules_.extended_full_pull_m) {
        line_m_ = rules_.extended_full_pull_m;
        outcome.change = SledChange::kLineExtended;
        return outcome;
    }
    if (reached_m >= line_m_) {
        // Too easy: heavier, so that a pull like this stops at the line.
        outcome = {.change = SledChange::kHeavier, .steps = steps_for(reached_m - line_m_)};
        sled_ = sled::heavier(sled_, outcome.steps);
        entry.may_keep = false;
        schedule_repull(puller);
    } else if (reached_m < rules_.sled_target_m) {
        outcome = {.change = SledChange::kLighter,
                   .steps = steps_for(rules_.sled_target_m - reached_m)};
        sled_ = lighter(sled_, outcome.steps, rules_.step_m);
    } else {
        return outcome;
    }
    last_test_m_ = reached_m;
    last_change_steps_ = outcome.change == SledChange::kHeavier ? outcome.steps : -outcome.steps;
    // After every change the next puller tests the sled again.
    if (head_ < queue_size_ && queue_[head_].kind == PullKind::kCompetition) {
        queue_[head_].kind = PullKind::kTest;
    }
    return outcome;
}

Outcome Event::record(double reached_m) noexcept {
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
            entry.tested = true;
            entry.test_reached_m = reached;
            entry.test_pull_number = completed_;
            outcome = judge_test(pull->puller, reached);
            break;
        case PullKind::kCompetition:
        case PullKind::kRepull:
            entry.pulled = true;
            entry.reached_m = reached;
            break;
        case PullKind::kPullOff:
            entry.pull_off_rounds = pull->pull_off_round;
            entry.pull_off_reached_m = reached;
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
    } else {
        schedule_repull(puller);
    }
    advance();
}

void Event::start_pull_off() noexcept {
    // Who pulls in the next pull-off round, in running order: after the
    // main round every full pull, filled up to the places with the best
    // runs from the qualifying distance on; after a pull-off, those who
    // made a full pull again.
    std::array<bool, kMaxPullers> in{};
    int qualifiers = 0;
    double best_full_m = -1.0;
    for (int i = 0; i < count_; ++i) {
        const Entry& e = entries_[static_cast<std::size_t>(i)];
        const bool candidate =
            pull_off_round_ == 0 ? e.pulled : e.pull_off_rounds == pull_off_round_;
        const double reached = pull_off_round_ == 0 ? e.reached_m : e.pull_off_reached_m;
        if (candidate && reached >= line_m_) {
            in[static_cast<std::size_t>(i)] = true;
            ++qualifiers;
            best_full_m = std::max(best_full_m, reached);
        }
    }
    while (pull_off_round_ == 0 && qualifiers < rules_.pull_off_places) {
        int best = -1;
        for (int i = 0; i < count_; ++i) {
            const Entry& e = entries_[static_cast<std::size_t>(i)];
            if (!in[static_cast<std::size_t>(i)] && e.pulled &&
                e.reached_m >= rules_.qualifying_distance_m &&
                (best < 0 || e.reached_m > entries_[static_cast<std::size_t>(best)].reached_m)) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        in[static_cast<std::size_t>(best)] = true;
        ++qualifiers;
    }
    if (qualifiers < 2 || pull_off_round_ >= kMaxPullOffRounds) {
        queue_size_ = 0;
        head_ = 0;
        return;
    }
    // Heavy enough that the strongest pull so far stops at the line; one
    // step at least.
    const int steps = best_full_m >= line_m_ ? steps_for(best_full_m - line_m_) : 1;
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
        standings_[size++] = {.puller = i,
                              .place = 0,
                              .distance_m = official(e.reached_m),
                              .full_pull = e.reached_m >= line_m_,
                              .pull_off_rounds = e.pull_off_rounds,
                              .pull_off_distance_m = official(e.pull_off_reached_m)};
    }
    const auto ahead = [](const Standing& a, const Standing& b) {
        if (a.pull_off_rounds != b.pull_off_rounds) {
            return a.pull_off_rounds > b.pull_off_rounds;
        }
        if (a.pull_off_distance_m != b.pull_off_distance_m) {
            return a.pull_off_distance_m > b.pull_off_distance_m;
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
