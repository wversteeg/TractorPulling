#include "tp/core/field/live.hpp"
#include "tp/core/field/play.hpp"
#include "tp/data/event/event_loader.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>

namespace tp::field {
namespace {

// Few pullers and two gears, so the debug build plays the events quickly.
constexpr int kPullers = 6;
constexpr std::uint64_t kSeed = 2026;
// The step budget per advance(), about a game frame's worth.
constexpr std::int64_t kSlice = 5000;

struct EventSetup {
    event::Rules rules;
    Venue venue;
    Field field;
    Variation variation;
    PullToPull pull_to_pull;
    std::array<int, event::kMaxPullers> order{};

    [[nodiscard]] std::span<const int> running_order() const {
        return {order.data(), static_cast<std::size_t>(field.count)};
    }
};

// The shipped club event, with a smaller field; throws if it cannot load.
EventSetup shipped() {
    const auto definition =
        data::load_event(std::filesystem::path{TP_DATA_DIR} / "events" / "club_pull.toml");
    if (!definition) {
        throw std::runtime_error("cannot load the club event");
    }
    const data::EventDefinition& d = definition.value();
    Variation variation = d.variation;
    variation.lowest_gear = 5;
    const auto field = generate(d.reference, variation, d.class_rules, kPullers, d.seed);
    if (!field) {
        throw std::runtime_error("cannot generate the field");
    }
    EventSetup setup{.rules = d.rules,
                     .venue = {.track = d.track.ground,
                               .sled = sled::with_conditions(d.sled, d.track.sled),
                               .max_duration_s = d.track.rules.max_duration_s,
                               .lane_width_m = d.track.rules.lane_width_m},
                     .field = *field,
                     .variation = variation,
                     .pull_to_pull = d.pull_to_pull};
    setup.order = event::running_order(kPullers, d.seed + 1);
    return setup;
}

// The live event with `player` pulling as play_event's scripted puller did:
// each of their pulls and decisions from `played`'s log, in turn.
std::unique_ptr<LiveEvent> replay(const EventSetup& s, const PlayedEvent& played, int player) {
    std::optional<LiveEvent> created = LiveEvent::create(
        s.rules, s.venue, s.field, s.variation, s.pull_to_pull, s.running_order(), player, kSeed);
    if (!created.has_value()) {
        ADD_FAILURE() << "the live event refused the setup";
        return nullptr;
    }
    auto live = std::make_unique<LiveEvent>(*created);
    std::size_t next = 0;
    // The player's next line in the scripted log.
    const auto players_line = [&](bool decision) -> const LogEntry* {
        for (; next < played.log_size; ++next) {
            const LogEntry& entry = played.log[next];
            const bool is_decision = entry.deciding >= 0;
            const int who = is_decision ? entry.deciding : entry.pull.puller;
            if (who == player && is_decision == decision) {
                return &played.log[next++];
            }
        }
        return nullptr;
    };
    for (int guard = 0; guard < 100'000; ++guard) {
        switch (live->turn()) {
            case LiveEvent::Turn::kRivals: {
                const std::int64_t steps = live->advance(kSlice);
                EXPECT_LE(steps, kSlice);
                break;
            }
            case LiveEvent::Turn::kPlayerPull: {
                const LogEntry* line = players_line(false);
                if (line == nullptr) {
                    ADD_FAILURE() << "the scripted event has no such pull";
                    return live;
                }
                const std::optional<event::Pull> pull = live->pending_pull();
                if (!pull.has_value()) {
                    ADD_FAILURE() << "no pull pending for the player";
                    return live;
                }
                EXPECT_EQ(pull->kind, line->pull.kind);
                const run::Result result{.ending = line->disqualified ? run::Ending::kOutOfBounds
                                                                      : run::Ending::kStalled,
                                         .distance_m = line->reached_m};
                live->record_player(result, line->gear);
                break;
            }
            case LiveEvent::Turn::kPlayerDecision: {
                const LogEntry* line = players_line(true);
                if (line == nullptr) {
                    ADD_FAILURE() << "the scripted event has no such decision";
                    return live;
                }
                live->decide_player(line->keep);
                break;
            }
            case LiveEvent::Turn::kFinished:
            case LiveEvent::Turn::kFailed:
                return live;
        }
    }
    ADD_FAILURE() << "the live event did not finish";
    return live;
}

// With the player pulling what the scripted puller pulled, the live event
// is play_event's, line for line: the rivals' gears, every pull, the
// operator's verdicts, the decisions and the standings. So the game's event
// is the tools' event, whoever the player is.
TEST(LiveEvent, IsPlayEventWithThePlayerPullingAsScripted) {
    const EventSetup s = shipped();
    const std::optional<PlayedEvent> scripted = play_event(
        s.rules, s.venue, s.field, s.variation, s.pull_to_pull, s.running_order(), kSeed);
    if (!scripted.has_value()) {
        FAIL() << "play_event refused the setup";
    }
    const auto played = std::make_unique<PlayedEvent>(*scripted);
    for (const int player : {s.order[0], s.order[3]}) {
        const std::unique_ptr<LiveEvent> live = replay(s, *played, player);
        ASSERT_NE(live, nullptr);
        ASSERT_EQ(live->turn(), LiveEvent::Turn::kFinished) << "player " << player;
        ASSERT_EQ(live->log().size(), played->log_size) << "player " << player;
        for (std::size_t i = 0; i < played->log_size; ++i) {
            const LogEntry& a = played->log[i];
            const LogEntry& b = live->log()[i];
            EXPECT_EQ(a.deciding, b.deciding) << i;
            EXPECT_EQ(a.keep, b.keep) << i;
            EXPECT_EQ(a.pull.puller, b.pull.puller) << i;
            EXPECT_EQ(a.pull.kind, b.pull.kind) << i;
            EXPECT_EQ(a.reached_m, b.reached_m) << i;
            EXPECT_EQ(a.outcome.change, b.outcome.change) << i;
            EXPECT_EQ(a.outcome.steps, b.outcome.steps) << i;
            EXPECT_EQ(a.full_pull_m, b.full_pull_m) << i;
            EXPECT_EQ(a.knives, b.knives) << i;
        }
        for (int p = 0; p < kPullers; ++p) {
            if (p != player) {
                EXPECT_EQ(live->gear(p), played->gears[static_cast<std::size_t>(p)]) << p;
            }
        }
        event::Event ended = played->event;
        const auto expected = ended.standings();
        const auto actual = live->standings();
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(actual[i].puller, expected[i].puller) << i;
            EXPECT_EQ(actual[i].place, expected[i].place) << i;
            EXPECT_EQ(actual[i].distance_m, expected[i].distance_m) << i;
        }
    }
}

// The rivals' work stops for the player: the player's pull waits until
// it is recorded, whatever the budget, and the pull asks for the event's
// sled and a run past the line.
TEST(LiveEvent, WaitsForThePlayer) {
    const EventSetup s = shipped();
    const int player = s.order[0];  // the first to pull: the test puller
    std::optional<LiveEvent> created = LiveEvent::create(
        s.rules, s.venue, s.field, s.variation, s.pull_to_pull, s.running_order(), player, kSeed);
    if (!created.has_value()) {
        FAIL() << "the live event refused the setup";
    }
    auto live = std::make_unique<LiveEvent>(*created);
    EXPECT_EQ(live->turn(), LiveEvent::Turn::kRivals) << "the rivals choose their gears first";
    EXPECT_LE(live->advance(1), 1);
    while (live->turn() == LiveEvent::Turn::kRivals) {
        live->advance(kSlice);
    }
    ASSERT_EQ(live->turn(), LiveEvent::Turn::kPlayerPull);
    EXPECT_EQ(live->advance(kSlice), 0);
    EXPECT_EQ(live->turn(), LiveEvent::Turn::kPlayerPull);
    const std::optional<event::Pull> pull = live->pending_pull();
    ASSERT_TRUE(pull.has_value());
    EXPECT_EQ(pull->puller, player);
    EXPECT_EQ(pull->kind, event::PullKind::kTest);
    EXPECT_EQ(live->pull_rules().track_length_m, s.rules.max_distance_m);
    EXPECT_EQ(live->pull_rules().full_pull_line_m, s.rules.full_pull_m);
    EXPECT_EQ(live->sled().knives.count, s.venue.sled.knives.count);

    // A short test pull: the operator lightens the sled, and the rivals go on.
    const event::Outcome outcome =
        live->record_player({.ending = run::Ending::kStalled, .distance_m = 60.0}, 5);
    EXPECT_EQ(outcome.change, event::SledChange::kLighter);
    EXPECT_EQ(live->turn(), LiveEvent::Turn::kRivals);
    EXPECT_EQ(live->log().back().pull.puller, player);
    EXPECT_EQ(live->log().back().gear, 5);
}

TEST(LiveEvent, RefusesWhatPlayEventWould) {
    const EventSetup s = shipped();
    EXPECT_FALSE(LiveEvent::create(s.rules, s.venue, s.field, s.variation, s.pull_to_pull,
                                   s.running_order(), kPullers, kSeed));
    EXPECT_FALSE(LiveEvent::create(s.rules, s.venue, s.field, s.variation, s.pull_to_pull,
                                   s.running_order(), -1, kSeed));
    EXPECT_FALSE(LiveEvent::create(s.rules, s.venue, s.field, s.variation, s.pull_to_pull,
                                   s.running_order().first(kPullers - 1), 0, kSeed));
}

}  // namespace
}  // namespace tp::field
