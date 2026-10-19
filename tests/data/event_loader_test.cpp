#include "tp/data/event/event_loader.hpp"

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/hash/fnv1a.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

const std::filesystem::path kEvent =
    std::filesystem::path{TP_DATA_DIR} / "events" / "club_pull.toml";

std::string shipped_text() {
    const std::ifstream in{kEvent};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string with(std::string_view from, std::string_view to) {
    std::string text = shipped_text();
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view text) {
    const auto result = parse_event(text, "event.toml", kEvent.parent_path());
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// What a scripted event comes to: every pull, and the standings.
struct EventLog {
    std::uint64_t hash = 0;
    int pulls = 0;
    int full_pulls_in_round = 0;
    int pull_off_pulls = 0;
    std::array<event::Standing, event::kMaxPullers> standings{};
    std::size_t standing_count = 0;
};

EventLog run(const EventDefinition& definition) {
    EventLog log;
    const auto field = field::generate(definition.reference, definition.variation,
                                       definition.class_rules, definition.pullers, definition.seed);
    const auto order = event::running_order(definition.pullers, definition.seed + 1);
    const field::Venue venue{.track = definition.track.ground,
                             .sled = sled::with_conditions(definition.sled, definition.track.sled),
                             .max_duration_s = definition.track.rules.max_duration_s,
                             .lane_width_m = definition.track.rules.lane_width_m};
    const auto played =
        field ? field::play_event(
                    definition.rules, venue, *field, definition.variation, definition.pull_to_pull,
                    std::span{order.data(), static_cast<std::size_t>(definition.pullers)},
                    definition.seed + 2)
              : std::nullopt;
    EXPECT_TRUE(played.has_value());
    if (!played) {
        return log;
    }
    hash::Fnv1a hash;
    for (const field::LogEntry& entry : played->entries()) {
        if (entry.deciding >= 0) {
            continue;
        }
        hash.add(entry.reached_m);
        ++log.pulls;
        if (entry.pull.kind == event::PullKind::kPullOff) {
            ++log.pull_off_pulls;
        } else if (entry.reached_m >= entry.full_pull_m) {
            ++log.full_pulls_in_round;
        }
    }
    event::Event ev = played->event;
    const auto standings = ev.standings();
    log.standing_count = standings.size();
    std::copy(standings.begin(), standings.end(), log.standings.begin());
    log.hash = hash.value();
    return log;
}

TEST(EventLoader, LoadsTheShippedEvent) {
    const auto result = load_event(kEvent);
    ASSERT_TRUE(result.ok()) << result.error().message;
    const EventDefinition& definition = result.value();
    EXPECT_EQ(definition.pullers, 15);
    EXPECT_EQ(definition.seed, 2026U);
    EXPECT_EQ(definition.rules.full_pull_m, 100.0);
    EXPECT_EQ(definition.rules.extended_full_pull_m, 110.0);
    EXPECT_EQ(definition.rules.pull_off_places, 4);
    EXPECT_EQ(definition.reference.driver.gear, 6);
    EXPECT_EQ(definition.variation.lowest_gear, 4);
    EXPECT_EQ(definition.variation.highest_gear, 6);
    EXPECT_EQ(definition.variation.track_reading.low, 0.5);
    EXPECT_EQ(definition.pull_to_pull.grip_share, 0.01);
    EXPECT_EQ(definition.pull_to_pull.clutch_slip_in_s, 0.3);
    EXPECT_EQ(definition.variation.mass_kg.high, 3550.0);
    EXPECT_EQ(definition.track.rules.track_length_m, 100.0);
}

TEST(EventLoader, RejectsNonsense) {
    EXPECT_NE(error_of(with("pullers = 15", "pullers = 40")).find("event.pullers: must be 1 to 32"),
              std::string::npos);
    EXPECT_NE(error_of(with("gears = [4, 6]", "gears = [4, 12]"))
                  .find("event.field.gears: must be [lowest, highest] within the gearbox"),
              std::string::npos);
    EXPECT_NE(error_of(with("mass_kg = [3300.0, 3550.0]", "mass_kg = [3300.0]"))
                  .find("event.field.mass_kg: must be [low, high]"),
              std::string::npos);
    EXPECT_NE(error_of(with("extended_full_pull_m = 110.0", "extended_full_pull_m = 90.0"))
                  .find("event.rules: need 0 < qualifying_distance_m"),
              std::string::npos);
    EXPECT_NE(error_of(with("pull_to_pull_grip_share = 0.01", "pull_to_pull_grip_share = -0.1"))
                  .find("event.field: pull_to_pull values must be >= 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("track_reading = [0.5, 0.95]", "track_reading = [0.5, 1.5]"))
                  .find("track_reading within [0, 1]"),
              std::string::npos);
    EXPECT_NE(error_of(with("seed = 2026", "seed = -1")).find("event.seed: must not be negative"),
              std::string::npos);
    EXPECT_NE(error_of(with("../sleds/light_class.toml", "../sleds/missing.toml"))
                  .find("missing.toml: cannot open file"),
              std::string::npos);
}

// The shipped club event looks like a real one: a couple of full pulls at
// most, the pull-off for four, and a winner from it.
TEST(ShippedEvent, PlaysOutLikeAClubEvent) {
    const auto definition = load_event(kEvent);
    ASSERT_TRUE(definition.ok()) << definition.error().message;
    const EventLog log = run(definition.value());
    EXPECT_EQ(log.standing_count, 15U);
    EXPECT_LE(log.full_pulls_in_round, 4);
    EXPECT_GT(log.pull_off_pulls, 0);
    EXPECT_LE(log.pull_off_pulls, 4 * event::kMaxPullOffRounds);
    EXPECT_EQ(log.standings[0].place, 1);
    EXPECT_GT(log.standings[0].pull_off_rounds, 0);
    for (std::size_t i = 0; i < log.standing_count; ++i) {
        EXPECT_GE(log.standings[i].distance_m, 80.0) << "everyone gets past 80 m";
    }
}

// A replay: the same event file gives the same pulls to the last bit, on
// every compiler, build type and platform.
TEST(ShippedEvent, ReplayMatchesRegression) {
    const auto definition = load_event(kEvent);
    ASSERT_TRUE(definition.ok()) << definition.error().message;
    const EventLog first = run(definition.value());
    const EventLog again = run(definition.value());
    EXPECT_EQ(again.hash, first.hash);
    EXPECT_EQ(first.pulls, 21);
    EXPECT_EQ(first.hash, 0xd1aea02caf8ee264ULL);
}

}  // namespace
}  // namespace tp::data
