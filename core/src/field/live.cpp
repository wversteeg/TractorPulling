#include "tp/core/field/live.hpp"

#include "tp/core/time/fixed_step_clock.hpp"

#include <algorithm>

namespace tp::field {

namespace {

constexpr double kDt_s =
    static_cast<double>(kSimulationStepNs) / static_cast<double>(kNanosecondsPerSecond);

}  // namespace

std::optional<LiveEvent> LiveEvent::create(const event::Rules& rules, const Venue& track_venue,
                                           const Field& field, const Variation& variation,
                                           const PullToPull& pull_to_pull,
                                           std::span<const int> running_order, int player,
                                           std::uint64_t seed) noexcept {
    // The track's spots where they lie today, as play_event lays them.
    const Venue venue = on_the_day(track_venue, seed);
    if (running_order.size() != static_cast<std::size_t>(field.count) || player < 0 ||
        player >= field.count ||
        run::validate(field::trial_pull_rules(rules, venue, rules.full_pull_m))) {
        return std::nullopt;
    }
    const std::optional<event::Event> event =
        event::Event::create(rules, venue.sled, running_order);
    if (!event) {
        return std::nullopt;
    }
    for (int i = 0; i < field.count; ++i) {
        const Puller& puller = field.pullers[static_cast<std::size_t>(i)];
        if (!pull::Model::create(
                {.tractor = puller.tractor, .sled = venue.sled, .track = venue.track})) {
            return std::nullopt;
        }
    }
    LiveEvent live{rules, venue, field, variation, pull_to_pull, *event, player, seed};
    live.settle();
    return live;
}

LiveEvent::LiveEvent(const event::Rules& rules, const Venue& venue, const Field& field,
                     const Variation& variation, const PullToPull& pull_to_pull,
                     const event::Event& event, int player, std::uint64_t seed) noexcept
    : rules_(rules),
      venue_(venue),
      field_(field),
      variation_(variation),
      pull_to_pull_(pull_to_pull),
      event_(event),
      player_(player),
      seed_(seed),
      guesses_(seed),
      pulls_(seed ^ kPullStream),
      trial_gear_(variation.lowest_gear) {}

LiveEvent::Turn LiveEvent::turn() const noexcept {
    if (failed_) {
        return Turn::kFailed;
    }
    if (choosing_) {
        return Turn::kRivals;
    }
    if (event_.finished()) {
        return Turn::kFinished;
    }
    if (const std::optional<int> deciding = event_.decision_due()) {
        return *deciding == player_ ? Turn::kPlayerDecision : Turn::kRivals;
    }
    const std::optional<event::Pull> next = event_.next();
    return next && next->puller == player_ ? Turn::kPlayerPull : Turn::kRivals;
}

run::Rules LiveEvent::pull_rules() const noexcept {
    // A practice pull, with no pull of the event's due, ends at the line as
    // a competition pull does.
    const std::optional<event::Pull> next = event_.next();
    return field::pull_rules(rules_, venue_, event_.full_pull_m(),
                             next ? next->kind : event::PullKind::kCompetition);
}

int LiveEvent::gear(int puller) const noexcept {
    return puller >= 0 && puller < field_.count ? gears_[static_cast<std::size_t>(puller)] : 0;
}

std::optional<event::Pull> LiveEvent::pending_pull() const noexcept {
    return turn() == Turn::kPlayerPull ? event_.next() : std::nullopt;
}

std::int64_t LiveEvent::advance(std::int64_t max_steps) noexcept {
    std::int64_t done = 0;
    while (done < max_steps && turn() == Turn::kRivals) {
        if (!job_.has_value()) {
            start_next();
            if (failed_) {
                return done;
            }
            continue;
        }
        Job& job = *job_;
        done += step_job(job, max_steps - done);
        if (!job.state.result.has_value()) {
            continue;
        }
        const run::Result result = *job.state.result;
        job_.reset();
        if (choosing_) {
            finish_trial(result.distance_m);
        } else {
            finish_pull(result);
        }
    }
    return done;
}

void LiveEvent::start_next() noexcept {
    // The player's tractor is tried too, as play_event tries every
    // puller's: the sled operator reckons with how far it should pull. The
    // player picks their own gear; the guess drawn for them goes unused.
    if (choosing_) {
        Puller trying = field_.pullers[static_cast<std::size_t>(trial_puller_)];
        trying.driver.gear = trial_gear_;
        start_job(trying, venue_.sled, field::trial_pull_rules(rules_, venue_, rules_.full_pull_m));
        return;
    }
    if (event_.decision_due().has_value()) {
        settle();
        return;
    }
    const std::optional<event::Pull> next = event_.next();
    if (!next.has_value()) {
        failed_ = true;
        return;
    }
    ThisPull pull = on_this_pull(field_.pullers[static_cast<std::size_t>(next->puller)],
                                 event_.sled(), pull_to_pull_, pulls_);
    const run::Rules rules = pull_rules();
    // The crew walks the lane and puts the sled where it reads it best, as
    // in play_event.
    const std::optional<track::Model> track = track::Model::create(venue_.track);
    if (!track) {
        failed_ = true;
        return;
    }
    pull.puller.driver.start_y_m =
        crew_line(crew_report(*track, next->kind, event_.pulls_done(), rules,
                              crew_seed(seed_, next->puller, event_.pulls_done())),
                  pull.puller, rules);
    if (start_job(pull.puller, pull.sled, rules)) {
        job_pull_ = next;
        job_gear_ = pull.puller.driver.gear;
    }
}

bool LiveEvent::start_job(const Puller& puller, const sled::Params& sled,
                          const run::Rules& rules) noexcept {
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = puller.tractor,
                             .sled = sled,
                             .track = venue_.track,
                             .start_y_m = puller.driver.start_y_m});
    if (!model) {
        failed_ = true;
        return false;
    }
    job_.emplace(Job{.model = *model,
                     .state = run::start(*model),
                     .plan = puller.driver,
                     .driver = {},
                     .rules = rules});
    return true;
}

// As run_pull, a slice at a time: the same steps, so the same result.
std::int64_t LiveEvent::step_job(Job& job, std::int64_t max_steps) noexcept {
    std::int64_t steps = 0;
    while (steps < max_steps && !job.state.result) {
        const double t_s = static_cast<double>(job.state.steps) * kDt_s;
        run::step(job.state, job.model, job.rules,
                  driver::input(job.plan, job.driver, t_s, job.state.pull.tractor,
                                job.model.tractor().params()),
                  kDt_s);
        ++steps;
    }
    return steps;
}

// As best_gear: on a tie the higher gear.
void LiveEvent::finish_trial(double reached_m) noexcept {
    if (trial_gear_ == variation_.lowest_gear) {
        lowest_m_ = reached_m;
    }
    if (reached_m >= best_m_) {
        best_m_ = reached_m;
        best_gear_ = trial_gear_;
    }
    ++trial_gear_;
    if (trial_gear_ > variation_.highest_gear) {
        choose(gear_for_the_day({.gear = best_gear_, .distance_m = best_m_},
                                {.gear = variation_.lowest_gear, .distance_m = lowest_m_}));
    }
}

// As guess_gear after best_gear, for the puller being tried; then the next.
void LiveEvent::choose(const GearTrial& day) noexcept {
    const auto at = static_cast<std::size_t>(trial_puller_);
    event_.expect(trial_puller_, day.distance_m);
    const int guess = guess_gear(field_.pullers[at], day.gear, variation_.lowest_gear,
                                 variation_.highest_gear, guesses_);
    field_.pullers[at].driver.gear = guess;
    gears_[at] = guess;
    ++trial_puller_;
    trial_gear_ = variation_.lowest_gear;
    best_m_ = -1.0;
    best_gear_ = 0;
    lowest_m_ = 0.0;
    if (trial_puller_ >= field_.count) {
        choosing_ = false;
        settle();
    }
}

void LiveEvent::finish_pull(const run::Result& result) noexcept {
    LogEntry* entry = next_entry();
    if (entry == nullptr || !job_pull_.has_value()) {
        failed_ = true;
        return;
    }
    entry->pull = *job_pull_;
    entry->reached_m = result.distance_m;
    entry->disqualified = result.ending == run::Ending::kOutOfBounds;
    entry->gear = job_gear_;
    entry->outcome =
        entry->disqualified ? event_.record_disqualified() : event_.record(result.distance_m);
    entry->full_pull_m = event_.full_pull_m();
    entry->knives = event_.sled().knives.count;
    entry->pushdown_start_m = event_.sled().pushdown_start_m;
    entry->box_mass_kg = event_.sled().box_mass_kg;
    job_pull_.reset();
    settle();
}

event::Outcome LiveEvent::record_player(const run::Result& result, int gear) noexcept {
    const std::optional<event::Pull> pull = pending_pull();
    if (!pull.has_value()) {
        return {};
    }
    LogEntry* entry = next_entry();
    if (entry == nullptr) {
        return {};
    }
    // The draw a scripted puller would take, so the rivals' draws stay
    // play_event's whoever the player is.
    static_cast<void>(on_this_pull(field_.pullers[static_cast<std::size_t>(player_)], event_.sled(),
                                   pull_to_pull_, pulls_));
    entry->pull = *pull;
    entry->reached_m = result.distance_m;
    entry->disqualified = result.ending == run::Ending::kOutOfBounds;
    entry->gear = gear;
    entry->outcome =
        entry->disqualified ? event_.record_disqualified() : event_.record(result.distance_m);
    entry->full_pull_m = event_.full_pull_m();
    entry->knives = event_.sled().knives.count;
    entry->pushdown_start_m = event_.sled().pushdown_start_m;
    entry->box_mass_kg = event_.sled().box_mass_kg;
    settle();
    return entry->outcome;
}

int LiveEvent::defer_player() noexcept {
    const std::optional<event::Pull> pull = pending_pull();
    if (!pull.has_value() || log_size_ >= log_.size()) {
        return 0;
    }
    const int moved = event_.defer(event::kDeferPlaces);
    if (moved == 0) {
        return 0;
    }
    LogEntry* entry = next_entry();
    if (entry == nullptr) {
        return moved;
    }
    entry->pull = *pull;
    entry->moved_back = moved;
    entry->full_pull_m = event_.full_pull_m();
    entry->knives = event_.sled().knives.count;
    entry->pushdown_start_m = event_.sled().pushdown_start_m;
    entry->box_mass_kg = event_.sled().box_mass_kg;
    return moved;
}

void LiveEvent::decide_player(bool keep) noexcept {
    if (turn() != Turn::kPlayerDecision) {
        return;
    }
    LogEntry* entry = next_entry();
    if (entry == nullptr) {
        return;
    }
    entry->deciding = player_;
    entry->keep = keep;
    event_.decide(keep);
    settle();
}

void LiveEvent::settle() noexcept {
    while (!failed_ && !choosing_ && !event_.finished()) {
        const std::optional<int> deciding = event_.decision_due();
        if (!deciding || *deciding == player_) {
            return;
        }
        LogEntry* entry = next_entry();
        if (entry == nullptr) {
            return;
        }
        entry->deciding = *deciding;
        entry->keep = event_.suggested_keep();
        event_.decide(entry->keep);
    }
}

LogEntry* LiveEvent::next_entry() noexcept {
    if (log_size_ >= kMaxLog) {
        failed_ = true;
        return nullptr;
    }
    LogEntry& entry = log_[log_size_++];
    entry = LogEntry{};
    return &entry;
}

crew::Report LiveEvent::player_crew_report() const noexcept {
    const std::optional<track::Model> track = track::Model::create(venue_.track);
    if (!track) {
        return {};
    }
    const std::optional<event::Pull> pull = pending_pull();
    const event::PullKind kind = pull ? pull->kind : event::PullKind::kCompetition;
    return crew_report(*track, kind, event_.pulls_done(), pull_rules(),
                       crew_seed(seed_, player_, event_.pulls_done()));
}

}  // namespace tp::field
