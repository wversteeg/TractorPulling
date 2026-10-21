#include "tp/core/field/live_season.hpp"

#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>

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

}  // namespace
}  // namespace tp::field
