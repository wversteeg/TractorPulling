#include "tp/core/career/career.hpp"

#include "tp/core/event/event.hpp"
#include "tp/core/season/season.hpp"

#include <gtest/gtest.h>

#include <array>
#include <optional>

namespace tp::career {
namespace {

// The tractor path as it will be (docs/career.md): Standard 3.5 t, Sport
// 2.5 and 4.5 t, Supersport 3.4, 3.6 and 4.5 t, Pro Stock, Limited Super
// Stock and Super Stock, each at belofte, promo and top; the championship
// wins a rung.
constexpr Ladder kTractorPath{.rungs = 27, .promotion_place = 1};

TEST(Career, ValidatesTheLadder) {
    EXPECT_FALSE(validate(kTractorPath));
    EXPECT_FALSE(validate(Ladder{.rungs = 1, .promotion_place = 1}));
    EXPECT_FALSE(validate(Ladder{.rungs = kMaxRungs, .promotion_place = event::kMaxPullers}));
    EXPECT_EQ(validate(Ladder{.rungs = 0, .promotion_place = 1}), ParamError::kRungCountOutOfRange);
    EXPECT_EQ(validate(Ladder{.rungs = kMaxRungs + 1, .promotion_place = 1}),
              ParamError::kRungCountOutOfRange);
    EXPECT_EQ(validate(Ladder{.rungs = 27, .promotion_place = 0}),
              ParamError::kPromotionPlaceOutOfRange);
    EXPECT_EQ(validate(Ladder{.rungs = 27, .promotion_place = event::kMaxPullers + 1}),
              ParamError::kPromotionPlaceOutOfRange);
}

TEST(Career, StartsOnTheFirstRungWithNothingAboveOpen) {
    const Progress start;
    EXPECT_TRUE(valid(start, kTractorPath));
    EXPECT_EQ(start.rung, 0);
    EXPECT_FALSE(may_move_up(start));
}

TEST(Career, TellsAProgressThatDoesNotFitTheLadder) {
    EXPECT_TRUE(valid(Progress{.rung = 2, .unlocked = 5}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.rung = -1, .unlocked = 0}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.rung = 3, .unlocked = 2}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.rung = 0, .unlocked = kTractorPath.rungs}, kTractorPath));
}

TEST(Career, WinningTheSeasonOpensTheRungAbove) {
    Progress p;
    EXPECT_TRUE(finish_season(p, kTractorPath, 1));
    EXPECT_EQ(p.unlocked, 1);
    // Opened, not moved: the player is asked.
    EXPECT_EQ(p.rung, 0);
    EXPECT_TRUE(may_move_up(p));
}

TEST(Career, OnlyThePromotionPlaceOpensARung) {
    Progress p;
    EXPECT_FALSE(finish_season(p, kTractorPath, 2));
    EXPECT_FALSE(finish_season(p, kTractorPath, 0));
    EXPECT_FALSE(may_move_up(p));
    // A ladder that promotes the podium.
    const Ladder podium{.rungs = 27, .promotion_place = 3};
    EXPECT_TRUE(finish_season(p, podium, 3));
    EXPECT_EQ(p.unlocked, 1);
}

TEST(Career, ARungOpenedStaysOpenForAPlayerWhoStays) {
    Progress p;
    ASSERT_TRUE(finish_season(p, kTractorPath, 1));
    // Defending the title: a second title opens nothing more, a lost
    // season closes nothing.
    EXPECT_FALSE(finish_season(p, kTractorPath, 1));
    EXPECT_FALSE(finish_season(p, kTractorPath, 7));
    EXPECT_EQ(p.unlocked, 1);
    EXPECT_TRUE(may_move_up(p));
}

TEST(Career, MovesUpOneRungAtATime) {
    Progress p;
    EXPECT_FALSE(move_up(p));
    ASSERT_TRUE(finish_season(p, kTractorPath, 1));
    EXPECT_TRUE(move_up(p));
    EXPECT_EQ(p.rung, 1);
    EXPECT_FALSE(may_move_up(p));
    EXPECT_FALSE(move_up(p));
    EXPECT_TRUE(valid(p, kTractorPath));
}

TEST(Career, ClimbsTheWholeLadderAndNoFurther) {
    Progress p;
    for (int season = 0; season < kTractorPath.rungs - 1; ++season) {
        ASSERT_TRUE(finish_season(p, kTractorPath, 1));
        ASSERT_TRUE(move_up(p));
    }
    EXPECT_EQ(p.rung, kTractorPath.rungs - 1);
    // The top rung's title opens nothing.
    EXPECT_FALSE(finish_season(p, kTractorPath, 1));
    EXPECT_FALSE(may_move_up(p));
    EXPECT_TRUE(valid(p, kTractorPath));
}

// The player's place is read from the championship, a shared title too.
TEST(Career, ReadsThePlayersPlaceFromTheChampionship) {
    const std::array<season::Standing, 3> second{{{.puller = 2, .place = 1, .points = 40},
                                                  {.puller = 0, .place = 2, .points = 37},
                                                  {.puller = 1, .place = 3, .points = 30}}};
    Progress p;
    EXPECT_FALSE(finish_season(p, kTractorPath, second, 0));
    EXPECT_TRUE(finish_season(p, kTractorPath, second, 2));
    EXPECT_EQ(p.unlocked, 1);

    const std::array<season::Standing, 2> shared{
        {{.puller = 1, .place = 1, .points = 40}, {.puller = 0, .place = 1, .points = 40}}};
    Progress q;
    EXPECT_TRUE(finish_season(q, kTractorPath, shared, 0));
    // Not in the field: nothing opens.
    Progress r;
    EXPECT_FALSE(finish_season(r, kTractorPath, shared, 5));
}

// For testing a class: a career can start on any rung, with nothing above
// open.
TEST(Career, StartsOnAnyRungForTesting) {
    const std::optional<Progress> top = starting_at(kTractorPath, kTractorPath.rungs - 1);
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top.value_or(Progress{}).rung, kTractorPath.rungs - 1);
    EXPECT_EQ(top.value_or(Progress{}).unlocked, kTractorPath.rungs - 1);
    EXPECT_FALSE(may_move_up(top.value_or(Progress{})));
    EXPECT_EQ(starting_at(kTractorPath, 0).value_or(Progress{.rung = 5, .unlocked = 5}).rung, 0);
    EXPECT_FALSE(starting_at(kTractorPath, kTractorPath.rungs).has_value());
    EXPECT_FALSE(starting_at(kTractorPath, -1).has_value());
}

TEST(Career, AOneRungLadderNeverPromotes) {
    const Ladder one{.rungs = 1, .promotion_place = 1};
    Progress p;
    EXPECT_FALSE(finish_season(p, one, 1));
    EXPECT_FALSE(may_move_up(p));
}

}  // namespace
}  // namespace tp::career
