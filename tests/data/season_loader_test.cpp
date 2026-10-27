#include "tp/data/season/season_loader.hpp"

#include "tp/core/field/play.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/math/random.hpp"
#include "tp/core/season/season.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <numeric>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

const std::filesystem::path kSeasons = std::filesystem::path{TP_DATA_DIR} / "seasons";

constexpr std::string_view kTwoEvents = R"(
[season]
event = "../events/club_pull.toml"
seed = 7
points = [20, 17, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3]
prize_money_eur = [700, 500, 400]
garage = "../garages/ntto_standard_3500.toml"

[[season.events]]
name = "Clay-sand"
track = "../tracks/ideal_clay_sand.toml"

[[season.events]]
name = "Covered heavy clay"
track = "../tracks/covered_heavy_clay.toml"
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kTwoEvents};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view text) {
    const auto result = parse_season(text, "season.toml", kSeasons);
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

TEST(SeasonLoader, LoadsTheShippedSeason) {
    const auto result = load_season(kSeasons / "club_season.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const SeasonDefinition& season = result.value();
    EXPECT_EQ(season.rounds.size(), 10U);
    EXPECT_EQ(season.event.pullers, 15);
    EXPECT_EQ(season.points.places, 15);
    EXPECT_EQ(season::points_for(season.points, 1), 20);
    EXPECT_EQ(season::points_for(season.points, 2), 17);
    EXPECT_EQ(season::points_for(season.points, 3), 15);
    EXPECT_EQ(season::points_for(season.points, 4), 14);
    EXPECT_EQ(season.rounds.front().name, "Spring opener, watered start");
    ASSERT_EQ(season.prize_money_eur.size(), 10U);
    EXPECT_EQ(season.prize_money_eur.front(), 1500);
    EXPECT_EQ(season.garage.catalogue.count, 5U);
    EXPECT_EQ(season.garage.texts.size(), 5U);
    EXPECT_EQ(season.garage.texts.front().name, "Injection pump");
    EXPECT_EQ(season.garage.catalogue.start_money_eur, 1000);
}

TEST(SeasonLoader, RejectsNonsense) {
    EXPECT_NE(error_of(with("points = [20, 17", "points = [17, 20"))
                  .find("season.points: must be >= 0, never more for a lower place"),
              std::string::npos);
    EXPECT_NE(error_of(with("points = [20,", "points = [20.5,"))
                  .find("season.points: must be whole numbers"),
              std::string::npos);
    EXPECT_NE(error_of(with("name = \"Clay-sand\"\n", ""))
                  .find("season.events: every event needs a name and a track"),
              std::string::npos);
    EXPECT_NE(error_of(with("ideal_clay_sand.toml", "missing.toml")).find("cannot open file"),
              std::string::npos);
    EXPECT_NE(error_of(with("seed = 7", "seed = -7")).find("season.seed: must not be negative"),
              std::string::npos);
    for (const char* prizes : {"[700, 800, 400]", "[700, 500.5, 400]", "[700, 500, -1]"}) {
        EXPECT_NE(error_of(with("[700, 500, 400]", prizes)).find("season.prize_money_eur"),
                  std::string::npos)
            << prizes;
    }
    EXPECT_NE(error_of(with("garage = \"../garages/ntto_standard_3500.toml\"\n", ""))
                  .find("season.garage"),
              std::string::npos);
}

// Two events, played as tp_pull season plays them: every event scores the
// table's points once, the champion has the most, and the same file gives
// the same season to the last bit.
TEST(ShippedSeason, TwoEventsScoreAndReplay) {
    const auto loaded = parse_season(kTwoEvents, "season.toml", kSeasons);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    const SeasonDefinition& season = loaded.value();
    const EventDefinition& base = season.event;
    const auto field = field::generate(base.reference, base.variation, base.class_rules,
                                       base.pullers, season.seed);
    auto championship = season::Championship::create(season.points, base.pullers);
    if (!field || !championship) {
        FAIL() << "the field or championship";
    }
    hash::Fnv1a hash;
    std::uint64_t number = 0;
    for (const SeasonRound& round : season.rounds) {
        const std::uint64_t event_seed = math::SplitMix64{season.seed + number++}.next();
        const auto order = event::running_order(base.pullers, event_seed);
        const field::Venue venue{.track = round.track.ground,
                                 .sled = sled::with_conditions(base.sled, round.track.sled),
                                 .max_duration_s = round.track.rules.max_duration_s,
                                 .lane_width_m = round.track.rules.lane_width_m};
        const auto played = field::play_event(
            base.rules, venue, *field, base.variation, base.pull_to_pull,
            std::span{order.data(), static_cast<std::size_t>(base.pullers)}, event_seed + 1);
        if (!played) {
            FAIL() << round.name;
        }
        for (const field::LogEntry& entry : played->entries()) {
            hash.add(entry.reached_m);
        }
        event::Event ev = played->event;
        ASSERT_TRUE(championship->add(ev.standings()));
    }
    const auto standings = championship->standings();
    const int awarded =
        std::accumulate(standings.begin(), standings.end(), 0,
                        [](int sum, const season::Standing& s) { return sum + s.points; });
    int table_total = 0;
    for (int place = 1; place <= base.pullers; ++place) {
        table_total += season::points_for(season.points, place);
    }
    // Pullers sharing a place both score it, so ties only add points.
    EXPECT_GE(awarded, 2 * table_total);
    EXPECT_LT(awarded, 2 * table_total + 20);
    EXPECT_EQ(standings[0].place, 1);
    EXPECT_GE(standings[0].points, standings[1].points);
    EXPECT_EQ(hash.value(), 0x77735cc039f349c5ULL);
}

}  // namespace
}  // namespace tp::data
