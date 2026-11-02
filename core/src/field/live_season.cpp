#include "tp/core/field/live_season.hpp"

#include "tp/core/math/random.hpp"

#include <algorithm>

namespace tp::field {

std::uint64_t event_seed(std::uint64_t season_seed, int number) noexcept {
    return math::SplitMix64{season_seed + static_cast<std::uint64_t>(number)}.next();
}

std::optional<LiveSeason> LiveSeason::create(const event::Rules& rules, const Puller& reference,
                                             const Variation& variation,
                                             const rules::ClassRules& class_rules,
                                             const PullToPull& pull_to_pull,
                                             const season::Points& points, int pullers, int events,
                                             std::uint64_t seed) noexcept {
    // An event needs a rival or more.
    if (pullers < 2 || events < 1 || events > season::kMaxEvents) {
        return std::nullopt;
    }
    std::optional<Field> field = generate(reference, variation, class_rules, pullers, seed);
    const std::optional<season::Championship> championship =
        season::Championship::create(points, pullers);
    if (!field || !championship) {
        return std::nullopt;
    }
    // The player pulls the class's reference tractor, driven by hand.
    field->pullers[static_cast<std::size_t>(kPlayer)] = reference;
    return LiveSeason{rules, variation, pull_to_pull, *field, *championship, events, seed};
}

LiveSeason::LiveSeason(const event::Rules& rules, const Variation& variation,
                       const PullToPull& pull_to_pull, const Field& field,
                       const season::Championship& championship, int events,
                       std::uint64_t seed) noexcept
    : rules_(rules),
      variation_(variation),
      pull_to_pull_(pull_to_pull),
      field_(field),
      championship_(championship),
      events_(events),
      seed_(seed) {}

bool LiveSeason::start_event(const Venue& venue) noexcept {
    if (!scored_ || started_ >= events_) {
        return false;
    }
    const std::uint64_t seed = event_seed(seed_, started_);
    const auto order = event::running_order(field_.count, seed);
    event_ = LiveEvent::create(rules_, venue, field_, variation_, pull_to_pull_,
                               std::span{order.data(), static_cast<std::size_t>(field_.count)},
                               kPlayer, seed + 1);
    if (!event_.has_value()) {
        return false;
    }
    ++started_;
    scored_ = false;
    return true;
}

bool LiveSeason::score_event() noexcept {
    if (scored_ || !event_.has_value() || event_->turn() != LiveEvent::Turn::kFinished) {
        return false;
    }
    const std::span<const event::Standing> standings = event_->standings();
    scored_ = championship_.add(standings);
    if (scored_) {
        std::ranges::copy(standings,
                          results_[static_cast<std::size_t>(events_scored() - 1)].begin());
    }
    return scored_;
}

bool LiveSeason::score_event(std::span<const std::int64_t> prize_by_place,
                             garage::Career& career) noexcept {
    const std::optional<event::Standing> standing = player_standing();
    if (!score_event()) {
        return false;
    }
    if (standing) {
        career.money_eur += garage::prize_eur(prize_by_place, *standing);
    }
    return true;
}

std::optional<event::Standing> LiveSeason::player_standing() noexcept {
    if (!event_.has_value()) {
        return std::nullopt;
    }
    for (const event::Standing& s : event_->standings()) {
        if (s.puller == kPlayer) {
            return s;
        }
    }
    return std::nullopt;
}

bool LiveSeason::player_may_change_tractor() const noexcept {
    if (!event_.has_value() || event_->turn() == LiveEvent::Turn::kFinished) {
        return true;
    }
    return std::ranges::none_of(event_->log(), [](const LogEntry& e) {
        return e.deciding < 0 && e.pull.puller == kPlayer;
    });
}

bool LiveSeason::add_pulled_event(std::span<const event::Standing> standings) noexcept {
    if (!scored_ || started_ >= events_ ||
        standings.size() != static_cast<std::size_t>(field_.count)) {
        return false;
    }
    // Each of the field's pullers once.
    std::array<bool, event::kMaxPullers> seen{};
    for (const event::Standing& s : standings) {
        if (s.puller < 0 || s.puller >= field_.count || s.place < 1 || s.place > field_.count ||
            seen[static_cast<std::size_t>(s.puller)]) {
            return false;
        }
        seen[static_cast<std::size_t>(s.puller)] = true;
    }
    if (!championship_.add(standings)) {
        return false;
    }
    std::ranges::copy(standings, results_[static_cast<std::size_t>(events_scored() - 1)].begin());
    ++started_;
    event_.reset();
    return true;
}

std::span<const event::Standing> LiveSeason::event_standings(int number) const noexcept {
    if (number < 0 || number >= events_scored()) {
        return {};
    }
    return {results_[static_cast<std::size_t>(number)].data(),
            static_cast<std::size_t>(field_.count)};
}

}  // namespace tp::field
