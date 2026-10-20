#include "tp/core/season/season.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace tp::season {
namespace {

// The club's table: 20, 17, 15, then one less per place.
Points club_points() {
    Points points;
    const std::array table = {20, 17, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3};
    std::copy(table.begin(), table.end(), points.by_place.begin());
    points.places = static_cast<int>(table.size());
    return points;
}

// An event's standings: pullers in finishing order, places as given.
std::vector<event::Standing> result(std::initializer_list<std::array<int, 2>> puller_place) {
    std::vector<event::Standing> standings;
    for (const auto& [puller, place] : puller_place) {
        standings.push_back({.puller = puller, .place = place});
    }
    return standings;
}

Championship make(int pullers) {
    const auto championship = Championship::create(club_points(), pullers);
    if (!championship) {
        std::abort();
    }
    return *championship;
}

TEST(SeasonPoints, FollowTheTable) {
    const Points points = club_points();
    EXPECT_EQ(validate(points), std::nullopt);
    EXPECT_EQ(points_for(points, 1), 20);
    EXPECT_EQ(points_for(points, 2), 17);
    EXPECT_EQ(points_for(points, 3), 15);
    EXPECT_EQ(points_for(points, 4), 14);
    EXPECT_EQ(points_for(points, 15), 3);
    EXPECT_EQ(points_for(points, 16), 0) << "beyond the table";
    EXPECT_EQ(points_for(points, 0), 0);
}

TEST(SeasonPoints, RejectsNonsense) {
    Points points = club_points();
    points.by_place[3] = 16;  // 4th more than 3rd
    EXPECT_EQ(validate(points), ParamError::kPointsOutOfOrder);
    points = club_points();
    points.by_place[14] = -1;
    EXPECT_EQ(validate(points), ParamError::kPointsOutOfOrder);
    points.places = 0;
    EXPECT_EQ(validate(points), ParamError::kPlacesOutOfRange);
    EXPECT_FALSE(Championship::create(club_points(), 0).has_value());
}

// Points add up over the events; the most points win the championship.
TEST(Championship, AddsUpThePoints) {
    Championship championship = make(3);
    championship.add(result({{0, 1}, {1, 2}, {2, 3}}));
    championship.add(result({{1, 1}, {0, 2}, {2, 3}}));
    championship.add(result({{1, 1}, {2, 2}, {0, 3}}));
    EXPECT_EQ(championship.events(), 3);
    const auto standings = championship.standings();
    ASSERT_EQ(standings.size(), 3U);
    EXPECT_EQ(standings[0].puller, 1);
    EXPECT_EQ(standings[0].points, 17 + 20 + 20);
    EXPECT_EQ(standings[0].wins, 2);
    EXPECT_EQ(standings[0].event_points[0], 17);
    EXPECT_EQ(standings[1].puller, 0);
    EXPECT_EQ(standings[1].points, 20 + 17 + 15);
    EXPECT_EQ(standings[2].points, 15 + 15 + 17);
}

// Equal points: more wins first; equal in both, a shared place.
TEST(Championship, WinsBreakATieAndEqualsShare) {
    Championship championship = make(3);
    championship.add(result({{0, 1}, {1, 2}, {2, 2}}));  // 1 and 2 share 2nd
    championship.add(result({{1, 1}, {2, 2}, {0, 3}}));
    // 0: 20 + 15, 1 win; 1: 17 + 20, 1 win; 2: 17 + 17, no win.
    const auto standings = championship.standings();
    EXPECT_EQ(standings[0].puller, 1);
    EXPECT_EQ(standings[1].puller, 0);
    EXPECT_EQ(standings[2].puller, 2);

    Championship level = make(2);
    level.add(result({{0, 1}, {1, 2}}));
    level.add(result({{1, 1}, {0, 2}}));
    EXPECT_EQ(level.standings()[0].place, 1);
    EXPECT_EQ(level.standings()[1].place, 1);
}

// A disqualified pull scores nothing, and wins nothing, whatever its place.
TEST(Championship, DisqualifiedScoresNothing) {
    Championship championship = make(3);
    std::vector<event::Standing> standings = result({{0, 1}, {1, 2}, {2, 3}});
    standings[2].disqualified = true;
    championship.add(standings);
    // Everyone disqualified: the shared first place is no win.
    std::vector<event::Standing> all_out = result({{0, 1}, {1, 1}, {2, 1}});
    for (event::Standing& s : all_out) {
        s.disqualified = true;
    }
    championship.add(all_out);
    const std::span<const Standing> totals = championship.standings();
    ASSERT_EQ(totals.size(), 3U);
    EXPECT_EQ(totals[0].puller, 0);
    EXPECT_EQ(totals[0].points, 20);
    EXPECT_EQ(totals[0].wins, 1);
    EXPECT_EQ(totals[2].puller, 2);
    EXPECT_EQ(totals[2].points, 0);
    EXPECT_EQ(totals[2].event_points[0], 0);
}

TEST(Championship, RejectsStrangersAndAFullSeason) {
    Championship championship = make(2);
    EXPECT_FALSE(championship.add(result({{0, 1}, {5, 2}})));
    EXPECT_EQ(championship.events(), 0);
    for (int i = 0; i < kMaxEvents; ++i) {
        ASSERT_TRUE(championship.add(result({{0, 1}, {1, 2}})));
    }
    EXPECT_FALSE(championship.add(result({{0, 1}, {1, 2}})));
    EXPECT_EQ(championship.standings()[0].points, 20 * kMaxEvents);
}

}  // namespace
}  // namespace tp::season
