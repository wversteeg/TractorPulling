#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/data/event/event_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace tp::data {
namespace {

// The counting distances of an event's pullers short of a full pull (and
// not disqualified), as the standings give them.
std::vector<double> short_of_the_line(EventDefinition definition, std::uint64_t seed) {
    definition.seed = seed;
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
    std::vector<double> out;
    if (!played) {
        return out;
    }
    event::Event ev = played->event;
    for (const event::Standing& s : ev.standings()) {
        if (!s.full_pull && !s.disqualified) {
            out.push_back(s.distance_m);
        }
    }
    std::ranges::sort(out);
    return out;
}

// The most distances within any `window_m` [m].
int most_within(const std::vector<double>& sorted, double window_m) {
    int most = 0;
    std::size_t end = 0;
    for (std::size_t begin = 0; begin < sorted.size(); ++begin) {
        end = std::max(end, begin);
        while (end < sorted.size() && sorted[end] < sorted[begin] + window_m) {
            ++end;
        }
        most = std::max(most, static_cast<int>(end - begin));
    }
    return most;
}

constexpr double kWindow_m = 1.0;
// At most this many of a field of fifteen within a metre, and the field
// short of the line spread over at least this much [m].
constexpr int kMostWithinAMetre = 6;
constexpr double kLeastSpread_m = 5.0;

// The field spreads out (K-9): the sled's push-down puts at most part of
// the sled on the pan, so its drag levels out instead of rising like a
// wall, and each tractor stops where its own power and grip give out. In
// every shipped event, over several seeds, no more than a handful of the
// pullers short of the line stop within a metre of each other, and they
// spread over several metres. (On a push-down that lifts the wheels off,
// ten of fifteen stopped within a metre.)
TEST(FieldSpread, TheFieldSpreadsOutInEveryShippedEvent) {
    const std::filesystem::path events = std::filesystem::path{TP_DATA_DIR} / "events";
    for (const char* file :
         {"club_pull.toml", "standard_3500.toml", "sport_2500_promo.toml", "sport_2500_top.toml",
          "sport_4500_belofte.toml", "sport_4500_promo.toml", "sport_4500_top.toml"}) {
        const auto definition = load_event(events / file);
        ASSERT_TRUE(definition.ok()) << definition.error().message;
        for (const std::uint64_t seed : {1U, 2U, 3U, 4U}) {
            const std::vector<double> d = short_of_the_line(definition.value(), seed);
            if (d.size() < 2) {
                continue;
            }
            EXPECT_LE(most_within(d, kWindow_m), kMostWithinAMetre) << file << ", seed " << seed;
            EXPECT_GE(d.back() - d.front(), kLeastSpread_m) << file << ", seed " << seed;
        }
    }
}

}  // namespace
}  // namespace tp::data
