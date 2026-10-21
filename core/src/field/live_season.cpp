#include "tp/core/field/live_season.hpp"

#include "tp/core/math/random.hpp"

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
    scored_ = championship_.add(event_->standings());
    return scored_;
}

}  // namespace tp::field
