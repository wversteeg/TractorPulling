#include "tp/data/event/event_loader.hpp"

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/units/conversions.hpp"

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
    EXPECT_EQ(definition.rules.pull_off_full_pull_m, 120.0);
    EXPECT_EQ(definition.rules.qualifying_below_line_m, 20.0);
    EXPECT_EQ(definition.pull_to_pull.pan_drag_share, 0.06);
    EXPECT_EQ(definition.game.pullers, 12);
    ASSERT_EQ(definition.game.rival_names.size(), 11U);
    EXPECT_EQ(definition.game.rival_names.front(), "Henk");
    // Two guests at each event from a pool of six, the field's but for
    // their strength.
    EXPECT_EQ(definition.game.guests, 2);
    ASSERT_EQ(definition.game.guest_names.size(), 6U);
    EXPECT_EQ(definition.game.guest_names.front(), "Evert");
    EXPECT_EQ(definition.game.guest_variation.torque_scale.high, 1.06);
    EXPECT_EQ(definition.game.guest_variation.traction_limit.high, 0.96);
    EXPECT_EQ(definition.game.guest_variation.mass_kg.high, 3550.0);
    EXPECT_EQ(definition.game.season_guests().pool, 6);
    EXPECT_EQ(definition.reference.driver.gear, 5);
    EXPECT_EQ(definition.variation.lowest_gear, 4);
    EXPECT_EQ(definition.variation.highest_gear, 5);
    EXPECT_EQ(definition.variation.track_reading.low, 0.5);
    EXPECT_EQ(definition.pull_to_pull.grip_share, 0.01);
    EXPECT_EQ(definition.pull_to_pull.clutch_slip_in_s, 0.3);
    EXPECT_EQ(definition.variation.mass_kg.high, 3550.0);
    EXPECT_EQ(definition.variation.hitch_height_m.low, 0.38);
    EXPECT_EQ(definition.variation.hitch_height_m.high, 0.5);
    EXPECT_DOUBLE_EQ(definition.variation.rated_omega_rad_s.low, units::rpm_to_rad_s(2200.0));
    EXPECT_DOUBLE_EQ(definition.variation.rated_omega_rad_s.high, units::rpm_to_rad_s(2650.0));
    EXPECT_EQ(definition.variation.fewest_load_downshift_gears, 0);
    EXPECT_EQ(definition.variation.most_load_downshift_gears, 1);
    EXPECT_EQ(definition.track.rules.track_length_m, 100.0);
    EXPECT_EQ(definition.rules.strong_full_pull_from, 0.8);
    EXPECT_EQ(definition.rules.trouble_share, 0.6);
    // Left out, the operator judges every pull alike.
    const auto plain =
        parse_event(with("trouble_share = 0.6\n", ""), "event.toml", kEvent.parent_path());
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_FALSE(plain.value().rules.trouble_share.has_value());
}

TEST(EventLoader, RejectsNonsense) {
    EXPECT_NE(error_of(with("pullers = 15", "pullers = 40")).find("event.pullers: must be 1 to 32"),
              std::string::npos);
    EXPECT_NE(error_of(with("gears = [4, 5]", "gears = [4, 12]"))
                  .find("event.field.gears: must be [lowest, highest] within the gearbox"),
              std::string::npos);
    EXPECT_NE(error_of(with("mass_kg = [3200.0, 3550.0]", "mass_kg = [3200.0]"))
                  .find("event.field.mass_kg: must be [low, high]"),
              std::string::npos);
    EXPECT_NE(error_of(with("extended_full_pull_m = 110.0", "extended_full_pull_m = 90.0"))
                  .find("event.rules: need 0 < sled_target_m"),
              std::string::npos);
    EXPECT_NE(error_of(with("pull_to_pull_grip_share = 0.01", "pull_to_pull_grip_share = -0.1"))
                  .find("event.field: pull_to_pull values must be >= 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("track_reading = [0.5, 0.95]", "track_reading = [0.5, 1.5]"))
                  .find("track_reading within [0, 1]"),
              std::string::npos);
    EXPECT_NE(error_of(with("downshift_share = [0.82, 0.95]", "downshift_share = [0.82, 1.0]"))
                  .find("downshift_share within [0, 1)"),
              std::string::npos);
    EXPECT_NE(error_of(with("load_downshift_gears = [0, 1]", "load_downshift_gears = [1, 0]"))
                  .find("event.field.load_downshift_gears: must be [fewest, most]"),
              std::string::npos);
    EXPECT_NE(error_of(with("load_downshift_gears = [0, 1]", "load_downshift_gears = [0, 1.5]"))
                  .find("event.field.load_downshift_gears: must be [fewest, most]"),
              std::string::npos);
    EXPECT_NE(error_of(with("load_downshift_gears = [0, 1]", "load_downshift_gears = [0, 9]"))
                  .find("fewer than the gearbox's gears"),
              std::string::npos);
    EXPECT_NE(error_of(with("hitch_height_m = [0.38, 0.50]", "hitch_height_m = [0.5, 0.38]"))
                  .find("event.field: every range"),
              std::string::npos);
    EXPECT_NE(error_of(with("rated_rpm = [2200.0, 2650.0]", "rated_rpm = [2500.0, 2200.0]"))
                  .find("event.field: every range"),
              std::string::npos);
    EXPECT_NE(error_of(with("rated_rpm = [2200.0, 2650.0]", "rated_rpm = [0.0, 2500.0]"))
                  .find("rated_rpm above 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("rated_rpm = [2200.0, 2650.0]", "rated_rpm = [500.0, 600.0]"))
                  .find("event.field.rated_rpm: the reference engine cannot take it"),
              std::string::npos);
    EXPECT_NE(error_of(with("downshift_share = 0.9", "downshift_share = 1.2")).find("event.driver"),
              std::string::npos);
    EXPECT_NE(
        error_of(with("launch_hold_share = 0.8", "launch_hold_share = 1.0")).find("event.driver"),
        std::string::npos);
    EXPECT_NE(error_of(with("pullers = 12\nrival_names", "pullers = 1\nrival_names"))
                  .find("event.game.pullers"),
              std::string::npos);
    EXPECT_NE(error_of(with("\"Ilse\"]", "\"\"]")).find("event.game.rival_names"),
              std::string::npos);
    EXPECT_NE(error_of(with(", \"Ilse\"]", "]")).find("event.game.rival_names"), std::string::npos);
    EXPECT_NE(error_of(with("guests = 2", "guests = 7")).find("event.game.guests"),
              std::string::npos);
    EXPECT_NE(error_of(with("guests = 2", "guests = -1")).find("event.game.guests"),
              std::string::npos);
    EXPECT_NE(error_of(with("\"Wendy\"]", "\"\"]")).find("event.game.guest_names"),
              std::string::npos);
    EXPECT_NE(error_of(with("torque_scale = [0.84, 1.06]", "torque_scale = [1.06, 0.84]"))
                  .find("event.guests"),
              std::string::npos);
    EXPECT_NE(error_of(with("trouble_share = 0.6", "trouble_share = 1.5"))
                  .find("event.rules: need strong_full_pull_from 0 to 1"),
              std::string::npos);
    EXPECT_NE(error_of(with("seed = 2026", "seed = -1")).find("event.seed: must not be negative"),
              std::string::npos);
    EXPECT_NE(error_of(with("../sleds/light_class.toml", "../sleds/missing.toml"))
                  .find("missing.toml: cannot open file"),
              std::string::npos);
}

// The launch hold is optional: the club event's driver holds the clutch
// while the revs are low (on the 15 t sled a farm engine stalls otherwise);
// left out, the driver lets the clutch in on time.
TEST(EventLoader, TheLaunchHoldIsOptional) {
    const auto held = load_event(kEvent);
    ASSERT_TRUE(held.ok()) << held.error().message;
    EXPECT_EQ(held.value().reference.driver.launch_hold_share, 0.8);
    EXPECT_EQ(held.value().reference.driver.launch_hold_max_s, 8.0);
    const auto plain = parse_event(with("launch_hold_share = 0.8\nlaunch_hold_max_s = 8.0\n", ""),
                                   "event.toml", kEvent.parent_path());
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_EQ(plain.value().reference.driver.launch_hold_share, 0.0);
}

// The Supersport's drivers play the throttle against wheelspin; optional,
// off by default.
TEST(EventLoader, ReadsTheDriversSpinEase) {
    const auto supersport =
        load_event(std::filesystem::path{TP_DATA_DIR} / "events" / "supersport_3600_top.toml");
    ASSERT_TRUE(supersport.ok()) << supersport.error().message;
    EXPECT_GT(supersport.value().reference.driver.spin_ease_slip, 0.0);
    const auto club = load_event(kEvent);
    ASSERT_TRUE(club.ok()) << club.error().message;
    EXPECT_EQ(club.value().reference.driver.spin_ease_slip, 0.0);
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
    EXPECT_EQ(first.pulls, 19);
    EXPECT_EQ(first.hash, 0x7e0336471be28b3aULL);
}

// An event's field may run bigger turbos than the reference engine's:
// boost_bar in bar on file, Pa in the core; the club event has none.
TEST(EventLoader, ReadsTheFieldsTurbos) {
    const auto top =
        load_event(std::filesystem::path{TP_DATA_DIR} / "events" / "supersport_3600_top.toml");
    ASSERT_TRUE(top.ok()) << top.error().message;
    const field::Range& boost = top.value().variation.boost_pressure_pa;
    EXPECT_EQ(boost.low, units::bar_to_pa(3.0));
    EXPECT_GT(boost.high, boost.low);
    const auto club = load_event(kEvent);
    ASSERT_TRUE(club.ok()) << club.error().message;
    EXPECT_EQ(club.value().variation.boost_pressure_pa.high, 0.0);
}

}  // namespace
}  // namespace tp::data
