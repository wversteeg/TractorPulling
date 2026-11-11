#include "tp/core/field/play.hpp"

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/math/random.hpp"

namespace tp::field {

run::Rules trial_pull_rules(const event::Rules& rules, const Venue& venue,
                            double full_pull_m) noexcept {
    return {.track_length_m = rules.max_distance_m,
            .max_duration_s = venue.max_duration_s,
            .lane_width_m = venue.lane_width_m,
            .full_pull_line_m = full_pull_m};
}

run::Rules pull_rules(const event::Rules& rules, const Venue& venue, double full_pull_m,
                      event::PullKind kind) noexcept {
    run::Rules out = trial_pull_rules(rules, venue, full_pull_m);
    if (kind == event::PullKind::kCompetition || kind == event::PullKind::kRepull) {
        out.track_length_m = full_pull_m;
    }
    return out;
}

std::uint64_t log_hash(std::span<const LogEntry> log) noexcept {
    // Integers as their 64-bit two's complement, so -1 hashes the same
    // everywhere.
    const auto add_int = [](hash::Fnv1a& hash, std::int64_t value) {
        hash.add(static_cast<std::uint64_t>(value));
    };
    hash::Fnv1a hash;
    for (const LogEntry& entry : log) {
        add_int(hash, entry.pull.puller);
        add_int(hash, static_cast<std::int64_t>(entry.pull.kind));
        add_int(hash, entry.pull.pull_off_round);
        hash.add(entry.reached_m);
        hash.add(entry.disqualified);
        add_int(hash, static_cast<std::int64_t>(entry.outcome.change));
        add_int(hash, entry.outcome.steps);
        add_int(hash, entry.gear);
        hash.add(entry.full_pull_m);
        add_int(hash, entry.knives);
        hash.add(entry.pushdown_start_m);
        add_int(hash, entry.deciding);
        hash.add(entry.keep);
        // Only the player moves back, never a scripted puller: an event
        // without it hashes as it always has.
        if (entry.moved_back != 0) {
            add_int(hash, entry.moved_back);
        }
    }
    return hash.value();
}

std::optional<PlayedEvent> play_event(const event::Rules& rules, const Venue& venue,
                                      const Field& field, const Variation& variation,
                                      const PullToPull& pull_to_pull,
                                      std::span<const int> running_order,
                                      std::uint64_t seed) noexcept {
    if (running_order.size() != static_cast<std::size_t>(field.count)) {
        return std::nullopt;
    }
    std::optional<event::Event> event = event::Event::create(rules, venue.sled, running_order);
    if (!event) {
        return std::nullopt;
    }
    PlayedEvent played{.event = *event, .log = {}, .log_size = 0, .gears = {}};

    // Every driver reads the track and picks a gear for the day.
    math::SplitMix64 guesses{seed};
    Field today = field;
    for (int i = 0; i < field.count; ++i) {
        Puller& puller = today.pullers[static_cast<std::size_t>(i)];
        const std::optional<GearTrial> best = best_gear_trial(
            puller, venue.sled, venue.track, variation.lowest_gear, variation.highest_gear,
            trial_pull_rules(rules, venue, rules.full_pull_m));
        if (!best) {
            return std::nullopt;
        }
        // The sled operator reckons with how far each tractor should pull.
        played.event.expect(i, best->distance_m);
        puller.driver.gear =
            guess_gear(puller, best->gear, variation.lowest_gear, variation.highest_gear, guesses);
        played.gears[static_cast<std::size_t>(i)] = puller.driver.gear;
    }

    math::SplitMix64 pulls{seed ^ kPullStream};
    // The track the crews walk.
    const std::optional<track::Model> walked = track::Model::create(venue.track);
    if (!walked) {
        return std::nullopt;
    }
    const track::Model& track = *walked;
    event::Event& ev = played.event;
    while (!ev.finished() && played.log_size < kMaxLog) {
        LogEntry& entry = played.log[played.log_size++];
        if (const std::optional<int> deciding = ev.decision_due()) {
            entry.deciding = *deciding;
            entry.keep = ev.suggested_keep();
            ev.decide(entry.keep);
            continue;
        }
        const std::optional<event::Pull> pull = ev.next();
        if (!pull) {
            return std::nullopt;
        }
        ThisPull this_pull = on_this_pull(today.pullers[static_cast<std::size_t>(pull->puller)],
                                          ev.sled(), pull_to_pull, pulls);
        const run::Rules this_rules = pull_rules(rules, venue, ev.full_pull_m(), pull->kind);
        // The crew walks the lane and puts the sled where it reads it best.
        this_pull.puller.driver.start_y_m =
            crew_line(crew_report(track, pull->kind, ev.pulls_done(), this_rules,
                                  crew_seed(seed, pull->puller, ev.pulls_done())),
                      this_pull.puller, this_rules);
        const Puller& puller = this_pull.puller;
        const std::optional<run::Result> result =
            run_pull(puller, this_pull.sled, venue.track, this_rules);
        if (!result) {
            return std::nullopt;
        }
        entry.pull = *pull;
        entry.reached_m = result->distance_m;
        entry.disqualified = result->ending == run::Ending::kOutOfBounds;
        entry.gear = puller.driver.gear;
        entry.outcome =
            entry.disqualified ? ev.record_disqualified() : ev.record(result->distance_m);
        entry.full_pull_m = ev.full_pull_m();
        entry.knives = ev.sled().knives.count;
        entry.pushdown_start_m = ev.sled().pushdown_start_m;
        entry.box_mass_kg = ev.sled().box_mass_kg;
    }
    if (!ev.finished()) {
        return std::nullopt;
    }
    return played;
}

}  // namespace tp::field
