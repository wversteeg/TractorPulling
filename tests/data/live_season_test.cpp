#include "tp/core/field/live_season.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace tp::field {
namespace {

// Few pullers and two gears, so the debug build plays the events quickly.
constexpr int kPullers = 5;
constexpr int kEvents = 2;
constexpr std::uint64_t kSeed = 7;
constexpr std::int64_t kSlice = 20'000;
// The player's every pull: short of the line, so the season is quick.
constexpr double kPlayerReach_m = 80.0;
constexpr int kPlayerGear = 5;

data::SeasonDefinition club_season() {
    auto season =
        data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / "club_season.toml");
    if (!season) {
        throw std::runtime_error(season.error().message);
    }
    data::SeasonDefinition definition = season.value();
    definition.event.variation.lowest_gear = 5;
    return definition;
}

std::unique_ptr<LiveSeason> start(const data::SeasonDefinition& d) {
    const data::EventDefinition& e = d.event;
    std::optional<LiveSeason> season =
        LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull,
                           d.points, kPullers, kEvents, kSeed);
    if (!season.has_value()) {
        throw std::runtime_error("the season was refused");
    }
    return std::make_unique<LiveSeason>(*season);
}

// Plays the event being pulled to the end: the rivals in slices, the
// player's pulls at kPlayerReach_m and their decisions as suggested.
void play_out(LiveEvent& event) {
    for (int guard = 0; guard < 100'000; ++guard) {
        switch (event.turn()) {
            case LiveEvent::Turn::kRivals:
                event.advance(kSlice);
                break;
            case LiveEvent::Turn::kPlayerPull:
                event.record_player({.ending = run::Ending::kStalled, .distance_m = kPlayerReach_m},
                                    kPlayerGear);
                break;
            case LiveEvent::Turn::kPlayerDecision:
                event.decide_player(event.suggested_keep());
                break;
            case LiveEvent::Turn::kFinished:
            case LiveEvent::Turn::kFailed:
                return;
        }
    }
}

// The player pulls the class's reference tractor; the rivals are the
// field drawn for the season.
TEST(LiveSeason, ThePlayerPullsTheReference) {
    const data::SeasonDefinition d = club_season();
    const std::unique_ptr<LiveSeason> season = start(d);
    EXPECT_EQ(season->puller_count(), kPullers);
    EXPECT_EQ(season->player().tractor.chassis.mass_kg, d.event.reference.tractor.chassis.mass_kg);
    EXPECT_EQ(season->player().tractor.engine.rated_omega_rad_s,
              d.event.reference.tractor.engine.rated_omega_rad_s);
}

// The sled operator's trials see the tractor the player pulls today: a
// weaker one is expected to get less far than the reference.
TEST(LiveSeason, TheOperatorExpectsThePlayersOwnTractor) {
    const data::SeasonDefinition d = club_season();
    const Venue venue = data::venue(d.event.sled, d.rounds.front().track);
    const auto expected_m = [&](const std::optional<vehicle::Params>& tractor) {
        const std::unique_ptr<LiveSeason> season = start(d);
        const bool started =
            tractor ? season->start_event(venue, *tractor) : season->start_event(venue);
        EXPECT_TRUE(started);
        LiveEvent& event = *season->event();
        // The trials run before the first pull.
        while (event.turn() == LiveEvent::Turn::kRivals &&
               event.expected_m(LiveSeason::kPlayer) <= 0.0) {
            event.advance(kSlice);
        }
        return event.expected_m(LiveSeason::kPlayer);
    };
    vehicle::Params weak = d.event.reference.tractor;
    weak.engine = engine::with_torque_scale(weak.engine, 0.7).value_or(weak.engine);
    const double reference_m = expected_m(std::nullopt);
    EXPECT_GT(reference_m, 0.0);
    EXPECT_LT(expected_m(weak), reference_m);
}

// The events in turn, each scored in the championship once finished; the
// next starts only after, and the season is over after the last.
TEST(LiveSeason, PlaysTheEventsInTurn) {
    const data::SeasonDefinition d = club_season();
    const std::unique_ptr<LiveSeason> season = start(d);
    EXPECT_EQ(season->event(), nullptr);
    EXPECT_FALSE(season->score_event()) << "no event yet";
    for (int i = 0; i < kEvents; ++i) {
        EXPECT_EQ(season->events_started(), i);
        const data::SeasonRound& round = d.rounds[static_cast<std::size_t>(i)];
        ASSERT_TRUE(season->start_event(data::venue(d.event.sled, round.track)));
        EXPECT_FALSE(season->start_event(data::venue(d.event.sled, round.track)))
            << "the event is not scored yet";
        LiveEvent* event = season->event();
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->player(), LiveSeason::kPlayer);
        EXPECT_FALSE(season->score_event()) << "not finished";
        play_out(*event);
        ASSERT_EQ(event->turn(), LiveEvent::Turn::kFinished);
        EXPECT_TRUE(season->score_event());
        EXPECT_FALSE(season->score_event()) << "scored once";
        EXPECT_EQ(season->events_scored(), i + 1);
    }
    EXPECT_TRUE(season->over());
    EXPECT_FALSE(season->start_event(data::venue(d.event.sled, d.rounds.front().track)));
    int points = 0;
    for (const season::Standing& s : season->standings()) {
        points += s.points;
    }
    EXPECT_GT(points, 0);
    EXPECT_EQ(season->standings().size(), static_cast<std::size_t>(kPullers));
}

// The prize for the player's place is paid as the event is scored, once;
// the garage is open until the player's first pull of an event, and again
// once it is over.
TEST(LiveSeason, PaysThePrizeOnceAndClosesTheGarageWhileThePlayerPulls) {
    const data::SeasonDefinition d = club_season();
    const std::unique_ptr<LiveSeason> season = start(d);
    garage::Career career{.money_eur = 100, .levels = {}};
    EXPECT_TRUE(season->player_may_change_tractor()) << "before the season's first event";
    EXPECT_FALSE(season->player_standing().has_value());
    ASSERT_TRUE(season->start_event(data::venue(d.event.sled, d.rounds.front().track)));
    EXPECT_TRUE(season->player_may_change_tractor()) << "before the player's first pull";
    LiveEvent* event = season->event();
    ASSERT_NE(event, nullptr);
    bool closed_after_a_pull = false;
    for (int guard = 0; guard < 100'000 && event->turn() != LiveEvent::Turn::kFinished; ++guard) {
        if (event->turn() == LiveEvent::Turn::kPlayerPull) {
            event->record_player({.ending = run::Ending::kStalled, .distance_m = kPlayerReach_m},
                                 kPlayerGear);
            closed_after_a_pull = closed_after_a_pull || !season->player_may_change_tractor();
        } else if (event->turn() == LiveEvent::Turn::kPlayerDecision) {
            event->decide_player(event->suggested_keep());
        } else {
            event->advance(kSlice);
        }
    }
    ASSERT_EQ(event->turn(), LiveEvent::Turn::kFinished);
    EXPECT_TRUE(closed_after_a_pull);
    EXPECT_TRUE(season->player_may_change_tractor()) << "the event is over";
    const std::optional<event::Standing> standing = season->player_standing();
    ASSERT_TRUE(standing.has_value());
    const std::int64_t prize_eur =
        garage::prize_eur(d.prize_money_eur, standing.value_or(event::Standing{}));
    EXPECT_GT(prize_eur, 0) << "place " << standing.value_or(event::Standing{}).place;
    EXPECT_TRUE(season->score_event(d.prize_money_eur, career));
    EXPECT_EQ(career.money_eur, 100 + prize_eur);
    EXPECT_FALSE(season->score_event(d.prize_money_eur, career)) << "scored once";
    EXPECT_EQ(career.money_eur, 100 + prize_eur) << "paid once";
}

// Plays the event being pulled with the player pulling and deciding as
// play_event's scripted puller did in `played`.
void replay(LiveEvent& event, const PlayedEvent& played) {
    std::size_t next = 0;
    const auto players_line = [&](bool decision) -> const LogEntry* {
        for (; next < played.log_size; ++next) {
            const LogEntry& entry = played.log[next];
            const bool is_decision = entry.deciding >= 0;
            const int who = is_decision ? entry.deciding : entry.pull.puller;
            if (who == LiveSeason::kPlayer && is_decision == decision) {
                return &played.log[next++];
            }
        }
        return nullptr;
    };
    for (int guard = 0; guard < 100'000; ++guard) {
        switch (event.turn()) {
            case LiveEvent::Turn::kRivals:
                event.advance(kSlice);
                break;
            case LiveEvent::Turn::kPlayerPull: {
                const LogEntry* line = players_line(false);
                if (line == nullptr) {
                    ADD_FAILURE() << "the scripted event has no such pull";
                    return;
                }
                event.record_player({.ending = line->disqualified ? run::Ending::kOutOfBounds
                                                                  : run::Ending::kStalled,
                                     .distance_m = line->reached_m},
                                    line->gear);
                break;
            }
            case LiveEvent::Turn::kPlayerDecision: {
                const LogEntry* line = players_line(true);
                if (line == nullptr) {
                    ADD_FAILURE() << "the scripted event has no such decision";
                    return;
                }
                event.decide_player(line->keep);
                break;
            }
            case LiveEvent::Turn::kFinished:
            case LiveEvent::Turn::kFailed:
                return;
        }
    }
}

// Each event is the tools' season's (tp_pull season: play_event on the
// season's field, the running order from event_seed and the pulls from one
// more), line for line, with the player pulling as its scripted puller 0
// did; for the first event and a later one.
TEST(LiveSeason, EachEventIsTheToolsEvent) {
    const data::SeasonDefinition d = club_season();
    const data::EventDefinition& e = d.event;
    const std::unique_ptr<LiveSeason> season = start(d);
    const std::optional<Field> generated =
        generate(e.reference, e.variation, e.class_rules, kPullers, kSeed);
    if (!generated.has_value()) {
        FAIL() << "the field was refused";
    }
    Field field = *generated;
    field.pullers[static_cast<std::size_t>(LiveSeason::kPlayer)] = e.reference;
    for (int n = 0; n < kEvents; ++n) {
        const Venue venue = data::venue(e.sled, d.rounds[static_cast<std::size_t>(n)].track);
        const std::uint64_t seed = event_seed(kSeed, n);
        const auto order = event::running_order(kPullers, seed);
        const std::optional<PlayedEvent> scripted =
            play_event(e.rules, venue, field, e.variation, e.pull_to_pull,
                       std::span{order.data(), static_cast<std::size_t>(kPullers)}, seed + 1);
        if (!scripted.has_value()) {
            FAIL() << "play_event refused event " << n;
        }
        const auto played = std::make_unique<PlayedEvent>(*scripted);
        ASSERT_TRUE(season->start_event(venue));
        LiveEvent* event = season->event();
        ASSERT_NE(event, nullptr);
        replay(*event, *played);
        ASSERT_EQ(event->turn(), LiveEvent::Turn::kFinished) << "event " << n;
        ASSERT_EQ(event->log().size(), played->log_size) << "event " << n;
        for (std::size_t i = 0; i < played->log_size; ++i) {
            const LogEntry& a = played->log[i];
            const LogEntry& b = event->log()[i];
            EXPECT_EQ(a.deciding, b.deciding) << n << ' ' << i;
            EXPECT_EQ(a.keep, b.keep) << n << ' ' << i;
            EXPECT_EQ(a.pull.puller, b.pull.puller) << n << ' ' << i;
            EXPECT_EQ(a.pull.kind, b.pull.kind) << n << ' ' << i;
            EXPECT_EQ(a.reached_m, b.reached_m) << n << ' ' << i;
            EXPECT_EQ(a.outcome.change, b.outcome.change) << n << ' ' << i;
            EXPECT_EQ(a.full_pull_m, b.full_pull_m) << n << ' ' << i;
        }
        ASSERT_TRUE(season->score_event());
    }
}

// A season picked up again (a saved season): a new LiveSeason for the same
// seed, the first event scored from the first's standings, has the same
// championship and goes on with the same next event, pull for pull.
TEST(LiveSeason, ResumesFromTheEventsPulled) {
    const data::SeasonDefinition d = club_season();
    const std::unique_ptr<LiveSeason> played = start(d);
    ASSERT_TRUE(played->start_event(data::venue(d.event.sled, d.rounds[0].track)));
    play_out(*played->event());
    ASSERT_TRUE(played->score_event());
    ASSERT_EQ(played->event_standings(0).size(), static_cast<std::size_t>(kPullers));
    EXPECT_TRUE(played->event_standings(1).empty()) << "not scored";

    const std::unique_ptr<LiveSeason> resumed = start(d);
    ASSERT_TRUE(resumed->add_pulled_event(played->event_standings(0)));
    EXPECT_EQ(resumed->events_scored(), 1);
    EXPECT_EQ(resumed->events_started(), 1);
    EXPECT_EQ(resumed->event(), nullptr);
    const auto a = played->standings();
    const auto b = resumed->standings();
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].puller, b[i].puller) << i;
        EXPECT_EQ(a[i].place, b[i].place) << i;
        EXPECT_EQ(a[i].points, b[i].points) << i;
        EXPECT_EQ(a[i].wins, b[i].wins) << i;
    }

    for (LiveSeason* season : {played.get(), resumed.get()}) {
        ASSERT_TRUE(season->start_event(data::venue(d.event.sled, d.rounds[1].track)));
        LiveEvent* event = season->event();
        ASSERT_NE(event, nullptr);
        play_out(*event);
        ASSERT_EQ(event->turn(), LiveEvent::Turn::kFinished);
    }
    const LiveEvent* event_a = played->event();
    const LiveEvent* event_b = resumed->event();
    if (event_a == nullptr || event_b == nullptr) {
        FAIL() << "no second event";
    }
    const auto log_a = event_a->log();
    const auto log_b = event_b->log();
    ASSERT_EQ(log_a.size(), log_b.size());
    for (std::size_t i = 0; i < log_a.size(); ++i) {
        EXPECT_EQ(log_a[i].pull.puller, log_b[i].pull.puller) << i;
        EXPECT_EQ(log_a[i].reached_m, log_b[i].reached_m) << i;
        EXPECT_EQ(log_a[i].deciding, log_b[i].deciding) << i;
    }
}

// Standings that are not the field's are refused, as is an event while
// one is unscored or after the season.
TEST(LiveSeason, RefusesStandingsThatAreNotTheFields) {
    const data::SeasonDefinition d = club_season();
    const std::unique_ptr<LiveSeason> season = start(d);
    std::vector<event::Standing> standings;
    standings.reserve(kPullers);
    for (int p = 0; p < kPullers; ++p) {
        standings.push_back({.puller = p, .place = p + 1});
    }
    EXPECT_FALSE(season->add_pulled_event(std::span{standings}.first(kPullers - 1)))
        << "a puller missing";
    std::vector<event::Standing> twice = standings;
    twice[1].puller = 0;
    EXPECT_FALSE(season->add_pulled_event(twice)) << "a puller twice";
    std::vector<event::Standing> outside = standings;
    outside[1].puller = kPullers;
    EXPECT_FALSE(season->add_pulled_event(outside)) << "a puller outside the field";
    std::vector<event::Standing> no_place = standings;
    no_place[1].place = 0;
    EXPECT_FALSE(season->add_pulled_event(no_place)) << "no place";
    std::vector<event::Standing> past_the_field = standings;
    past_the_field[1].place = kPullers + 1;
    EXPECT_FALSE(season->add_pulled_event(past_the_field)) << "a place past the field";
    EXPECT_EQ(season->events_scored(), 0);

    ASSERT_TRUE(season->start_event(data::venue(d.event.sled, d.rounds[0].track)));
    EXPECT_FALSE(season->add_pulled_event(standings)) << "an event is unscored";
    play_out(*season->event());
    ASSERT_TRUE(season->score_event());
    EXPECT_TRUE(season->add_pulled_event(standings));
    EXPECT_TRUE(season->over());
    EXPECT_FALSE(season->add_pulled_event(standings)) << "the season is over";
}

TEST(LiveSeason, RefusesABadSeason) {
    const data::SeasonDefinition d = club_season();
    const data::EventDefinition& e = d.event;
    EXPECT_FALSE(LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules,
                                    e.pull_to_pull, d.points, 1, kEvents, kSeed));
    EXPECT_FALSE(LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules,
                                    e.pull_to_pull, d.points, kPullers, 0, kSeed));
    EXPECT_FALSE(LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules,
                                    e.pull_to_pull, d.points, kPullers, season::kMaxEvents + 1,
                                    kSeed));
}

// The season's guests: two of the pool at every event, after the
// competitors, drawn the same for the same seed; marked in the standings,
// passed over by the championship, kept by a save; and each event is the
// tools' event with them (play_event on the field with the guests).
TEST(LiveSeason, GuestsPullAtEveryEvent) {
    const data::SeasonDefinition d = club_season();
    const data::EventDefinition& e = d.event;
    Guests guests = e.game.season_guests();
    guests.variation.lowest_gear = e.variation.lowest_gear;
    ASSERT_EQ(guests.per_event, 2);
    const auto create = [&] {
        std::optional<LiveSeason> season =
            LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull,
                               d.points, kPullers, kEvents, kSeed, guests);
        if (!season.has_value()) {
            throw std::runtime_error("the season was refused");
        }
        return std::make_unique<LiveSeason>(*season);
    };
    const std::unique_ptr<LiveSeason> season = create();
    EXPECT_EQ(season->puller_count(), kPullers) << "the competitors";
    for (int n = 0; n < kEvents; ++n) {
        const std::array<int, kMaxGuests> chosen = season->guests_at_event(n);
        EXPECT_NE(chosen[0], chosen[1]) << n;
        EXPECT_GE(chosen[0], 0);
        EXPECT_LT(chosen[1], guests.pool);
        EXPECT_EQ(chosen, guests_at(kSeed, n, guests)) << "drawn from the seed alone";
    }

    // The first event: the tools' event with the guests, line for line.
    const Venue venue = data::venue(e.sled, d.rounds[0].track);
    std::optional<Field> field = generate(e.reference, e.variation, e.class_rules, kPullers, kSeed);
    const std::optional<Field> pool = guest_pool(e.reference, guests, e.class_rules, kSeed);
    ASSERT_TRUE(field.has_value() && pool.has_value());
    field->pullers[static_cast<std::size_t>(LiveSeason::kPlayer)] = e.reference;
    const std::array<int, kMaxGuests> chosen = guests_at(kSeed, 0, guests);
    const Field today = with_guests(*field, *pool, std::span{chosen.data(), 2});
    ASSERT_EQ(today.count, kPullers + 2);
    const std::uint64_t seed = event_seed(kSeed, 0);
    const auto order = event::running_order(today.count, seed);
    const std::optional<PlayedEvent> scripted =
        play_event(e.rules, venue, today, e.variation, e.pull_to_pull,
                   std::span{order.data(), static_cast<std::size_t>(today.count)}, seed + 1, 2);
    ASSERT_TRUE(scripted.has_value());
    const auto played = std::make_unique<PlayedEvent>(*scripted);
    ASSERT_TRUE(season->start_event(venue));
    LiveEvent* event = season->event();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->puller_count(), kPullers + 2);
    EXPECT_FALSE(event->guest(kPullers - 1));
    EXPECT_TRUE(event->guest(kPullers));
    EXPECT_TRUE(event->guest(kPullers + 1));
    replay(*event, *played);
    ASSERT_EQ(event->turn(), LiveEvent::Turn::kFinished);
    ASSERT_EQ(event->log().size(), played->log_size);
    for (std::size_t i = 0; i < played->log_size; ++i) {
        EXPECT_EQ(played->log[i].pull.puller, event->log()[i].pull.puller) << i;
        EXPECT_EQ(played->log[i].reached_m, event->log()[i].reached_m) << i;
    }
    ASSERT_TRUE(season->score_event());
    const std::span<const event::Standing> first = season->event_standings(0);
    ASSERT_EQ(first.size(), static_cast<std::size_t>(kPullers + 2));
    for (const event::Standing& s : first) {
        EXPECT_EQ(s.guest, s.puller >= kPullers) << s.puller;
    }
    EXPECT_EQ(season->standings().size(), static_cast<std::size_t>(kPullers))
        << "the championship is the competitors'";

    // Picked up again from the standings, guests and all; without the
    // guests' marks they are not the event's.
    const std::unique_ptr<LiveSeason> resumed = create();
    std::vector<event::Standing> unmarked(first.begin(), first.end());
    for (event::Standing& s : unmarked) {
        s.guest = false;
    }
    EXPECT_FALSE(resumed->add_pulled_event(unmarked));
    EXPECT_FALSE(resumed->add_pulled_event(first.first(static_cast<std::size_t>(kPullers))))
        << "the guests missing";
    ASSERT_TRUE(resumed->add_pulled_event(first));
    for (std::size_t i = 0; i < static_cast<std::size_t>(kPullers); ++i) {
        EXPECT_EQ(resumed->standings()[i].puller, season->standings()[i].puller) << i;
        EXPECT_EQ(resumed->standings()[i].points, season->standings()[i].points) << i;
    }

    // A pool too small for the guests at each event is refused.
    Guests too_many = guests;
    too_many.per_event = guests.pool + 1;
    EXPECT_FALSE(LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules,
                                    e.pull_to_pull, d.points, kPullers, kEvents, kSeed, too_many));
}

}  // namespace
}  // namespace tp::field
