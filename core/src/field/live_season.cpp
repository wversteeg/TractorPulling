#include "tp/core/field/live_season.hpp"

#include "tp/core/math/random.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace tp::field {

namespace {

// The guests' draws, apart from the field's and the events'.
constexpr std::uint64_t kGuestStream = 0xd1b54a32d192ed03ULL;

// A field of none: a puller has no empty state, so its slots hold the
// reference.
template <std::size_t... I>
Field no_one(const Puller& reference, std::index_sequence<I...> /*slots*/) noexcept {
    return Field{.pullers = {{((void)I, reference)...}}, .count = 0};
}

}  // namespace

std::uint64_t event_seed(std::uint64_t season_seed, int number) noexcept {
    return math::SplitMix64{season_seed + static_cast<std::uint64_t>(number)}.next();
}

bool guests_valid(const Guests& guests, int competitors) noexcept {
    return guests.pool >= 0 && guests.pool <= event::kMaxPullers && guests.per_event >= 0 &&
           guests.per_event <= kMaxGuests && guests.per_event <= guests.pool &&
           competitors + guests.per_event <= event::kMaxPullers;
}

std::optional<Field> guest_pool(const Puller& reference, const Guests& guests,
                                const rules::ClassRules& class_rules,
                                std::uint64_t season_seed) noexcept {
    if (guests.pool == 0) {
        return no_one(reference, std::make_index_sequence<event::kMaxPullers>{});
    }
    return generate(reference, guests.variation, class_rules, guests.pool,
                    season_seed ^ kGuestStream);
}

std::array<int, kMaxGuests> guests_at(std::uint64_t season_seed, int number,
                                      const Guests& guests) noexcept {
    std::array<int, kMaxGuests> chosen{};
    const int pool = std::clamp(guests.pool, 0, event::kMaxPullers);
    const int count = std::clamp(guests.per_event, 0, std::min(pool, kMaxGuests));
    std::array<int, event::kMaxPullers> left{};
    for (int i = 0; i < pool; ++i) {
        left[static_cast<std::size_t>(i)] = i;
    }
    // The first `count` of a Fisher-Yates shuffle of the pool.
    math::SplitMix64 random{event_seed(season_seed, number) ^ kGuestStream};
    for (int i = 0; i < count; ++i) {
        const auto j = i + static_cast<int>(random.below(static_cast<std::uint64_t>(pool - i)));
        std::swap(left[static_cast<std::size_t>(i)], left[static_cast<std::size_t>(j)]);
        chosen[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)];
    }
    return chosen;
}

Field with_guests(const Field& competitors, const Field& pool,
                  std::span<const int> chosen) noexcept {
    Field out = competitors;
    for (const int guest : chosen) {
        if (guest < 0 || guest >= pool.count || out.count >= event::kMaxPullers) {
            continue;
        }
        out.pullers[static_cast<std::size_t>(out.count++)] =
            pool.pullers[static_cast<std::size_t>(guest)];
    }
    return out;
}

std::optional<LiveSeason> LiveSeason::create(const event::Rules& rules, const Puller& reference,
                                             const Variation& variation,
                                             const rules::ClassRules& class_rules,
                                             const PullToPull& pull_to_pull,
                                             const season::Points& points, int pullers, int events,
                                             std::uint64_t seed, const Guests& guests) noexcept {
    // An event needs a rival or more.
    if (pullers < 2 || events < 1 || events > season::kMaxEvents ||
        !guests_valid(guests, pullers)) {
        return std::nullopt;
    }
    std::optional<Field> field = generate(reference, variation, class_rules, pullers, seed);
    const std::optional<Field> pool = guest_pool(reference, guests, class_rules, seed);
    const std::optional<season::Championship> championship =
        season::Championship::create(points, pullers);
    if (!field || !pool || !championship) {
        return std::nullopt;
    }
    // The player pulls the class's reference tractor, driven by hand.
    field->pullers[static_cast<std::size_t>(kPlayer)] = reference;
    return LiveSeason{rules, variation,     pull_to_pull, *field, guests,
                      *pool, *championship, events,       seed};
}

LiveSeason::LiveSeason(const event::Rules& rules, const Variation& variation,
                       const PullToPull& pull_to_pull, const Field& field, const Guests& guests,
                       const Field& guest_pool, const season::Championship& championship,
                       int events, std::uint64_t seed) noexcept
    : rules_(rules),
      variation_(variation),
      pull_to_pull_(pull_to_pull),
      field_(field),
      guests_(guests),
      guest_pool_(guest_pool),
      championship_(championship),
      events_(events),
      seed_(seed) {}

bool LiveSeason::start_event(const Venue& venue) noexcept {
    return start_with(venue, field_);
}

bool LiveSeason::start_event(const Venue& venue, const vehicle::Params& player_tractor) noexcept {
    Field field = field_;
    field.pullers[static_cast<std::size_t>(kPlayer)].tractor = player_tractor;
    return start_with(venue, field);
}

bool LiveSeason::start_with(const Venue& venue, const Field& competitors) noexcept {
    if (!scored_ || started_ >= events_) {
        return false;
    }
    const std::array<int, kMaxGuests> chosen = guests_at_event(started_);
    const Field field =
        with_guests(competitors, guest_pool_,
                    std::span{chosen.data(), static_cast<std::size_t>(guests_.per_event)});
    const std::uint64_t seed = event_seed(seed_, started_);
    const auto order = event::running_order(field.count, seed);
    event_ = LiveEvent::create(rules_, venue, field, variation_, pull_to_pull_,
                               std::span{order.data(), static_cast<std::size_t>(field.count)},
                               kPlayer, seed + 1, guests_.per_event);
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
        const auto number = static_cast<std::size_t>(events_scored() - 1);
        std::ranges::copy(standings, results_[number].begin());
        result_sizes_[number] = static_cast<int>(standings.size());
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
    const int lines = field_.count + guests_.per_event;
    if (!scored_ || started_ >= events_ || standings.size() != static_cast<std::size_t>(lines)) {
        return false;
    }
    // Each of the field's pullers once, and each of the event's guests once,
    // marked as guests.
    std::array<bool, event::kMaxPullers> seen{};
    for (const event::Standing& s : standings) {
        if (s.puller < 0 || s.puller >= lines || s.place < 1 || s.place > lines ||
            s.guest != (s.puller >= field_.count) || seen[static_cast<std::size_t>(s.puller)]) {
            return false;
        }
        seen[static_cast<std::size_t>(s.puller)] = true;
    }
    if (!championship_.add(standings)) {
        return false;
    }
    const auto number = static_cast<std::size_t>(events_scored() - 1);
    std::ranges::copy(standings, results_[number].begin());
    result_sizes_[number] = lines;
    ++started_;
    event_.reset();
    return true;
}

std::span<const event::Standing> LiveSeason::event_standings(int number) const noexcept {
    if (number < 0 || number >= events_scored()) {
        return {};
    }
    return {results_[static_cast<std::size_t>(number)].data(),
            static_cast<std::size_t>(result_sizes_[static_cast<std::size_t>(number)])};
}

}  // namespace tp::field
